#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include "types.h"

#if defined(__unix__) || defined(__SWITCH__)
#include "hw/sh4/dyna/blockmanager.h"
#include "log/LogManager.h"
#include "emulator.h"
#include "rend/mainui.h"
#include "oslib/directory.h"
#include "oslib/oslib.h"

#include <cstdarg>
#include <csignal>
#include <unistd.h>

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
#include "client/linux/handler/exception_handler.h"
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
// The following folders are checked in this order:
// $HOME/.reicast
// $HOME/.config/flycast
// $HOME/.config/reicast
// If no folder exists, $HOME/.config/flycast is created and used.
std::string find_user_config_dir()
{
#ifdef __SWITCH__
	flycast::mkdir("/flycast", 0755);
	return "/flycast/";
#else
	struct stat info;
	std::string xdg_home;
	if (nowide::getenv("HOME") != NULL)
	{
		// Support for the legacy config dir at "$HOME/.reicast"
		std::string legacy_home = (std::string)nowide::getenv("HOME") + "/.reicast/";
		if (flycast::stat(legacy_home.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))
			// "$HOME/.reicast" already exists, let's use it!
			return legacy_home;

		/* If $XDG_CONFIG_HOME is not set, we're supposed to use "$HOME/.config" instead.
		 * Consult the XDG Base Directory Specification for details:
		 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
		 */
		xdg_home = (std::string)nowide::getenv("HOME") + "/.config";
	}
	if (nowide::getenv("XDG_CONFIG_HOME") != NULL)
		// If XDG_CONFIG_HOME is set explicitly, we'll use that instead of $HOME/.config
		xdg_home = (std::string)nowide::getenv("XDG_CONFIG_HOME");

	if (!xdg_home.empty())
	{
		std::string fullpath = xdg_home + "/flycast/";
		if (flycast::stat(fullpath.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))
			// Found .config/flycast
			return fullpath;
		fullpath = xdg_home + "/reicast/";
		if (flycast::stat(fullpath.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))
			// Found .config/reicast
			return fullpath;

		// Create .config/flycast
		fullpath = xdg_home + "/flycast/";
		flycast::mkdir(fullpath.c_str(), 0755);

		return fullpath;
	}
	// Unable to detect config dir, use the current folder
	return ".";
#endif
}

// Find the user data directory.
// The following folders are checked in this order:
// $HOME/.reicast/data
// $HOME/.local/share/flycast
// $HOME/.local/share/reicast
// If no folder exists, $HOME/.local/share/flycast is created and used.
std::string find_user_data_dir()
{
#ifdef __SWITCH__
	flycast::mkdir("/flycast/data", 0755);
	return "/flycast/data/";
#else
	struct stat info;
	std::string xdg_home;
	if (nowide::getenv("HOME") != NULL)
	{
		// Support for the legacy config dir at "$HOME/.reicast/data"
		std::string legacy_data = (std::string)nowide::getenv("HOME") + "/.reicast/data/";
		if (flycast::stat(legacy_data.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))
			// "$HOME/.reicast/data" already exists, let's use it!
			return legacy_data;

		/* If $XDG_DATA_HOME is not set, we're supposed to use "$HOME/.local/share" instead.
		 * Consult the XDG Base Directory Specification for details:
		 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
		 */
		xdg_home = (std::string)nowide::getenv("HOME") + "/.local/share";
	}
	if (nowide::getenv("XDG_DATA_HOME") != NULL)
		// If XDG_DATA_HOME is set explicitly, we'll use that instead of $HOME/.local/share
		xdg_home = (std::string)nowide::getenv("XDG_DATA_HOME");

	if (!xdg_home.empty())
	{
		std::string fullpath = xdg_home + "/flycast/";
		if (flycast::stat(fullpath.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))
			// Found .local/share/flycast
			return fullpath;
		fullpath = xdg_home + "/reicast/";
		if (flycast::stat(fullpath.c_str(), &info) == 0 && (info.st_mode & S_IFDIR))
			// Found .local/share/reicast
			return fullpath;

		// Create .local/share/flycast
		fullpath = xdg_home + "/flycast/";
		flycast::mkdir(fullpath.c_str(), 0755);

		return fullpath;
	}
	// Unable to detect data dir, use the current folder
	return ".";
#endif
}

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

// Find a file in the user and system config directories.
// The following folders are checked in this order:
// $HOME/.reicast
// $HOME/.config/flycast
// $HOME/.config/reicast
// if XDG_CONFIG_DIRS is defined:
//   <$XDG_CONFIG_DIRS>/flycast
//   <$XDG_CONFIG_DIRS>/reicast
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
	if (nowide::getenv("HOME") != NULL)
	{
		// Support for the legacy config dir at "$HOME/.reicast"
		dirs.push_back((std::string)nowide::getenv("HOME") + "/.reicast/");
		xdg_home = (std::string)nowide::getenv("HOME") + "/.config";
	}
	if (nowide::getenv("XDG_CONFIG_HOME") != NULL)
		// If XDG_CONFIG_HOME is set explicitly, we'll use that instead of $HOME/.config
		xdg_home = (std::string)nowide::getenv("XDG_CONFIG_HOME");
	if (!xdg_home.empty())
	{
		// XDG config locations
		dirs.push_back(xdg_home + "/flycast/");
		dirs.push_back(xdg_home + "/reicast/");
	}

	if (nowide::getenv("XDG_CONFIG_DIRS") != NULL)
	{
		std::string path = (std::string)nowide::getenv("XDG_CONFIG_DIRS");
		addDirectoriesFromPath(dirs, path, "/flycast/");
		addDirectoriesFromPath(dirs, path, "/reicast/");
	}
	else
	{
		dirs.push_back("/etc/flycast/"); // This isn't part of the XDG spec, but much more common than /etc/xdg/
		dirs.push_back("/etc/xdg/flycast/");
	}
#endif
	dirs.push_back("./");

	return dirs;
}

// Find a file in the user data directories.
// The following folders are checked in this order:
// $HOME/.reicast/data
// $HOME/.local/share/flycast
// $HOME/.local/share/reicast
// if XDG_DATA_DIRS is defined:
//   <$XDG_DATA_DIRS>/flycast
//   <$XDG_DATA_DIRS>/reicast
// else
//   /usr/local/share/flycast
//   /usr/share/flycast
//   /usr/local/share/reicast
//   /usr/share/reicast
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
	if (nowide::getenv("HOME") != NULL)
	{
		// Support for the legacy data dir at "$HOME/.reicast/data"
		dirs.push_back((std::string)nowide::getenv("HOME") + "/.reicast/data/");
		xdg_home = (std::string)nowide::getenv("HOME") + "/.local/share";
	}
	if (nowide::getenv("XDG_DATA_HOME") != NULL)
		// If XDG_DATA_HOME is set explicitly, we'll use that instead of $HOME/.local/share
		xdg_home = (std::string)nowide::getenv("XDG_DATA_HOME");
	if (!xdg_home.empty())
	{
		// XDG data locations
		dirs.push_back(xdg_home + "/flycast/");
		dirs.push_back(xdg_home + "/reicast/");
		dirs.push_back(xdg_home + "/reicast/data/");
	}

	if (nowide::getenv("XDG_DATA_DIRS") != NULL)
	{
		std::string path = (std::string)nowide::getenv("XDG_DATA_DIRS");

		addDirectoriesFromPath(dirs, path, "/flycast/");
		addDirectoriesFromPath(dirs, path, "/reicast/");
	}
	else
	{
		dirs.push_back("/usr/local/share/flycast/");
		dirs.push_back("/usr/share/flycast/");
		dirs.push_back("/usr/local/share/reicast/");
		dirs.push_back("/usr/share/reicast/");
	}
	if (nowide::getenv("FLYCAST_BIOS_PATH") != NULL)
	{
		std::string path = (std::string)nowide::getenv("FLYCAST_BIOS_PATH");
		addDirectoriesFromPath(dirs, path, "/");
	}
#endif
	dirs.push_back("./");
	dirs.push_back("data/");

	return dirs;
}

#if defined(USE_BREAKPAD)
static bool dumpCallback(const google_breakpad::MinidumpDescriptor& descriptor, void* context, bool succeeded)
{
	printf("Minidump saved to '%s'\n", descriptor.path());
	return succeeded;
}
#endif

int main(int argc, char* argv[])
{
#if defined(__SWITCH__)
	socketInitializeDefault();
	nxlinkStdio();
	//appletSetFocusHandlingMode(AppletFocusHandlingMode_NoSuspend);
#endif
#if defined(USE_BREAKPAD)
	google_breakpad::MinidumpDescriptor descriptor("/tmp");
	google_breakpad::ExceptionHandler eh(descriptor, NULL, dumpCallback, NULL, true, -1);
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

	mainui_loop();

	flycast_term();

	os_UninstallFaultHandler();

#if defined(USE_EVDEV)
	input_evdev_close();
#endif

#if defined(SUPPORT_X11)
	x11_window_destroy();
#endif

#if defined(USE_SDL)
	sdl_window_destroy();
#endif
#if defined(__SWITCH__)
	socketExit();
#endif

	return 0;
}

#if defined(__unix__)
void os_DebugBreak()
{
	raise(SIGTRAP);
}
#endif

#endif // __unix__ || __SWITCH__
