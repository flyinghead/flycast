#pragma once

#include "types.h"
/*
**	cfg* prototypes, if you pass NULL to a cfgSave* it will wipe out the section
**	} if you pass it to lpKey it will wipe out that particular entry
**	} if you add write to something it will create it if its not present
**	} ** Strings passed to LoadStr should be MAX_PATH in size ! **
*/

bool cfgOpen();
s32   cfgLoadInt(const char * lpSection, const char * lpKey,s32 Default);
void  cfgSaveInt(const char * lpSection, const char * lpKey, s32 Int);
void  cfgLoadStr(const char * lpSection, const char * lpKey, char * lpReturn,const char* lpDefault);
std::string  cfgLoadStr(const char * Section, const char * Key, const char* Default);
void  cfgSaveStr(const char * lpSection, const char * lpKey, const char * lpString);
void  cfgSaveBool(const char * Section, const char * Key, bool BoolValue);
bool  cfgLoadBool(const char * Section, const char * Key,bool Default);
s32  cfgExists(const char * Section, const char * Key);
void cfgSetVirtual(const char * lpSection, const char * lpKey, const char * lpString);

bool ParseCommandLine(int argc,char* argv[]);

void cfgSetGameId(const char *id);
const char *cfgGetGameId();
bool cfgHasGameSpecificConfig();
void cfgMakeGameSpecificConfig();
void cfgDeleteGameSpecificConfig();
void cfgSetAutoSave(bool autoSave);
