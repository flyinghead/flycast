#include "cfg.h"
#include "ini.h"
#include "stdclass.h"
#include <cerrno>

namespace config
{

static std::string cfgPath;
static bool autoSaving = true;
static IniFile cfgdb;

static void saveFile()
{
	FILE* cfgfile = nowide::fopen(cfgPath.c_str(), "wt");
	if (!cfgfile) {
		WARN_LOG(COMMON, "Error: Unable to open file '%s' for saving", cfgPath.c_str());
	}
	else {
		cfgdb.save(cfgfile);
		std::fclose(cfgfile);
	}
}

static void autoSave() {
	if (autoSaving)
		saveFile();
}

bool open()
{
	if (get_writable_config_path("").empty())
		// Config dir not set (android onboarding)
		return false;

	const char* filename = "emu.cfg";
	std::string config_path_read = get_readonly_config_path(filename);
	cfgPath = get_writable_config_path(filename);

	FILE* cfgfile = nowide::fopen(config_path_read.c_str(), "rt");
	if (cfgfile != nullptr) {
		cfgdb.load(cfgfile);
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
			saveFile();
		}
	}

	return true;
}

void saveStr(const std::string& section, const std::string& key, const std::string& value) {
	cfgdb.set(section, key, value);
	autoSave();
}

std::string loadStr(const std::string& section, const std::string& key, const std::string& def) {
	return cfgdb.getString(section, key, def);
}

void saveInt(const std::string& section, const std::string& key, int value) {
	cfgdb.set(section, key, value);
	autoSave();
}

int loadInt(const std::string& section, const std::string& key, int def) {
	return cfgdb.getInt(section, key, def);
}

int64_t loadInt64(const std::string& section, const std::string& key, int64_t def) {
	return cfgdb.getInt64(section, key, def);
}
void saveInt64(const std::string& section, const std::string& key, int64_t value) {
	cfgdb.set(section, key, value);
	autoSave();
}

void saveBool(const std::string& section, const std::string& key, bool value) {
	cfgdb.set(section, key, value);
	autoSave();
}

bool loadBool(const std::string& section, const std::string& key, bool def) {
	return cfgdb.getBool(section, key, def);
}

void setTransient(const std::string& section, const std::string& key, const std::string& value) {
	cfgdb.set(section, key, value, true);
}

bool isTransient(const std::string& section, const std::string& key) {
	return cfgdb.isTransient(section, key);
}

bool hasSection(const std::string& section) {
	return cfgdb.hasSection(section);
}

void deleteSection(const std::string& section) {
	cfgdb.deleteSection(section);
}

void deleteEntry(const std::string& section, const std::string& key) {
	cfgdb.deleteEntry(section, key);
}

void setAutoSave(bool autoSaving)
{
	config::autoSaving = autoSaving;
	if (autoSaving)
		saveFile();
}

void saveFloat(const std::string& section, const std::string& key, float value) {
	cfgdb.set(section, key, value);
	autoSave();
}
float loadFloat(const std::string& section, const std::string& key, float def) {
	return cfgdb.getFloat(section, key, def);
}

}	// namespace config
