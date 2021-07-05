#pragma once
#include "types.h"

void os_SetWindowText(const char* text);
double os_GetSeconds();

void os_DoEvents();
void os_CreateWindow();
void os_SetupInput();

#ifdef _MSC_VER
#include <intrin.h>
#endif

u32 static INLINE bitscanrev(u32 v)
{
#ifdef __GNUC__
	return 31-__builtin_clz(v);
#else
	unsigned long rv;
	_BitScanReverse(&rv,v);
	return rv;
#endif
}

namespace hostfs
{
	std::string getVmuPath(const std::string& port);
	std::string getJvsEepromPath();

	std::string findFlash(const std::string& prefix, const std::string& names);
	std::string getFlashSavePath(const std::string& prefix, const std::string& name);
	std::string findNaomiBios(const std::string& name);

	std::string getSavestatePath(int index, bool writable);

	std::string getTextureLoadPath(const std::string& gameId);
	std::string getTextureDumpPath();

	std::string getVulkanCachePath();

	std::string getBiosFontPath();
}
