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
#include "tinygettext/tinygettext.hpp"
#include "tinygettext/file_system.hpp"
#include "tinygettext/log.hpp"
#include "log/Log.h"
#ifdef _WIN32
#include <windows.h>
#include <nowide/stackstring.hpp>
#endif
#include <vector>
#include <sstream>
#include <unordered_set>

using namespace tinygettext;

#if defined(TARGET_MAC) && !defined(LIBRETRO)
extern std::string os_Locale();
#endif

namespace i18n
{

static bool inited;

class ResourceFileSystem : public FileSystem
{
	std::vector<std::string> open_directory(const std::string& pathname) override {
		return resource::listDirectory(pathname);
	}
	std::unique_ptr<std::istream> open_file(const std::string& filename) override
	{
		size_t size;
		std::unique_ptr<u8[]> data = resource::load(filename, size);
		if (data == nullptr)
			return nullptr;
		std::string str((const char *)&data[0], (const char *)&data[size]);
		return std::make_unique<std::istringstream>(str);
	}
};

static Dictionary *dictionary;

static std::string getUnixLocaleVariant(const std::string& locale)
{
	size_t at = locale.find('@');
	if (at == locale.npos)
		return {};
	size_t end = locale.find_first_of("_-.@", ++at);
	if (end == locale.npos)
		return locale.substr(at);
	else
		return locale.substr(at, end - at);
}

void parseLocale(const std::string& locale, std::string& language, std::string& country, std::string& variant)
{
	size_t sep = locale.find_first_of("_-.@");
	language = locale.substr(0, sep);
	country.clear();
	variant.clear();
	if (sep == locale.npos)
		return;
	if (locale[sep] == '.') {
		variant = getUnixLocaleVariant(locale);
		return;
	}
	size_t sep2 = locale.find_first_of("_-.@", ++sep);
	if (sep2 == locale.npos) {
		country = locale.substr(sep);
		return;
	}
	country = locale.substr(sep, sep2 - sep);
	if (locale[sep2] == '.') {
		variant = getUnixLocaleVariant(locale);
		return;
	}
	bool lastIsVariant = locale[sep2] == '@';
	sep = ++sep2;
	sep2 = locale.find_first_of("_-.@", sep);
	if (sep2 == locale.npos)
		variant = locale.substr(sep);
	else
		variant = locale.substr(sep, sep2 - sep);
	if (!lastIsVariant)
		std::swap(country, variant);
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
	Log::set_log_info_callback([](const std::string& msg)
	{
		static std::unordered_set<std::string> msgFilter;
		if (!msg.empty() && msgFilter.count(msg) == 0)
		{
			INFO_LOG(COMMON, "%s", msg.substr(0, msg.length() - 1).c_str());
			msgFilter.insert(msg);
		}
	});
	Log::set_log_warning_callback([](const std::string& msg) {
		if (!msg.empty())
			WARN_LOG(COMMON, "%s", msg.substr(0, msg.length() - 1).c_str());
	});
	Log::set_log_error_callback([](const std::string& msg) {
		if (!msg.empty())
			ERROR_LOG(COMMON, "%s", msg.substr(0, msg.length() - 1).c_str());
	});

	std::string locale = getCurrentLocale();
	std::string language, country, variant;
	parseLocale(locale, language, country, variant);

	static DictionaryManager dictMgr(std::make_unique<ResourceFileSystem>());
	dictMgr.set_language(Language::from_spec(language, country, variant));
	dictMgr.add_directory("i18n");
	dictionary = &dictMgr.get_dictionary();
}

static const std::string& translate(const std::string& msg) {
	init();
	return dictionary->translate(msg);
}

std::string Ts(const std::string& msg) {
	return translate(msg);
}

const char *T(const char *msg)
{
	if (msg == nullptr)
		return nullptr;
	std::string smsg(msg);
	const std::string& tr = translate(smsg);
	if (&tr == &smsg)
		return msg;
	else
		return tr.c_str();
}

std::string translateCtx_s(const std::string& context, const std::string& msg) {
	return dictionary->translate_ctxt(context, msg);
}

const char *translateCtx(const std::string& context, const char *msg)
{
	if (msg == nullptr)
		return nullptr;
	std::string smsg(msg);
	const std::string& tr = dictionary->translate_ctxt(context, smsg);
	if (&tr == &smsg)
		return msg;
	else
		return tr.c_str();
}

std::string translatePlural_s(const std::string& msg, const std::string& msgPlural, int num) {
	return dictionary->translate_plural(msg, msgPlural, num);
}

const char *translatePlural(const char *msg, const char *msgPlural, int num) {
	if (msg == nullptr)
		return nullptr;
	std::string smsg(msg);
	std::string smsgPlural(msgPlural);
	const std::string& tr = dictionary->translate_plural(smsg, smsgPlural, num);
	if (&tr == &smsg)
		return msg;
	else if (&tr == &smsgPlural)
		return msgPlural;
	else
		return tr.c_str();
}

#if !defined(LIBRETRO) && (defined(TARGET_MAC) || defined(_WIN32))
std::string getCurrentLocale()
{
#if defined(TARGET_MAC)
	return os_Locale();
#elif defined(_WIN32)
	ULONG numLanguages = 0;
	ULONG bufferSize = 0;
	if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLanguages, nullptr, &bufferSize) || bufferSize == 0)
	{
		ERROR_LOG(COMMON, "GetUserPreferredUILanguages(size) failed: %lx", GetLastError());
		return "en";
	}

	std::vector<wchar_t> buffer(bufferSize);
	if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLanguages, buffer.data(), &bufferSize))
	{
		ERROR_LOG(COMMON, "GetUserPreferredUILanguages(data) failed: %lx", GetLastError());
		return "en";
	}

	nowide::stackstring name;
	if (name.convert(buffer.data()))
		return name.get();

	ERROR_LOG(COMMON, "UTF-16 to UTF-8 conversion failed");
	return "en";
#endif
}
#endif
}
