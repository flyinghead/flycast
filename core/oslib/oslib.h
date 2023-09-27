#pragma once
#include "types.h"
#include <vector>
#if defined(__SWITCH__)
#include <malloc.h>
#endif

void os_SetWindowText(const char* text);
double os_GetSeconds();

void os_DoEvents();
void os_CreateWindow();
void os_SetupInput();
void os_TermInput();
void os_InstallFaultHandler();
void os_UninstallFaultHandler();
void os_RunInstance(int argc, const char *argv[]);

#ifdef _MSC_VER
#include <intrin.h>
#endif

u32 static inline bitscanrev(u32 v)
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

	std::string getArcadeFlashPath();

	std::string findFlash(const std::string& prefix, const std::string& names);
	std::string getFlashSavePath(const std::string& prefix, const std::string& name);
	std::string findNaomiBios(const std::string& name);

	std::string getSavestatePath(int index, bool writable);

	std::string getTextureLoadPath(const std::string& gameId);
	std::string getTextureDumpPath();

	std::string getShaderCachePath(const std::string& filename);
}

static inline void *allocAligned(size_t alignment, size_t size)
{
#ifdef _WIN32
	return _aligned_malloc(size, alignment);
#elif defined(__SWITCH__)
   return memalign(alignment, size);
#else
	void *data;
	if (posix_memalign(&data, alignment, size) != 0)
		return nullptr;
	else
		return data;
#endif
}

static inline void freeAligned(void *p)
{
#ifdef _WIN32
	_aligned_free(p);
#else
	free(p);
#endif
}

void registerCrash(const char *directory, const char *path);
void uploadCrashes(const std::string& directory);
