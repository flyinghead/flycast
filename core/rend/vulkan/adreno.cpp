/*
	Copyright 2024 flyinghead

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
#include "build.h"
#if defined(__ANDROID__) && !defined(LIBRETRO) && HOST_CPU == CPU_ARM64
#include "adreno.h"
#include <dlfcn.h>
#include "cfg/option.h"
#include <adrenotools/driver.h>
#include "json.hpp"
using namespace nlohmann;
#include "archive/ZipArchive.h"
#include "oslib/directory.h"
#include "stdclass.h"

std::string getNativeLibraryPath();
std::string getFilesPath();

const std::string DRIVER_PATH = "/gpu_driver/";
static void *libvulkanHandle;

static json loadDriverMeta()
{
	std::string fullPath = getFilesPath() + DRIVER_PATH + "meta.json";
	FILE *f = nowide::fopen(fullPath.c_str(), "rt");
	if (f == nullptr) {
		WARN_LOG(RENDERER, "Can't open %s", fullPath.c_str());
		return json{};
	}
	std::string content(4096, '\0');
	size_t l = fread(content.data(), 1, content.size(), f);
	fclose(f);
	if (l <= 0) {
		WARN_LOG(RENDERER, "Can't read %s", fullPath.c_str());
		return json{};
	}
	content.resize(l);
	try {
		return json::parse(content);
	} catch (const json::exception& e) {
		WARN_LOG(COMMON, "Corrupted meta.json file: %s", e.what());
		return json{};
	}
}

static std::string getLibraryName()
{
	json v = loadDriverMeta();
	std::string name;
	try {
		v.at("libraryName").get_to(name);
	} catch (const json::exception& e) {
	}
	return name;
}

PFN_vkGetInstanceProcAddr loadVulkanDriver()
{
	// If the user has selected a custom driver, try to load it
	if (config::CustomGpuDriver)
	{
		std::string libName = getLibraryName();
		if (!libName.empty())
		{
			std::string driverPath = getFilesPath() + DRIVER_PATH;
			std::string tmpLibDir = getFilesPath() + "/tmp/";
			mkdir(tmpLibDir.c_str(), 0755);
			//std::string redirectDir = get_writable_data_path("");
			libvulkanHandle = adrenotools_open_libvulkan(
					RTLD_NOW | RTLD_LOCAL,
					ADRENOTOOLS_DRIVER_CUSTOM /* | ADRENOTOOLS_DRIVER_FILE_REDIRECT */,
					tmpLibDir.c_str(),
					getNativeLibraryPath().c_str(),
					driverPath.c_str(),
					libName.c_str(),
					nullptr, //redirectDir.c_str(),
					nullptr);
			if (libvulkanHandle == nullptr) {
				char *error = dlerror();
				WARN_LOG(RENDERER, "Failed to load custom Vulkan driver %s%s: %s", driverPath.c_str(), libName.c_str(), error ? error : "");
			}
		}
	}
	if (libvulkanHandle == nullptr)
	{
		libvulkanHandle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
		if (libvulkanHandle == nullptr)
		{
			char *error = dlerror();
			WARN_LOG(RENDERER, "Failed to load system Vulkan driver: %s", error ? error : "");
			return nullptr;
		}
	}

	return reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(libvulkanHandle, "vkGetInstanceProcAddr"));
}

void unloadVulkanDriver()
{
	if (libvulkanHandle != nullptr) {
		dlclose(libvulkanHandle);
		libvulkanHandle = nullptr;
	}
}

bool getCustomGpuDriverInfo(std::string& name, std::string& description, std::string& vendor, std::string& version)
{
	json j = loadDriverMeta();
	try {
		j.at("name").get_to(name);
	} catch (const json::exception& e) {
		return false;
	}
	try {
		j.at("description").get_to(description);
	} catch (const json::exception& e) {
		description = "";
	}
	try {
		j.at("vendor").get_to(vendor);
	} catch (const json::exception& e) {
		vendor = "";
	}
	try {
		j.at("driverVersion").get_to(version);
	} catch (const json::exception& e) {
		version = "";
	}

	return true;
}

void uploadCustomGpuDriver(const std::string& zipPath)
{
	FILE *zipf = nowide::fopen(zipPath.c_str(), "rb");
	if (zipf == nullptr)
		throw FlycastException("Can't open zip file");
	ZipArchive archive;
	if (!archive.Open(zipf))
		throw FlycastException("Invalid zip file");
	std::string fullPath = getFilesPath() + DRIVER_PATH;
	flycast::mkdir(fullPath.c_str(), 0755);
	// Clean driver directory
	DIR *dir = flycast::opendir(fullPath.c_str());
	if (dir != nullptr)
	{
		while (true)
		{
			dirent *direntry = flycast::readdir(dir);
			if (direntry == nullptr)
				break;
			std::string name = direntry->d_name;
			if (name == "." || name == "..")
				continue;
			name = fullPath + name;
			unlink(name.c_str());
		}
	}
	// Extract and save files
	for (size_t i = 0; ; i++)
	{
		ArchiveFile *afile = archive.OpenFileByIndex(i);
		if (afile == nullptr)
			break;
		FILE *f = fopen((fullPath + afile->getName()).c_str(), "wb");
		if (f == nullptr) {
			delete afile;
			throw FlycastException("Can't save files");
		}
		u8 buf[8_KB];
		while (true)
		{
			u32 len = afile->Read(buf, sizeof(buf));
			if (len < 0)
			{
				fclose(f);
				delete afile;
				throw FlycastException("Can't read zip");
			}
			if (len == 0)
				break;
			if (fwrite(buf, 1, len, f) != len)
			{
				fclose(f);
				delete afile;
				throw FlycastException("Can't save files");
			}
		}
		fclose(f);
		delete afile;
	}
}

#endif // __ANDROID__ && !LIBRETRO && arm64
