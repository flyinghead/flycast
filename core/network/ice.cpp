/*
	Copyright 2025 Flyinghead <flyinghead.github@gmail.com>

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
#ifdef USE_ICE
#include "ice.h"
#include "types.h"
#include "hw/sh4/modules/modules.h"
#include "util/tsqueue.h"
#include "oslib/oslib.h"
#include "oslib/http_client.h"
#include "emulator.h"
#include "log/LogManager.h"
#include "ui/gui.h"
#include "ui/gui_util.h"
#include "stdclass.h"
#include "hw/sh4/sh4_sched.h"
#include <juice/juice.h>
#include <cstring>
#include <memory>
#include <atomic>
#include <mutex>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

namespace ice
{

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
		level = LogTypes::LOG_LEVELS::LWARNING;
		break;
	case JUICE_LOG_LEVEL_ERROR:
	case JUICE_LOG_LEVEL_FATAL:
		level = LogTypes::LOG_LEVELS::LERROR;
		break;
	}
	GenericLog(level, LogTypes::LOG_TYPE::NETWORK, __FILE__, __LINE__, "%s", message);
}

class Writer;

class IceSession : public SerialPort::Pipe
{
	struct CustomWsppConfig : public websocketpp::config::asio_client
	{
		static bool const enable_multithreading = false;
		static const size_t connection_read_buffer_size = 4_KB;
		static const size_t max_message_size = 2_KB;
		static const size_t max_http_body_size = 2_KB;
	};
	using WsClient = websocketpp::client<CustomWsppConfig>;

public:
	~IceSession()
	{
		EventManager::unlisten(Event::Start, onEmuEvent, this);
		EventManager::unlisten(Event::Terminate, onEmuEvent, this);
		EventManager::unlisten(Event::LoadState, onEmuEvent, this);
		destroyJuiceAgent();
		destroyWebSocket();
		SCIFSerialPort::Instance().setPipe(nullptr);
	}

	void init(const std::string& username, bool matchCode)
	{
		{
			Lock _(mutex);
			userlist.clear();
			chat.clear();
		}
		this->matchCode = matchCode;
		std::string room;
		if (matchCode) {
			this->username.clear();
			room = "meet" + http::urlEncode(username);
		}
		else
		{
			if (isF355())
				room = "f355";
			else if (settings.content.gameId == "MAXIMUM SPEED")
				room = "maxspeed";
			else if (settings.content.gameId == "HDR-0040")		// Cyber Troopers Virtual On: Oratorio Tangram (JP)
				room = "vonot";
			else if (settings.content.gameId == "T6804M")		// Aero Dancing F	FIXME not working.
				room = "aerof";
			else if (settings.content.gameId == "T6807M")		// Aero Dancing i
				room = "aeroi";
			// TODO sega tetris, hell gate
			else
				throw FlycastException("Game not supported");
			this->username = username;
		}
		EventManager::listen(Event::Start, onEmuEvent, this);
		EventManager::listen(Event::Terminate, onEmuEvent, this);
		EventManager::listen(Event::LoadState, onEmuEvent, this);
		createWebSocket(room);
	}

	State getState() const {
		return state;
	}

	std::string getStatusText() const {
		Lock _(mutex);
		return statusText;
	}

	std::vector<std::string> getUserList() const {
		Lock _(mutex);
		return userlist;
	}

	std::string getChallenger() const {
		Lock _(mutex);
		return opponent;
	}

	void sendChallenge(const std::string& user)
	{
		if (state != Online && state != ChalRefused)
			return;
		{
			Lock _(mutex);
			opponent = user;
		}
		send("challenge " + user);
		state = ChalSent;
	}

	void respondChallenge(bool accept)
	{
		if (state == ChalReceived)
		{
			{
				Lock _(mutex);
				send("chalresp " + opponent + " " + std::to_string((int)accept));
			}
			if (accept) {
				state = ChalAccepted;
				createJuiceAgent();
			}
			else {
				state = Online;
				Lock _(mutex);
				opponent.clear();
			}
		}
	}

	void say(const std::string& msg)
	{
		if (state != Offline)
			send("say " + msg);
		addChat(username + ": " + msg);
	}

	std::vector<std::string> getChat() {
		Lock _(mutex);
		return chat;
	}

	struct Stats
	{
		float txQueueSize;
		float rxQueueSize;
		float rxSpeed;
		float txSpeed;
	};

	Stats getStats()
	{
		u64 now = getTimeMs();
		Stats s {};
		if (stat.timestamp != 0)
		{
			if (stat.timestamp == now) {
				s = stat.last;
				return s;
			}
			float cur = (float)stat.rxBytes.exchange(0) * 1000 / (now - stat.timestamp);
			s.rxSpeed = stat.last.rxSpeed * .3f + cur * .7f;
			cur = (float)stat.txBytes.exchange(0) * 1000 / (now - stat.timestamp);
			s.txSpeed = stat.last.txSpeed * .3f + cur * .7f;
			s.rxQueueSize = (float)recvQueue.size() * .7f + stat.last.rxQueueSize * .3f;
			s.txQueueSize = (float)txBufferSize * .7f + stat.last.txQueueSize * .3f;
			stat.last = s;
		}
		stat.timestamp = now;
		return s;
	}

private:
	void createWebSocket(const std::string& room)
	{
		// Initialize websocketpp
		wsclient.clear_access_channels(websocketpp::log::alevel::all);
		wsclient.clear_error_channels(websocketpp::log::elevel::all);
		wsclient.init_asio();
		wsclient.start_perpetual();
		asioThread.reset(new std::thread(&WsClient::run, &wsclient));
		// Connect to lobby server
		std::error_code ec;
		WsClient::connection_ptr con = wsclient.get_connection("ws://lobby.flyca.st/" + room, ec);
		if (ec)
			throw FlycastException("Connection to lobby failed: " + ec.message());
		hdl = con->get_handle();
		// Set handlers
		con->set_open_handler([this](websocketpp::connection_hdl hdl)
		{
			state = Online;
			if (!matchCode) {
				send("join " + this->username);
				addChat(": " + this->username + " joined");
			}
		});
		con->set_fail_handler([this](websocketpp::connection_hdl hdl)
		{
			WsClient::connection_ptr con = wsclient.get_con_from_hdl(hdl);
			{
				Lock _(mutex);
				statusText = con->get_ec().message();
			}
			WARN_LOG(NETWORK, "Connection to lobby failed: %s", statusText.c_str());
			if (state != Playing)
				state = Offline;
			hdl.reset();
		});
		con->set_close_handler([this](websocketpp::connection_hdl hdl)
		{
			WsClient::connection_ptr con = wsclient.get_con_from_hdl(hdl);
			INFO_LOG(NETWORK, "Connection to lobby closed: %s %s",
					websocketpp::close::status::get_string(con->get_remote_close_code()).c_str(),
					con->get_remote_close_reason().c_str());
			if (state != Playing)
			{
				state = Offline;
				if (con->get_remote_close_code() != websocketpp::close::status::going_away) {
					Lock _(mutex);
					statusText = con->get_remote_close_reason().empty() ?
							websocketpp::close::status::get_string(con->get_remote_close_code())
							: con->get_remote_close_reason();
				}
			}
			hdl.reset();
			{
				Lock _(mutex);
				userlist.clear();
				chat.clear();
			}
		});
		con->set_message_handler([this](websocketpp::connection_hdl hdl, WsClient::message_ptr msg) {
			onWsReceive(msg->get_payload());
		});
		wsclient.connect(con);
	}

	void destroyWebSocket()
	{
		if (asioThread != nullptr && asioThread->joinable())
		{
			wsclient.stop_perpetual();
			std::error_code ec;
			wsclient.close(hdl, websocketpp::close::status::going_away, "", ec);
			if (ec && ec != websocketpp::error::bad_connection)
				INFO_LOG(NETWORK, "Error closing connection: %s", ec.message().c_str());
			asioThread->join();	// FIXME risk of being blocked here
			asioThread.reset();
		}
	}

	void send(const std::string& message)
	{
		std::error_code ec;
   		wsclient.send(hdl, message, websocketpp::frame::opcode::text, ec);
   		if (ec)
   			INFO_LOG(NETWORK, "Error sending message: %s", ec.message().c_str());
	}

	void onWsReceive(const std::string& message)
	{
		DEBUG_LOG(NETWORK, "onWsReceive: %s", message.c_str());
		auto spc = message.find(' ');
		std::string op = message.substr(0, spc);
		std::string args;
		if (spc != std::string::npos)
			args = message.substr(spc + 1);
		if (op == "join") {
			addChat(": " + args + " joined");
			Lock _(mutex);
			userlist.emplace_back(std::move(args));
		}
		else if (op == "userlist")
		{
			Lock _(mutex);
			userlist.clear();
			while (true)
			{
				spc = args.find(' ');
				if (spc == std::string::npos) {
					if (!args.empty())
						userlist.emplace_back(std::move(args));
					break;
				}
				else {
					userlist.push_back(args.substr(0, spc));
					args = args.substr(spc + 1);
				}
			}
		}
		else if (op == "candidate") {
			if (agent != nullptr)
				juice_set_remote_description(agent, args.c_str());
		}
		else if (op == "leave")
		{
			{
				Lock _(mutex);
				for (auto it = userlist.begin(); it != userlist.end(); ++it)
					if (*it == args) {
						userlist.erase(it);
						break;
					}
			}
			if (!matchCode)
				addChat(": " + args + " left");
			Lock _(mutex);
			if ((state == ChalSent || state == ChalReceived) && opponent == args) {
				state = Online;
				opponent.clear();
			}
		}
		else if (op == "challenge")
		{
			if (state != Online && state != ChalRefused) {
				// busy -> refused
				send("chalresp " + args + " 0");
			}
			else {
				state = ChalReceived;
				{
					Lock _(mutex);
					opponent = args;
				}
				if (matchCode) {
					state = ChalAccepted;
					createJuiceAgent();
				}
			}
		}
		else if (op == "chalresp")
		{
			if (state == ChalSent)
			{
				if (args == "0") {
					state = ChalRefused;
					Lock _(mutex);
					opponent.clear();
				}
				else {
					state = ChalAccepted;
					createJuiceAgent();
				}
			}
		}
		else if (op == "say") {
			addChat(args);
		}
		else {
			WARN_LOG(NETWORK, "Invalid ws message: %s", message.c_str());
		}
	}

	void addChat(const std::string& text)
	{
		Lock _(mutex);
		chat.push_back(text);
		if (chat.size() > 20)
			chat.erase(chat.begin());
	}

	void createJuiceAgent()
	{
		juice_set_log_level(JUICE_LOG_LEVEL_INFO);
		juice_set_log_handler(juiceLogHandler);
		juice_config_t config {};
		config.stun_server_host = "lobby.flyca.st";
		config.stun_server_port = 25001;
		config.cb_state_changed = [](juice_agent_t *agent, juice_state_t state, void *user_ptr) {
			((IceSession *)user_ptr)->onJuiceStateChanged(state);
		};
		config.cb_recv = [](juice_agent_t *agent, const char *data, size_t size, void *user_ptr) {
			((IceSession *)user_ptr)->onJuiceReceive(data, size);
		};
		config.cb_gathering_done = [](juice_agent_t *agent, void *user_ptr) {
			((IceSession *)user_ptr)->onJuiceGatheringDone();
		};
		config.user_ptr = this;
		juice_turn_server_t turnServer {};
		turnServer.host = config.stun_server_host;
		turnServer.port = config.stun_server_port;
		turnServer.username = "flycast";
		turnServer.password = "rules";
		config.turn_servers = &turnServer;
		config.turn_servers_count = 1;

		agent = juice_create(&config);
		if (agent == nullptr)
			throw FlycastException("Connection failed");
		juice_gather_candidates(agent);
	}

	void destroyJuiceAgent()
	{
		if (agent != nullptr)
			juice_destroy(agent);
		agent = nullptr;
	}

	void sendCandidates()
	{
		Lock _(mutex);
		if (opponent.empty() || agent == nullptr)
			return;
		char sdp[JUICE_MAX_SDP_STRING_LEN];
		juice_get_local_description(agent, sdp, JUICE_MAX_SDP_STRING_LEN);
		send("candidate " + opponent + " " + std::string(sdp));
	}

	void onJuiceStateChanged(juice_state_t state)
	{
		if (state == JUICE_STATE_COMPLETED)
		{
			char local[128] {};
			juice_get_selected_candidates(agent, local, sizeof(local), NULL, 0);
			INFO_LOG(NETWORK, "Juice connection completed: using %s", local);
			{
				Lock _(mutex);
				if (strstr(local, "typ host") != nullptr)
					statusText = "Direct peer to peer";
				else if (strstr(local, "typ relay") != nullptr)
					statusText = "Using relay";
				else
					// typ srflx: server reflexive (NAT)
					// typ prflx: peer reflexive (NAT)
					statusText = "NAT";
			}
			this->state = Playing;
			setWriter();
			SCIFSerialPort::Instance().setPipe(this);
			// TODO disconnect from websocket while in game?
		}
		else if (state == JUICE_STATE_FAILED && this->state == Playing)
		{
			os_notify("Connection lost", 5000);
			if (asioThread != nullptr)
			{
				if (matchCode)
				{
					// Disconnect from the lobby when using a match code.
					// Actually we might want to disconnect as soon as the connection is completed.
					destroyWebSocket();
					this->state = Offline;
				}
				else {
					this->state = Online;
				}
			}
			else {
				this->state = Offline;
			}
			Lock _(mutex);
			statusText = "Connection lost";
		}
	}
	void onJuiceReceive(const char *data, size_t size)
	{
		stat.rxBytes += size;
		while (size-- != 0)
			recvQueue.push(*data++);
	}
	void onJuiceGatheringDone() {
		sendCandidates();
	}

	bool isF355() const {
		return settings.content.gameId == "MK-0100"			// F355 (US)
				|| settings.content.gameId == "T8118D  50"	// F355 (EU)
				|| settings.content.gameId == "HDR-0100";	// F355 (JP)
	}

	void emuStartGame()
	{
		// If we are connected, restore the pipe interface when starting a new game
		if (state == Playing) {
			setWriter();
			SCIFSerialPort::Instance().setPipe(this);
		}
	}
	void emuTerminateGame()
	{
		// Leave the lobby but keep the active juice session
		destroyWebSocket();
		if (state != Playing)
			state = Offline;
		if (dump != nullptr) {
			fclose(dump);
			dump = nullptr;
		}
		txBufferSize = 0;
		recvQueue.clear();
		writer.reset();
	}

	static void onEmuEvent(Event event, void *arg)
	{
		IceSession *ice = (IceSession *)arg;
		switch (event)
		{
		case Event::Start:
			ice->emuStartGame();
			break;
		case Event::Terminate:
			ice->emuTerminateGame();
			break;
		case Event::LoadState:
			ice->recvQueue.clear();
			ice->txBufferSize = 0;
			break;
		default:
			break;
		}
	}

	// Serial port
	void write(u8 data) override;

	void sendBreak() override {
		if (state == Playing)
			WARN_LOG(NETWORK, "ice: Ignoring sent break");
	}
	int available() override {
		return recvQueue.size();
	}
	u8 read() override
	{
		if (recvQueue.empty())
			return 0;
		else
			return recvQueue.pop();
	}

	void flushTxBuffer()
	{
		juice_send(agent, (const char *)&txBuffer[0], txBufferSize);
		stat.txBytes += txBufferSize;
		txBufferSize = 0;
	}

	void setWriter();

	juice_agent_t *agent = nullptr;
	WsClient wsclient;
	std::unique_ptr<std::thread> asioThread;
	websocketpp::connection_hdl hdl;
	mutable std::mutex mutex;
	std::string username;
	std::vector<std::string> userlist;
	std::string opponent;
	State state = Offline;
	std::string statusText;
	TsQueue<u8> recvQueue;
	std::vector<std::string> chat;
	bool matchCode = false;
	std::array<uint8_t, 256> txBuffer;
	u32 txBufferSize = 0;
	struct {
		u64 timestamp = 0;
		std::atomic_int rxBytes = 0;
		std::atomic_int txBytes = 0;
		Stats last {};
	} stat;
	std::unique_ptr<Writer> writer;
	FILE *dump = nullptr;

	using Lock = std::lock_guard<std::mutex>;
	friend class F355Writer;
	friend class StandardWriter;
	friend class MaxSpeedWriter;
};

class Writer
{
public:
	Writer(IceSession& ice) : ice(ice) {}
	virtual ~Writer() {}
	virtual void write() = 0;

protected:
	IceSession& ice;
};

class StandardWriter : public Writer
{
public:
	StandardWriter(IceSession& ice) : Writer(ice) {}
	~StandardWriter() {
		if (sh4SchedId != -1)
			sh4_sched_unregister(sh4SchedId);
	}

	void write() override
	{
		if (sh4SchedId == -1)
			sh4SchedId = sh4_sched_register(0, sh4ShedCallback, this);
		sh4_sched_request(sh4SchedId, SH4_CYCLES);
	}

private:
	static int sh4ShedCallback(int tag, int cycles, int jitter, void *arg)
	{
		StandardWriter *self = (StandardWriter *)arg;
		if (self->ice.txBufferSize != 0)
			self->ice.flushTxBuffer();
		return 0;
	}

	int sh4SchedId = -1;
	constexpr static int SH4_CYCLES = SH4_MAIN_CLOCK / 10000 * 25;	// 2.5 ms seems to be fine for vonot at least
};

class F355Writer : public Writer
{
public:
	F355Writer(IceSession& ice) : Writer(ice) {}

	void write() override
	{
		curSize++;
		// Parse F355 packets
		// 2 types:
		// 'L' 'Q' followed by 32 bytes
		// 'X' 'X' followed by 156 bytes (only in race)
		switch (curSize)
		{
		case 1:
			switch (ice.txBuffer[0])
			{
			case 'L':
				LQ = true;
				break;
			case 'X':
				LQ = false;
				break;
			default:
				// resynchronize
				INFO_LOG(NETWORK, "Resync: size %d\n", curSize);
				ice.flushTxBuffer();
				curSize = 0;
				break;
			}
			break;
		case 2:
			if ((ice.txBuffer[0] == 'L' && ice.txBuffer[1] != 'Q')
					|| (ice.txBuffer[0] == 'X' && ice.txBuffer[1] != 'X')) {
				// resynchronize
				INFO_LOG(NETWORK, "Resync: size %d\n", curSize);
				ice.flushTxBuffer();
				curSize = 0;
			}
			break;
		case 34:
			ice.flushTxBuffer();
			if (LQ)
				curSize = 0;
			break;
		case 158:
			ice.flushTxBuffer();
			curSize = 0;
			break;
		default:
			// flush every 34 bytes
			if (ice.txBufferSize == 34)
				ice.flushTxBuffer();
			break;
		}
	}

private:
	u32 curSize = 0;
	bool LQ = false;
};

class MaxSpeedWriter : public Writer
{
public:
	MaxSpeedWriter(IceSession& ice) : Writer(ice) {}

	void write() override
	{
		// packets:
		// 'M' 'A' 'X' <sz> <payload>
		// sz must be >= 3. total packet size is sz + 4
		u8 last = ice.txBuffer[ice.txBufferSize - 1];
		switch (ice.txBufferSize)
		{
		case 1:
			if (last != 'M')
				ice.flushTxBuffer();
			break;
		case 2:
			if (last != 'A')
				ice.flushTxBuffer();
			break;
		case 3:
			if (last != 'X')
				ice.flushTxBuffer();
			break;
		case 4:
			if (last < 3)
				ice.flushTxBuffer();
			break;
		default:
			if (ice.txBufferSize == ice.txBuffer[3] + 4u)
				ice.flushTxBuffer();
			break;
		}
	}
};

void IceSession::setWriter()
{
	if (isF355())
		writer = std::make_unique<F355Writer>(*this);
	else if (settings.content.gameId == "MAXIMUM SPEED")
		writer = std::make_unique<MaxSpeedWriter>(*this);
	else
		writer = std::make_unique<StandardWriter>(*this);
}

void IceSession::write(u8 data)
{
	if (state == Playing)
	{
#ifdef DUMP_SERIAL
		if (dump == nullptr)
			dump = fopen("serial-dump.bin", "wb");
		if (dump != nullptr)
			fputc(data, dump);
#endif

		txBuffer[txBufferSize++] = data;
		if (txBufferSize == txBuffer.size()) {
			WARN_LOG(NETWORK, "txBuffer full");
			flushTxBuffer();
		}
		else {
			writer->write();
		}
	}
}

std::unique_ptr<IceSession> session;

void init(const std::string& username, bool matchCode)
{
	session.reset();
	session = std::make_unique<IceSession>();
	session->init(username, matchCode);
}

State getState()
{
	if (session == nullptr)
		return Offline;
	else
		return session->getState();
}

std::string getStatusText()
{
	if (session != nullptr)
		return session->getStatusText();
	else
		return {};
}

std::vector<std::string> getUserList()
{
	if (session == nullptr)
		return {};
	else
		return session->getUserList();
}

void sendChallenge(const std::string& user) {
	if (session != nullptr)
		session->sendChallenge(user);
}

std::string getChallenger()
{
	if (session != nullptr)
		return session->getChallenger();
	else
		return {};
}

void respondChallenge(bool accept) {
	if (session != nullptr)
		session->respondChallenge(accept);
}

void say(const std::string& msg) {
	if (session != nullptr)
		session->say(msg);
}

std::vector<std::string> getChat()
{
	if (session == nullptr)
		return {};
	else
		return session->getChat();
}

void term() {
	session.reset();
}

void displayStats()
{
	if (getState() != Playing)
		return;

	IceSession::Stats s = session->getStats();
	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
	ImguiStyleVar _1(ImGuiStyleVar_WindowBorderSize, 0);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ScaledVec2(95, 0));
	ImGui::SetNextWindowBgAlpha(0.7f);
	if (ImGui::Begin("##icestats", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs))
	{
		ImguiStyleColor _2(ImGuiCol_PlotHistogram, ImVec4(0.557f, 0.268f, 0.965f, 1.f));

		// Games set serial speed:
		// vonot	260416
		// f355		223214
		// aero i	38109
		// aero f	28935
		// tetris	111607
		// maxspeed	223214
		// hell gate 57870

		// TX/RX Queues
		ImGui::Text("Send Q");
		ImGui::ProgressBar(s.txQueueSize / 100.f, ImVec2(-1, uiScaled(10.f)), "");
		ImGui::Text("Tx Speed");
		ImGui::ProgressBar(s.txSpeed / 20000.f, ImVec2(-1, uiScaled(10.f)), "");

		ImGui::Text("Recv Q");
		ImGui::ProgressBar(s.rxQueueSize / 100.f, ImVec2(-1, uiScaled(10.f)), "");
		ImGui::Text("Rx Speed");
		ImGui::ProgressBar(s.rxSpeed / 20000.f, ImVec2(-1, uiScaled(10.f)), "");

		ImGui::TextWrapped("%s", session->getStatusText().c_str());
	}
	ImGui::End();
}

}	// namespace ice
#endif // USE_ICE
