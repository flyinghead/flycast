#include "ini.h"
#include "types.h"
#include <sstream>
#include "stdclass.h"

namespace emucfg {

/* ConfigEntry */

const std::string& ConfigEntry::get_string() const
{
	return this->value;
}

int ConfigEntry::get_int()
{
	if (strstr(this->value.c_str(), "0x") != NULL)
	{
		return (int)strtoul(this->value.c_str(), NULL, 16);
	}
	else
	{
		return atoi(this->value.c_str());
	}
}

bool ConfigEntry::get_bool()
{
	if (stricmp(this->value.c_str(), "yes") == 0 ||
		  stricmp(this->value.c_str(), "true") == 0 ||
		  stricmp(this->value.c_str(), "on") == 0 ||
		  stricmp(this->value.c_str(), "1") == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

/* ConfigSection */

bool ConfigSection::has_entry(const std::string& name)
{
	return (this->entries.count(name) == 1);
}

ConfigEntry* ConfigSection::get_entry(const std::string& name)
{
	if(this->has_entry(name))
	{
		return &this->entries[name];
	}
	return NULL;
}

void ConfigSection::set(const std::string& name, const std::string& value)
{
	ConfigEntry new_entry = { value };
	this->entries[name] = new_entry;
}

void ConfigSection::delete_entry(const std::string& name)
{
	this->entries.erase(name);
}

/* ConfigFile */

ConfigSection* ConfigFile::add_section(const std::string& name, bool is_virtual)
{
	ConfigSection new_section;
	if (is_virtual)
	{
		this->virtual_sections.insert(std::make_pair(name, new_section));
		return &this->virtual_sections[name];
	}
	this->sections.insert(std::make_pair(name, new_section));
	return &this->sections[name];
}

bool ConfigFile::has_section(const std::string& name)
{
	return (this->virtual_sections.count(name) == 1 || this->sections.count(name) == 1);
}

bool ConfigFile::has_entry(const std::string& section_name, const std::string& entry_name)
{
	ConfigSection* section = this->get_section(section_name, true);
	if ((section != NULL) && section->has_entry(entry_name))
	{
		return true;
	}
	section = this->get_section(section_name, false);
	return ((section != NULL) && section->has_entry(entry_name));
}

ConfigSection* ConfigFile::get_section(const std::string& name, bool is_virtual)
{
	if(is_virtual)
	{
		if (this->virtual_sections.count(name) == 1)
		{
			return &this->virtual_sections[name];
		}
	}
	else
	{
		if (this->sections.count(name) == 1)
		{
			return &this->sections[name];
		}
	}
	return NULL;
}

ConfigEntry* ConfigFile::get_entry(const std::string& section_name, const std::string& entry_name)
{
	ConfigSection* section = this->get_section(section_name, true);
	if(section != NULL && section->has_entry(entry_name))
	{
		return section->get_entry(entry_name);
	}

		section = this->get_section(section_name, false);
	if(section != NULL)
	{
		return section->get_entry(entry_name);
	}
	return NULL;

}

std::string ConfigFile::get(const std::string& section_name, const std::string& entry_name, const std::string& default_value)
{
	ConfigEntry* entry = this->get_entry(section_name, entry_name);
	if (entry == NULL)
	{
		return default_value;
	}
	else
	{
		return entry->get_string();
	}
}

int ConfigFile::get_int(const std::string& section_name, const std::string& entry_name, int default_value)
{
	ConfigEntry* entry = this->get_entry(section_name, entry_name);
	if (entry == NULL)
	{
		return default_value;
	}
	else
	{
		return entry->get_int();
	}
}

bool ConfigFile::get_bool(const std::string& section_name, const std::string& entry_name, bool default_value)
{
	ConfigEntry* entry = this->get_entry(section_name, entry_name);
	if (entry == NULL)
	{
		return default_value;
	}
	else
	{
		return entry->get_bool();
	}
}

void ConfigFile::set(const std::string& section_name, const std::string& entry_name, const std::string& value, bool is_virtual)
{
	ConfigSection* section = this->get_section(section_name, is_virtual);
	if(section == NULL)
	{
		section = this->add_section(section_name, is_virtual);
	}
	section->set(entry_name, value);
}

void ConfigFile::set_int(const std::string& section_name, const std::string& entry_name, int value, bool is_virtual)
{
	std::stringstream str_value;
	str_value << value;
	this->set(section_name, entry_name, str_value.str(), is_virtual);
}

void ConfigFile::set_bool(const std::string& section_name, const std::string& entry_name, bool value, bool is_virtual)
{
	std::string str_value = (value ? "yes" : "no");
	this->set(section_name, entry_name, str_value, is_virtual);
}

void ConfigFile::parse(FILE* file)
{
	if (file == nullptr)
		return;

	char line[512];
	std::string section;
	int cline = 0;
	while (true)
	{
		if (std::fgets(line, sizeof(line), file) == nullptr)
			break;
		cline++;
		std::string s(line);
		s = trim_ws(s, " \r\n");
		if (s.empty())
			continue;
		if (s.length() >= 3 && s[0] == '[' && s[s.length() - 1] == ']')
		{
			section = s.substr(1, s.length() - 2);
			continue;
		}
		auto eqpos = s.find('=');
		if (eqpos == std::string::npos)
		{
			WARN_LOG(COMMON, "Malformed entry on config - ignoring line %d: %s", cline, s.c_str());
			continue;
		}
		std::string property = trim_ws(s.substr(0, eqpos));
		std::string value = trim_ws(s.substr(eqpos + 1));
		if (value.length() >= 2 && value[0] == '"' && value[value.length() - 1] == '"')
			value = value.substr(1, value.length() - 2);
		set(section, property, value);
	}
}

void ConfigFile::save(FILE* file)
{
	for (const auto& section_it : this->sections)
	{
		const std::string& section_name = section_it.first;
		const ConfigSection& section = section_it.second;

		if (!section_name.empty())
			std::fprintf(file, "[%s]\n", section_name.c_str());

		for (const auto& entry_it : section.entries)
		{
			const std::string& entry_name = entry_it.first;
			const ConfigEntry& entry = entry_it.second;
			std::fprintf(file, "%s = %s\n", entry_name.c_str(), entry.get_string().c_str());
		}
		if (!section_name.empty())
			std::fputc('\n', file);
	}
}

void ConfigFile::delete_section(const std::string& section_name) {
	sections.erase(section_name);
	virtual_sections.erase(section_name);
}

void ConfigFile::delete_entry(const std::string& section_name, const std::string& entry_name)
{
	ConfigSection *section = get_section(section_name, false);
	if (section != NULL)
		section->delete_entry(entry_name);
}

bool ConfigFile::is_virtual(const std::string& section_name, const std::string& entry_name)
{
	ConfigSection *section = get_section(section_name, true);
	if (section == nullptr)
		return false;
	return section->has_entry(entry_name);
}

} // namespace emucfg

