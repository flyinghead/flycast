#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && !TARGET_OS_IPHONE
// The macOS app's main() and the os_* glue live in shell/apple/emulator-osx,
// which the test build leaves out (CMakeLists.txt). Provide what the core
// links against.
#include <cstdarg>
#include <cstdio>
#include <string>

int darw_printf(const char *text, ...)
{
	va_list args;
	va_start(args, text);
	int n = vprintf(text, args);
	va_end(args);
	return n;
}
void os_DoEvents()
{
}
void os_RunInstance(int argc, const char *argv[])
{
}
std::string os_PrecomposedString(std::string string)
{
	return string;
}
namespace hostfs {
std::string getScreenshotsPath()
{
	return "";
}
}
namespace i18n {
std::string getSystemLocale()
{
	return "en_US";
}
}
#endif

#if !defined(__ANDROID__) && !defined(__APPLE__)
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
const char *getThreadName()
{
	return "threadname";
}
#endif
#endif
