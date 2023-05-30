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

void os_SetupInput()
{
}
void os_TermInput()
{
}

void UpdateInputState()
{
}

void os_DoEvents()
{
}

void os_CreateWindow()
{
}

void os_RunInstance(int argc, const char *argv[])
{
}
#endif

#ifdef _WIN32
#include <windows.h>

static LARGE_INTEGER qpf;
static double  qpfd;
//Helper functions
double os_GetSeconds()
{
	static bool initme = (QueryPerformanceFrequency(&qpf), qpfd=1/(double)qpf.QuadPart);
	LARGE_INTEGER time_now;

	QueryPerformanceCounter(&time_now);
	static LARGE_INTEGER time_now_base = time_now;
	return (time_now.QuadPart - time_now_base.QuadPart)*qpfd;
}
#endif
