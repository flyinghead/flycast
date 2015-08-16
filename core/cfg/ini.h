#pragma once
#include "types.h"

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