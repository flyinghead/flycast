/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "ggpo_types.h"
#include "udp.h"

SOCKET
CreateSocket(uint16 bind_port, bool v6)
{
   int af = v6 ? AF_INET6 : AF_INET;
   SOCKET s = socket(af, SOCK_DGRAM, 0);
   int optval = 1;
   setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&optval, sizeof optval);
   optval = 0;
   setsockopt(s, SOL_SOCKET, SO_LINGER, (const char *)&optval, sizeof optval);

   // non-blocking...
#ifndef _WIN32
	fcntl(s, F_SETFL, O_NONBLOCK);
#else
	u_long iMode = 1;
	ioctlsocket(s, FIONBIO, &iMode);
#endif

#if defined(__APPLE__)
   optval = 9216;
#else
   optval = 16384;
#endif
   setsockopt(s, SOL_SOCKET, SO_SNDBUF, (const char *)&optval, sizeof optval);
   setsockopt(s, SOL_SOCKET, SO_RCVBUF, (const char *)&optval, sizeof optval);

   sockaddr_storage addr_storage{};
   socklen_t addrlen = 0;
   if (!v6) {
      auto addr = reinterpret_cast<sockaddr_in*>(&addr_storage);
      addr->sin_family = AF_INET;
      addr->sin_addr.s_addr = htonl(INADDR_ANY);
      addr->sin_port = htons(bind_port);
      addrlen = sizeof(sockaddr_in);
   } else {
      auto addr = reinterpret_cast<sockaddr_in6*>(&addr_storage);
      addr->sin6_family = AF_INET6;
      addr->sin6_addr = in6addr_any;
      addr->sin6_port = htons(bind_port);
      addrlen = sizeof(sockaddr_in6);
   }

   if (bind(s, (sockaddr *)&addr_storage, addrlen) == 0) {
     LogInfo("Udp bound to port: %d.", bind_port);
	 return s;
   }
   closesocket(s);
   return INVALID_SOCKET;
}

Udp::Udp() :
   _socket_v4(INVALID_SOCKET),
   _socket_v6(INVALID_SOCKET),
   _callbacks(NULL)
{
}

Udp::~Udp(void)
{
   if (_socket_v4 != INVALID_SOCKET) {
      closesocket(_socket_v4);
      _socket_v4 = INVALID_SOCKET;
   }
   if (_socket_v6 != INVALID_SOCKET) {
      closesocket(_socket_v6);
      _socket_v6 = INVALID_SOCKET;
   }
}

void
Udp::Init(uint16 port, Poll *poll, Callbacks *callbacks)
{
   _callbacks = callbacks;
   poll->RegisterLoop(this);

   Log("binding udp socket to port %d.", port);
   _socket_v4 = CreateSocket(port, false);
   if (_socket_v4 == INVALID_SOCKET)
	   throw GGPOException("Socket creation or bind failed", GGPO_ERRORCODE_NETWORK_ERROR);
   _socket_v6 = CreateSocket(port, true);
}

void
Udp::SendTo(char *buffer, int len, int flags, struct sockaddr *dst, int destlen)
{
   ASSERT(dst->sa_family == AF_INET || dst->sa_family == AF_INET6);
   bool v4 = dst->sa_family == AF_INET;
   SOCKET sock = v4 ? _socket_v4 : _socket_v6;
   // FIXME: Using sizeof(sockaddr_storage) as destlen causes sendto to fail, at least on macOS.
   destlen = v4 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
   if (sock == INVALID_SOCKET) {
      return;
   }
   int res = sendto(sock, buffer, len, flags, dst, destlen);
   if (res == SOCKET_ERROR) {
	  int err = WSAGetLastError();
      if (err != WSAEWOULDBLOCK) {
         LogError("unknown error in sendto (erro: %d  wsaerr: %d).", res, err);
      }
      ASSERT(false && "Unknown error in sendto");
      return;
   }
   char dst_ip[1024];
   int port = 0;
   if (v4) {
      inet_ntop(AF_INET, &((sockaddr_in*)dst)->sin_addr, dst_ip, ARRAY_SIZE(dst_ip));
      port = ntohs(((sockaddr_in*)dst)->sin_port);
   } else {
      inet_ntop(AF_INET6, &((sockaddr_in6*)dst)->sin6_addr, dst_ip, ARRAY_SIZE(dst_ip));
      port = ntohs(((sockaddr_in6*)dst)->sin6_port);
   }
   Log("sent packet length %d to %s:%d (ret:%d).", len, dst_ip, port, res);
}

bool
Udp::OnLoopPoll(void *cookie)
{
   uint8 recv_buf[MAX_UDP_PACKET_SIZE];

   for (int s = 0; s < 2; s++) for (;;) {
      SOCKET sock = s == 0 ? _socket_v4 : _socket_v6;
      if (sock == INVALID_SOCKET) {
         continue;
      }
      sockaddr_storage recv_addr{};
      socklen_t recv_addr_len = sizeof(recv_addr);
      int len = recvfrom(sock, (char *)recv_buf, MAX_UDP_PACKET_SIZE, 0, (struct sockaddr *)&recv_addr, &recv_addr_len);

      // TODO: handle len == 0... indicates a disconnect.

      if (len == -1) {
         int error = WSAGetLastError();
         if (error != WSAEWOULDBLOCK) {
            LogError("recvfrom WSAGetLastError returned %d (%x).", error, error);
         }
         break;
      } else if (len > 0) {
         char src_ip[1024];
         if (recv_addr.ss_family == AF_INET) {
            inet_ntop(AF_INET, &((sockaddr_in*)&recv_addr)->sin_addr, src_ip, ARRAY_SIZE(src_ip));
            int port = ntohs(((sockaddr_in*)&recv_addr)->sin_port);
            Log("recvfrom returned (len:%d  from:%s:%d).", len, src_ip, port);
         } else if (recv_addr.ss_family == AF_INET6) {
            inet_ntop(AF_INET6, &((sockaddr_in6*)&recv_addr)->sin6_addr, src_ip, ARRAY_SIZE(src_ip));
            int port = ntohs(((sockaddr_in6*)&recv_addr)->sin6_port);
            Log("recvfrom returned (len:%d  from:%s:%d).", len, src_ip, port);
         }
         UdpMsg *msg = (UdpMsg *)recv_buf;
         _callbacks->OnMsg(recv_addr, msg, len);
      } 
   }
   return true;
}
