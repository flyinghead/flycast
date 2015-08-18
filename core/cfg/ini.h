#pragma once
#include "types.h"
#include <map>

//A config remains virtual only as long as a write at it
//doesn't override the virtual value.While a config is virtual, a copy of its 'real' value is held and preserved

//Is this a virtual entry ?
#define CEM_VIRTUAL 1
//Should the value be saved ?
#define CEM_SAVE  2
//is this entry readonly ?
#define CEM_READONLY 4
//the move is from loading ?
#define CEM_LOAD 8

struct ConfigEntry {
	string value;
	int flags; //TODO: These have no effect right now
	string get_string();
	int get_int();
	bool get_bool();
};

struct ConfigSection {
	std::map<string, ConfigEntry> entries;
	bool has_entry(string name);
	void set(string name, string value, int flags);
	ConfigEntry* get_entry(string name);
};

struct ConfigFile {
	private:
		std::map<string, ConfigSection> sections;
		ConfigSection* add_section(string name);
		ConfigSection* get_section(string name);
		ConfigEntry* get_entry(string section_name, string entry_name);


	public:
		bool has_section(string name);
		bool has_entry(string section_name, string entry_name);

		void parse(FILE* fd);
		void save(FILE* fd);

		/* getting values */
		string get(string section_name, string entry_name, string default_value = "");
		int get_int(string section_name, string entry_name, int default_value = 0);
		bool get_bool(string section_name, string entry_name, bool default_value = false);
		/* setting values */
		void set(string section_name, string entry_name, string value, int flags = 0);
		void set_int(string section_name, string entry_name, int value, int flags = 0);
		void set_bool(string section_name, string entry_name, bool value, int flags = 0);
};
