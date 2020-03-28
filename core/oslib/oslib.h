#pragma once
#include "types.h"

#include <algorithm>
#include <cctype>

void os_SetWindowText(const char* text);
double os_GetSeconds();

void os_DoEvents();
void os_CreateWindow();
void os_SetupInput();
void WriteSample(s16 right, s16 left);

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

void os_DebugBreak();

static inline std::string get_file_extension(const std::string& s)
{
	size_t dot = s.find_last_of('.');
	if (dot >= s.length())
		return "";
	std::string ext = s.substr(dot + 1, s.length() - dot - 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });
	return ext;
}
