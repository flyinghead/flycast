/*
	Created on: Sep 15, 2018

	Copyright 2018 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#if !defined(_MSC_VER)

#include "stdclass.h"
#include "oslib/oslib.h"

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
}

#include "net_platform.h"

#include "types.h"
#include "cfg/cfg.h"
#include "picoppp.h"
#include "miniupnp.h"
#include "reios/reios.h"
#include "hw/naomi/naomi_cart.h"
#include "cfg/option.h"
#include "emulator.h"

#include <map>
#include <mutex>
#include <queue>
#include <future>

#define RESOLVER1_OPENDNS_COM "208.67.222.222"
#define AFO_ORIG_IP 0x83f2fb3f		// 63.251.242.131 in network order
#define IGP_ORIG_IP 0xef2bd2cc		// 204.210.43.239 in network order

static pico_device *pico_dev;

static std::queue<u8> in_buffer;
static std::queue<u8> out_buffer;

static std::mutex in_buffer_lock;
static std::mutex out_buffer_lock;

static pico_ip4 dcaddr;
static pico_ip4 dnsaddr;
static pico_socket *pico_tcp_socket, *pico_udp_socket;

static pico_ip4 afo_ip;

struct socket_pair
{
	socket_pair() : pico_sock(nullptr), native_sock(INVALID_SOCKET) {}
	socket_pair(pico_socket *pico_sock, sock_t native_sock) : pico_sock(pico_sock), native_sock(native_sock) {}
	~socket_pair() {
		if (pico_sock != nullptr)
			pico_socket_close(pico_sock);
		if (native_sock != INVALID_SOCKET)
			closesocket(native_sock);
	}
	socket_pair(socket_pair &&) = default;
	socket_pair(const socket_pair&) = delete;
	socket_pair& operator=(const socket_pair&) = delete;

	pico_socket *pico_sock;
	sock_t native_sock;
	std::vector<char> in_buffer;
	bool shutdown = false;

	void receive_native()
	{
		size_t len;
		const char *data;
		char buf[536];

		if (!in_buffer.empty())
		{
			len = in_buffer.size();
			data = &in_buffer[0];
		}
		else
		{
			if (native_sock == INVALID_SOCKET)
			{
				if (!shutdown && pico_sock->q_out.size == 0)
				{
					pico_socket_shutdown(pico_sock, PICO_SHUT_RDWR);
					shutdown = true;
				}
				return;
			}
			int r = (int)recv(native_sock, buf, sizeof(buf), 0);
			if (r == 0)
			{
				INFO_LOG(MODEM, "Socket[%d] recv(%zd) returned 0 -> EOF", short_be(pico_sock->remote_port), sizeof(buf));
				closesocket(native_sock);
				native_sock = INVALID_SOCKET;
				return;
			}
			if (r < 0)
			{
				if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
				{
					perror("recv tcp socket");
					closesocket(native_sock);
					native_sock = INVALID_SOCKET;
				}
				return;
			}
			len = r;
			data = buf;
		}
		if (pico_sock->remote_port == short_be(5011) && len >= 5)
		{
			// Visual Concepts sport games
			if (buf[0] == 1)
				memcpy(&buf[1], &pico_sock->local_addr.ip4.addr, 4);
		}

		int r2 = pico_socket_send(pico_sock, data, (int)len);
		if (r2 < 0)
			INFO_LOG(MODEM, "error TCP sending: %s", strerror(pico_err));
		else if (r2 < (int)len)
		{
			if (r2 > 0 || in_buffer.empty())
			{
				len -= r2;
				std::vector<char> remain(len);
				memcpy(&remain[0], &data[r2], len);
				std::swap(in_buffer, remain);
			}
		}
		else
		{
			in_buffer.clear();
		}
	}
};

// tcp sockets
static std::map<pico_socket *, socket_pair> tcp_sockets;
static std::map<pico_socket *, sock_t> tcp_connecting_sockets;
// udp sockets: src port -> socket fd
static std::map<uint16_t, sock_t> udp_sockets;

struct GamePortList {
	const char *gameId[10];
	uint16_t udpPorts[10];
	uint16_t tcpPorts[10];
};
static GamePortList GamesPorts[] = {
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
	{ // NBA 2K1,2K2 / NFL 2K1,2K2 / NCAA 2K2
		{ "MK-51063", "HDR-0150",					// NBA 2K1
		  "MK-51178", "HDR-0197", "MK-5117850",   // NBA 2K2
		  "MK-51062", "HDR-0144",					// NFL 2K1
		  "MK-51168", "HDR-0196",					// NFL 2K2
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
		{ 2300, 6500, 47624, 13139 }, // FIXME 2300-2400 ?
		{ 2300, 47624 },			  // FIXME 2300-2400 ?
	},
	{ // Planet Ring
		{ "MK-5114864", "MK-5112550" },
		{ 7648, 1285, 1028 },
		{ },
	},
	{ // StarLancer
		{ "T40209N", "T17723D 05" },
		{ 2300, 6500, 47624 }, // FIXME 2300-2400 ?
		{ 2300, 47624 },	   // FIXME 2300-2400 ?
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

// listening port -> socket fd
static std::map<uint16_t, sock_t> tcp_listening_sockets;

static bool pico_stack_inited;
static bool pico_thread_running = false;
extern "C" int dont_reject_opt_vj_hack;

static void read_native_sockets();
void get_host_by_name(const char *name, pico_ip4 dnsaddr);
int get_dns_answer(pico_ip4 *address, pico_ip4 dnsaddr);

static int modem_read(pico_device *dev, void *data, int len)
{
	u8 *p = (u8 *)data;

	int count = 0;
	out_buffer_lock.lock();
	while (!out_buffer.empty() && count < len)
	{
		*p++ = out_buffer.front();
		out_buffer.pop();
		count++;
	}
	out_buffer_lock.unlock();

    return count;
}

static int modem_write(pico_device *dev, const void *data, int len)
{
	u8 *p = (u8 *)data;

	in_buffer_lock.lock();
	for (int i = 0; i < len; i++)
	{
		while (in_buffer.size() > 1024)
		{
			in_buffer_lock.unlock();
			if (!pico_thread_running)
				return 0;
			PICO_IDLE();
			in_buffer_lock.lock();
		}
		in_buffer.push(*p++);
	}
	in_buffer_lock.unlock();

    return len;
}

void write_pico(u8 b)
{
	out_buffer_lock.lock();
	out_buffer.push(b);
	out_buffer_lock.unlock();
}

int read_pico()
{
	in_buffer_lock.lock();
	if (in_buffer.empty())
	{
		in_buffer_lock.unlock();
		return -1;
	}
	else
	{
		u32 b = in_buffer.front();
		in_buffer.pop();
		in_buffer_lock.unlock();
		return b;
	}
}

static void read_from_dc_socket(pico_socket *pico_sock, sock_t nat_sock)
{
	char buf[1510];

	int r = pico_socket_read(pico_sock, buf, sizeof(buf));
	if (r > 0)
	{
		if (send(nat_sock, buf, r, 0) < r)
		{
			perror("tcp_callback send");
			tcp_sockets.erase(pico_sock);
		}
	}
}

static void tcp_callback(uint16_t ev, pico_socket *s)
{
	if (ev & PICO_SOCK_EV_RD)
	{
		auto it = tcp_sockets.find(s);
		if (it == tcp_sockets.end())
		{
			if (tcp_connecting_sockets.find(s) == tcp_connecting_sockets.end())
				INFO_LOG(MODEM, "Unknown socket: remote port %d", short_be(s->remote_port));
		}
		else
		{
			read_from_dc_socket(it->first, it->second.native_sock);
		}
	}

	if (ev & PICO_SOCK_EV_CONN)
	{
		pico_ip4 orig;
		uint16_t port;
		char peer[30];
		int yes = 1;

		pico_socket *sock_a = pico_socket_accept(s, &orig, &port);
		if (sock_a == NULL)
		{
			// Also called for child sockets
			if (tcp_sockets.find(s) == tcp_sockets.end())
				INFO_LOG(MODEM, "pico_socket_accept: %s\n", strerror(pico_err));
		}
		else
		{
			pico_ipv4_to_string(peer, sock_a->local_addr.ip4.addr);
			//printf("Connection established from port %d to %s:%d\n", short_be(port), peer, short_be(sock_a->local_port));
			pico_socket_setoption(sock_a, PICO_TCP_NODELAY, &yes);
			pico_tcp_set_linger(sock_a, 10000);
			/* Set keepalive options */
	//		uint32_t ka_val = 5;
	//		pico_socket_setoption(sock_a, PICO_SOCKET_OPT_KEEPCNT, &ka_val);
	//		ka_val = 30000;
	//		pico_socket_setoption(sock_a, PICO_SOCKET_OPT_KEEPIDLE, &ka_val);
	//		ka_val = 5000;
	//		pico_socket_setoption(sock_a, PICO_SOCKET_OPT_KEEPINTVL, &ka_val);

			sock_t sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (!VALID(sockfd))
			{
				perror("socket");
			}
			else
			{
				sockaddr_in serveraddr;
				memset(&serveraddr, 0, sizeof(serveraddr));
				serveraddr.sin_family = AF_INET;
				serveraddr.sin_addr.s_addr = sock_a->local_addr.ip4.addr;
		        if (serveraddr.sin_addr.s_addr == AFO_ORIG_IP			// Alien Front Online
					|| serveraddr.sin_addr.s_addr == IGP_ORIG_IP)		// Internet Game Pack
				{
		        	serveraddr.sin_addr.s_addr = afo_ip.addr;		// same ip for both for now
				}

				serveraddr.sin_port = sock_a->local_port;
				set_non_blocking(sockfd);
				if (connect(sockfd, (sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
				{
					if (get_last_error() != EINPROGRESS && get_last_error() != L_EWOULDBLOCK)
					{
						pico_ipv4_to_string(peer, sock_a->local_addr.ip4.addr);
						INFO_LOG(MODEM, "TCP connection to %s:%d failed: %s", peer, short_be(sock_a->local_port), strerror(get_last_error()));
						closesocket(sockfd);
					}
					else
						tcp_connecting_sockets[sock_a] = sockfd;
				}
				else
				{
					set_tcp_nodelay(sockfd);

					tcp_sockets.emplace(std::piecewise_construct,
					              std::forward_as_tuple(sock_a),
					              std::forward_as_tuple(sock_a, sockfd));
				}
			}
		}
	}

	if (ev & PICO_SOCK_EV_FIN) {
		auto it = tcp_sockets.find(s);
		if (it != tcp_sockets.end())
		{
			tcp_sockets.erase(it);
		}
		else
		{
			auto it2 = tcp_connecting_sockets.find(s);
			if (it2 != tcp_connecting_sockets.end())
			{
				closesocket(it2->second);
				tcp_connecting_sockets.erase(it2);
			}
			else
				INFO_LOG(MODEM, "PICO_SOCK_EV_FIN: Unknown socket: remote port %d", short_be(s->remote_port));
		}
	}

	if (ev & PICO_SOCK_EV_ERR) {
		INFO_LOG(MODEM, "Socket error received: %s", strerror(pico_err));
		auto it = tcp_sockets.find(s);
		if (it == tcp_sockets.end())
			INFO_LOG(MODEM, "PICO_SOCK_EV_ERR: Unknown socket: remote port %d", short_be(s->remote_port));
		else
			tcp_sockets.erase(it);
	}

	if (ev & PICO_SOCK_EV_CLOSE)
	{
		auto it = tcp_sockets.find(s);
		if (it == tcp_sockets.end())
		{
			INFO_LOG(MODEM, "PICO_SOCK_EV_CLOSE: Unknown socket: remote port %d", short_be(s->remote_port));
		}
		else
		{
			if (it->second.native_sock != INVALID_SOCKET)
				shutdown(it->second.native_sock, SHUT_WR);
			pico_socket_shutdown(s, PICO_SHUT_RD);
		}
	}

//	if (ev & PICO_SOCK_EV_WR)
//	{
//	}
}

static sock_t find_udp_socket(uint16_t src_port)
{
	auto it = udp_sockets.find(src_port);
	if (it != udp_sockets.end())
		return it->second;

	sock_t sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (!VALID(sockfd))
	{
		perror("socket");
		return -1;
	}
#ifndef _WIN32
	fcntl(sockfd, F_SETFL, O_NONBLOCK);
#else
	u_long optl = 1;
	ioctlsocket(sockfd, FIONBIO, &optl);
#endif
	int broadcastEnable = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcastEnable, sizeof(broadcastEnable));

	// FIXME Need to clean up at some point?
	udp_sockets[src_port] = sockfd;

	return sockfd;
}

static void udp_callback(uint16_t ev, pico_socket *s)
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
			r = pico_socket_recvfrom_extended(s, buf, sizeof(buf), &src_addr.addr, &src_port, &msginfo);

			if (r <= 0)
			{
				if (r < 0)
					INFO_LOG(MODEM, "error UDP recv: %s", strerror(pico_err));
				break;
			}
			sock_t sockfd = find_udp_socket(src_port);
			if (VALID(sockfd))
			{
				sockaddr_in dst_addr;
				socklen_t addr_len = sizeof(dst_addr);
				memset(&dst_addr, 0, sizeof(dst_addr));
				dst_addr.sin_family = AF_INET;
				dst_addr.sin_addr.s_addr = msginfo.local_addr.ip4.addr;
				dst_addr.sin_port = msginfo.local_port;
				if (sendto(sockfd, buf, r, 0, (const sockaddr *)&dst_addr, addr_len) < 0)
					perror("sendto udp socket");
			}
		}
	}

	if (ev & PICO_SOCK_EV_ERR) {
		INFO_LOG(MODEM, "UDP Callback error received");
	}
}

static void read_native_sockets()
{
	int r;
	sockaddr_in src_addr;
	socklen_t addr_len;

	// Accept incoming TCP connections
	for (auto it = tcp_listening_sockets.begin(); it != tcp_listening_sockets.end(); it++)
	{
		addr_len = sizeof(src_addr);
		memset(&src_addr, 0, addr_len);
		sock_t sockfd = accept(it->second, (sockaddr *)&src_addr, &addr_len);
		if (!VALID(sockfd))
		{
			if (get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
				perror("accept");
			continue;
		}
    	//printf("Incoming TCP connection from %08x to port %d\n", src_addr.sin_addr.s_addr, short_be(it->first));
    	pico_socket *ps = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, &tcp_callback);
    	if (ps == NULL)
    	{
    		INFO_LOG(MODEM, "pico_socket_open failed: error %d", pico_err);
    		closesocket(sockfd);
    		continue;
    	}
    	ps->local_addr.ip4.addr = src_addr.sin_addr.s_addr;
    	ps->local_port = src_addr.sin_port;
    	if (pico_socket_connect(ps, &dcaddr.addr, it->first) != 0)
    	{
    		INFO_LOG(MODEM, "pico_socket_connect failed: error %d", pico_err);
    		closesocket(sockfd);
    		pico_socket_close(ps);
    		continue;
    	}
    	set_non_blocking(sockfd);
    	set_tcp_nodelay(sockfd);

		tcp_sockets.emplace(std::piecewise_construct,
		              std::forward_as_tuple(ps),
		              std::forward_as_tuple(ps, sockfd));

	}

	// Check connecting outbound TCP sockets
	fd_set write_fds;
	FD_ZERO(&write_fds);
	int max_fd = -1;
	for (auto it = tcp_connecting_sockets.begin(); it != tcp_connecting_sockets.end(); it++)
	{
		FD_SET(it->second, &write_fds);
		max_fd = std::max(max_fd, (int)it->second);
	}
	timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	if (select(max_fd + 1, NULL, &write_fds, NULL, &tv) > 0)
	{
		for (auto it = tcp_connecting_sockets.begin(); it != tcp_connecting_sockets.end(); )
		{
			if (!FD_ISSET(it->second, &write_fds))
			{
				it++;
				continue;
			}
#ifdef _WIN32
			char value;
#else
			int value;
#endif
			socklen_t l = sizeof(int);
			if (getsockopt(it->second, SOL_SOCKET, SO_ERROR, &value, &l) < 0 || value)
			{
				char peer[30];
				pico_ipv4_to_string(peer, it->first->local_addr.ip4.addr);
				INFO_LOG(MODEM, "TCP connection to %s:%d failed: %s", peer, short_be(it->first->local_port), strerror(get_last_error()));
				pico_socket_close(it->first);
				closesocket(it->second);
			}
			else
			{
				set_tcp_nodelay(it->second);

				tcp_sockets.emplace(std::piecewise_construct,
				              std::forward_as_tuple(it->first),
				              std::forward_as_tuple(it->first, it->second));

				read_from_dc_socket(it->first, it->second);
			}
			it = tcp_connecting_sockets.erase(it);
		}
	}

	static char buf[1500];
	pico_msginfo msginfo;

	// Read UDP sockets
	for (auto it = udp_sockets.begin(); it != udp_sockets.end(); it++)
	{
		if (!VALID(it->second))
			continue;

		addr_len = sizeof(src_addr);
		memset(&src_addr, 0, addr_len);
		r = (int)recvfrom(it->second, buf, sizeof(buf), 0, (sockaddr *)&src_addr, &addr_len);
		// filter out messages coming from ourselves (happens for broadcasts)
		if (r > 0 && !is_local_address(src_addr.sin_addr.s_addr))
		{
			msginfo.dev = pico_dev;
			msginfo.tos = 0;
			msginfo.ttl = 0;
			msginfo.local_addr.ip4.addr = src_addr.sin_addr.s_addr;
			msginfo.local_port = src_addr.sin_port;

			int r2 = pico_socket_sendto_extended(pico_udp_socket, buf, r, &dcaddr, it->first, &msginfo);
			if (r2 < r)
				INFO_LOG(MODEM, "error UDP sending to %d: %s", short_be(it->first), strerror(pico_err));
		}
		else if (r < 0 && get_last_error() != L_EAGAIN && get_last_error() != L_EWOULDBLOCK)
		{
			perror("recvfrom udp socket");
			continue;
		}
	}

	// Read TCP sockets
	for (auto it = tcp_sockets.begin(); it != tcp_sockets.end(); )
	{
		it->second.receive_native();
		if (it->second.pico_sock == nullptr)
			it = tcp_sockets.erase(it);
		else
			it++;
	}
}

static void close_native_sockets()
{
	for (auto it = udp_sockets.begin(); it != udp_sockets.end(); it++)
		closesocket(it->second);
	udp_sockets.clear();
	tcp_sockets.clear();
	for (auto it = tcp_connecting_sockets.begin(); it != tcp_connecting_sockets.end(); it++)
	{
		pico_socket_close(it->first);
		closesocket(it->second);
	}
	tcp_connecting_sockets.clear();
}

static int modem_set_speed(pico_device *dev, uint32_t speed)
{
    return 0;
}

static void check_dns_entries()
{
    static uint32_t dns_query_start = 0;
    static uint32_t dns_query_attempts = 0;

	if (afo_ip.addr == 0)
	{
		if (!dns_query_start)
		{
			dns_query_start = PICO_TIME_MS();
			get_host_by_name("auriga.segasoft.com", dnsaddr);	// Alien Front Online server
		}
		else
		{
			if (get_dns_answer(&afo_ip, dnsaddr) == 0)
			{
				dns_query_attempts = 0;
				dns_query_start = 0;
				char afoip[16];
				pico_ipv4_to_string(afoip, afo_ip.addr);
				INFO_LOG(MODEM, "AFO server IP is %s", afoip);
			}
			else
			{
				if (PICO_TIME_MS() - dns_query_start > 1000)
				{
					if (++dns_query_attempts >= 5)
					{
						u32 addr;
						pico_string_to_ipv4("146.185.135.179", &addr);	// Default address
						memcpy(&afo_ip.addr, &addr, sizeof(addr));
						dns_query_attempts = 0;
					}
					else
						// Retry
						dns_query_start = 0;
				}
			}
		}
	}
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

//#define BBA_PCAPNG_DUMP
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
	u64 now = (u64)(os_GetSeconds() * 1000000.0);
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
	if (pcapngDump != nullptr)
	{
		fclose(pcapngDump);
		pcapngDump = nullptr;
	}
}
void pico_receive_eth_frame(const u8 *frame, u32 size)
{
	dumpFrame(frame, size);
	if (pico_dev != nullptr)
		pico_stack_recv(pico_dev, (u8 *)frame, size);
}

static int send_eth_frame(pico_device *dev, void *data, int len)
{
	dumpFrame((const u8 *)data, len);
	return pico_send_eth_frame((const u8 *)data, len);
}

static void *pico_thread_func(void *)
{
    if (!pico_stack_inited)
    {
    	pico_stack_init();
    	pico_stack_inited = true;
#if _WIN32
		static WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 0), &wsaData) != 0)
			WARN_LOG(MODEM, "WSAStartup failed");
#endif
    }

	// Find the network ports for the current game
	const GamePortList *ports = nullptr;
	std::string gameId;
	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		gameId = std::string(ip_meta.product_number, sizeof(ip_meta.product_number));
		gameId = trim_trailing_ws(gameId);
	}
	else
	{
		gameId = naomi_game_id;
	}
	for (u32 i = 0; i < ARRAY_SIZE(GamesPorts) && ports == nullptr; i++)
	{
		const auto& game = GamesPorts[i];
		for (u32 j = 0; j < ARRAY_SIZE(game.gameId) && game.gameId[j] != nullptr; j++)
		{
			if (gameId == game.gameId[j])
			{
				NOTICE_LOG(MODEM, "Found network ports for game %s", gameId.c_str());
				ports = &game;
				break;
			}
		}
	}
	// Web TV requires the VJ compression option, which picotcp doesn't support.
	// This hack allows WebTV to connect although the correct fix would
	// be to implement VJ compression.
	dont_reject_opt_vj_hack = gameId == "6107117" || gameId == "610-7390" || gameId == "610-7391" ? 1 : 0;

	std::future<MiniUPnP> upnp =
		std::async(std::launch::async, [ports]() {
			// Initialize miniupnpc and map network ports
			MiniUPnP upnp;
			if (ports != nullptr)
			{
				if (!upnp.Init())
					WARN_LOG(MODEM, "UPNP Init failed");
				else
				{
					for (u32 i = 0; i < ARRAY_SIZE(ports->udpPorts) && ports->udpPorts[i] != 0; i++)
						if (!upnp.AddPortMapping(ports->udpPorts[i], false))
							WARN_LOG(MODEM, "UPNP AddPortMapping UDP %d failed", ports->udpPorts[i]);
					for (u32 i = 0; i < ARRAY_SIZE(ports->tcpPorts) && ports->tcpPorts[i] != 0; i++)
						if (!upnp.AddPortMapping(ports->tcpPorts[i], true))
							WARN_LOG(MODEM, "UPNP AddPortMapping TCP %d failed", ports->tcpPorts[i]);
				}
			}
			return upnp;
		});

	// Empty queues
    {
		std::queue<u8> empty;
		in_buffer_lock.lock();
		std::swap(in_buffer, empty);
		in_buffer_lock.unlock();

		std::queue<u8> empty2;
		out_buffer_lock.lock();
		std::swap(out_buffer, empty2);
		out_buffer_lock.unlock();
    }

	u32 addr;
	pico_string_to_ipv4(config::DNS.get().c_str(), &addr);
	memcpy(&dnsaddr.addr, &addr, sizeof(addr));

	// Create ppp/eth device
	if (!config::EmulateBBA)
	{
		// PPP
		pico_dev = pico_ppp_create();
		if (!pico_dev)
			return NULL;
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
		pico_ppp_set_serial_set_speed(pico_dev, modem_set_speed);
		pico_dev->proxied = 1;

		pico_ppp_connect(pico_dev);
	}
	else
	{
		// Ethernet
		pico_dev = pico_eth_create();
		if (pico_dev == nullptr)
			return nullptr;
		pico_dev->send = &send_eth_frame;
		pico_dev->proxied = 1;

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
			WARN_LOG(MODEM, "DHCP server init failed");
	}

    pico_udp_socket = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_UDP, &udp_callback);
    if (pico_udp_socket == NULL) {
    	INFO_LOG(MODEM, "error opening UDP socket: %s", strerror(pico_err));
		return nullptr;
    }
    int yes = 1;
    pico_ip4 inaddr_any = {0};
    uint16_t listen_port = 0;
    int ret = pico_socket_bind(pico_udp_socket, &inaddr_any, &listen_port);
    if (ret < 0)
    	INFO_LOG(MODEM, "error binding UDP socket to port %u: %s", short_be(listen_port), strerror(pico_err));

    pico_tcp_socket = pico_socket_open(PICO_PROTO_IPV4, PICO_PROTO_TCP, &tcp_callback);
    if (pico_tcp_socket == NULL) {
    	INFO_LOG(MODEM, "error opening TCP socket: %s", strerror(pico_err));
    }
    pico_socket_setoption(pico_tcp_socket, PICO_TCP_NODELAY, &yes);
    ret = pico_socket_bind(pico_tcp_socket, &inaddr_any, &listen_port);
    if (ret < 0) {
    	INFO_LOG(MODEM, "error binding TCP socket to port %u: %s", short_be(listen_port), strerror(pico_err));
    }
    else
    {
        if (pico_socket_listen(pico_tcp_socket, 10) != 0)
        	INFO_LOG(MODEM, "error listening on port %u", short_be(listen_port));
    }

	// Open listening sockets
	sockaddr_in saddr;
	socklen_t saddr_len = sizeof(saddr);
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	if (ports != nullptr)
	{
		for (u32 i = 0; i < ARRAY_SIZE(ports->udpPorts) && ports->udpPorts[i] != 0; i++)
		{
			uint16_t port = short_be(ports->udpPorts[i]);
			sock_t sockfd = find_udp_socket(port);
			saddr.sin_port = port;

			if (::bind(sockfd, (sockaddr *)&saddr, saddr_len) < 0)
			{
				perror("bind");
				closesocket(sockfd);
				auto it = udp_sockets.find(port);
				if (it != udp_sockets.end())
					it->second = INVALID_SOCKET;
				continue;
			}
		}

		for (u32 i = 0; i < ARRAY_SIZE(ports->tcpPorts) && ports->tcpPorts[i] != 0; i++)
		{
			uint16_t port = short_be(ports->tcpPorts[i]);
			saddr.sin_port = port;
			sock_t sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if (::bind(sockfd, (sockaddr *)&saddr, saddr_len) < 0)
			{
				perror("bind");
				closesocket(sockfd);
				continue;
			}
			if (listen(sockfd, 5) < 0)
			{
				perror("listen");
				closesocket(sockfd);
				continue;
			}
			set_non_blocking(sockfd);
			tcp_listening_sockets[port] = sockfd;
		}
	}

	while (pico_thread_running)
    {
    	read_native_sockets();
    	pico_stack_tick();
    	check_dns_entries();
		PICO_IDLE();
    }

    for (auto it = tcp_listening_sockets.begin(); it != tcp_listening_sockets.end(); it++)
    	closesocket(it->second);
	close_native_sockets();
	pico_socket_close(pico_tcp_socket);
	pico_socket_close(pico_udp_socket);

	if (pico_dev)
	{
		if (!config::EmulateBBA)
		{
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
	pico_stack_tick();
	if (ports != nullptr)
		upnp.get().Term();

	return NULL;
}

static cThread pico_thread(pico_thread_func, NULL);

bool start_pico()
{
	emu.setNetworkState(true);
	if (pico_thread_running)
		return false;
	pico_thread_running = true;
	pico_thread.Start();

    return true;
}

void stop_pico()
{
	emu.setNetworkState(false);
	pico_thread_running = false;
	pico_thread.WaitToEnd();
}

#else

#include "types.h"

bool start_pico() { return false; }
void stop_pico() { }
void write_pico(u8 b) { }
int read_pico() { return -1; }
void pico_receive_eth_frame(const u8* frame, u32 size) {}

#endif
