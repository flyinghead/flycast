/*
	Copyright 2025 flyinghead

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
#pragma once
#if defined(__ANDROID__) && !defined(LIBRETRO)
#include "android_locale.h"
#endif
#include <string>
#include <time.h>
#include <locale>
#include <locale.h>

namespace i18n
{
#if defined(__ANDROID__) && !defined(LIBRETRO)

std::string getCurrentLocale();
std::string formatShortDateTime(time_t time);

#else

using std::locale;

void init();

#if !defined(LIBRETRO) && !defined(__SWITCH__) && !defined(_WIN32)

static inline std::string getCurrentLocale() {
	return setlocale(LC_MESSAGES, nullptr);
}

#else

std::string getCurrentLocale();

#endif

static inline std::string formatShortDateTime(time_t time)
{
	tm t;
#ifdef  _WIN32
	tm *ptm = localtime(&time);
	if (ptm == nullptr)
		return {};
	t = *ptm;
#else
	if (localtime_r(&time, &t) == nullptr)
		return {};
#endif
	std::string s(256, '\0');
	// %x The preferred date representation for the current locale without the time (posix: %m/%d/%y)
	// %X same for the time (posix: %H:%M:%S)
	s.resize(strftime(s.data(), s.size(), "%x %X", &t));
	return s;
}

#endif	// !ANDROID

std::string Ts(const std::string& msg);
const char *T(const char *msg);

// To mark a string as needing translation only
#define Tnop(string) ((char *)(string))

const char *translateCtx(const std::string& context, const char *msg);

const char *translatePlural(const char *msg, const char *msgPlural, int num);

}
