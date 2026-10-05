/*
	Copyright 2026 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "types.h"
#include <asio.hpp>
#include "emulator.h"
#include "hw/modem/modem.h"
#include "netservice.h"
#include "rawmodem.h"
#include "hw/mem/addrspace.h"
#include "ice.h"
#include "util/tsqueue.h"
#include "oslib/i18n.h"
#include "oslib/oslib.h"
#include <juice/juice.h>
#include <stdlib.h>
#include <chrono>
#include <condition_variable>
#include <mutex>

//
// Emulates the Capcom Direct gaming service.
// After being matched, the two opponents' consoles disconnect from the lobby server.
// Then one peer sets his modem in answer mode and the other one calls it directly.
// Once the match is over, they disconnect and reconnect to the lobby server.
//
// Emulating all this requires a complex dance involving a special network service and server
// and a tight relationship with the modem.
// 1. Once a capcom direct game disconnects from the network, two modem callbacks are installed:
//    one when dialing out and one when detecting the modem is in answer mode.
// 2. The modem detects a dial out with a specific number, or detects answer mode.
//    The caller keeps the modem ringing.
// 3. Both peers connect to the battle server and register: game, caller/callee, battle code.
//    They start gathering local candidates for the ICE connection
//    and send them to the server/peer when ready.
// 4. The battle server matches the 2 connections and send a START message to the callee.
//    From this point on, the server only pipes data between peers without interfering.
// 5. On receiving START, the callee makes the phone ring.
// 6. The game detects the phone ringing and picks up the call to start handshaking/negotiation
// 7. The callee waits for the ICE connection to be established if needed,
//    and sends START to the caller, prompting him to start modem handshaking/negotiation.
// 8. Both peers start exchanging game data

// TODO callee modem sometimes freezes when other party leaves -> spawn, hmgeo

// TODO some chars left in modem or buffer when starting a new match (invalid packet received from server/peer, mvsc2)

// TODO intermittent lock up at battle start after PING/PONG/START (zero3, mvsc2)

// Capcom Direct games
//  1 Marvel vs. Capcom 2				T1215M
//  2 Power Stone 2						T1218M
//  3 Street Fighter III 3rd Strike		T1209M
//  4 Street Fighter Zero 3 for M.S.	T1230M
//  5 Spawn								T1216M
//  6 Vampire Chronicle for M.S.		T1235M
//  7 Netto De Tennis					T1234M
//  8 Capcom vs. SNK Pro				T1247M
//  9 Jojo's Bizarre Adventure for M.S. T1231M
// 10 Project Justice					T1221M
// 11 Super Street Fighter IIX for M.S.	T1236M
// 12 Tech Romancer for M.S.			T1232M
// 13 Taisen Net Gimmick				T1248M
// 14 Heavy Metal: Geomatrix			T1246M
// 15 Super Puzzle Fighter IIX for M.S.	T1250M

namespace net::modbba
{

const std::string games[] {
		"T1215M", "T1218M", "T1209M", "T1230M", "T1216M", "T1235M",
		"T1234M", "T1247M", "T1231M", "T1221M", "T1236M", "T1232M",
		"T1248M", "T1246M", "T1250M"
};

class CapcomIceNetService;

class CapcomDirect
{
public:
	CapcomDirect();
	~CapcomDirect();

private:
	void init();
	void term();
	void onLoadGame();
	void startCapcomDirectService();
	void stopCapcomDirectService();
	void onNetworkChange();
	static void onEvent(Event event, void *arg);
	void resetModemCallbacks();
	std::string getBattleCode();

	bool active = false;
	bool caller = false;
	uint8_t gameId = 0;
	std::unique_ptr<CapcomIceNetService> capcomService;
};

static CapcomDirect capcom;

class CapcomIceNetService : public Service
{
public:
	bool start() override
	{
		juice_set_log_level(JUICE_LOG_LEVEL_INFO);
		juice_set_log_handler(juiceLogHandler);

		initStatus = std::async(std::launch::async, [this]()
		{
			ice::STUNConfig cfg = ice::STUNConfig::get();

			juice_config_t config {};
			config.concurrency_mode = JUICE_CONCURRENCY_MODE_POLL;
			config.stun_server_host = cfg.stun_host.c_str();
			config.stun_server_port = cfg.stun_port;

			juice_turn_server_t turnServer {};
			turnServer.host = cfg.turn_host.c_str();
			turnServer.port = cfg.turn_port;
			turnServer.username = cfg.turn_username.c_str();
			turnServer.password = cfg.turn_password.c_str();
			config.turn_servers = &turnServer;
			config.turn_servers_count = 1;

			config.cb_state_changed = [](juice_agent_t *agent, juice_state_t state, void *user_ptr) {
				((CapcomIceNetService *)user_ptr)->onJuiceStateChanged(state);
			};
			config.cb_recv = [](juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
				((CapcomIceNetService *)user_ptr)->onJuiceReceive(data, size);
			};
			config.cb_gathering_done = [](juice_agent_t *agent, void *user_ptr) {
				((CapcomIceNetService *)user_ptr)->onJuiceGatheringDone();
			};
			config.user_ptr = this;

			agent = juice_create(&config);
			if (agent == nullptr) {
				ERROR_LOG(NETWORK, "Juice agent creation failed");
				return false;
			}
			juice_gather_candidates(agent);

			return true;
		});

		return true;
	}

	void sendRegister(int game, bool caller, u32 battleCode)
	{
		this->caller = caller;
		serverConn = std::make_unique<BattleServerConn>(*this);
		serverConn->connect(game, caller, battleCode);
	}

	void stop() override
	{
		juiceCompleted = false;
		txBuffering.stop();
		if (getJuiceAgent() != nullptr) {
			juice_destroy(agent);
			agent = nullptr;
		}
		if (serverConn != nullptr) {
			serverConn->disconnect();
			serverConn.reset();
		}
	}

	void sendStart()
	{
		{
			std::lock_guard<std::mutex> _(mutex);
			if (!juiceCompleted) {
				startSent = true;
				return;
			}
		}
		if (serverConn == nullptr)
			ERROR_LOG(NETWORK, "Can't send Start: serverConn is null");
		else
			serverConn->sendStart();
	}

	bool receiveStart()
	{
		if (serverConn == nullptr)
			return false;
		return serverConn->receiveStart();
	}

	void writeModem(u8 b) override
	{
		// Games packet sizes:
		// mvsc2		4
		// pstone2		34
		// sf3strike3	16
		// sfzero3		16
		// spawn		20
		// vampire		6
		// tennis		20
		// cvspro		16
		// jojo			11, 17 or 23
		// pjustice		16
		// ssf8			16
		// techromancer	20
		// taisenng		28
		// hmgeo		12
		// puzzle		16

		txQueue.push(b);
		txBuffering.reset();
		if (startSync && b != 0xa && (b < 'A' || b > 'Z'))
		{
			// Crude initial synchronization
			u64 t0 = getTimeMs();
			while (rxQueue.empty() && getTimeMs() -  t0 < 500)
				;
			startSync = false;
		}
	}

	void flushTxQueue()
	{
		if (txQueue.empty() || !juiceCompleted)
			return;
		std::string data;
		data.reserve(txQueue.size());
		/*
		// Detect packet losses
		data.reserve(txQueue.size() + 4);
		++txSeq;
		data.push_back(txSeq);
		data.push_back(txSeq >> 8);
		data.push_back(txSeq >> 16);
		data.push_back(txSeq >> 24);
		*/
		while (!txQueue.empty())
			data.push_back((char)txQueue.pop());
		int ret = juice_send(getJuiceAgent(), data.c_str(), data.size());
		if (ret)
			WARN_LOG(NETWORK, "juice send failed: %d", ret);
	}

	int readModem() override
	{
		if (rxQueue.empty())
			return -1;
		u8 c = rxQueue.pop();
		if (startSync && c != 0xa && (c < 'A' || c > 'Z'))
			startSync = false;
		return c;
	}
	int modemAvailable() override {
		return rxQueue.size();
	}

	void receiveEthFrame(const u8 *frame, u32 size) override {
	}

	juice_agent_t *getJuiceAgent()
	{
		if (initStatus.valid())
			if (!initStatus.get())
				return nullptr;
		return agent;
	}

private:
	void onJuiceGatheringDone()
	{
		if (agent == nullptr)
			return;
		char sdp[JUICE_MAX_SDP_STRING_LEN];
		juice_get_local_description(agent, sdp, JUICE_MAX_SDP_STRING_LEN);

		if (serverConn == nullptr) {
			ERROR_LOG(NETWORK, "Can't send candidates: serverConn is null");
			return;
		}
		serverConn->sendCandidates(sdp);
	}

	void onJuiceStateChanged(juice_state_t state)
	{
		if (state == JUICE_STATE_COMPLETED)
		{
			char local[128] {};
			juice_get_selected_candidates(agent, local, sizeof(local), NULL, 0);
			INFO_LOG(NETWORK, "Juice connection completed: using %s", local);
			std::string connType;
			if (strstr(local, "typ host") != nullptr)
				connType = i18n::Ts("Direct peer to peer");
			else if (strstr(local, "typ relay") != nullptr)
				connType = i18n::Ts("Using relay");
			else
				// typ srflx: server reflexive (NAT)
				// typ prflx: peer reflexive (NAT)
				connType = i18n::Ts("NAT");
			os_notify(i18n::T("Connected to peer"), 5000, connType.c_str());

			bool startSendNow = false;
			{
				std::lock_guard<std::mutex> _(mutex);
				startSendNow = startSent;
				juiceCompleted = true;
			}
			if (!caller && startSendNow)
			{
				if (serverConn == nullptr) {
					ERROR_LOG(NETWORK, "Can't send start: serverConn is null");
					return;
				}
				serverConn->sendStart();
			}
			txBuffering.start();
		}
		else if (state == JUICE_STATE_FAILED)
		{
			WARN_LOG(NETWORK, "ICE connection failed or closed");
			// TODO something?
		}
	}

	void onJuiceReceive(const char *data, size_t len)
	{
		/*
		// Detect packet losses
		u32 seq;
		memcpy(&seq, data, 4);
		if (seq != rxSeq)
			WARN_LOG(NETWORK, "Packet out of order: expected %d got %d", rxSeq, seq);
		rxSeq = seq + 1;
		len -= 4;
		data += 4;
		*/
		while (len--)
			rxQueue.push(*data++);
	}

	static void juiceLogHandler(juice_log_level_t jlevel, const char *message)
	{
		LogTypes::LOG_LEVELS level;
		switch (jlevel)
		{
		case JUICE_LOG_LEVEL_NONE:
		case JUICE_LOG_LEVEL_VERBOSE:
		case JUICE_LOG_LEVEL_DEBUG:
		default:
			level = LogTypes::LOG_LEVELS::LDEBUG;
			break;
		case JUICE_LOG_LEVEL_INFO:
			level = LogTypes::LOG_LEVELS::LINFO;
			break;
		case JUICE_LOG_LEVEL_WARN:
			if (strstr(message, "TURN CreatePermission") != nullptr)
				// error pops up a lot with standard.relay.metered.ca
				level = LogTypes::LOG_LEVELS::LINFO;
			else
				level = LogTypes::LOG_LEVELS::LWARNING;
			break;
		case JUICE_LOG_LEVEL_ERROR:
		case JUICE_LOG_LEVEL_FATAL:
			level = LogTypes::LOG_LEVELS::LERROR;
			break;
		}
		GenericLog(level, LogTypes::LOG_TYPE::NETWORK, __FILE__, __LINE__, "%s", message);
	}

	class BattleServerConn
	{
	public:
		BattleServerConn(CapcomIceNetService& service)
			: service(service)
		{}

		void connect(int game, bool caller, u32 battleCode)
		{
			io_context = std::make_unique<asio::io_context>();
			socket = std::make_unique<asio::ip::tcp::socket>(*io_context);
			thread = std::thread([this, game, caller, battleCode]() {
				try {
					// Resolve host name
					std::string hostname = "capcom.flyca.st";
					asio::ip::tcp::resolver resolver(*io_context);
					asio::error_code ec;
					auto it = resolver.resolve(hostname, std::to_string(7658), ec);
					if (ec)
						throw FlycastException(ec.message());
					if (it.empty())
						throw FlycastException(i18n::Ts("Host not found"));
					asio::ip::tcp::endpoint endpoint = *it.begin();

					// Connect to host
					socket->connect(endpoint, ec);
					if (ec)
						throw FlycastException(ec.message());
					os_notify(i18n::T("Connected to battle server"), 5000, hostname.c_str());
					receive();
					sendRegister(game, caller, battleCode);

					io_context->run();
				} catch (const FlycastException& e) {
					ERROR_LOG(NETWORK, "Capcom connection error: %s", e.what());
					os_notify(i18n::T("Can't connect to battle server"), 8000, e.what());
				} catch (const std::runtime_error& e) {
					ERROR_LOG(NETWORK, "BattleServerConn::thread: error: %s", e.what());
				}
			});
		}

		void disconnect()
		{
			close();
			socket.reset();
			if (io_context != nullptr)
			{
				io_context->stop();
				thread.join();
				io_context.reset();
			}
		}

		void sendStart()
		{
			if (io_context == nullptr)
				return;
			io_context->post([this]() {
				outPacket.insert(outPacket.end(), std::begin(MAGIC), std::end(MAGIC));
				outPacket.push_back(2);
				outPacket.push_back(0);
				send();
			});
		}

		void sendCandidates(const char *cands)
		{
			if (io_context == nullptr)
				return;
			std::string candidates = cands;
			io_context->post([this, candidates]() {
				outPacket.insert(outPacket.end(), std::begin(MAGIC), std::end(MAGIC));
				outPacket.push_back(3);
				size_t len = candidates.size();
				if (len <= 0xfe) {
					outPacket.push_back(len);
				}
				else
				{
					outPacket.push_back(0xff);
					outPacket.push_back(len & 0xff);
					outPacket.push_back(len >> 8);
				}
				outPacket.insert(outPacket.end(), (const u8 *)&candidates[0], (const u8 *)&candidates[len]);
				send();
			});
		}

		bool receiveStart() const {
			return startReceived;
		}

	private:
		void sendRegister(int game, bool caller, u32 battleCode)
		{
			outPacket.insert(outPacket.end(), std::begin(MAGIC), std::end(MAGIC));
			outPacket.push_back(1);
			outPacket.push_back(6);
			outPacket.push_back(game);
			outPacket.push_back(caller);
			outPacket.insert(outPacket.end(), (const u8 *)&battleCode, (const u8 *)&battleCode + 4);
			send();
		}

		using iterator = asio::buffers_iterator<asio::const_buffers_1>;

		std::pair<iterator, bool>
		static packetMatcher(iterator begin, iterator end)
		{
			if (end - begin < 6)
				return std::make_pair(begin, false);
			iterator it = begin;
			u16 len = (u8)*(it + 5);
			if (len == 0xff)
			{
				if (end - begin < 8)
					return std::make_pair(begin, false);
				len = (u8)*(it + 6) | ((u8)*(it + 7) << 8);
				len += 2;
			}
			len += 6;
			if (end - begin < len)
				return std::make_pair(begin, false);
			else
				return std::make_pair(begin + len, true);
		}

		void receive()
		{
			asio::async_read_until(*socket, asio::dynamic_buffer(packet, 64_KB), packetMatcher,
					std::bind(&BattleServerConn::handlePacket, this,
									asio::placeholders::error,
									asio::placeholders::bytes_transferred));
		}
		void handlePacket(const std::error_code& ec, size_t len)
		{
			if (ec || len == 0)
			{
				if (ec && ec != asio::error::eof && ec != asio::error::operation_aborted)
					ERROR_LOG(NETWORK, "Receive error: %s", ec.message().c_str());
				close();
				return;
			}
			if (memcmp(packet.data(), MAGIC, sizeof(MAGIC)))
			{
				ERROR_LOG(NETWORK, "Invalid magic number in packet");
				close();
				return;
			}
			switch (packet[4])
			{
			case 2: // start
				startReceived = true;
				break;
			case 3: // candidates
				{
					unsigned start;
					if (packet[5] == 0xff)
						start = 8;
					else
						start = 6;
					std::string candidates = std::string((const char *)&packet[start], (const char *)&packet[len]);
					INFO_LOG(NETWORK, "Peer candidates: %s", candidates.c_str());
					juice_set_remote_description(service.getJuiceAgent(), candidates.c_str());
					break;
				}
			default:
				ERROR_LOG(NETWORK, "Invalid packet type: %d", packet[4]);
				break;
			}
			packet.erase(packet.begin(), packet.begin() + len);
			receive();
		}

		void send()
		{
			if (sending)
				return;
			sending = true;
			asio::async_write(*socket, asio::buffer(outPacket),
				std::bind(&BattleServerConn::onSent, this,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));
		}
		void onSent(const std::error_code& ec, size_t len)
		{
			if (ec || len == 0)
			{
				if (ec && ec != asio::error::eof)
					WARN_LOG(COMMON, "Write error: %s", ec.message().c_str());
				close();
				return;
			}
			sending = false;
			if (len < outPacket.size()) {
				outPacket.erase(outPacket.begin(), outPacket.begin() + len);
				send();
			}
			else {
				outPacket.clear();
			}
		}

		void close()
		{
			if (socket != nullptr) {
				std::error_code ignored;
				socket->close(ignored);
			}
		}

		CapcomIceNetService& service;
		std::thread thread;
		std::unique_ptr<asio::io_context> io_context;
		std::unique_ptr<asio::ip::tcp::socket> socket;
		std::vector<u8> packet;
		std::vector<u8> outPacket;
		bool sending = false;
		bool startReceived = false;
	};

	class TxBuffering
	{
	public:
		TxBuffering(CapcomIceNetService& service, int timeoutMs)
			: service(service), timeoutMs(timeoutMs)
		{}

		void start()
		{
			stopping = false;
			thread = std::thread([this]() {
				while (true)
				{
					std::unique_lock lk(mutex);
					condvar.wait_for(lk, std::chrono::milliseconds(timeoutMs));
					if (stopping)
						break;
					if (resetting) {
						resetting = false;
						continue;
					}
					lk.unlock();
					service.flushTxQueue();
				}
			});
		}
		void stop()
		{
			if (thread.joinable())
			{
				{
					std::lock_guard<std::mutex> _(mutex);
					stopping = true;
				}
				condvar.notify_one();
				thread.join();
			}
		}
		void reset()
		{
			{
				std::lock_guard<std::mutex> _(mutex);
				resetting = true;
			}
			condvar.notify_one();
		}

	private:
		CapcomIceNetService& service;
		const int timeoutMs;
		std::thread thread;
		std::mutex mutex;
		std::condition_variable condvar;
		bool stopping = false;
		bool resetting = false;
	};

	juice_agent_t *agent = nullptr;
	std::future<bool> initStatus;
	TsQueue<u8> rxQueue;
	TsQueue<u8> txQueue;
	TxBuffering txBuffering { *this, 2 };
	std::unique_ptr<BattleServerConn> serverConn;
	bool caller = false;
	bool startSync = true;
	bool juiceCompleted = false;
	bool startSent = false;
	std::mutex mutex;
	//u32 rxSeq = 1;
	//u32 txSeq = 0;

	static constexpr u8 MAGIC[] { 0xBA, 0x11, 0x1E, 0x01 };
};

CapcomDirect::CapcomDirect() {
	init();
}
CapcomDirect::~CapcomDirect() {
	term();
}
void CapcomDirect::init()
{
	EventManager::listen(Event::Start, onEvent, this);
	EventManager::listen(Event::Terminate, onEvent, this);
	EventManager::listen(Event::Network, onEvent, this);
	EventManager::listen(Event::LoadState, onEvent, this);
}
void CapcomDirect::term()
{
	EventManager::unlisten(Event::Start, onEvent, this);
	EventManager::unlisten(Event::Terminate, onEvent, this);
	EventManager::unlisten(Event::Network, onEvent, this);
	EventManager::unlisten(Event::LoadState, onEvent, this);
}
void CapcomDirect::onLoadGame()
{
	gameId = 0;
	for (unsigned i = 0; i < std::size(games); i++)
		if (games[i] == settings.content.gameId) {
			gameId = i + 1;
			break;
		}
	active = gameId != 0;
}

void CapcomDirect::startCapcomDirectService()
{
	INFO_LOG(NETWORK, "Capcom network service started");
	capcomService = std::make_unique<CapcomIceNetService>();
	setCustomService(capcomService.get());
	capcomService->start();
	std::string battleCode = getBattleCode();
	INFO_LOG(NETWORK, "Capcom battle code: %s (%s)", battleCode.c_str(), caller ? "caller" : "callee");
	u32 shortCode = strtol(battleCode.substr(0, 8).c_str(), nullptr, 16);
	capcomService->sendRegister(gameId, caller, shortCode);
	if (caller)
		// Keep the phone ringing until we receive Start from the callee
		modemKeepRinging(true);
	modemPeriodicCallback(100_sh4ms, [this]() {
		if (capcomService->receiveStart())
		{
			INFO_LOG(NETWORK, "%s: Start received", caller ? "caller" : "callee");
			// TODO p2p mode isn't working
			modemPeer2Peer(false);
			if (caller) {
				// Pick up the phone and start the handshake
				modemKeepRinging(false);
			}
			else
			{
				// Simulate an incoming call (ringing)
				modemIncomingCall();
				// Wait for the handshake to begin, then send Start to the caller
				modemOnHandshake([this]() {
					capcomService->sendStart();
					modemOnHandshake({});
				});
			}
			modemPeriodicCallback(0, {});
		}
	});
}
void CapcomDirect::stopCapcomDirectService()
{
	setCustomService(nullptr);
	if (capcomService != nullptr) {
		capcomService->stop();
		capcomService.reset();
	}
	resetModemCallbacks();
	INFO_LOG(NETWORK, "Capcom network service stopped");
}

void CapcomDirect::onNetworkChange()
{
	if (!active)
		return;
	if (!settings.network.online && capcomService == nullptr)
	{
		// detect answering mode
		modemOnAnswerMode([this]() {
			caller = false;
			startCapcomDirectService();
			modemOnAnswerMode({});
		});
		// detect calling party using phone#
		modemOnDial([this](const std::string& number) {
			if (number == "20130201")
			{
				caller = true;
				startCapcomDirectService();
				modemOnAnswerMode({});
			}
			else {
				stopCapcomDirectService();
			}
		});
		modemPeer2Peer(false);
	}
}

void CapcomDirect::onEvent(Event event, void *arg)
{
	CapcomDirect *self = (CapcomDirect *)arg;
	switch (event)
	{
	case Event::Start:
		self->onLoadGame();
		break;
	case Event::Terminate:
		self->active = false;
		self->stopCapcomDirectService();
		break;
	case Event::Network:
		self->onNetworkChange();
		break;
	case Event::LoadState:
		self->stopCapcomDirectService();
		break;
	default:
		break;
	}
}

void CapcomDirect::resetModemCallbacks()
{
	modemOnAnswerMode({});
	modemOnDial({});
	modemOnHandshake({});
	modemPeriodicCallback(0, {});
	modemPeer2Peer(false);
	modemKeepRinging(false);
}

std::string CapcomDirect::getBattleCode()
{
	constexpr u32 addresses[] {
		0x8c2b522c, // mvsc2
		0x0c81d410, // pstone2
		0x8ceadbd4, // sf3strike3
		0x8c223b54, // sfzero3
		0x0c2de580, // spawn
		0x8c340c68, // vampire
		0x0c237a1c, // tennis
		0x8c30f46c, // cvspro
		0x8c8997c8, // jojo
		0x0c3f5974, // pjustice
		0x8c31e6c8, // ssf8
		0x8c4af628, // tech romancer
		0x8c42b320, // taisen net gimmick
		0x0c2d6d00, // hmgeo
		0x0c2450dc, // puzzle8
	};
	if (gameId == 0 || gameId > std::size(addresses))
		return {};
	const u32 address = addresses[(int)gameId - 1];
	std::string battleCode;
	for (int i = 0; i < 14; i++)
		battleCode += addrspace::read8(address + i);
	return battleCode;
}

} // namespace
