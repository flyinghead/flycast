#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include "types.h"

#if defined(__unix__) || defined(__SWITCH__)
#include "log/LogManager.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "oslib/directory.h"
#include "oslib/oslib.h"
#include "stdclass.h"

#include <csignal>
#include <string>
#include <unistd.h>
#include <vector>

#if defined(__SWITCH__)
#include "nswitch.h"
#endif

#if defined(SUPPORT_DISPMANX)
	#include "dispmanx.h"
#endif

#if defined(SUPPORT_X11)
	#include "x11.h"
#endif

#if defined(USE_SDL)
	#include "sdl/sdl.h"
#endif

#if defined(USE_EVDEV)
	#include "evdev.h"
#endif

#ifdef USE_BREAKPAD
#include "breakpad/client/linux/handler/exception_handler.h"
#endif

void os_SetupInput()
{
#if defined(USE_EVDEV)
	input_evdev_init();
#endif

#if defined(SUPPORT_X11)
	input_x11_init();
#endif

#if defined(USE_SDL)
	input_sdl_init();
#endif
}

void os_TermInput()
{
#if defined(USE_EVDEV)
	input_evdev_close();
#endif
#if defined(USE_SDL)
	input_sdl_quit();
#endif
}

void UpdateInputState()
{
	#if defined(USE_EVDEV)
		input_evdev_handle();
	#endif

	#if defined(USE_SDL)
		input_sdl_handle();
	#endif
}

void os_DoEvents()
{
	#if defined(SUPPORT_X11)
		input_x11_handle();
		event_x11_handle();
	#endif
}

void os_SetWindowText(const char * text)
{
	#if defined(SUPPORT_X11)
		x11_window_set_text(text);
	#endif
	#if defined(USE_SDL)
		sdl_window_set_text(text);
	#endif
}

void os_CreateWindow()
{
	#if defined(SUPPORT_DISPMANX)
		dispmanx_window_create();
	#endif
	#if defined(SUPPORT_X11)
		x11_window_create();
	#endif
	#if defined(USE_SDL)
		sdl_window_create();
	#endif
}

void common_linux_setup();

// Find the user config directory.
// $HOME/.config/flycast on linux
std::string find_user_config_dir()
{
#ifdef __SWITCH__
	flycast::mkdir("/flycast", 0755);
	return "/flycast/";
#else
	std::string xdg_home;
	if (nowide::getenv("XDG_CONFIG_HOME") != nullptr)
		// If XDG_CONFIG_HOME is set explicitly, we'll use that instead of $HOME/.config
		xdg_home = (std::string)nowide::getenv("XDG_CONFIG_HOME");
	else if (nowide::getenv("HOME") != nullptr)
		/* If $XDG_CONFIG_HOME is not set, we're supposed to use "$HOME/.config" instead.
		 * Consult the XDG Base Directory Specification for details:
		 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
		 */
		xdg_home = (std::string)nowide::getenv("HOME") + "/.config";

	if (!xdg_home.empty())
	{
		std::string fullpath = xdg_home + "/flycast/";
		struct stat info;
		if (flycast::stat(fullpath.c_str(), &info) != 0 || (info.st_mode & S_IFDIR) == 0)
			// Create .config/flycast
			flycast::mkdir(fullpath.c_str(), 0755);

		return fullpath;
	}
	// Unable to detect config dir, use the current folder
	return ".";
#endif
}

// Find the user data directory.
// $HOME/.local/share/flycast on linux
std::string find_user_data_dir()
{
#ifdef __SWITCH__
	flycast::mkdir("/flycast/data", 0755);
	return "/flycast/data/";
#else
	std::string xdg_home;
	if (nowide::getenv("XDG_DATA_HOME") != nullptr)
		// If XDG_DATA_HOME is set explicitly, we'll use that instead of $HOME/.local/share
		xdg_home = (std::string)nowide::getenv("XDG_DATA_HOME");
	else if (nowide::getenv("HOME") != nullptr)
		/* If $XDG_DATA_HOME is not set, we're supposed to use "$HOME/.local/share" instead.
		 * Consult the XDG Base Directory Specification for details:
		 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
		 */
		xdg_home = (std::string)nowide::getenv("HOME") + "/.local/share";

	if (!xdg_home.empty())
	{
		std::string fullpath = xdg_home + "/flycast/";
		struct stat info;
		if (flycast::stat(fullpath.c_str(), &info) != 0 || (info.st_mode & S_IFDIR) == 0)
			// Create .local/share/flycast
			flycast::mkdir(fullpath.c_str(), 0755);

		return fullpath;
	}
	// Unable to detect data dir, use the current folder
	return ".";
#endif
}

#ifndef __SWITCH__
static void addDirectoriesFromPath(std::vector<std::string>& dirs, const std::string& path, const std::string& suffix)
{
	std::string::size_type pos = 0;
	std::string::size_type n = path.find(':', pos);
	while (n != std::string::npos)
	{
		if (n != pos)
			dirs.push_back(path.substr(pos, n - pos) + suffix);
		pos = n + 1;
		n = path.find(':', pos);
	}
	// Separator not found
	if (pos < path.length())
		dirs.push_back(path.substr(pos) + suffix);
}
#endif

// Find a file in the user and system config directories.
// The following folders are checked in this order:
// $HOME/.config/flycast
// if XDG_CONFIG_DIRS is defined:
//   <$XDG_CONFIG_DIRS>/flycast
// else
//   /etc/flycast/
//   /etc/xdg/flycast/
// .
std::vector<std::string> find_system_config_dirs()
{
	std::vector<std::string> dirs;

#ifdef __SWITCH__
	dirs.push_back("/flycast/");
#else
	std::string xdg_home;
	if (nowide::getenv("XDG_CONFIG_HOME") != nullptr)
		// If XDG_CONFIG_HOME is set explicitly, we'll use that instead of $HOME/.config
		xdg_home = (std::string)nowide::getenv("XDG_CONFIG_HOME");
	else if (nowide::getenv("HOME") != nullptr)
		xdg_home = (std::string)nowide::getenv("HOME") + "/.config";
	if (!xdg_home.empty())
		// XDG config locations
		dirs.push_back(xdg_home + "/flycast/");

	if (nowide::getenv("XDG_CONFIG_DIRS") != nullptr)
	{
		std::string path = (std::string)nowide::getenv("XDG_CONFIG_DIRS");
		addDirectoriesFromPath(dirs, path, "/flycast/");
	}
	else
	{
#ifdef FLYCAST_SYSCONFDIR
		const std::string config_dir(FLYCAST_SYSCONFDIR);
		dirs.push_back(config_dir);
#endif
		dirs.push_back("/etc/flycast/"); // This isn't part of the XDG spec, but much more common than /etc/xdg/
		dirs.push_back("/etc/xdg/flycast/");
	}
#endif
	dirs.push_back("./");

	return dirs;
}

// Find a file in the user data directories.
// The following folders are checked in this order:
// $HOME/.local/share/flycast
// if XDG_DATA_DIRS is defined:
//   <$XDG_DATA_DIRS>/flycast
// else
//   /usr/local/share/flycast
//   /usr/share/flycast
// <$FLYCAST_BIOS_PATH>
// ./
// ./data
std::vector<std::string> find_system_data_dirs()
{
	std::vector<std::string> dirs;

#ifdef __SWITCH__
	dirs.push_back("/flycast/data/");
#else
	std::string xdg_home;
	if (nowide::getenv("XDG_DATA_HOME") != nullptr)
		// If XDG_DATA_HOME is set explicitly, we'll use that instead of $HOME/.local/share
		xdg_home = (std::string)nowide::getenv("XDG_DATA_HOME");
	else if (nowide::getenv("HOME") != nullptr)
		xdg_home = (std::string)nowide::getenv("HOME") + "/.local/share";
	if (!xdg_home.empty())
		// XDG data locations
		dirs.push_back(xdg_home + "/flycast/");

	if (nowide::getenv("XDG_DATA_DIRS") != nullptr)
	{
		std::string path = (std::string)nowide::getenv("XDG_DATA_DIRS");
		addDirectoriesFromPath(dirs, path, "/flycast/");
	}
	else
	{
#ifdef FLYCAST_DATADIR
		const std::string data_dir(FLYCAST_DATADIR);
		dirs.push_back(data_dir);
#endif
		dirs.push_back("/usr/local/share/flycast/");
		dirs.push_back("/usr/share/flycast/");
	}
	if (nowide::getenv("FLYCAST_BIOS_PATH") != nullptr)
	{
		std::string path = (std::string)nowide::getenv("FLYCAST_BIOS_PATH");
		addDirectoriesFromPath(dirs, path, "/");
	}
#endif
	dirs.push_back("./");
	dirs.push_back("data/");

	return dirs;
}

static const char *selfPath;

void os_RunInstance(int argc, const char *argv[])
{
	if (fork() == 0)
	{
		std::vector<char *> localArgs;
		localArgs.push_back((char *)selfPath);
		for (int i = 0; i < argc; i++)
			localArgs.push_back((char *)argv[i]);
		localArgs.push_back(nullptr);
		execv(selfPath, &localArgs[0]);
	}
}

#if defined(USE_BREAKPAD)
static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded)
{
	if (succeeded)
		registerCrash(descriptor.directory().c_str(), descriptor.path());

	return succeeded;
}
#endif

int main(int argc, char* argv[])
{
	selfPath = argv[0];
#if defined(__SWITCH__)
	socketInitializeDefault();
	nxlinkStdio();
	//appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
#endif
#if defined(USE_BREAKPAD)
	google_breakpad::MinidumpDescriptor descriptor("/tmp");
	google_breakpad::ExceptionHandler eh(descriptor, nullptr, dumpCallback, nullptr, true, -1);
#endif

	LogManager::Init();

	// Set directories
	set_user_config_dir(find_user_config_dir());
	set_user_data_dir(find_user_data_dir());
	for (const auto& dir : find_system_config_dirs())
		add_system_config_dir(dir);
	for (const auto& dir : find_system_data_dirs())
		add_system_data_dir(dir);
	INFO_LOG(BOOT, "Config dir is: %s", get_writable_config_path("").c_str());
	INFO_LOG(BOOT, "Data dir is:   %s", get_writable_data_path("").c_str());

#if defined(USE_SDL)
	// init video now: on rpi3 it installs a sigsegv handler(?)
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		die("SDL: Initialization failed!");
	}
#endif

#if defined(__unix__)
	common_linux_setup();
#endif

	if (flycast_init(argc, argv))
		die("Flycast initialization failed\n");

#if defined(USE_BREAKPAD)
	auto async = std::async(std::launch::async, uploadCrashes, "/tmp");
#endif

	mainui_loop();

#if defined(SUPPORT_X11)
	x11_window_destroy();
#endif
#if defined(USE_SDL)
	sdl_window_destroy();
#endif

	flycast_term();
	os_UninstallFaultHandler();

#if defined(__SWITCH__)
	socketExit();
#endif

	return 0;
}

#if defined(__unix__)
[[noreturn]] void os_DebugBreak()
{
	raise(SIGTRAP);
	std::abort();
}
#endif

#endif // __unix__ || __SWITCH__
