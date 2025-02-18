/* Copyright (C) 1991-2020 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

#ifndef	_SYS_UN_H
#define	_SYS_UN_H	1

#include <sys/socket.h>
#include <netinet/in.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* Structure describing the address of an AF_LOCAL (aka AF_UNIX) socket.  */
struct sockaddr_un
{
	sa_family_t sun_family;
	char sun_path[108];		/* Path name.  */
};

/* Should be defined in sockets.h */
struct ipv6_mreq
{
  struct in6_addr ipv6mr_multiaddr;
  unsigned int    ipv6mr_interface;
};

/* Should be declared in net/if.h */
char* if_indextoname(unsigned int, char*);
unsigned int if_nametoindex(const char*);

#ifdef	__cplusplus
}
#endif

#endif	/* sys/un.h  */
