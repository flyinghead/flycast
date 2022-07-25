#include "cfg.h"
#include "ini.h"
#include "stdclass.h"

#include <cerrno>

static std::string cfgPath;
static bool save_config = true;
static bool autoSave = true;

static emucfg::ConfigFile cfgdb;

static void saveConfigFile()
{
	FILE* cfgfile = nowide::fopen(cfgPath.c_str(), "wt");
	if (!cfgfile)
	{
		WARN_LOG(COMMON, "Error: Unable to open file '%s' for saving", cfgPath.c_str());
	}
	else
	{
		cfgdb.save(cfgfile);
		std::fclose(cfgfile);
	}
}
void cfgSaveStr(const std::string& section, const std::string& key, const std::string& value)
{
	cfgdb.set(section, key, value);

	if (save_config && autoSave)
		saveConfigFile();
}

bool cfgOpen()
{
	if (get_writable_config_path("").empty())
		// Config dir not set (android onboarding)
		return false;

	const char* filename = "emu.cfg";
	std::string config_path_read = get_readonly_config_path(filename);
	cfgPath = get_writable_config_path(filename);

	FILE* cfgfile = nowide::fopen(config_path_read.c_str(), "r");
	if(cfgfile != NULL) {
		cfgdb.parse(cfgfile);
		std::fclose(cfgfile);
	}
	else
	{
		// Config file can't be opened
		int error_code = errno;
		WARN_LOG(COMMON, "Warning: Unable to open the config file '%s' for reading (%s)", config_path_read.c_str(), strerror(error_code));
		if (error_code == ENOENT || cfgPath != config_path_read)
		{
			// Config file didn't exist
			INFO_LOG(COMMON, "Creating new empty config file at '%s'", cfgPath.c_str());
			saveConfigFile();
		}
		else
		{
			// There was some other error (may be a permissions problem or something like that)
			save_config = false;
		}
	}

	return true;
}

std::string cfgLoadStr(const std::string& section, const std::string& key, const std::string& def)
{
	return cfgdb.get(section, key, def);
}

void  cfgSaveInt(const std::string& section, const std::string& key, s32 value)
{
	cfgSaveStr(section, key, std::to_string(value));
}

s32 cfgLoadInt(const std::string& section, const std::string& key, s32 def)
{
	return cfgdb.get_int(section, key, def);
}

int64_t cfgLoadInt64(const std::string& section, const std::string& key, int64_t def) {
	return cfgdb.get_int64(section, key, def);
}
void cfgSaveInt64(const std::string& section, const std::string& key, int64_t value) {
	cfgdb.set_int64(section, key, value);
}

void  cfgSaveBool(const std::string& section, const std::string& key, bool value)
{
	cfgSaveStr(section, key, value ? "yes" : "no");
}

bool  cfgLoadBool(const std::string& section, const std::string& key, bool def)
{
	return cfgdb.get_bool(section, key, def);
}

void cfgSetVirtual(const std::string& section, const std::string& key, const std::string& value)
{
	cfgdb.set(section, key, value, true);
}

bool cfgIsVirtual(const std::string& section, const std::string& key)
{
	return cfgdb.is_virtual(section, key);
}

bool cfgHasSection(const std::string& section)
{
	return cfgdb.has_section(section);
}

void cfgDeleteSection(const std::string& section)
{
	cfgdb.delete_section(section);
}

void cfgDeleteEntry(const std::string& section, const std::string& key)
{
	cfgdb.delete_entry(section, key);
}

void cfgSetAutoSave(bool autoSave)
{
	::autoSave = autoSave;
	if (autoSave)
		saveConfigFile();
}
