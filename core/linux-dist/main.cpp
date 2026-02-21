#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS 1
#endif
#include "types.h"

#if defined(__unix__) || defined(__HAIKU__) || defined(__vita__)
#include "log/LogManager.h"
#include "emulator.h"
#include "ui/mainui.h"
#include "oslib/directory.h"
#include "oslib/oslib.h"
#include "oslib/i18n.h"
#include "stdclass.h"

#include <csignal>
#include <string>
#include <unistd.h>
#include <vector>
#include <exception>

#ifdef __vita__
#include <vitasdk.h>
#include <vitaGL.h>
#include <xxhash.h>
int _newlib_heap_size_user = 250 * 1024 * 1024;
unsigned int sceUserMainThreadStackSize = 1 * 1024 * 1024;
bool is_standalone = false;

extern "C" {
void *__wrap_calloc(uint32_t nmember, uint32_t size) { return vglCalloc(nmember, size); }
void __wrap_free(void *addr) { vglFree(addr); };
void *__wrap_malloc(uint32_t size) { return vglMalloc(size); };
void *__wrap_memalign(uint32_t alignment, uint32_t size) { return vglMemalign(alignment, size); };
void *__wrap_realloc(void *ptr, uint32_t size) { return vglRealloc(ptr, size); };
void *__wrap_memcpy (void *dst, const void *src, size_t num) { return sceClibMemcpy(dst, src, num); };
void *__wrap_memset (void *ptr, int value, size_t num) { return sceClibMemset(ptr, value, num); };
}

void early_fatal_error(const char *msg) {
	vglInit(0);
	SceMsgDialogUserMessageParam msg_param;
	sceClibMemset(&msg_param, 0, sizeof(SceMsgDialogUserMessageParam));
	msg_param.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;
	msg_param.msg = (const SceChar8*)msg;
	SceMsgDialogParam param;
	sceMsgDialogParamInit(&param);
	param.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
	param.userMsgParam = &msg_param;
	sceMsgDialogInit(&param);
	while (sceMsgDialogGetStatus() != SCE_COMMON_DIALOG_STATUS_FINISHED) {
		vglSwapBuffers(GL_TRUE);
	}
	sceKernelExitProcess(0);
}
#endif

#if defined(SUPPORT_X11)
	#include "x11.h"
#endif

#if defined(USE_SDL)
	#include "sdl/sdl.h"
#endif

#ifdef USE_BREAKPAD
#include "breakpad/client/linux/handler/exception_handler.h"
#endif

void os_DoEvents()
{
	#if defined(SUPPORT_X11)
		input_x11_handle();
		event_x11_handle();
	#endif
}

void common_linux_setup();

// Find the user config directory.
// $HOME/.config/flycast on linux
static std::string find_user_config_dir()
{
#ifdef __vita__
	flycast::mkdir("ux0:data/flycast", 0777);
	return "ux0:data/flycast/";
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
static std::string find_user_data_dir()
{
#ifdef __vita__
	flycast::mkdir("ux0:data/flycast/data", 0777);
	return "ux0:data/flycast/data/";
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

#ifndef __vita__
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
static std::vector<std::string> find_system_config_dirs()
{
	std::vector<std::string> dirs;

#ifdef __vita__
	dirs.push_back("ux0:data/flycast/");
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
static std::vector<std::string> find_system_data_dirs()
{
	std::vector<std::string> dirs;

#ifdef __vita__
	dirs.push_back("ux0:data/flycast/data/");
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
#ifndef __vita__
	if (fork() == 0)
	{
		std::vector<char *> localArgs;
		localArgs.push_back((char *)selfPath);
		for (int i = 0; i < argc; i++)
			localArgs.push_back((char *)argv[i]);
		localArgs.push_back(nullptr);
		execv(selfPath, &localArgs[0]);
	}
#endif
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
#if defined(USE_BREAKPAD)
	google_breakpad::MinidumpDescriptor descriptor("/tmp");
	google_breakpad::ExceptionHandler eh(descriptor, nullptr, dumpCallback, nullptr, true, -1);
#endif

	LogManager::Init();
	i18n::init();

	// Set directories
	set_user_config_dir(find_user_config_dir());
	set_user_data_dir(find_user_data_dir());
	for (const auto& dir : find_system_config_dirs())
		add_system_config_dir(dir);
	for (const auto& dir : find_system_data_dirs())
		add_system_data_dir(dir);
	INFO_LOG(BOOT, "Config dir is: %s", get_writable_config_path("").c_str());
	INFO_LOG(BOOT, "Data dir is:   %s", get_writable_data_path("").c_str());

#ifdef __vita__
	SceIoStat st1, st2;
	// Checking for libshacccg.suprx existence
	if (!(sceIoGetstat("ur0:/data/libshacccg.suprx", &st1) >= 0 || sceIoGetstat("ur0:/data/external/libshacccg.suprx", &st2) >= 0))
		early_fatal_error("Error: Runtime shader compiler (libshacccg.suprx) is not installed.");

	// Checking for kubridge existence
	if (!(sceIoGetstat("ux0:/tai/kubridge.skprx", &st1) >= 0 || sceIoGetstat("ur0:/tai/kubridge.skprx", &st2) >= 0))
		early_fatal_error("Error: kubridge.skprx is not installed.");

	// Checking for kubridge version
	FILE *f = fopen("ux0:/tai/kubridge.skprx", "rb");
	if (!f)
		f = fopen("ur0:/tai/kubridge.skprx", "rb");
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	void *buf = vglMalloc(size);
	fread(buf, 1, size, f);
	fclose(f);
	uint32_t kubridge_hash = XXH32(buf, size, 7);
	vglFree(buf);
	if (kubridge_hash == 0xFDAE199B)
		early_fatal_error("Error: kubridge.skprx is outdated.");

	char boot_params[1024];
	char *launch_argv[2];
	argc = 0;

	// Check if we launched flycast from a custom bubble
	sceAppMgrGetAppParam(boot_params);
	if (strstr(boot_params,"psgm:play") && strstr(boot_params, "&param=")) {
		argc = 2;
		launch_argv[1] = strstr(boot_params, "&param=") + 7;
		is_standalone = true;
	}
	argv = launch_argv;

	scePowerSetArmClockFrequency(444);
	scePowerSetBusClockFrequency(222);
	scePowerSetGpuClockFrequency(222);
	scePowerSetGpuXbarClockFrequency(166);
	SDL_setenv("VITA_DISABLE_TOUCH_BACK", "1", 1); // Disabling rearpad
	vglSetParamBufferSize(8 * 1024 * 1024);
	vglUseCachedMem(GL_TRUE);
	vglInitWithCustomThreshold(0, 960, 544, 256 * 1024 * 1024, 0, 0, 0, SCE_GXM_MULTISAMPLE_4X);
#endif

#if defined(USE_SDL)
	// init video now: on rpi3 it installs a sigsegv handler(?)
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		die("SDL: Initialization failed!");
	}
#endif

	common_linux_setup();

	if (flycast_init(argc, argv))
		die("Flycast initialization failed\n");

#if defined(USE_BREAKPAD)
	auto async = std::async(std::launch::async, uploadCrashes, "/tmp");
#endif

	try {
		mainui_loop();
	} catch (const std::exception& e) {
		ERROR_LOG(BOOT, "mainui_loop error: %s", e.what());
	} catch (...) {
		ERROR_LOG(BOOT, "mainui_loop unknown exception");
	}

	flycast_term();
	os_UninstallFaultHandler();

	return 0;
}

#ifndef __vita__
[[noreturn]] void os_DebugBreak()
{
	raise(SIGTRAP);
	std::abort();
}
#endif

#endif // __unix__ || __vita__
