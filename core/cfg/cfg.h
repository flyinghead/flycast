#pragma once

#include "types.h"

/*
**	cfg* prototypes, if you pass NULL to a cfgSave* it will wipe out the section
**	} if you pass it to lpKey it will wipe out that particular entry
**	} if you add write to something it will create it if its not present
**	} ** Strings passed to LoadStr should be MAX_PATH in size ! **
*/

bool cfgOpen();
s32   cfgLoadInt(const wchar * lpSection, const wchar * lpKey,s32 Default);
void  cfgSaveInt(const wchar * lpSection, const wchar * lpKey, s32 Int);
void  cfgLoadStr(const wchar * lpSection, const wchar * lpKey, wchar * lpReturn,const wchar* lpDefault);
string  cfgLoadStr(const wchar * Section, const wchar * Key, const wchar* Default);
void  cfgSaveStr(const wchar * lpSection, const wchar * lpKey, const wchar * lpString);
s32  cfgExists(const wchar * Section, const wchar * Key);
void cfgSetVirtual(const wchar * lpSection, const wchar * lpKey, const wchar * lpString);

bool ParseCommandLine(int argc,wchar* argv[]);

struct ConfigEntry
{
	u32 flags;
	string name;
	string value;
	string valueVirtual;
	ConfigEntry* next;
	ConfigEntry(ConfigEntry*);
	string GetValue();
	void SaveFile(FILE*);
};

struct ConfigSection
{
	u32 flags;
	string name;
	ConfigEntry* entrys;
	ConfigSection* next;
	~ConfigSection();
	ConfigSection(ConfigSection*);
	ConfigEntry* FindEntry(string);
	void SetEntry(string, string, u32);
	void SaveFile(FILE*);
};

struct ConfigFile
{
	ConfigSection* entrys;
	~ConfigFile();
	void ParseFile(FILE*);
	void SaveFile(FILE*);
	ConfigSection* FindSection(string);
	ConfigSection* GetEntry(string);
	s32 Exists(const wchar *, const wchar *);
	void  LoadStr(const wchar *, const wchar *, wchar *,const wchar*);
	string  LoadStr(const wchar *, const wchar *, const wchar*);
	s32  LoadInt(const wchar *, const wchar *,s32);
};