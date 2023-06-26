/*
	Copyright 2023 flyinghead

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
#include "storage.h"
#include "directory.h"
#include "stdclass.h"

// For macOS
std::string os_PrecomposedString(std::string string);

namespace hostfs
{

CustomStorage& customStorage();

#if !defined(__ANDROID__) || defined(LIBRETRO)
CustomStorage& customStorage()
{
	class NullStorage : public CustomStorage
	{
	public:
		bool isKnownPath(const std::string& path) override { return false; }
		std::vector<FileInfo> listContent(const std::string& path) override { return std::vector<FileInfo>(); }
		FILE *openFile(const std::string& path, const std::string& mode) override { die("Not implemented"); }
		std::string getParentPath(const std::string& path) override { die("Not implemented"); }
		std::string getSubPath(const std::string& reference, const std::string& relative) override { die("Not implemented"); }
		FileInfo getFileInfo(const std::string& path) override { die("Not implemented"); }
		void addStorage(bool isDirectory, bool writeAccess, void (*callback)(bool cancelled, std::string selectedPath)) override {
			die("Not implemented");
		}
	};
	static NullStorage storage;

	return storage;
}
#endif

#ifdef _WIN32
static const std::string separators = "/\\";
static const std::string native_separator = "\\";
#else
static const std::string separators = "/";
static const std::string native_separator = "/";
#endif

class StdStorage : public Storage
{
public:
	bool isKnownPath(const std::string& path) override {
		// assume it's known since standard storage should be asked last
		return true;
	}

	std::vector<FileInfo> listContent(const std::string& path) override
	{
		if (path.empty())
			return listRoots();

		DIR *dir = flycast::opendir(path.c_str());
		if (dir == nullptr) {
			WARN_LOG(COMMON, "Cannot read directory '%s' errno 0x%x", path.c_str(), errno);
			throw StorageException("Can't read directory " + path);
		}
		std::vector<FileInfo> entries;
		while (true)
		{
			dirent *direntry = flycast::readdir(dir);
			if (direntry == nullptr)
				break;

			FileInfo entry;
			entry.name = direntry->d_name;
#ifdef __APPLE__
			entry.name = os_PrecomposedString(entry.name);
#endif
			if (entry.name == "." || entry.name == "..")
				continue;
			if (path.find_last_of(separators) != path.size() - 1)
				entry.path = path + native_separator + entry.name;
			else
				entry.path = path + entry.name;
			// Silently skip unreadable entries
			if (flycast::access(entry.path.c_str(), R_OK) != 0)
				continue;

			bool isDir = false;
#ifndef _WIN32
			if (direntry->d_type == DT_DIR)
				isDir = true;
			else if (direntry->d_type == DT_UNKNOWN || direntry->d_type == DT_LNK)
			{
				struct stat st;
				if (flycast::stat(entry.path.c_str(), &st) != 0)
				{
					WARN_LOG(COMMON, "Cannot stat file '%s' errno 0x%x", entry.path.c_str(), errno);
					continue;
				}
				if (S_ISDIR(st.st_mode))
					isDir = true;
			}
#else // _WIN32
			nowide::wstackstring wname;
			if (wname.convert(entry.path.c_str()))
			{
				DWORD attr = GetFileAttributesW(wname.get());
				if (attr != INVALID_FILE_ATTRIBUTES)
				{
					if (attr & FILE_ATTRIBUTE_HIDDEN)
						continue;
					if (attr & FILE_ATTRIBUTE_DIRECTORY)
						isDir = true;
				}
			}
#endif
			entry.isDirectory = isDir;
			entries.emplace_back(entry);
		}
		flycast::closedir(dir);

		return entries;
	}

	FILE *openFile(const std::string& path, const std::string& mode) override
	{
		return nowide::fopen(path.c_str(), mode.c_str());
	}

	std::string getParentPath(const std::string& path) override
	{
		auto lastSlash = path.find_last_of(separators);
#ifdef _WIN32
		if (path.size() == 2 && std::isalpha(static_cast<u8>(path[0])) && path[1] == ':')
			return "";
		if (lastSlash == 2 && path.size() == 3 && std::isalpha(static_cast<u8>(path[0])) && path[1] == ':')
			return "";
#else
		if (lastSlash == 0)
			return "/";
#endif
		if (lastSlash == std::string::npos)
			return "." + native_separator;
		std::string parentPath = path.substr(0, lastSlash);
#ifdef _WIN32
		if (parentPath.size() == 2 && std::isalpha(static_cast<u8>(parentPath[0])) && parentPath[1] == ':')
			parentPath += '\\';
#endif
		if (flycast::access(parentPath.c_str(), R_OK) != 0)
			return "";
		else
			return parentPath;
	}

	std::string getSubPath(const std::string& reference, const std::string& subpath) override
	{
		// FIXME this is incorrect if reference isn't a directory
		return reference + native_separator + subpath;
	}

	FileInfo getFileInfo(const std::string& path) override
	{
		FileInfo info;
		info.path = path;
		size_t slash = get_last_slash_pos(path);
		if (slash != std::string::npos && slash < path.size() - 1)
			info.name = path.substr(slash + 1);
		else
			info.name = path;
		info.isWritable = flycast::access(path.c_str(), W_OK) == 0;
#ifndef _WIN32
		struct stat st;
		if (flycast::stat(path.c_str(), &st) != 0) {
			INFO_LOG(COMMON, "Cannot stat file '%s' errno %d", path.c_str(), errno);
			throw StorageException("Cannot stat " + path);
		}
		info.isDirectory = S_ISDIR(st.st_mode);
		info.size = st.st_size;
#else // _WIN32
		nowide::wstackstring wname;
		if (wname.convert(path.c_str()))
		{
			WIN32_FILE_ATTRIBUTE_DATA fileAttribs;
			if (GetFileAttributesExW(wname.get(), GetFileExInfoStandard, &fileAttribs))
			{
				info.isDirectory = (fileAttribs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
				info.size = fileAttribs.nFileSizeLow + ((u64)fileAttribs.nFileSizeHigh << 32);
			}
			else
			{
				INFO_LOG(COMMON, "Cannot get attrbutes of '%s' error 0x%x", path.c_str(), GetLastError());
				throw StorageException("Cannot get attributes of " + path);
			}
		}
		else
		{
			throw StorageException("Invalid file name " + path);
		}
#endif
		return info;
	}

private:
	std::vector<FileInfo> listRoots()
	{
#ifdef _WIN32
		std::vector<FileInfo> driveList;
		// List all the drives
		u32 drives = GetLogicalDrives();
		for (int i = 0; i < 32; i++)
			if ((drives & (1 << i)) != 0)
			{
				std::string name = std::string(1, (char)('A' + i)) + ":";
				driveList.emplace_back(name, name + "\\", true);
			}
#ifdef TARGET_UWP
		// Add the home directory to the list of drives as it's not accessible from the root
		std::string home;
		const char *home_drive = nowide::getenv("HOMEDRIVE");
		if (home_drive != nullptr)
			home = home_drive;
		home += nowide::getenv("HOMEPATH");
		driveList.emplace_back(home, home, true);
#endif
		return driveList;

#elif defined(__ANDROID__)
		std::vector<FileInfo> roots;
		const char *home = nowide::getenv("FLYCAST_HOME");
		while (home != nullptr)
		{
			const char *pcolon = strchr(home, ':');
			if (pcolon != nullptr)
			{
				roots.emplace_back(std::string(home, pcolon - home), std::string(home, pcolon - home), true);
				home = pcolon + 1;
			}
			else
			{
				roots.emplace_back(home, home, true);
				home = nullptr;
			}
		}

		return roots;

#else

		return listContent("/");
#endif
	}
};

static StdStorage stdStorage;

std::vector<FileInfo> AllStorage::listContent(const std::string& path)
{
	if (path.empty())
	{
		std::vector<FileInfo> entries = stdStorage.listContent(path);
		std::vector<FileInfo> customEntries = customStorage().listContent(path);
		entries.insert(entries.end(), customEntries.begin(), customEntries.end());

		return entries;
	}
	if (customStorage().isKnownPath(path))
		return customStorage().listContent(path);
	else
		return stdStorage.listContent(path);
}

FILE *AllStorage::openFile(const std::string& path, const std::string& mode)
{
	if (customStorage().isKnownPath(path))
		return customStorage().openFile(path, mode);
	else
		return stdStorage.openFile(path, mode);
}

std::string AllStorage::getParentPath(const std::string& path)
{
	if (customStorage().isKnownPath(path))
		return customStorage().getParentPath(path);
	else
		return stdStorage.getParentPath(path);
}

std::string AllStorage::getSubPath(const std::string& reference, const std::string& subpath)
{
	if (customStorage().isKnownPath(reference))
		return customStorage().getSubPath(reference, subpath);
	else
		return stdStorage.getSubPath(reference, subpath);
}

FileInfo AllStorage::getFileInfo(const std::string& path)
{
	if (customStorage().isKnownPath(path))
		return customStorage().getFileInfo(path);
	else
		return stdStorage.getFileInfo(path);
}

std::string AllStorage::getDefaultDirectory()
{
	std::string directory;
#if defined(__ANDROID__)
	const char *home = nowide::getenv("FLYCAST_HOME");
	if (home != NULL)
	{
		const char *pcolon = strchr(home, ':');
		if (pcolon != NULL)
			directory = std::string(home, pcolon - home);
		else
			directory = home;
	}
#elif defined(__unix__) || defined(__APPLE__)
	const char *home = nowide::getenv("HOME");
	if (home != NULL)
		directory = home;
#elif defined(_WIN32)
	if (nowide::getenv("HOMEPATH") != NULL)
	{
		const char *home_drive = nowide::getenv("HOMEDRIVE");
		if (home_drive != NULL)
			directory = home_drive;
		directory += nowide::getenv("HOMEPATH");
	}
#elif defined(__SWITCH__)
	directory = "/";
#endif
	if (directory.empty())
	{
		directory = get_writable_config_path("");
		if (directory.empty())
			directory = ".";
	}
	return directory;
}

AllStorage& storage()
{
	static AllStorage storage;

	return storage;
}

void addStorage(bool isDirectory, bool writeAccess, void (*callback)(bool cancelled, std::string selectedPath))
{
	customStorage().addStorage(isDirectory, writeAccess, callback);
}

}
