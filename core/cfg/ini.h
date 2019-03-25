#pragma once
#include "types.h"
#include <map>

namespace emucfg {

struct ConfigEntry {
	std::string value;
	std::string get_string();
	int get_int();
	bool get_bool();
};

struct ConfigSection {
	std::map<std::string, ConfigEntry> entries;
	bool has_entry(const std::string& name);
	void set(const std::string& name, const std::string& value);
	void delete_entry(const std::string& name);
	ConfigEntry* get_entry(const std::string& name);
};

struct ConfigFile {
	private:
		std::map<std::string, ConfigSection> sections;
		std::map<std::string, ConfigSection> virtual_sections;
		ConfigSection* add_section(const std::string& name, bool is_virtual);
		ConfigSection* get_section(const std::string& name, bool is_virtual);
		ConfigEntry* get_entry(const std::string& section_name, const std::string& entry_name);


	public:
		bool has_section(const std::string& name);
		bool has_entry(const std::string& section_name, const std::string& entry_name);

		void parse(FILE* fd);
		void save(FILE* fd);

		/* getting values */
		string get(const std::string& section_name, const std::string& entry_name, const std::string& default_value = "");
		int get_int(const std::string& section_name, const std::string& entry_name, int default_value = 0);
		bool get_bool(const std::string& section_name, const std::string& entry_name, bool default_value = false);
		/* setting values */
		void set(const std::string& section_name, const std::string& entry_name, const std::string& value, bool is_virtual = false);
		void set_int(const std::string& section_name, const std::string& entry_name, int value, bool is_virtual = false);
		void set_bool(const std::string& section_name, const std::string& entry_name, bool value, bool is_virtual = false);

		void delete_section(const std::string& section_name);
		void delete_entry(const std::string& section_name, const std::string& entry_name);
};

} // namespace emucfg

