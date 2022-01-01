/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */
#if defined(__unix__) || defined(__APPLE__) || defined(__SWITCH__)

#include "platform_linux.h"
#include <time.h>
#include <strings.h>
#include <chrono>

uint32_t GGPOPlatform::GetCurrentTimeMS()
{
	using namespace std::chrono;
	static steady_clock::time_point startTime = steady_clock::now();

	return (uint32_t)duration_cast<milliseconds>(steady_clock::now() - startTime).count();
}

int GGPOPlatform::GetConfigInt(const char* name)
{
   char *buf = getenv(name);
   if (buf == nullptr)
      return 0;
   return atoi(buf);
}

bool GGPOPlatform::GetConfigBool(const char* name)
{
	char *buf = getenv(name);
	if (buf == nullptr)
		return false;
   return atoi(buf) != 0 || strcasecmp(buf, "true") == 0;
}

#endif
