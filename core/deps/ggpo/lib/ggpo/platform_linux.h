/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GGPO_LINUX_H_
#define _GGPO_LINUX_H_

#include <limits>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <cstdint>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifdef __SWITCH__
#include "nswitch.h"
#else
#include <netinet/ip.h>
#endif
#include <netinet/tcp.h>
#include <arpa/inet.h>

using SOCKET = int;
#define closesocket close
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define WSAEWOULDBLOCK EWOULDBLOCK

constexpr size_t MAX_PATH = 4096;
#ifdef INT_MAX
#undef INT_MAX
#endif
constexpr int INT_MAX = std::numeric_limits<int>::max();

class GGPOPlatform {
public:  // types
   typedef pid_t ProcessID;

public:  // functions
   static ProcessID GetProcessID() { return getpid(); }
   static void AssertFailed(char *msg) { fprintf(stderr, "%s", msg); }
   static uint32_t GetCurrentTimeMS();
   static int GetConfigInt(const char* name);
   static bool GetConfigBool(const char* name);
};

extern "C" {

inline static void DebugBreak()
{
	__builtin_trap();
}

inline static int WSAGetLastError() {
	return errno;
}

}

#endif
