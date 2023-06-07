#pragma once

#include "types.h"

bool cfgOpen();
s32 cfgLoadInt(const std::string& section, const std::string& key, s32 def);
void cfgSaveInt(const std::string& section, const std::string& key, s32 value);
int64_t cfgLoadInt64(const std::string& section, const std::string& key, int64_t def);
void cfgSaveInt64(const std::string& section, const std::string& key, int64_t value);
std::string cfgLoadStr(const std::string& section, const std::string& key, const std::string& def);
void cfgSaveStr(const std::string& section, const std::string& key, const std::string& value);
void cfgSaveBool(const std::string& section, const std::string& key, bool value);
bool cfgLoadBool(const std::string& section, const std::string& key, bool def);
void cfgSetVirtual(const std::string& section, const std::string& key, const std::string& value);
bool cfgIsVirtual(const std::string& section, const std::string& key);

void ParseCommandLine(int argc, char *argv[]);

void cfgSetAutoSave(bool autoSave);
bool cfgHasSection(const std::string& section);
void cfgDeleteSection(const std::string& section);
void cfgDeleteEntry(const std::string& section, const std::string& key);
