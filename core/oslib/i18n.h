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
#ifdef __ANDROID__
#include "android_locale.h"
#endif
#include "log/Log.h"
#include <string>
#include <time.h>
#include <locale>
#include <locale.h>

namespace i18n
{
#ifdef __ANDROID__

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

// Note: if the message isn't found (or in English) and has no ImGui id (...##imguiId), the 'msg' argument will be returned.
// If needed, it is the responsibility of the caller to make sure the reference is still valid after the statement is executed.
// Examples:
// printf("%s", T("something").c_str());	// this is ok, the reference is still valid when printf is called.
// const std::string& tr = T("crash");		// tr is a reference to a temporary string that has been deleted -> crash
// std::string str = T("no crash");			// this is ok, the temporary string is copied before being deleted.
// Furthermore, this function may use a static variable to store the result and thus isn't re-entrant
//
const std::string& T(const std::string& msg);

const char *Tcs(const char *msg);

}
