/*
	Created on: Apr 13, 2020

	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#ifndef _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef __SWITCH__
#include "nswitch.h"
#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif
#ifndef INET_ADDRSTRLEN
#define INET_ADDRSTRLEN sizeof(struct sockaddr_in)
#endif
#define SOL_TCP 6 // Shrug
#else
#include <netinet/ip.h>
#endif // __SWITCH__
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <sys/select.h>
#else
#include <ws2tcpip.h>
#endif

#ifndef _WIN32
#define closesocket close
typedef int sock_t;
#define VALID(s) ((s) >= 0)
#define L_EWOULDBLOCK EWOULDBLOCK
#define L_EAGAIN EAGAIN
#define get_last_error() (errno)
#define INVALID_SOCKET (-1)
#define perror(s) do { INFO_LOG(MODEM, "%s: %s", (s) != NULL ? (s) : "", strerror(get_last_error())); } while (false)
#else
typedef SOCKET sock_t;
#define VALID(s) ((s) != INVALID_SOCKET)
#define L_EWOULDBLOCK WSAEWOULDBLOCK
#define L_EAGAIN WSAEWOULDBLOCK
#define get_last_error() (WSAGetLastError())
#define perror(s) do { INFO_LOG(MODEM, "%s: Winsock error: %d\n", (s) != NULL ? (s) : "", WSAGetLastError()); } while (false)
#define SHUT_WR SD_SEND
#define SHUT_RD SD_RECEIVE
#endif

bool is_local_address(u32 addr);

static inline void set_non_blocking(sock_t fd)
{
#ifndef _WIN32
	fcntl(fd, F_SETFL, O_NONBLOCK);
#else
	u_long optl = 1;
	ioctlsocket(fd, FIONBIO, &optl);
#endif
}

static inline void set_tcp_nodelay(sock_t fd)
{
	int optval = 1;
	socklen_t optlen = sizeof(optval);
#if defined(_WIN32)
	struct protoent *tcp_proto = getprotobyname("TCP");
	setsockopt(fd, tcp_proto->p_proto, TCP_NODELAY, (const char *)&optval, optlen);
#elif !defined(__APPLE__) && !defined(__FreeBSD__) && !defined(__NetBSD__)
	setsockopt(fd, SOL_TCP, TCP_NODELAY, (const void *)&optval, optlen);
#else
	struct protoent *tcp_proto = getprotobyname("TCP");
	setsockopt(fd, tcp_proto->p_proto, TCP_NODELAY, &optval, optlen);
#endif
}

static inline bool set_recv_timeout(sock_t fd, int delayms)
{
#ifdef _WIN32
    const DWORD dwDelay = delayms;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&dwDelay, sizeof(DWORD)) == 0;
#else
    struct timeval tv;
    tv.tv_sec = delayms / 1000;
    tv.tv_usec = (delayms % 1000) * 1000;
    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
#endif
}

#if defined(_WIN32) && _WIN32_WINNT < 0x0600
static inline const char *inet_ntop(int af, const void* src, char* dst, int cnt)
{
    struct sockaddr_in srcaddr;

    memset(&srcaddr, 0, sizeof(struct sockaddr_in));
    memcpy(&srcaddr.sin_addr, src, sizeof(srcaddr.sin_addr));

    srcaddr.sin_family = af;
    if (WSAAddressToString((struct sockaddr *)&srcaddr, sizeof(struct sockaddr_in), 0, dst, (LPDWORD)&cnt) != 0)
        return nullptr;
    else
    	return dst;
}
#endif
