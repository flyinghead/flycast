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
#include "i18n.h"
#include "resources.h"
#include "cfg/ini.h"
#include "stdclass.h"
#ifdef _WIN32
#include <windows.h>
#include <nowide/stackstring.hpp>
#endif

namespace i18n
{

static bool inited;
static std::map<std::string, std::string> messages;

static void load(const std::string& language)
{
	size_t size;
	std::unique_ptr<u8[]> data = resource::load("i18n/" + language, size);
	if (data == nullptr)
		return;
	config::IniFile cat;
	cat.load(std::string((char *)data.get(), size), true);
	std::vector<std::string> msgs = cat.getEntryNames("");
	for (const auto& msgId : msgs) {
		messages[msgId] = cat.getString("", msgId);
		DEBUG_LOG(BOOT, "Loaded: %s=%s", msgId.c_str(), messages[msgId].c_str());
	}
}

void init()
{
	if (inited)
		return;
	inited = true;
#ifndef LIBRETRO
	try {
		std::locale::global(std::locale(""));
	} catch (const std::runtime_error& e) {
		INFO_LOG(BOOT, "Error setting c++ locale: %s", e.what());
		setlocale(LC_ALL, "");
	}
#endif
	messages.clear();
	std::string locale = getCurrentLocale();
	size_t pos = locale.find_first_of("_-.");
	if (pos == locale.npos)
		pos = locale.length();
	std::string language = locale.substr(0, pos);
	if (language.length() < 2)
		return;
	load(language);
	if (pos == locale.length())
		return;
	++pos;
	size_t pos2 = locale.find_first_of("_-.", pos);
	if (pos2 == locale.npos)
		pos2 = locale.length();
	std::string variant = locale.substr(pos, pos2 - pos);
	std::string lowvar = variant;
	string_tolower(lowvar);
	if (lowvar == "hans")
		variant = "CH";
	else if (lowvar == "hant")
		variant = "TW";
	load(language + "_" + variant);
}

const std::string& T(const std::string& msg)
{
	init();
	auto it = messages.find(msg);
	if (it == messages.end())
		return msg;
	else
		return it->second;
}

const char *Tcs(const char *msg)
{
	std::string in { msg };
	const std::string& out = T(in);
	if (&out == &in)
		return msg;
	else
		return out.c_str();
}

#if defined(_WIN32) && !defined(LIBRETRO)
std::string getCurrentLocale()
{
	wchar_t wname[128];
	if (GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, LOCALE_SNAME, wname, std::size(wname)) == 0) {
		ERROR_LOG(COMMON, "GetLocaleEx failed: %x", GetLastError());
		return "en";
	}
	nowide::stackstring name;
	if (name.convert(wname))
		return name.get();
	else
		return "en";
}
#endif
}
