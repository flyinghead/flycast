#include "types.h"

#if HOST_OS==OS_LINUX
#include "hw/sh4/dyna/blockmanager.h"
#include "log/LogManager.h"
#include "emulator.h"

#include <cstdarg>
#include <csignal>
#include <unistd.h>

#if defined(SUPPORT_DISPMANX)
	#include "dispmanx.h"
#endif

#if defined(SUPPORT_X11)
	#include "x11.h"
#endif

#if defined(USE_SDL)
	#include "sdl/sdl.h"
#endif

#if defined(USES_HOMEDIR)
	#include <sys/stat.h>
#endif

#if defined(USE_EVDEV)
	#include "evdev.h"
#endif

#if defined(USE_JOYSTICK)
    #include "cfg/cfg.h"
	#include "joystick.h"
#endif

#ifdef TARGET_PANDORA
	#include <signal.h>
	#include <execinfo.h>
	#include <sys/soundcard.h>
#endif

#if FEAT_HAS_NIXPROF
#include "profiler/profiler.h"
#endif

#if defined(USE_JOYSTICK)
	/* legacy joystick input */
	static int joystick_fd = -1; // Joystick file descriptor
#endif

void os_SetupInput()
{
#if defined(USE_EVDEV)
	input_evdev_init();
#endif

#if defined(USE_JOYSTICK)
	int joystick_device_id = cfgLoadInt("input", "joystick_device_id", JOYSTICK_DEFAULT_DEVICE_ID);
	if (joystick_device_id < 0) {
		INFO_LOG(INPUT, "Legacy Joystick input disabled by config.");
	}
	else
	{
		int joystick_device_length = snprintf(NULL, 0, JOYSTICK_DEVICE_STRING, joystick_device_id);
		char* joystick_device = (char*)malloc(joystick_device_length + 1);
		sprintf(joystick_device, JOYSTICK_DEVICE_STRING, joystick_device_id);
		joystick_fd = input_joystick_init(joystick_device);
		free(joystick_device);
	}
#endif

#if defined(SUPPORT_X11)
	input_x11_init();
#endif

#if defined(USE_SDL)
	input_sdl_init();
#endif
}

void UpdateInputState(u32 port)
{
	#if defined(USE_JOYSTICK)
		input_joystick_handle(joystick_fd, port);
	#endif

	#if defined(USE_EVDEV)
		input_evdev_handle(port);
	#endif

	#if defined(USE_SDL)
		input_sdl_handle(port);
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
void* rend_thread(void* p);

#ifdef TARGET_PANDORA
	void clean_exit(int sig_num)
	{
		void* array[10];
		size_t size;

		if (joystick_fd >= 0) { close(joystick_fd); }
		for (int port = 0; port < 4 ; port++)
		{
			if (evdev_controllers[port]->fd >= 0)
			{
				close(evdev_controllers[port]->fd);
			}
		}

		x11_window_destroy();

		// finish cleaning
		if (sig_num!=0)
		{
			write(2, "\nSignal received\n", sizeof("\nSignal received\n"));

			size = backtrace(array, 10);
			backtrace_symbols_fd(array, size, STDERR_FILENO);
			exit(1);
		}
	}
#endif

std::string find_user_config_dir()
{
	#ifdef USES_HOMEDIR
		struct stat info;
		std::string home = "";
		if(getenv("HOME") != NULL)
		{
			// Support for the legacy config dir at "$HOME/.reicast"
			std::string legacy_home = (std::string)getenv("HOME") + "/.reicast";
			if((stat(legacy_home.c_str(), &info) == 0) && (info.st_mode & S_IFDIR))
			{
				// "$HOME/.reicast" already exists, let's use it!
				return legacy_home;
			}

			/* If $XDG_CONFIG_HOME is not set, we're supposed to use "$HOME/.config" instead.
			 * Consult the XDG Base Directory Specification for details:
			 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
			 */
			home = (std::string)getenv("HOME") + "/.config/reicast";
		}
		if(getenv("XDG_CONFIG_HOME") != NULL)
		{
			// If XDG_CONFIG_HOME is set explicitly, we'll use that instead of $HOME/.config
			home = (std::string)getenv("XDG_CONFIG_HOME") + "/reicast";
		}

		if(!home.empty())
		{
			if((stat(home.c_str(), &info) != 0) || !(info.st_mode & S_IFDIR))
			{
				// If the directory doesn't exist yet, create it!
				mkdir(home.c_str(), 0755);
			}
			return home;
		}
	#endif

	// Unable to detect config dir, use the current folder
	return ".";
}

std::string find_user_data_dir()
{
	#ifdef USES_HOMEDIR
		struct stat info;
		std::string data = "";
		if(getenv("HOME") != NULL)
		{
			// Support for the legacy config dir at "$HOME/.reicast"
			std::string legacy_data = (std::string)getenv("HOME") + "/.reicast";
			if((stat(legacy_data.c_str(), &info) == 0) && (info.st_mode & S_IFDIR))
			{
				// "$HOME/.reicast" already exists, let's use it!
				return legacy_data;
			}

			/* If $XDG_DATA_HOME is not set, we're supposed to use "$HOME/.local/share" instead.
			 * Consult the XDG Base Directory Specification for details:
			 *   http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html#variables
			 */
			data = (std::string)getenv("HOME") + "/.local/share/reicast";
		}
		if(getenv("XDG_DATA_HOME") != NULL)
		{
			// If XDG_DATA_HOME is set explicitly, we'll use that instead of $HOME/.config
			data = (std::string)getenv("XDG_DATA_HOME") + "/reicast";
		}

		if(!data.empty())
		{
			if((stat(data.c_str(), &info) != 0) || !(info.st_mode & S_IFDIR))
			{
				// If the directory doesn't exist yet, create it!
				mkdir(data.c_str(), 0755);
			}
			return data;
		}
	#endif

	// Unable to detect config dir, use the current folder
	return ".";
}

std::vector<std::string> find_system_config_dirs()
{
	std::vector<std::string> dirs;
	if (getenv("XDG_CONFIG_DIRS") != NULL)
	{
		std::string s = (std::string)getenv("XDG_CONFIG_DIRS");

		std::string::size_type pos = 0;
		std::string::size_type n = s.find(':', pos);
		while(n != std::string::npos)
		{
			dirs.push_back(s.substr(pos, n-pos) + "/reicast");
			pos = n + 1;
			n = s.find(':', pos);
		}
		// Separator not found
		dirs.push_back(s.substr(pos) + "/reicast");
	}
	else
	{
		dirs.push_back("/etc/reicast"); // This isn't part of the XDG spec, but much more common than /etc/xdg/
		dirs.push_back("/etc/xdg/reicast");
	}
	return dirs;
}

std::vector<std::string> find_system_data_dirs()
{
	std::vector<std::string> dirs;
	if (getenv("XDG_DATA_DIRS") != NULL)
	{
		std::string s = (std::string)getenv("XDG_DATA_DIRS");

		std::string::size_type pos = 0;
		std::string::size_type n = s.find(':', pos);
		while(n != std::string::npos)
		{
			dirs.push_back(s.substr(pos, n-pos) + "/reicast");
			pos = n + 1;
			n = s.find(':', pos);
		}
		// Separator not found
		dirs.push_back(s.substr(pos) + "/reicast");
	}
	else
	{
		dirs.push_back("/usr/local/share/reicast");
		dirs.push_back("/usr/share/reicast");
	}
	return dirs;
}

int main(int argc, char* argv[])
{
	LogManager::Init();
	#ifdef TARGET_PANDORA
		signal(SIGSEGV, clean_exit);
		signal(SIGKILL, clean_exit);
	#endif

	/* Set directories */
	set_user_config_dir(find_user_config_dir());
	set_user_data_dir(find_user_data_dir());
	std::vector<std::string> dirs;
	dirs = find_system_config_dirs();
	for (std::size_t i = 0; i < dirs.size(); i++)
	{
		add_system_data_dir(dirs[i]);
	}
	dirs = find_system_data_dirs();
	for (std::size_t i = 0; i < dirs.size(); i++)
	{
		add_system_data_dir(dirs[i]);
	}
	INFO_LOG(BOOT, "Config dir is: %s", get_writable_config_path("/").c_str());
	INFO_LOG(BOOT, "Data dir is:   %s", get_writable_data_path("/").c_str());

	#if defined(USE_SDL)
		if (SDL_Init(0) != 0)
		{
			die("SDL: Initialization failed!");
		}
	#endif

	common_linux_setup();

	settings.profile.run_counts=0;

	if (reicast_init(argc, argv))
		die("Reicast initialization failed\n");

	#if FEAT_HAS_NIXPROF
	install_prof_handler(1);
	#endif
	rend_thread(NULL);

	#ifdef TARGET_PANDORA
		clean_exit(0);
	#endif

	dc_term();

	#if defined(USE_EVDEV)
		input_evdev_close();
	#endif

	#if defined(SUPPORT_X11)
		x11_window_destroy();
	#endif

#if defined(USE_SDL)
	sdl_window_destroy();
#endif

	return 0;
}
#endif

int get_mic_data(u8* buffer) { return 0; }

void os_DebugBreak()
{
	raise(SIGTRAP);
}



