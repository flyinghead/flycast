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
#include "ini.h"
#include "types.h"
#include "stdclass.h"

namespace config {

const IniFile::Entry *IniFile::getEntry(const std::string& sectionName, const std::string& name) const
{
	auto it = sections.find(sectionName);
	if (it == sections.end())
		return nullptr;
	const Section& section = it->second;
	auto it2 = section.entries.find(name);
	if (it2 == section.entries.end())
		return nullptr;
	return &it2->second;
}

const std::string *IniFile::getRaw(const std::string& section, const std::string& name) const
{
	const Entry *entry = getEntry(section, name);
	if (entry == nullptr)
		return nullptr;
	if (entry->transient)
		return &entry->transientValue;
	else
		return &entry->value;
}

const std::string& IniFile::getString(const std::string& section, const std::string& entry, const std::string& defaultValue) const
{
	const std::string *pValue = getRaw(section, entry);
	if (pValue == nullptr)
		return defaultValue;
	else
		return *pValue;
}

bool IniFile::getBool(const std::string& section, const std::string& entry, bool defaultValue) const
{
	const std::string *pValue = getRaw(section, entry);
	if (pValue == nullptr)
		return defaultValue;
	if (!stricmp("yes", pValue->c_str())
			|| !stricmp("true", pValue->c_str())
			|| !stricmp("on", pValue->c_str())
			|| !stricmp("1", pValue->c_str()))
		return true;
	else
		return false;
}

static bool hasHexPrefix(const std::string& s)
{
	size_t pos = 0;
	while (pos < s.length() - 1 && std::isspace((int8_t)s[pos]))
		pos++;
	if (pos + 1 >= s.length())
		return false;
	return (s[pos] == '0' && (s[pos + 1] == 'x' || s[pos + 1] == 'X'));
}

int IniFile::getInt(const std::string& section, const std::string& entry, int defaultValue) const
{
	const std::string *pValue = getRaw(section, entry);
	if (pValue == nullptr)
		return defaultValue;
	std::istringstream ss(*pValue);
	ss.imbue(std::locale::classic());
	if (hasHexPrefix(*pValue))
		ss.setf(std::ios_base::hex, std::ios_base::basefield);
	else
		ss.setf(std::ios_base::dec, std::ios_base::basefield);
	unsigned value = defaultValue;
	ss >> value;
	return (int)value;
}

int64_t IniFile::getInt64(const std::string& section, const std::string& entry, int64_t defaultValue) const
{
	const std::string *pValue = getRaw(section, entry);
	if (pValue == nullptr)
		return defaultValue;
	std::istringstream ss(*pValue);
	ss.imbue(std::locale::classic());
	if (hasHexPrefix(*pValue))
		ss.setf(std::ios_base::hex, std::ios_base::basefield);
	else
		ss.setf(std::ios_base::dec, std::ios_base::basefield);
	int64_t value = defaultValue;
	ss >> value;
	return value;
}

float IniFile::getFloat(const std::string& section, const std::string& entry, float defaultValue) const
{
	const std::string *pValue = getRaw(section, entry);
	if (pValue == nullptr || pValue->empty())
		return defaultValue;
	std::istringstream ss(*pValue);
	ss.imbue(std::locale::classic());
	float value = defaultValue;
	ss >> value;
	return value;
}

void IniFile::setRaw(const std::string& sectionName, const std::string& entryName, const std::string& value, bool transient)
{
	Section& section = sections[sectionName];
	Entry& entry = section.entries[entryName];
	if (entry.name.empty())
		entry.name = entryName;
	if (transient) {
		entry.transientValue = value;
		entry.transient = true;
	}
	else {
		entry.value = value;
	}
}

static std::string handleEscapeSeq(const std::string& s)
{
	std::string ret;
	ret.reserve(s.length());
	for (size_t i = 0; i < s.length(); i++)
	{
		char c = s[i];
		if (c == '\\' && i < s.length() - 1)
		{
			switch (s[++i])
			{
			case 'n':
				ret += '\n';
				break;
			case 't':
				ret += '\t';
				break;
			case 'r':
				ret += '\r';
				break;
			case 'f':
				ret += '\f';
				break;
			case '\\':
				ret += '\\';
				break;
			default:
				WARN_LOG(COMMON, "Unrecognized escape sequence [\\%c] in [%s]", s[i], s.c_str());
				ret += '\\';
				--i;
				break;
			}
		}
		else {
			ret += c;
		}
	}
	return ret;
}

void IniFile::load(const std::string& data, bool cEscape)
{
	std::istringstream fss(data);
	int cline = 1;
	std::string curSection;
	for (std::string line; std::getline(fss, line); cline++)
	{
		size_t pos = 0;
		while (pos < line.length() && std::isspace((int8_t)line[pos]))
			pos++;
		if (pos == line.length() || line[pos] == ';')
			// Ignore empty lines and comments
			continue;
		if (line[pos] == '[')
		{
			// Section
			size_t end = line.find(']', ++pos);
			if (end == line.npos) {
				WARN_LOG(COMMON, "Missing ']' character - ignoring line %d: %s", cline, line.c_str());
				continue;
			}
			curSection = line.substr(pos, end - pos);
			if (cEscape)
				curSection = handleEscapeSeq(curSection);
		}
		else
		{
			// Entry
			pos = line.find('=', pos);
			if (pos == line.npos) {
				WARN_LOG(COMMON, "Malformed entry - ignoring line %d: %s", cline, line.c_str());
				continue;
			}
			std::string entry = trim_ws(line.substr(0, pos), " \t");
			if (entry.empty()) {
				WARN_LOG(COMMON, "Invalid empty entry name - ignoring line %d: %s", cline, line.c_str());
				continue;
			}
			std::string value = line.substr(pos + 1);
			// Space characters before and after the equal sign are for readability, and not part of the entry name or value
			if (value[0] == ' ')
				value = value.substr(1);
			if (value[0] == '"')
			{
				// quoted value
				size_t end = value.rfind('"');
				if (end == value.npos || end == 0)
					end = value.length();
				value = value.substr(1, end - 1);
			}
			if (cEscape) {
				entry = handleEscapeSeq(entry);
				value = handleEscapeSeq(value);
			}
			set(curSection, entry, value);
		}
	}
}

void IniFile::load(FILE *file, bool cEscape)
{
	std::string data;
	for (;;)
	{
		char buffer[4096];
		int n = fread(buffer, 1, sizeof(buffer), file);
		if (n <= 0)
			break;
		data += std::string(buffer, buffer + n);
	}
	load(data, cEscape);
}

void IniFile::save(std::string& data) const
{
	std::ostringstream ss;
	for (const auto& [sectionName, section] : sections)
	{
		if (!sectionName.empty())
			ss << '[' << sectionName << ']' << std::endl;
		for (const auto& [name, entry] : section.entries)
			ss << name << " = " << entry.value << std::endl;
		ss << std::endl;
	}
	data = ss.str();
}

void IniFile::save(FILE *file) const
{
	std::string data;
	save(data);
	fwrite(data.c_str(), 1, data.length(), file);
}

bool IniFile::hasSection(const std::string& section) const {
	return sections.count(section) != 0;
}
bool IniFile::hasEntry(const std::string& section, const std::string& name) const {
	return getEntry(section, name) != nullptr;
}
bool IniFile::isTransient(const std::string& section, const std::string& name) const
{
	const Entry *entry = getEntry(section, name);
	if (entry == nullptr)
		return false;
	else
		return entry->transient;
}

void IniFile::deleteSection(const std::string& section) {
	sections.erase(section);
}
void IniFile::deleteEntry(const std::string& sectionName, const std::string& entry)
{
	auto it = sections.find(sectionName);
	if (it == sections.end())
		return;
	Section& section = it->second;
	section.entries.erase(entry);
}

std::vector<std::string> IniFile::getEntryNames(const std::string& sectionName) const
{
	std::vector<std::string> v;
	auto it = sections.find(sectionName);
	if (it != sections.end())
	{
		const Section& section = it->second;
		for (const auto& [name, entry] : section.entries)
			v.push_back(name);
	}
	return v;
}

} // namespace config
