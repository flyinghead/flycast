/*
	Copyright 2024 flyinghead

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

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

#include "types.h"

//#define BBA_PCAPNG_DUMP

#ifdef __MINGW32__
#define _POSIX_SOURCE
#endif

extern "C" {
#include <pico_stack.h>
#include <pico_dev_ppp.h>
#include <pico_socket.h>
#include <pico_socket_tcp.h>
#include <pico_ipv4.h>
#include <pico_tcp.h>
#include <pico_dhcp_server.h>
#ifdef _MSC_VER
#pragma pack(pop)
#endif
}
#include <asio.hpp>

#include "net_platform.h"
#include "picoppp.h"
#include "miniupnp.h"
#include "cfg/option.h"
#include "emulator.h"
#include "oslib/oslib.h"
#include "util/tsqueue.h"
#include "util/shared_this.h"
#include "hw/bba/bba.h"

#include <unordered_map>
#include <mutex>
#include <future>

#define RESOLVER1_OPENDNS_COM "208.67.222.222"
#define AFO_ORIG_IP 0x83f2fb3f		// 63.251.242.131 in network order
#define IGP_ORIG_IP 0xef2bd2cc		// 204.210.43.239 in network order

constexpr int PICO_TICK_MS = 5;
static pico_device *pico_dev;

static TsQueue<u8> in_buffer;
static TsQueue<u8> out_buffer;

static pico_ip4 dcaddr;
static pico_ip4 dnsaddr;

struct pico_ip4 public_ip;
static pico_ip4 afo_ip;

struct GamePortList {
	const char *gameId[10];
	uint16_t udpPorts[10];
	uint16_t tcpPorts[10];
};
static const GamePortList GamesPorts[] = {
	{ // Alien Front Online
		{ "MK-51171" },
		{ 7980 },
		{ },
	},
	{ // ChuChu Rocket
		{ "MK-51049", "HDR-0039", "MK-5104950" },
		{ 9789 },
		{ },
	},
	{
		{
			"MK-51037", "HDR-0106"	// Daytona USA
			"HDR-0073"				// Sega Tetris
			"GENERIC", "T44501M"	// Golf Shiyouyo 2
									// (the dreamcastlive patched versions are id'ed as GENERIC)
		},
		{ 12079, 20675 },
	},
	{ // Dee Dee Planet
		{ "HDR-0041" },
		{ 9879 },
	},
	{ // Driving Strikers online demo
		{ "IND-161053" },
		{ 30099 },
	},
	{ // Floigan Bros
		{ "MK-51114" },
		{},
		{ 37001 },
	},
	{ // Internet Game Pack
		{ "MK-51138" },
		{ 5656 },
		{ 5011, 10500, 10501, 10502, 10503 },
	},
	{ // NBA 2K1,2K2 / NFL 2K1,2K2 / NCAA 2K2
		{ "MK-51063", "HDR-0150",				// NBA 2K1
		  "MK-51178", "HDR-0197", "MK-5117850",	// NBA 2K2
		  "MK-51062", "HDR-0144",				// NFL 2K1
		  "MK-51168", "HDR-0196",				// NFL 2K2
		  "MK-51176" },							// NCAA 2K2
		{ 5502, 5503, 5656 },
		{ 5011, 6666 },
	},
	{ // The Next Tetris
		{ "T40214N", "T17717D 50" },
		{ 3512 },
		{ 3512 },
	},
	{ // Ooga Booga
		{ "MK-51140" },
		{ 6001 },
		{ },
	},
	{ // PBA Tour Bowling 2001
		{ "T26702N" },
		{ 6500, 47624, 13139 }, // +dynamic DirectPlay port 2300-2400
		{ 47624 },				// +dynamic DirectPlay port 2300-2400
	},
	{ // Planet Ring
		{ "MK-5114864", "MK-5112550" },
		{ 7648, 1285, 1028 },
		{ },
	},
	{ // StarLancer
		{ "T40209N", "T17723D 05" },
		{ 6500, 47624 },	// +dynamic DirectPlay port 2300-2400
		{ 47624 },			// +dynamic DirectPlay port 2300-2400
	},
	{ // World Series Baseball 2K2
		{ "MK-51152", "HDR-0198" },
		{ 37171, 13713 },
		{ },
	},
	{ // Worms World Party
		{ "T22904N", "T7016D  50" },
		{ },
		{ 17219 },
	},

	{ // Atomiswave
		{ "FASTER THAN SPEED" },
		{ 8888 },
		{ },
	},
};

static bool pico_thread_running = false;
extern "C" int dont_reject_opt_vj_hack;
// ppp: wait until the game sends some data before responding. Required for WinCE games.
static bool passiveMode = true;

static bool start_pico();
u32 makeDnsQueryPacket(void *buf, const char *host);
pico_ip4 parseDnsResponsePacket(const void *buf, size_t len);

static int modem_read(pico_device *dev, void *data, int len)
{
	u8 *p = (u8 *)data;
	int count = 0;
	for (; !out_buffer.empty() && count < len; count++)
		*p++ = out_buffer.pop();

    return count;
}

static int modem_write(pico_device *dev, const void *data, int len)
{
	u8 *p = (u8 *)data;

	for (int i = 0; i < len; i++)
	{
		while (in_buffer.size() > 1024)
		{
			if (!pico_thread_running)
				return 0;
			PICO_IDLE();
		}
		in_buffer.push(*p++);
	}

    return len;
}

static void write_pico(u8 b)
{
	if (passiveMode) {
		in_buffer.clear();
		passiveMode = false;
	}
	out_buffer.push(b);
}

static int read_pico()
{
	if (in_buffer.empty() || passiveMode)
		return -1;
	else
		return in_buffer.pop();
}

static int pico_available() {
	return passiveMode ? 0 : in_buffer.size();
}

class DirectPlay
{
public:
	virtual ~DirectPlay() = default;
	virtual void processOutPacket(const u8 *data, int len) = 0;
};

class TcpSocket : public SharedThis<TcpSocket>
{
public:
	void connect(pico_socket *pico_sock)
	{
		this->pico_sock = pico_sock;
		attachPicoSocket();
		u32 remoteIp = pico_sock->local_addr.ip4.addr;
        if (remoteIp == AFO_ORIG_IP			// Alien Front Online
			|| remoteIp == IGP_ORIG_IP)		// Internet Game Pack
		{
        	remoteIp = afo_ip.addr;		// same ip for both for now
		}
		pico.state = Established;
        asio::ip::address_v4 addrv4(*(std::array<u8, 4> *)&remoteIp);
		asio::ip::tcp::endpoint endpoint(addrv4, htons(pico_sock->local_port));
		setName(endpoint);
		socket.async_connect(endpoint,
				std::bind(&TcpSocket::onConnect, shared_from_this(), asio::placeholders::error));
	}

	void start()
	{
		pico_sock = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, nullptr);
		if (pico_sock == nullptr) {
			INFO_LOG(NETWORK, "pico_socket_open failed: error %d", pico_err);
			return;
		}
		attachPicoSocket();
		const auto& endpoint = socket.remote_endpoint();
		setName(endpoint);
		memcpy(&pico_sock->local_addr.ip4.addr, endpoint.address().to_v4().to_bytes().data(), 4);
		pico_sock->local_port = htons(endpoint.port());
		if (pico_socket_connect(pico_sock, &dcaddr.addr, htons(socket.local_endpoint().port())) != 0)
		{
			INFO_LOG(NETWORK, "pico_socket_connect failed: error %d", pico_err);
			pico_socket_close(pico_sock);
			return;
		}
		asio.state = Established;
		socket.set_option(asio::ip::tcp::no_delay(true));
	}

	asio::ip::tcp::socket& getSocket() {
		return socket;
	}

	void close() {
		closeAll();
		directPlay.reset();
	}

private:
	TcpSocket(asio::io_context& io_context, std::shared_ptr<DirectPlay> directPlay)
		: io_context(io_context), socket(io_context), directPlay(directPlay) {
	}

	void setName(const asio::ip::tcp::endpoint& endpoint)
	{
		// for logging
		if (socket.is_open())
			name = std::to_string(socket.local_endpoint().port())
					+ " -> " + endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
		else
			name = "? -> " + endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
	}

	void attachPicoSocket()
	{
		pico_sock->wakeup = [](uint16_t ev, pico_socket *picoSock)
			{
				if (picoSock == nullptr || picoSock->priv == nullptr)
					ERROR_LOG(NETWORK, "Pico callback with null tcp socket");
				else
					static_cast<Ptr *>(picoSock->priv)->get()->picoCallback(ev);
			};
		pico_sock->priv = new Ptr(shared_from_this());
	}

	void detachPicoSocket()
	{
		pico.state = Closed;
		if (pico_sock != nullptr)
		{
			pico_sock->wakeup = nullptr;
			void *priv = pico_sock->priv;
			pico_sock = nullptr;
			delete static_cast<Ptr *>(priv);
			// Note: 'this' might have been deleted at this point
		}
	}

	void closeAll()
	{
		asio.state = Closed;
		asio::error_code ec;
		socket.close(ec);
		pico.state = Closed;
		if (pico_sock != nullptr)
			pico_socket_close(pico_sock);
	}

	void onConnect(const std::error_code& ec)
	{
		if (ec) {
			INFO_LOG(NETWORK, "TcpSocket[%s] outbound_connect failed: %s", name.c_str(), ec.message().c_str());
			closeAll();
		}
		else
		{
			asio.state = Established;
			socket.set_option(asio::ip::tcp::no_delay(true));
			setName(socket.remote_endpoint());
			DEBUG_LOG(NETWORK, "TcpSocket[%s] outbound connected", name.c_str());
			readAsync();
			picoCallback(0);
		}
	}

	void readAsync()
	{
		if (asio.readInProgress || asio.state != Established)
			return;
		verify(pico.pendingWrite == 0);
		asio.readInProgress = true;
		socket.async_read_some(asio::buffer(in_buffer),
				std::bind(&TcpSocket::onRead, shared_from_this(),
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));
	}

	void onRead(const std::error_code& ec, size_t len)
	{
		asio.readInProgress = false;
		if (ec || len == 0)
		{
			if (ec && ec != asio::error::eof && ec != asio::error::operation_aborted)
				INFO_LOG(NETWORK, "TcpSocket[%s] read error %s", name.c_str(),
						ec.message().c_str());
			else
				DEBUG_LOG(NETWORK, "TcpSocket[%s] asio EOF", name.c_str());
			if (pico_sock != nullptr)
			{
				if (pico.state == Established)
					pico_socket_shutdown(pico_sock, PICO_SHUT_WR);
				else if (pico.state == Closed)
					pico_socket_close(pico_sock);
			}
			asio.state = Closed;
			return;
		}
		if (pico_sock == nullptr)
			return;
		DEBUG_LOG(NETWORK, "TcpSocket[%s] inbound %d bytes", name.c_str(), (int)len);
		if (pico_sock->remote_port == short_be(5011) && len >= 5 && in_buffer[0] == 1)
			// Visual Concepts sport games
			memcpy((void *)&in_buffer[1], &pico_sock->local_addr.ip4.addr, 4);
		pico.pendingWrite = len;
		picoCallback(PICO_SOCK_EV_WR);
	}

	void onWritten(const std::error_code& ec, size_t len)
	{
		asio.writeInProgress = false;
		if (ec) {
			INFO_LOG(NETWORK, "TcpSocket[%s] write error: %s", name.c_str(), ec.message().c_str());
			closeAll();
		}
		else {
			DEBUG_LOG(NETWORK, "TcpSocket[%s] outbound %d bytes", name.c_str(), (int)len);
			picoCallback(0);
		}
	}

	void picoCallback(u16 ev)
	{
		ev |= pico.pendingEvent;
		pico.pendingEvent = 0;
		if (!socket.is_open())
		{
			if (ev & PICO_SOCK_EV_DEL) {
				detachPicoSocket();
			}
			else {
				if (ev != PICO_SOCK_EV_FIN)
					INFO_LOG(NETWORK, "TcpSocket[%s] asio socket is closed (ev %x, pendingW %d)", name.c_str(), ev, pico.pendingWrite);
				pico_socket_close(pico_sock);
			}
			return;
		}
		if (ev & PICO_SOCK_EV_RD)
		{
			verify(pico.state != Closed);
			if (asio.state == Connecting || asio.writeInProgress) {
				pico.pendingEvent |= PICO_SOCK_EV_RD;
			}
			else
			{
				// This callback might be called recursively if FIN is received
				pico.readInProgress = true;
				int r = pico_socket_read(pico_sock, sendbuf, sizeof(sendbuf));
				pico.readInProgress = false;
				DEBUG_LOG(NETWORK, "TcpSocket[%s] read event: pico.state %d, %d bytes", name.c_str(), pico.state, r);
				if (r > 0)
				{
					if (pico_sock->local_port == short_be(5011) && r >= 5 && sendbuf[0] == 1)
						// Visual Concepts sport games
						memcpy(&sendbuf[1], &public_ip.addr, 4);
					else
						directPlay->processOutPacket((const u8 *)&sendbuf[0], r);
					asio::async_write(socket, asio::buffer(sendbuf, r),
							std::bind(&TcpSocket::onWritten, shared_from_this(),
											asio::placeholders::error,
											asio::placeholders::bytes_transferred));
					asio.writeInProgress = true;
				}
				else if (r < 0)
				{
					INFO_LOG(NETWORK, "TcpSocket[%s] pico read error: %s", name.c_str(), strerror(pico_err));
					if (socket.is_open())
					{
						if (asio.state == Closed)
							socket.close();
						else
							socket.shutdown(asio::socket_base::shutdown_send);
					}
					pico_socket_close(pico_sock);
					pico.state = Closed;
				}
			}
		}

		if (ev & PICO_SOCK_EV_WR)
		{
			if (pico.pendingWrite > 0)
			{
				DEBUG_LOG(NETWORK, "TcpSocket[%s] write event: pico.state %d, %d bytes", name.c_str(), pico.state, pico.pendingWrite);
				if (pico.state == Connecting) {
					pico.pendingEvent |= PICO_SOCK_EV_WR;
				}
				else
				{
					int sent = pico_socket_write(pico_sock, &in_buffer[0], (int)pico.pendingWrite);
					if (sent < 0)
					{
						INFO_LOG(NETWORK, "TcpSocket[%s] pico send error: %s", name.c_str(), strerror(pico_err));
						pico.pendingWrite = 0;
						closeAll();
					}
					else if (sent < (int)pico.pendingWrite)
					{
						if (sent > 0)
						{
							// FIXME how to handle partial pico writes if any? PICO_SOCK_EV_WR?
							WARN_LOG(NETWORK, "TcpSocket[%s] Partial pico send: %d -> %d", name.c_str(), (int)pico.pendingWrite, sent);
							asio.state = Closed;
						}
					}
					else {
						pico.pendingWrite = 0;
						readAsync();
					}
				}
			}
			else {
				readAsync();
			}
		}

		if (ev & PICO_SOCK_EV_CONN)
		{
			DEBUG_LOG(NETWORK, "TcpSocket[%s] connect event", name.c_str());
			verify(pico.state == Connecting);
			pico.state = Established;
			readAsync();
		}

		if (ev & PICO_SOCK_EV_CLOSE)	// FIN received
		{
			DEBUG_LOG(NETWORK, "TcpSocket[%s] close event (pending ev %x, pico.reading %d, asio.writing %d)", name.c_str(),
					pico.pendingEvent, pico.readInProgress, asio.writeInProgress);
			if (pico.pendingEvent == 0 && !pico.readInProgress && !asio.writeInProgress)
			{
				pico.state = Closed;
				if (socket.is_open()) {
					pico_socket_shutdown(pico_sock, PICO_SHUT_RD);
					socket.shutdown(asio::socket_base::shutdown_send);
				}
				else {
					pico_socket_close(pico_sock);
				}
			}
			else {
				pico.pendingEvent |= PICO_SOCK_EV_CLOSE;
			}
		}

		if (ev & PICO_SOCK_EV_FIN)		// Socket is in the closed state
		{
			DEBUG_LOG(NETWORK, "TcpSocket[%s] FIN event (pending ev %x, asio.writing %d, pico.reading %d)", name.c_str(),
					pico.pendingEvent, asio.writeInProgress, pico.readInProgress);
			if (pico.pendingEvent == 0 && !asio.writeInProgress && !pico.readInProgress)
				closeAll();
			else
				pico.pendingEvent |= PICO_SOCK_EV_FIN;
		}

		if (ev & PICO_SOCK_EV_ERR) {
			INFO_LOG(NETWORK, "TcpSocket[%s] Pico socket error received: %s", name.c_str(), strerror(pico_err));
			closeAll();
		}

		if (ev & PICO_SOCK_EV_DEL)
			detachPicoSocket();
	}

	asio::io_context& io_context;
	asio::ip::tcp::socket socket;
	std::shared_ptr<DirectPlay> directPlay;
	pico_socket *pico_sock = nullptr;
	std::array<char, 536> in_buffer;
	char sendbuf[1510];
	enum State { Connecting, Established, Closed };
	struct {
		State state = Connecting;
		bool readInProgress = false;
		bool writeInProgress = false;
	} asio;
	struct {
		State state = Connecting;
		u16 pendingEvent = 0;
		u32 pendingWrite = 0;
		bool readInProgress = false;
	} pico;
	std::string name;
	friend super;
};

// Handles inbound tcp connections
class TcpAcceptor : public SharedThis<TcpAcceptor>
{
public:
	void start()
	{
		TcpSocket::Ptr newSock = TcpSocket::create(io_context, directPlay);
		sockets.push_back(newSock);

		acceptor.async_accept(newSock->getSocket(),
				std::bind(&TcpAcceptor::onAccept, shared_from_this(), newSock, asio::placeholders::error));
	}

	void stop() {
		acceptor.close();
		for (auto& socket : sockets)
			socket->close();
		sockets.clear();
		directPlay.reset();
	}

private:
	TcpAcceptor(asio::io_context& io_context, u16 port, std::shared_ptr<DirectPlay> directPlay)
		: io_context(io_context),
		  acceptor(asio::ip::tcp::acceptor(io_context,
				asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port))),
		  directPlay(directPlay)
	{
	}

	void onAccept(TcpSocket::Ptr newSock, const std::error_code& ec)
	{
		if (ec) {
			if (ec != asio::error::operation_aborted)
				INFO_LOG(NETWORK, "accept failed: %s", ec.message().c_str());
		}
		else
		{
			DEBUG_LOG(NETWORK, "Inbound TCP connection to port %d  from %s:%d", acceptor.local_endpoint().port(),
					newSock->getSocket().remote_endpoint().address().to_string().c_str(), newSock->getSocket().remote_endpoint().port());
			newSock->start();
			start();
		}
	}

	asio::io_context& io_context;
	asio::ip::tcp::acceptor acceptor;
	std::shared_ptr<DirectPlay> directPlay;
	std::vector<TcpSocket::Ptr> sockets;
	friend super;
};

// Handles outbound dc tcp sockets
class TcpSink
{
public:
	TcpSink(asio::io_context& io_context, std::shared_ptr<DirectPlay> directPlay)
		: io_context(io_context), directPlay(directPlay)
	{
		pico_sock = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, [](uint16_t ev, pico_socket *picoSock) {
			if (picoSock == nullptr || picoSock->priv == nullptr)
				WARN_LOG(NETWORK, "Pico callback with null tcp socket");
			else
				static_cast<TcpSink *>(picoSock->priv)->picoCallback(ev);
		});
		if (pico_sock == nullptr)
			ERROR_LOG(NETWORK, "error opening TCP socket: %s", strerror(pico_err));
		pico_sock->priv = this;
		int yes = 1;
		pico_socket_setoption(pico_sock, PICO_TCP_NODELAY, &yes);
		pico_ip4 inaddr_any = {};
		uint16_t listen_port = 0;
		int ret = pico_socket_bind(pico_sock, &inaddr_any, &listen_port);
		if (ret < 0)
			ERROR_LOG(NETWORK, "error binding TCP socket to port %u: %s", short_be(listen_port), strerror(pico_err));
		else if (pico_socket_listen(pico_sock, 10) != 0)
			ERROR_LOG(NETWORK, "error listening on port %u", short_be(listen_port));
	}

	~TcpSink() {
		if (pico_sock != nullptr)
			pico_sock->wakeup = nullptr;
	}

	void stop()
	{
		if (pico_sock != nullptr)
			pico_socket_close(pico_sock);
		directPlay.reset();
		for (auto& socket : sockets)
			socket->close();
		sockets.clear();
	}

private:
	void picoCallback(uint16_t ev)
	{
		if (ev & PICO_SOCK_EV_CONN)
		{
			pico_ip4 orig;
			uint16_t port;

			pico_socket *sock_a = pico_socket_accept(pico_sock, &orig, &port);
			if (sock_a == nullptr) {
				// Also called for child sockets
				INFO_LOG(NETWORK, "pico_socket_accept error: %s", strerror(pico_err));
			}
			else
			{
				char peer[30];
				int yes = 1;
				pico_ipv4_to_string(peer, sock_a->local_addr.ip4.addr);
				DEBUG_LOG(NETWORK, "TcpSink: Outbound from port %d to %s:%d", short_be(port), peer, short_be(sock_a->local_port));
				pico_socket_setoption(sock_a, PICO_TCP_NODELAY, &yes);
				pico_tcp_set_linger(sock_a, 10000);

				TcpSocket::Ptr psock = TcpSocket::create(io_context, directPlay);
				psock->connect(sock_a);
				sockets.push_back(psock);
			}
		}

		if (ev & PICO_SOCK_EV_ERR) {
			INFO_LOG(NETWORK, "TcpSink error: %s", strerror(pico_err));
			pico_socket_close(pico_sock);
		}

		if (ev & PICO_SOCK_EV_FIN)
			pico_socket_close(pico_sock);

		if (ev & (PICO_SOCK_EV_RD | PICO_SOCK_EV_WR))
			WARN_LOG(NETWORK, "TcpSink: R/W event %x", ev);

		if (ev & PICO_SOCK_EV_DEL) {
			pico_sock->priv = nullptr;
			pico_sock = nullptr;
		}
	}

	asio::io_context& io_context;
	std::shared_ptr<DirectPlay> directPlay;
	pico_socket *pico_sock;
	std::vector<TcpSocket::Ptr> sockets;
};

// Handles inbound datagram to a given port
class UdpSocket : public SharedThis<UdpSocket>
{
public:
	void start() {
		readAsync();
	}

	void sendto(const char *buf, size_t len, u32 addr, u16 port)
	{
		asio::ip::udp::endpoint destination(asio::ip::address_v4(addr), port);
		DEBUG_LOG(NETWORK, "UdpSocket: outbound %d bytes from %d to %s:%d", (int)len, socket.local_endpoint().port(),
				destination.address().to_string().c_str(), destination.port());
		std::error_code ec;
		socket.send_to(asio::buffer(buf, len), destination, 0, ec);
		if (ec && ec != asio::error::would_block)
			INFO_LOG(NETWORK, "UDP sendto failed: %s", ec.message().c_str());
	}

	void close() {
		asio::error_code ec;
		socket.close(ec);
	}

private:
	UdpSocket(asio::io_context& io_context, u16 port, pico_socket *pico_sock, u16 dcport)
		: io_context(io_context),
		  socket(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), port)),
		  pico_sock(pico_sock),
		  dcport(dcport)
	{
		asio::socket_base::broadcast option(true);
		socket.set_option(option);
		socket.non_blocking(true);
	}

	void readAsync()
	{
		socket.async_receive_from(asio::buffer(recvbuf), source,
			[this](const std::error_code& ec, size_t len)
			{
				if (ec) {
					INFO_LOG(NETWORK, "UDP recv_from failed: %s", ec.message().c_str());
					return;
				}
				DEBUG_LOG(NETWORK, "UdpSocket: received %d bytes to port %d from %s:%d", (int)len,
						dcport, source.address().to_string().c_str(), source.port());
				if (len == 0)
					WARN_LOG(NETWORK, "Received empty datagram");

				// filter out messages coming from ourselves (happens for broadcasts)
				u32 srcAddr = htonl(source.address().to_v4().to_uint());
				if (socket.local_endpoint().port() != source.port() || !is_local_address(srcAddr))
				{
					pico_msginfo msginfo;
					msginfo.dev = pico_dev;
					msginfo.tos = 0;
					msginfo.ttl = 0;
					msginfo.local_addr.ip4.addr = srcAddr;
					msginfo.local_port = htons(source.port());

					int r = pico_socket_sendto_extended(pico_sock, &recvbuf[0], len, &dcaddr, htons(dcport), &msginfo);
					if (r < (int)len)
						INFO_LOG(NETWORK, "error UDP sending to port %d: %s", dcport, strerror(pico_err));
				}
				readAsync();
			});
	}

	asio::io_context& io_context;
	asio::ip::udp::socket socket;
	pico_socket *pico_sock;
	std::array<u8, 1510> recvbuf;
	asio::ip::udp::endpoint source;	// source endpoint when receiving packets
	u16 dcport;
	friend super;
};

// Handles all outbound datagrams
class UdpSink
{
public:
	UdpSink(asio::io_context& io_context)
		: io_context(io_context)
	{
		pico_sock = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, [](u16 ev, pico_socket *picoSock) {
			if (picoSock == nullptr || picoSock->priv == nullptr)
				ERROR_LOG(NETWORK, "Pico callback with null udp sink");
			else
				static_cast<UdpSink *>(picoSock->priv)->picoCallback(ev);
		});
	    if (pico_sock == nullptr) {
	    	ERROR_LOG(NETWORK, "error opening UDP socket: %s", strerror(pico_err));
	    	return;
	    }
	    pico_sock->priv = this;
	    pico_ip4 inaddr_any = {0};
	    uint16_t listen_port = 0;
	    int ret = pico_socket_bind(pico_sock, &inaddr_any, &listen_port);
	    if (ret < 0)
	    	ERROR_LOG(NETWORK, "error binding UDP socket to port %u: %s", short_be(listen_port), strerror(pico_err));
	}

	~UdpSink() {
		if (pico_sock != nullptr)
			pico_sock->wakeup = nullptr;
	}

	void setDirectPlay(std::shared_ptr<DirectPlay> directPlay) {
		this->directPlay = directPlay;
	}

	UdpSocket::Ptr findSocket(u16 port)
	{
		auto it = sockets.find(port);
		if (it != sockets.end())
			return it->second;
		try {
			UdpSocket::Ptr sock;
			try {
				sock = UdpSocket::create(io_context, port, pico_sock, port);
			} catch (const std::system_error& e) {
				if (e.code() != asio::error::address_in_use)
					throw;
				// Use a random local port
				WARN_LOG(NETWORK, "Server UDP socket on port %d: address in use, using random port instead", port);
				sock = UdpSocket::create(io_context, 0, pico_sock, port);
			}
			sock->start();
			sockets[port] = sock;
			return sock;
		} catch (const std::system_error& e) {
			WARN_LOG(NETWORK, "Server UDP socket on port %d: %s", port, e.what());
			return nullptr;
		}
	}

	void stop()
	{
		for (auto& [port,sock] : sockets)
			sock->close();
		sockets.clear();
		if (pico_sock != nullptr)
			pico_socket_close(pico_sock);
		directPlay.reset();
	}

private:
	void picoCallback(u16 ev)
	{
		if (ev & PICO_SOCK_EV_RD)
		{
			char buf[1510];
			pico_ip4 src_addr;
			uint16_t src_port;
			pico_msginfo msginfo;
			int r = 0;
			while (true)
			{
				src_port = 0;
				src_addr = {};
				r = pico_socket_recvfrom_extended(pico_sock, buf, sizeof(buf), &src_addr.addr, &src_port, &msginfo);

				if (r < 0) {
					INFO_LOG(NETWORK, "error UDP recv: %s", strerror(pico_err));
					break;
				}
				if (r == 0 && src_port == 0 && src_addr.addr == 0)
					// No more packets
					break;
				if (r == 0)
					WARN_LOG(NETWORK, "Sending empty datagram");
				// Daytona USA
				if (msginfo.local_port == 0x2F2F && r >= 3 && buf[0] == 0x20 && buf[2] == 0x42)
				{
					if (buf[1] == 0x2b && r >= 37 + (int)sizeof(public_ip.addr))
					{
						// Start session packet
						char *p = &buf[37];
						if (memcmp(p, &dcaddr.addr, sizeof(dcaddr.addr)) == 0)
							memcpy(p, &public_ip.addr, sizeof(public_ip.addr));
					}
					else if (buf[1] == 0x15 && r >= 14 + (int)sizeof(public_ip.addr))
					{
						char *p = &buf[5];
						if (memcmp(p, &dcaddr.addr, sizeof(dcaddr.addr)) == 0)
							memcpy(p, &public_ip.addr, sizeof(public_ip.addr));
						p = &buf[14];
						if (memcmp(p, &dcaddr.addr, sizeof(dcaddr.addr)) == 0)
							memcpy(p, &public_ip.addr, sizeof(public_ip.addr));
					}
				}
				else if (msginfo.local_port == htons(47624))
					directPlay->processOutPacket((const u8 *)buf, r);
				UdpSocket::Ptr sock = findSocket(htons(src_port));
				if (sock)
					sock->sendto(buf, r, htonl(msginfo.local_addr.ip4.addr), htons(msginfo.local_port));
			}
		}
		if (ev & PICO_SOCK_EV_DEL) {
			pico_sock->wakeup = nullptr;
			pico_sock = nullptr;
		}
	}

	asio::io_context& io_context;
	pico_socket *pico_sock = nullptr;
	std::unordered_map<u16, UdpSocket::Ptr> sockets;
	std::shared_ptr<DirectPlay> directPlay;
};

class DirectPlayImpl : public DirectPlay, public SharedThis<DirectPlayImpl>
{
public:
	void processOutPacket(const u8 *data, int len) override
	{
		if (!isDirectPlay(data, len))
			return;

		u16 port = htons(*(u16 *)&data[6]);
		if (port >= 2300 && port <= 2400 && port != this->port)
		{
			NOTICE_LOG(NETWORK, "DirectPlay4 local port is %d", port);
			if (acceptor) {
				acceptor->stop();
				acceptor.reset();
			}
			forwardPorts(port, false);
			this->port = port;
			udpSink.findSocket(port);
			try {
				acceptor = TcpAcceptor::create(io_context, port, shared_from_this());
				acceptor->start();
			} catch (const std::system_error& e) {
				WARN_LOG(NETWORK, "DirectPlay TCP socket on port %d: %s", port, e.what());
			}
		}
		if (*(u16 *)&data[24] == 0x13)	// Add Forward Request
		{
			// This one is the guest game port, only UDP is used
			u16 port = htons(*(u16 *)&data[0x72]);
			if (port >= 2300 && port <= 2400 && port != this->gamePort)
			{
				if (*(u16 *)&data[0x62] == this->port)
					WARN_LOG(NETWORK, "DirectPlay4 AddForwardRequest expected port %d got %d", this->port, *(u16 *)&data[0x62]);
				NOTICE_LOG(NETWORK, "DirectPlay4 game port is %d", port);
				forwardPorts(port, true);
				this->gamePort = port;
				udpSink.findSocket(port);
			}
		}
	}

	~DirectPlayImpl()
	{
		stop();
		if (upnpCmd.valid())
			upnpCmd.get();
	}

	void stop() {
		if (acceptor)
			acceptor->stop();
		acceptor.reset();
	}

private:
	DirectPlayImpl(asio::io_context& io_context, UdpSink& udpSink, std::shared_ptr<MiniUPnP> upnp)
		: io_context(io_context), udpSink(udpSink), upnp(upnp) {
	}

	bool isDirectPlay(const u8 *data, int len)
	{
		return len >= 24 && (data[2] & 0xf0) == 0xb0 && data[3] == 0xfa
				// DirectPlay4 signature
				&& !memcmp(&data[20], "play", 4);
	}

	void forwardPorts(u16 port, bool udpOnly)
	{
		if (upnp && upnp->isInitialized())
		{
			if (upnpCmd.valid())
				upnpCmd.get();
			upnpCmd = std::async(std::launch::async, [this, port, udpOnly]()
			{
				if (!upnp->AddPortMapping(port, false))
					WARN_LOG(NETWORK, "UPNP AddPortMapping UDP %d failed", port);
				if (!udpOnly && !upnp->AddPortMapping(port, true))
					WARN_LOG(NETWORK, "UPNP AddPortMapping TCP %d failed", port);
			});
		}
	}

	u16 port = 0;
	u16 gamePort = 0;
	TcpAcceptor::Ptr acceptor;
	asio::io_context& io_context;
	UdpSink& udpSink;
	std::shared_ptr<MiniUPnP> upnp;
	std::future<void> upnpCmd;
	friend super;
};

class DnsResolver : public SharedThis<DnsResolver>
{
public:
	void resolve(const char *host, pico_ip4 *result)
	{
		// need to introduce a dns query object if concurrency is needed
		verify(!busy);
		busy = true;
		u32 len = makeDnsQueryPacket(buf, host);
		socket.async_send_to(asio::buffer(buf, len), nsEndpoint,
				std::bind(&DnsResolver::querySent, shared_from_this(),
						result,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));
	}

private:
	DnsResolver(asio::io_context& io_context, const char *nameServer)
		: io_context(io_context), socket(io_context)
	{
		using namespace asio::ip;
		udp::resolver resolver(io_context);
		nsEndpoint = *resolver.resolve(udp::v4(), nameServer, "53").begin();
		socket.open(udp::v4());
	}

	void querySent(pico_ip4 *result, const std::error_code& ec, size_t len)
	{
		if (!ec)
		{
			socket.async_receive_from(asio::mutable_buffer(buf, sizeof(buf)), nsEndpoint,
					std::bind(&DnsResolver::responseReceived, shared_from_this(),
						result,
						asio::placeholders::error,
						asio::placeholders::bytes_transferred));
		}
		else {
			busy = false;
		}
	}

	void responseReceived(pico_ip4 *result, const std::error_code& ec, size_t len)
	{
		if (!ec)
		{
			*result = parseDnsResponsePacket(buf, len);
			DEBUG_LOG(NETWORK, "dns resolved: %s (using %s)",
					asio::ip::address_v4(*(std::array<u8, 4> *)result).to_string().c_str(),
					nsEndpoint.address().to_string().c_str());
		}
		busy = false;
	}

	asio::io_context& io_context;
	asio::ip::udp::endpoint nsEndpoint;
	asio::ip::udp::socket socket;
	char buf[1024];
	bool busy = false;
	friend super;
};

static void resolveDns(asio::io_context& io_context)
{
	public_ip.addr = 0;
	afo_ip.addr = 0;
	DnsResolver::Ptr resolver = DnsResolver::create(io_context, RESOLVER1_OPENDNS_COM);
	resolver->resolve("myip.opendns.com", &public_ip);
	char str[16];
	pico_ipv4_to_string(str, dnsaddr.addr);
	resolver = DnsResolver::create(io_context, str);
	resolver->resolve("auriga.segasoft.com", &afo_ip);
}

static pico_device *pico_eth_create()
{
    pico_device *eth = (pico_device *)PICO_ZALLOC(sizeof(pico_device));
    if (!eth)
        return nullptr;

    const u8 mac_addr[6] = { 0xc, 0xa, 0xf, 0xe, 0, 1 };
	if (0 != pico_device_init(eth, "ETHPEER", mac_addr))
		return nullptr;

	DEBUG_LOG(NETWORK, "Device %s created", eth->name);

    return eth;
}

static FILE *pcapngDump;

static void dumpFrame(const u8 *frame, u32 size)
{
#ifdef BBA_PCAPNG_DUMP
	if (pcapngDump == nullptr)
	{
		pcapngDump = fopen("bba.pcapng", "wb");
		if (pcapngDump == nullptr)
		{
			const char *home = getenv("HOME");
			if (home != nullptr)
			{
				std::string path = home + std::string("/bba.pcapng");
				pcapngDump = fopen(path.c_str(), "wb");
			}
			if (pcapngDump == nullptr)
				return;
		}
		u32 blockType = 0x0A0D0D0A; // Section Header Block
		fwrite(&blockType, sizeof(blockType), 1, pcapngDump);
		u32 blockLen = 28;
		fwrite(&blockLen, sizeof(blockLen), 1, pcapngDump);
		u32 magic = 0x1A2B3C4D;
		fwrite(&magic, sizeof(magic), 1, pcapngDump);
		u32 version = 1; // 1.0
		fwrite(&version, sizeof(version), 1, pcapngDump);
		u64 sectionLength = ~0; // unspecified
		fwrite(&sectionLength, sizeof(sectionLength), 1, pcapngDump);
		fwrite(&blockLen, sizeof(blockLen), 1, pcapngDump);

		blockType = 1; // Interface Description Block
		fwrite(&blockType, sizeof(blockType), 1, pcapngDump);
		blockLen = 20;
		fwrite(&blockLen, sizeof(blockLen), 1, pcapngDump);
		const u32 linkType = 1; // Ethernet
		fwrite(&linkType, sizeof(linkType), 1, pcapngDump);
		const u32 snapLen = 0; // no limit
		fwrite(&snapLen, sizeof(snapLen), 1, pcapngDump);
		// TODO options? if name, ip/mac address
		fwrite(&blockLen, sizeof(blockLen), 1, pcapngDump);
	}
	const u32 blockType = 6; // Extended Packet Block
	fwrite(&blockType, sizeof(blockType), 1, pcapngDump);
	u32 roundedSize = ((size + 3) & ~3) + 32;
	fwrite(&roundedSize, sizeof(roundedSize), 1, pcapngDump);
	u32 ifId = 0;
	fwrite(&ifId, sizeof(ifId), 1, pcapngDump);
	u64 now = getTimeMs() * 1000;
	fwrite((u32 *)&now + 1, 4, 1, pcapngDump);
	fwrite(&now, 4, 1, pcapngDump);
	fwrite(&size, sizeof(size), 1, pcapngDump);
	fwrite(&size, sizeof(size), 1, pcapngDump);
	fwrite(frame, 1, size, pcapngDump);
	fwrite(frame, 1, roundedSize - size - 32, pcapngDump);
	fwrite(&roundedSize, sizeof(roundedSize), 1, pcapngDump);
#endif
}
static void closeDumpFile()
{
	if (pcapngDump != nullptr) {
		fclose(pcapngDump);
		pcapngDump = nullptr;
	}
}
static void pico_receive_eth_frame(const u8 *frame, u32 size)
{
	if (pico_dev == nullptr) {
		start_pico();
	}
	else {
		dumpFrame(frame, size);
		pico_stack_recv(pico_dev, (u8 *)frame, size);
	}
}

static int send_eth_frame(pico_device *dev, void *data, int len) {
	dumpFrame((const u8 *)data, len);
	return bba_recv_frame((const u8 *)data, len);
}

static void picoTick(const std::error_code& ec, asio::steady_timer *timer)
{
	if (ec) {
		ERROR_LOG(NETWORK, "picoTick timer error: %s", ec.message().c_str());
		return;
	}
	pico_stack_tick();
	timer->expires_at(timer->expiry() + asio::chrono::milliseconds(PICO_TICK_MS));
	timer->async_wait(std::bind(picoTick, asio::placeholders::error, timer));
}

class PicoThread
{
public:
	void start()
	{
		verify(!thread.joinable());
		io_context = std::make_unique<asio::io_context>();
		thread = std::thread(&PicoThread::run, this);
	}

	void stop()
	{
		if (!thread.joinable())
			return;
		io_context->stop();
		thread.join();
		io_context.reset();
	}

private:
	void run();

	const GamePortList *ports = nullptr;
	std::shared_ptr<MiniUPnP> upnp;
	bool usingPPP = false;
	std::thread thread;
	std::unique_ptr<asio::io_context> io_context;
};

void PicoThread::run()
{
	ThreadName _("PicoTCP");
	passiveMode = true;
	// Find the network ports for the current game
	ports = nullptr;
	for (u32 i = 0; i < std::size(GamesPorts) && ports == nullptr; i++)
	{
		const auto& game = GamesPorts[i];
		for (u32 j = 0; j < std::size(game.gameId) && game.gameId[j] != nullptr; j++)
		{
			if (settings.content.gameId == game.gameId[j])
			{
				NOTICE_LOG(NETWORK, "Found network ports for game %s", settings.content.gameId.c_str());
				ports = &game;
				break;
			}
		}
	}
	// Web TV requires the VJ compression option, which picotcp doesn't support.
	// This hack allows WebTV to connect although the correct fix would
	// be to implement VJ compression.
	dont_reject_opt_vj_hack = settings.content.gameId == "6107117"
			|| settings.content.gameId == "610-7390" || settings.content.gameId == "610-7391" ? 1 : 0;

	std::future<void> pnpFuture;
	if (ports != nullptr && config::EnableUPnP)
	{
		upnp = std::make_shared<MiniUPnP>();
		pnpFuture = std::move(
			std::async(std::launch::async, [this]()
			{
				// Initialize miniupnpc and map network ports
				ThreadName _("UPNP-init");
				if (!upnp->Init())
					WARN_LOG(NETWORK, "UPNP Init failed");
				else
				{
					for (u32 i = 0; i < std::size(ports->udpPorts) && ports->udpPorts[i] != 0; i++)
						if (!upnp->AddPortMapping(ports->udpPorts[i], false))
							WARN_LOG(NETWORK, "UPNP AddPortMapping UDP %d failed", ports->udpPorts[i]);
					for (u32 i = 0; i < std::size(ports->tcpPorts) && ports->tcpPorts[i] != 0; i++)
						if (!upnp->AddPortMapping(ports->tcpPorts[i], true))
							WARN_LOG(NETWORK, "UPNP AddPortMapping TCP %d failed", ports->tcpPorts[i]);
				}
			}));
	}

	// Empty queues
	in_buffer.clear();
	out_buffer.clear();

    // Find DNS ip address
	{
		std::string dnsName = config::DNS;
		if (dnsName == "46.101.91.123")
			// override legacy default with current one
			dnsName = "dns.flyca.st";
		asio::ip::udp::resolver resolver(*io_context);
		std::error_code ec;
		auto it = resolver.resolve(asio::ip::udp::v4(), dnsName, "53", ec);
		if (ec)
			WARN_LOG(NETWORK, "%s: %s", dnsName.c_str(), ec.message().c_str());
		if (!ec && !it.empty())
		{
			asio::ip::udp::endpoint endpoint = *it.begin();

			memcpy(&dnsaddr.addr, &endpoint.address().to_v4().to_bytes()[0], sizeof(dnsaddr.addr));
			char s[17];
			pico_ipv4_to_string(s, dnsaddr.addr);
			NOTICE_LOG(NETWORK, "%s IP is %s", dnsName.c_str(), s);
		}
		else
		{
			u32 addr;
			pico_string_to_ipv4("46.101.91.123", &addr);
			dnsaddr.addr = addr;
			WARN_LOG(NETWORK, "Can't resolve dns.flyca.st. Using default 46.101.91.123");
		}
	}
	resolveDns(*io_context);

	pico_stack_init();

	// Create ppp/eth device
	usingPPP = !config::EmulateBBA;
	u32 addr;
	if (usingPPP)
	{
		// PPP
		pico_dev = pico_ppp_create();
		if (!pico_dev)
			throw FlycastException("PicoTCP ppp creation failed");
		pico_string_to_ipv4("192.168.167.2", &addr);
		memcpy(&dcaddr.addr, &addr, sizeof(addr));
		pico_ppp_set_peer_ip(pico_dev, dcaddr);
		pico_string_to_ipv4("192.168.167.1", &addr);
		pico_ip4 ipaddr;
		memcpy(&ipaddr.addr, &addr, sizeof(addr));
		pico_ppp_set_ip(pico_dev, ipaddr);
		pico_ppp_set_dns1(pico_dev, dnsaddr);

		pico_ppp_set_serial_read(pico_dev, modem_read);
		pico_ppp_set_serial_write(pico_dev, modem_write);
		pico_ppp_set_serial_set_speed(pico_dev, [](pico_device *dev, uint32_t speed) { return 0; });
		pico_dev->proxied = 1;

		pico_ppp_connect(pico_dev);
	}
	else
	{
		// Ethernet
		pico_dev = pico_eth_create();
		if (pico_dev == nullptr)
			throw FlycastException("PicoTCP eth creation failed");
		pico_dev->send = &send_eth_frame;
		pico_dev->proxied = 1;
		pico_queue_protect(pico_dev->q_in);

		pico_string_to_ipv4("192.168.169.1", &addr);
		pico_ip4 ipaddr;
		memcpy(&ipaddr.addr, &addr, sizeof(addr));
		pico_string_to_ipv4("255.255.255.0", &addr);
		pico_ip4 netmask;
		memcpy(&netmask.addr, &addr, sizeof(addr));
		pico_ipv4_link_add(pico_dev, ipaddr, netmask);
		// dreamcast IP
		pico_string_to_ipv4("192.168.169.2", &addr);
		memcpy(&dcaddr.addr, &addr, sizeof(addr));

		pico_dhcp_server_setting dhcpSettings{ 0 };
		dhcpSettings.dev = pico_dev;
		dhcpSettings.server_ip = ipaddr;
		dhcpSettings.lease_time = long_be(24 * 60 * 60); // seconds
		dhcpSettings.netmask = netmask;
		dhcpSettings.pool_start = addr;
		dhcpSettings.pool_end = addr;
		dhcpSettings.pool_next = addr;
		dhcpSettings.dns_server.addr = dnsaddr.addr;
		if (pico_dhcp_server_initiate(&dhcpSettings) != 0)
			WARN_LOG(NETWORK, "DHCP server init failed");
	}

	// Create sinks
	UdpSink udpSink(*io_context);
	DirectPlayImpl::Ptr directPlay = DirectPlayImpl::create(*io_context, udpSink, upnp);
	udpSink.setDirectPlay(directPlay);
	TcpSink tcpSink(*io_context, directPlay);

	// Open listening sockets
	std::vector<TcpAcceptor::Ptr> acceptors;
	if (ports != nullptr)
	{
		for (u32 i = 0; i < std::size(ports->udpPorts) && ports->udpPorts[i] != 0; i++)
			udpSink.findSocket(ports->udpPorts[i]);

		for (u32 i = 0; i < std::size(ports->tcpPorts) && ports->tcpPorts[i] != 0; i++)
			try {
				auto acceptor = TcpAcceptor::create(*io_context, ports->tcpPorts[i], directPlay);
				acceptor->start();
				acceptors.push_back(std::move(acceptor));
			} catch (const std::system_error& e) {
				WARN_LOG(NETWORK, "Server TCP socket on port %d: %s", ports->tcpPorts[i], e.what());
			}
	}

	// pico stack timer
	asio::steady_timer timer(*io_context);
	picoTick({}, &timer);

	// main loop
	io_context->run();

	for (auto& acceptor : acceptors)
		acceptor->stop();
	acceptors.clear();
	tcpSink.stop();
	udpSink.stop();
	directPlay->stop();
	directPlay.reset();

	pico_stack_tick();
	pico_stack_tick();
	pico_stack_tick();

	if (pico_dev)
	{
		if (usingPPP) {
			pico_ppp_destroy(pico_dev);
		}
		else
		{
			closeDumpFile();
			pico_dhcp_server_destroy(pico_dev);
			pico_device_destroy(pico_dev);
		}
		pico_dev = nullptr;
	}
	pico_stack_deinit();
	if (upnp)
	{
		std::thread pnpTerm([upnp = this->upnp]() {
			upnp->Term();
		});
		pnpTerm.detach();
		upnp.reset();
	}
}

static PicoThread pico_thread;

static bool start_pico()
{
	emu.setNetworkState(true);
	if (pico_thread_running)
		return false;
	pico_thread_running = true;
	pico_thread.start();

    return true;
}

static void stop_pico()
{
	emu.setNetworkState(false);
	pico_thread_running = false;
	pico_thread.stop();
}

// picotcp mutex implementation
extern "C" {

void *pico_mutex_init(void) {
	return new std::mutex();
}

void pico_mutex_lock(void *mux) {
	((std::mutex *)mux)->lock();
}

void pico_mutex_unlock(void *mux) {
	((std::mutex *)mux)->unlock();
}

void pico_mutex_deinit(void *mux) {
	delete (std::mutex *)mux;
}

}

namespace net::modbba
{

bool PicoTcpService::start() {
	return start_pico();
}
void PicoTcpService::stop() {
	stop_pico();
}

void PicoTcpService::writeModem(u8 b) {
	write_pico(b);
}
int PicoTcpService::readModem() {
	return read_pico();
}
int PicoTcpService::modemAvailable() {
	return pico_available();
}

void PicoTcpService::receiveEthFrame(const u8 *frame, u32 size) {
	pico_receive_eth_frame(frame, size);
}

}

