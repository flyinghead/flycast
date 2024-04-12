#include <csignal>
#include <unistd.h>
#include "types.h"

#ifndef __ANDROID__
[[noreturn]] void os_DebugBreak()
{
	std::abort();
}

#ifdef _WIN32
HWND getNativeHwnd()
{
	return (HWND)NULL;
}
#endif

void os_DoEvents()
{
}

void os_RunInstance(int argc, const char *argv[])
{
}
#endif
