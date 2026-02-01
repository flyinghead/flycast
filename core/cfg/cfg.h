#pragma once
#include "types.h"

namespace config
{

bool open();
int loadInt(const std::string& section, const std::string& key, int def = 0);
void saveInt(const std::string& section, const std::string& key, int value);
int64_t loadInt64(const std::string& section, const std::string& key, int64_t def = 0);
void saveInt64(const std::string& section, const std::string& key, int64_t value);
std::string loadStr(const std::string& section, const std::string& key, const std::string& def = {});
void saveStr(const std::string& section, const std::string& key, const std::string& value);
bool loadBool(const std::string& section, const std::string& key, bool def = false);
void saveBool(const std::string& section, const std::string& key, bool value);
float loadFloat(const std::string& section, const std::string& key, float def = 0.f);
void saveFloat(const std::string& section, const std::string& key, float value);
void setTransient(const std::string& section, const std::string& key, const std::string& value);
bool isTransient(const std::string& section, const std::string& key);

void parseCommandLine(int argc, const char * const argv[]);

void setAutoSave(bool autoSave);
bool hasSection(const std::string& section);
void deleteSection(const std::string& section);
void deleteEntry(const std::string& section, const std::string& key);

}
