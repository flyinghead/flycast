/* -----------------------------------------------------------------------
 * GGPO.net (http://ggpo.net)  -  Copyright 2009 GroundStorm Studios, LLC.
 *
 * Use of this software is governed by the MIT license that can be found
 * in the LICENSE file.
 */

#include "ggpo_types.h"
#include "log/Log.h"
#include "log/LogManager.h"
#include <string>

void Log(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   Logv(fmt, args);
   va_end(args);
}

void Logv(const char* fmt, va_list args)
{
	std::string copy;
	if (fmt[strlen(fmt) - 1] == '\n') {
		copy = fmt;
		copy.pop_back();
		fmt = copy.c_str();
	}
	if (LogManager::GetInstance())
		LogManager::GetInstance()->Log(LogTypes::LDEBUG, LogTypes::NETWORK, __FILE__, __LINE__, fmt, args);
}
