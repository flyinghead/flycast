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
#include <cstdio>
#include <map>
#include <string>
#include <cstdint>
#include <vector>
#include <locale>
#include <sstream>

namespace config {

class IniFile
{
public:
	void load(FILE *file, bool cEscape = false);
	void load(const std::string& data, bool cEscape = false);
	void save(FILE *file) const;
	void save(std::string& data) const;
	bool hasSection(const std::string& section) const;
	bool hasEntry(const std::string& section, const std::string& name) const;
	bool isTransient(const std::string& section, const std::string& name) const;
	void deleteSection(const std::string& section);
	void deleteEntry(const std::string& section, const std::string& name);

	template<typename T>
	void set(const std::string& section, const std::string& entry, T value, bool transient = false) {
		setRaw(section, entry, std::to_string(value), transient);
	}

	const std::string& getString(const std::string& section, const std::string& entry, const std::string& defaultValue = {}) const;
	const std::string& get(const std::string& section, const std::string& entry, const std::string& defaultValue = {}) const {
		return getString(section, entry, defaultValue);
	}
	bool getBool(const std::string& section, const std::string& entry, bool defaultValue = false) const;
	int getInt(const std::string& section, const std::string& entry, int defaultValue = 0) const;
	int64_t getInt64(const std::string& section, const std::string& entry, int64_t defaultValue = 0ll) const;
	float getFloat(const std::string& section, const std::string& entry, float defaultValue = 0.f) const;

	std::vector<std::string> getEntryNames(const std::string& section) const;

private:
	struct Entry
	{
		std::string name;
		std::string value;
		std::string transientValue;
		bool transient = false;
	};
	struct Section
	{
		std::string name;
		std::map<std::string, Entry> entries;
	};
	const Entry *getEntry(const std::string& section, const std::string& entry) const;
	const std::string *getRaw(const std::string& section, const std::string& name) const;
	void setRaw(const std::string& section, const std::string& name, const std::string& value, bool transient);

	std::map<std::string, Section> sections;
};

template<>
inline void IniFile::set(const std::string& section, const std::string& entry, const std::string& value, bool transient) {
	setRaw(section, entry, value, transient);
}
template<>
inline void IniFile::set(const std::string& section, const std::string& entry, std::string value, bool transient) {
	setRaw(section, entry, value, transient);
}
template<>
inline void IniFile::set(const std::string& section, const std::string& entry, const char *value, bool transient) {
	setRaw(section, entry, value, transient);
}
template<>
inline void IniFile::set(const std::string& section, const std::string& entry, bool value, bool transient) {
	setRaw(section, entry, value ? "yes" : "no", transient);
}
template<>
inline void IniFile::set(const std::string& section, const std::string& entry, float value, bool transient)
{
	std::ostringstream ss;
	ss.imbue(std::locale::classic());
	ss.precision(7);
	ss << value;
	setRaw(section, entry, ss.str(), transient);
}

} // namespace config
