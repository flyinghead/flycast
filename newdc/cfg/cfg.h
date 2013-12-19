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
void  cfgSaveStr(const wchar * lpSection, const wchar * lpKey, const wchar * lpString);
s32  cfgExists(const wchar * Section, const wchar * Key);
void cfgSetVitual(const wchar * lpSection, const wchar * lpKey, const wchar * lpString);

bool ParseCommandLine(int argc,wchar* argv[]);