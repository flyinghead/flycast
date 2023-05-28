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
		if (file_exists(fullpath))
			return fullpath;
		for (const auto& path : config::ContentPath.get())
		{
			fullpath = path + "/" + name;
			if (file_exists(fullpath))
				return fullpath;
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
	if (file_exists(fullpath))
		return fullpath;
	for (const auto& path : config::ContentPath.get())
	{
		try {
			fullpath = hostfs::storage().getSubPath(path, name);
			hostfs::storage().getFileInfo(fullpath);
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
	if (writable)
		return get_writable_data_path(state_file);
	else
		return get_readonly_data_path(state_file);
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

}

#ifdef USE_BREAKPAD

#include "rend/boxart/http_client.h"
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
