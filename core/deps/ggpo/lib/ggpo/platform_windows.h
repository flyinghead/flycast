/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#ifndef _GGPO_WINDOWS_H_
#define _GGPO_WINDOWS_H_

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <chrono>

#include "ggpo_types.h"

class GGPOPlatform {
public:  // types
   typedef DWORD ProcessID;

public:  // functions
   static ProcessID GetProcessID() { return GetCurrentProcessId(); }
   static void AssertFailed(char *msg) { 
#ifndef TARGET_UWP
	   MessageBoxA(NULL, msg, "GGPO Assertion Failed", MB_OK | MB_ICONEXCLAMATION);
#endif
   }
   static uint32 GetCurrentTimeMS() {
#ifdef TARGET_UWP
	   using namespace std::chrono;
	   static steady_clock::time_point startTime = steady_clock::now();

	   return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - startTime).count();
#else
	   return timeGetTime();
#endif
   }
   static int GetConfigInt(const char* name);
   static bool GetConfigBool(const char* name);
};

#endif
