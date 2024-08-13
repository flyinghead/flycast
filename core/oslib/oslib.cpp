/*
	Copyright 2021 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "oslib.h"
#include "stdclass.h"
#include "cfg/cfg.h"
#include "cfg/option.h"
#include "nowide/fstream.hpp"
#include "storage.h"
#ifndef _WIN32
#include <unistd.h>
#endif
#if defined(USE_SDL)
#include "sdl/sdl.h"
#else
	#if defined(SUPPORT_X11)
		#include "linux-dist/x11.h"
	#endif
	#if defined(USE_EVDEV)
		#include "linux-dist/evdev.h"
	#endif
#endif
#if defined(_WIN32) && !defined(TARGET_UWP)
#include "windows/rawinput.h"
#include <shlobj.h>
#endif
#include "profiler/fc_profiler.h"

namespace hostfs
{

std::string getVmuPath(const std::string& port)
{
	if (port == "A1" && config::PerGameVmu && !settings.content.path.empty())
		return get_game_save_prefix() + "_vmu_save_A1.bin";

	char tempy[512];
	sprintf(tempy, "vmu_save_%s.bin", port.c_str());
	// VMU saves used to be stored in .reicast, not in .reicast/data
	std::string apath = get_writable_config_path(tempy);
	if (!file_exists(apath))
		apath = get_writable_data_path(tempy);
	return apath;
}

std::string getArcadeFlashPath()
{
	std::string nvmemSuffix = cfgLoadStr("net", "nvmem", "");
	return get_game_save_prefix() + nvmemSuffix;
}

std::string findFlash(const std::string& prefix, const std::string& names)
{
	const size_t npos = std::string::npos;
	size_t start = 0;
	while (start < names.size())
	{
		size_t semicolon = names.find(';', start);
		std::string name = names.substr(start, semicolon == npos ? semicolon : semicolon - start);

		size_t percent = name.find('%');
		if (percent != npos)
			name = name.replace(percent, 1, prefix);

		std::string fullpath = get_readonly_data_path(name);
		if (hostfs::storage().exists(fullpath))
			return fullpath;
		for (const auto& path : config::ContentPath.get())
		{
			try {
				fullpath = hostfs::storage().getSubPath(path, name);
				if (hostfs::storage().exists(fullpath))
					return fullpath;
			} catch (const hostfs::StorageException& e) {
			}
		}

		start = semicolon;
		if (start != npos)
			start++;
	}
	return "";

}

std::string getFlashSavePath(const std::string& prefix, const std::string& name)
{
	return get_writable_data_path(prefix + name);
}

std::string findNaomiBios(const std::string& name)
{
	std::string fullpath = get_readonly_data_path(name);
	if (hostfs::storage().exists(fullpath))
		return fullpath;
	for (const auto& path : config::ContentPath.get())
	{
		try {
			fullpath = hostfs::storage().getSubPath(path, name);
			if (hostfs::storage().exists(fullpath))
				return fullpath;
		} catch (const hostfs::StorageException& e) {
		}
	}
	return "";
}

std::string getSavestatePath(int index, bool writable)
{
	std::string state_file = get_file_basename(settings.content.fileName);

	char index_str[4] = "";
	if (index > 0) // When index is 0, use same name before multiple states is added
		sprintf(index_str, "_%d", std::min(99, index));

	state_file = state_file + index_str + ".state";
	if (index == -1)
		state_file += ".net";

	static std::string lastFile;
	static std::string lastPath;

	if (writable) {
		lastFile.clear();
		return get_writable_data_path(state_file);
	}
	else
	{
		if (lastFile != state_file) {
			lastFile = state_file;
			lastPath = get_readonly_data_path(state_file);
		}
		return lastPath;
	}
}

std::string getShaderCachePath(const std::string& filename)
{
	return get_writable_data_path(filename);
}

std::string getTextureLoadPath(const std::string& gameId)
{
	if (gameId.length() > 0)
		return get_readonly_data_path("textures/" + gameId) + "/";
	else
		return "";
}

std::string getTextureDumpPath()
{
	return get_writable_data_path("texdump/");
}

#if defined(__unix__) && !defined(__ANDROID__)

static std::string runCommand(const std::string& cmd)
{
	char buf[1024] {};
	FILE *fp = popen(cmd.c_str(), "r");
	if (fp == nullptr) {
		INFO_LOG(COMMON, "popen failed: %d", errno);
		return "";
	}
	std::string result;
	while (fgets(buf, sizeof(buf), fp) != nullptr)
		result += trim_trailing_ws(buf, "\n");

	int rc;
	if ((rc = pclose(fp)) != 0) {
		INFO_LOG(COMMON, "Command error: %d", rc);
		return "";
	}

	return result;
}

static std::string getScreenshotsPath()
{
	std::string picturesPath = runCommand("xdg-user-dir PICTURES");
	if (!picturesPath.empty())
		return picturesPath;
	const char *home = nowide::getenv("HOME");
	if (home != nullptr)
		return home;
	else
		return ".";
}

#elif defined(TARGET_UWP)
//TODO move to shell/uwp?
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

void saveScreenshot(const std::string& name, const std::vector<u8>& data)
{
	try {
		StorageFolder^ folder = KnownFolders::PicturesLibrary;	// or SavedPictures?
		if (folder == nullptr) {
			INFO_LOG(COMMON, "KnownFolders::PicturesLibrary is null");
			throw FlycastException("Can't find Pictures library");
		}
		nowide::wstackstring wstr;
		wchar_t *wname = wstr.convert(name.c_str());
		String^ msname = ref new String(wname);
		ArrayReference<u8> arrayRef(const_cast<u8*>(&data[0]), data.size());

		IAsyncOperation<StorageFile^>^ op = folder->CreateFileAsync(msname, CreationCollisionOption::FailIfExists);
		cResetEvent asyncEvent;
		op->Completed = ref new AsyncOperationCompletedHandler<StorageFile^>(
			[&asyncEvent, &arrayRef](IAsyncOperation<StorageFile^>^ op, AsyncStatus) {
				IAsyncAction^ action = FileIO::WriteBytesAsync(op->GetResults(), arrayRef);
				action->Completed = ref new AsyncActionCompletedHandler(
					[&asyncEvent](IAsyncAction^, AsyncStatus){
						asyncEvent.Set();
					});
			});
		asyncEvent.Wait();
	}
	catch (COMException^ e) {
		WARN_LOG(COMMON, "Save screenshot failed: %S", e->Message->Data());
		throw FlycastException("");
	}
}

#elif defined(_WIN32) && !defined(TARGET_UWP)

static std::string getScreenshotsPath()
{
	wchar_t *screenshotPath;
	if (FAILED(SHGetKnownFolderPath(FOLDERID_Screenshots, KF_FLAG_DEFAULT, NULL, &screenshotPath)))
		return get_writable_config_path("");
	nowide::stackstring path;
	std::string ret;
	if (path.convert(screenshotPath) == nullptr)
		ret = get_writable_config_path("");
	else
		ret = path.get();
	CoTaskMemFree(screenshotPath);

	return ret;
}

#else

std::string getScreenshotsPath();

#endif

#if !defined(__ANDROID__) && !defined(TARGET_UWP) && !defined(TARGET_IPHONE) && !defined(__SWITCH__)

void saveScreenshot(const std::string& name, const std::vector<u8>& data)
{
	std::string path = getScreenshotsPath();
	path += "/" + name;
	FILE *f = nowide::fopen(path.c_str(), "wb");
	if (f == nullptr)
		throw FlycastException(path);
	if (std::fwrite(&data[0], data.size(), 1, f) != 1) {
		std::fclose(f);
		unlink(path.c_str());
		throw FlycastException(path);
	}
	std::fclose(f);
}

#endif

} // namespace hostfs

void os_CreateWindow()
{
#if defined(USE_SDL)
	sdl_window_create();
#elif defined(SUPPORT_X11)
	x11_window_create();
#endif
}

void os_DestroyWindow()
{
#if defined(USE_SDL)
	sdl_window_destroy();
#elif defined(SUPPORT_X11)
	x11_window_destroy();
#endif
}

void os_SetupInput()
{
#if defined(USE_SDL)
	input_sdl_init();
#else
	#if defined(SUPPORT_X11)
		input_x11_init();
	#endif
	#if defined(USE_EVDEV)
		input_evdev_init();
	#endif
#endif
#if defined(_WIN32) && !defined(TARGET_UWP)
	if (config::UseRawInput)
		rawinput::init();
#endif
}

void os_TermInput()
{
#if defined(USE_SDL)
	input_sdl_quit();
#else
	#if defined(USE_EVDEV)
		input_evdev_close();
	#endif
#endif
#if defined(_WIN32) && !defined(TARGET_UWP)
	if (config::UseRawInput)
		rawinput::term();
#endif
}

void os_UpdateInputState()
{
	FC_PROFILE_SCOPE;

#if defined(USE_SDL)
	input_sdl_handle();
#else
	#if defined(USE_EVDEV)
		input_evdev_handle();
	#endif
#endif
}

#ifdef USE_BREAKPAD

#include "http_client.h"
#include "version.h"
#include "log/InMemoryListener.h"
#include "wsi/context.h"

#define FLYCAST_CRASH_LIST "flycast-crashes.txt"

void registerCrash(const char *directory, const char *path)
{
	char list[256];
	// Register .dmp in crash list
	snprintf(list, sizeof(list), "%s/%s", directory, FLYCAST_CRASH_LIST);
	FILE *f = nowide::fopen(list, "at");
	if (f != nullptr)
	{
		fprintf(f, "%s\n", path);
		fclose(f);
	}
	// Save last log lines
	InMemoryListener *listener = InMemoryListener::getInstance();
	if (listener != nullptr)
	{
		strncpy(list, path, sizeof(list) - 1);
		list[sizeof(list) - 1] = '\0';
		char *p = strrchr(list, '.');
		if (p != nullptr && (p - list) < (int)sizeof(list) - 4)
		{
			strcpy(p + 1, "log");
			FILE *f = nowide::fopen(list, "wt");
			if (f != nullptr)
			{
				std::vector<std::string> log = listener->getLog();
				for (const auto& line : log)
					fprintf(f, "%s", line.c_str());
				fprintf(f, "Version: %s\n", GIT_VERSION);
				fprintf(f, "Renderer: %d\n", (int)config::RendererType.get());
				GraphicsContext *gctx = GraphicsContext::Instance();
				if (gctx != nullptr)
					fprintf(f, "GPU: %s %s\n", gctx->getDriverName().c_str(), gctx->getDriverVersion().c_str());
				fprintf(f, "Game: %s\n", settings.content.gameId.c_str());
				fclose(f);
			}
		}
	}
}

void uploadCrashes(const std::string& directory)
{
	FILE *f = nowide::fopen((directory + "/" FLYCAST_CRASH_LIST).c_str(), "rt");
	if (f == nullptr)
		return;
	http::init();
	char line[256];
	bool uploadFailure = false;
	while (fgets(line, sizeof(line), f) != nullptr)
	{
		char *p = line + strlen(line) - 1;
		if (*p == '\n')
			*p = '\0';
		if (file_exists(line))
		{
			std::string dmpfile(line);
			std::string logfile = get_file_basename(dmpfile) + ".log";
#ifdef SENTRY_UPLOAD
			if (config::UploadCrashLogs)
			{
				NOTICE_LOG(COMMON, "Uploading minidump %s", line);
				std::string version = std::string(GIT_VERSION);
				if (file_exists(logfile))
				{
					nowide::ifstream ifs(logfile);
					if (ifs.is_open())
					{
						std::string line;
						while (std::getline(ifs, line))
							if (line.substr(0, 9) == "Version: ")
							{
								version = line.substr(9);
								break;
							}
					}
				}
				std::vector<http::PostField> fields;
				fields.emplace_back("upload_file_minidump", dmpfile, "application/octet-stream");
				fields.emplace_back("sentry[release]", version);
				if (file_exists(logfile))
					fields.emplace_back("flycast_log", logfile, "text/plain");
				// TODO config, gpu/driver, ...
				int rc = http::post(SENTRY_UPLOAD, fields);
				if (rc >= 200 && rc < 300) {
					nowide::remove(dmpfile.c_str());
					nowide::remove(logfile.c_str());
				}
				else
				{
					WARN_LOG(COMMON, "Upload failed: HTTP error %d", rc);
					uploadFailure = true;
				}
			}
			else
#endif
			{
				nowide::remove(dmpfile.c_str());
				nowide::remove(logfile.c_str());
			}
		}
	}
	http::term();
	fclose(f);
	if (!uploadFailure)
		nowide::remove((directory + "/" FLYCAST_CRASH_LIST).c_str());
}

#else

void registerCrash(const char *directory, const char *path) {}
void uploadCrashes(const std::string& directory) {}

#endif
