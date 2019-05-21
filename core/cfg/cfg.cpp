/*
	Config file crap
	Supports various things, as virtual config entries and such crap
	Works surprisingly well considering how old it is ...
*/

#define _CRT_SECURE_NO_DEPRECATE (1)
#include <errno.h>
#include "cfg.h"
#include "ini.h"

static string cfgPath;
static bool save_config = true;

static emucfg::ConfigFile cfgdb;
static string game_id;
static bool has_game_specific_config = false;

void savecfgf()
{
	FILE* cfgfile = fopen(cfgPath.c_str(),"wt");
	if (!cfgfile)
	{
		printf("Error: Unable to open file '%s' for saving\n", cfgPath.c_str());
	}
	else
	{
		cfgdb.save(cfgfile);
		fclose(cfgfile);
	}
}
void  cfgSaveStr(const wchar * Section, const wchar * Key, const wchar * String)
{
	const std::string section(Section);
	const std::string key(Key);
	const std::string value(String);
	if (cfgHasGameSpecificConfig())
	{
		if (cfgdb.get(section, key, "") == value)
			// Same value as main config: delete entry
			cfgdb.delete_entry(game_id, key);
		else
			cfgdb.set(game_id, key, value);
	}
	else
		cfgdb.set(section, key, value);
	if(save_config)
	{
		savecfgf();
	}
}
//New config code

/*
	I want config to be really flexible .. so , here is the new implementation :

	Functions :
	cfgLoadInt  : Load an int , if it does not exist save the default value to it and return it
	cfgSaveInt  : Save an int
	cfgLoadStr  : Load a str , if it does not exist save the default value to it and return it
	cfgSaveStr  : Save a str
	cfgExists   : Returns true if the Section:Key exists. If Key is null , it retuns true if Section exists

	Config parameters can be read from the config file , and can be given at the command line
	-cfg section:key=value -> defines a value at command line
	If a cfgSave* is made on a value defined by command line , then the command line value is replaced by it

	cfg values set by command line are not written to the cfg file , unless a cfgSave* is used
*/

///////////////////////////////
/*
**	This will verify there is a working file @ ./szIniFn
**	- if not present, it will write defaults
*/

bool cfgOpen()
{
	if (get_writable_config_path("").empty())
		// Config dir not set (android onboarding)
		return false;

	const char* filename = "/emu.cfg";
	string config_path_read = get_readonly_config_path(filename);
	cfgPath = get_writable_config_path(filename);

	FILE* cfgfile = fopen(config_path_read.c_str(),"r");
	if(cfgfile != NULL) {
		cfgdb.parse(cfgfile);
		fclose(cfgfile);
	}
	else
	{
		// Config file can't be opened
		int error_code = errno;
		printf("Warning: Unable to open the config file '%s' for reading (%s)\n", config_path_read.c_str(), strerror(error_code));
		if (error_code == ENOENT || cfgPath != config_path_read)
		{
			// Config file didn't exist
			printf("Creating new empty config file at '%s'\n", cfgPath.c_str());
			savecfgf();
		}
		else
		{
			// There was some other error (may be a permissions problem or something like that)
			save_config = false;
		}
	}

	return true;
}

//Implementations of the interface :)
//Section must be set
//If key is 0 , it looks for the section
//0 : not found
//1 : found section , key was 0
//2 : found section & key
s32  cfgExists(const wchar * Section, const wchar * Key)
{
	if(cfgdb.has_entry(string(Section), string(Key)))
	{
		return 2;
	}
	else
	{
		return (cfgdb.has_section(string(Section)) ? 1 : 0);
	}
}

void  cfgLoadStr(const wchar * Section, const wchar * Key, wchar * Return,const wchar* Default)
{
	string value = cfgdb.get(Section, Key, Default);
	// FIXME: Buffer overflow possible
	strcpy(Return, value.c_str());
}

string  cfgLoadStr(const wchar * Section, const wchar * Key, const wchar* Default)
{
	std::string v = cfgdb.get(string(Section), string(Key), string(Default));
	if (cfgHasGameSpecificConfig())
		v = cfgdb.get(game_id, string(Key), v);

	return v;
}

//These are helpers , mainly :)
void  cfgSaveInt(const wchar * Section, const wchar * Key, s32 Int)
{
	char str[32];
	sprintf(str, "%d", Int);
	cfgSaveStr(Section, Key, str);
}

s32 cfgLoadInt(const wchar * Section, const wchar * Key,s32 Default)
{
	s32 v = cfgdb.get_int(string(Section), string(Key), Default);
	if (cfgHasGameSpecificConfig())
		v = cfgdb.get_int(game_id, string(Key), v);

    return v;
}

void  cfgSaveBool(const wchar * Section, const wchar * Key, bool BoolValue)
{
	cfgSaveStr(Section, Key, BoolValue ? "yes" : "no");
}

bool  cfgLoadBool(const wchar * Section, const wchar * Key,bool Default)
{
	bool v = cfgdb.get_bool(string(Section), string(Key), Default);
	if (cfgHasGameSpecificConfig())
		v = cfgdb.get_bool(game_id, string(Key), v);

    return v;
}

void cfgSetVirtual(const wchar * Section, const wchar * Key, const wchar * String)
{
	cfgdb.set(string(Section), string(Key), string(String), true);
}

void cfgSetGameId(const char *id)
{
	game_id = id;
}

const char *cfgGetGameId()
{
	return game_id.c_str();
}

bool cfgHasGameSpecificConfig()
{
	return has_game_specific_config || cfgdb.has_section(game_id);
}

void cfgMakeGameSpecificConfig()
{
	has_game_specific_config = true;
}

void cfgDeleteGameSpecificConfig()
{
	has_game_specific_config = false;
	cfgdb.delete_section(game_id);
}
