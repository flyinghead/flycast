#ifndef __ANDROID__
#include <cstdlib>

[[noreturn]] void os_DebugBreak()
{
	std::abort();
}

void os_DoEvents()
{
}

void os_RunInstance(int argc, const char *argv[])
{
}

#ifdef _WIN32
void os_SetThreadName(const char *name)
{
}
#endif
#endif
