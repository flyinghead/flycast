/*
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

#include <file/file_path.h>

struct retro_vfs_interface* hostfs::LibretroStorage::vfs_interface = nullptr;

bool hostfs::LibretroStorage::isFileWriteable(const std::string& path)
{
	// Not ideal; would be nicer if the VFS API gave us a direct way of checking this.
	RFILE* file = filestream_open(path.c_str(), RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING, RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (file == nullptr)
		return false;

	filestream_close(file);
	return true;
}

hostfs::FileInfo hostfs::LibretroStorage::makeFileInfo(std::string name, std::string path)
{
	const bool is_directory = path_is_directory(path.c_str());
	const size_t size = path_get_size(path.c_str());
	const bool is_writeable = isFileWriteable(path);

	return {std::move(name), std::move(path), is_directory, size, is_writeable};
}

void hostfs::LibretroStorage::initialise(retro_environment_t environ_cb)
{
	retro_vfs_interface_info vfs_interface_info;
	vfs_interface_info.required_interface_version = 3; // We really want those directory functions.

	if (!environ_cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_interface_info))
		return;

	vfs_interface = vfs_interface_info.iface;

	filestream_vfs_init(&vfs_interface_info);
	path_vfs_init(&vfs_interface_info);
}

bool hostfs::LibretroStorage::isKnownPath(const std::string& path)
{
	return filestream_exists(path.c_str());
}

std::vector<hostfs::FileInfo> hostfs::LibretroStorage::listContent(const std::string& path)
{
	std::vector<FileInfo> list;

	struct retro_vfs_dir_handle* dir = vfs_interface->opendir(path.c_str(), true);

	if (dir == nullptr)
		return list;

	while (vfs_interface->readdir(dir))
	{
		const auto entry_name = vfs_interface->dirent_get_name(dir);
		const auto entry_path = path + PATH_DEFAULT_SLASH_C() + entry_name;

		list.emplace_back(makeFileInfo(entry_name, entry_path));
	}

	vfs_interface->closedir(dir);

	return list;
}

hostfs::File *hostfs::LibretroStorage::openFile(const std::string& path, const std::string& mode)
{
	bool extended = false;
	bool append = false;
	unsigned int mode_bitfield = 0;

	for (char character : mode)
	{
		switch (character)
		{
			case 'R':
			case 'r':
				if (mode_bitfield != 0)
					return nullptr;

				mode_bitfield = RETRO_VFS_FILE_ACCESS_READ;
				break;

			case 'W':
			case 'w':
				if (mode_bitfield != 0)
					return nullptr;

				mode_bitfield = RETRO_VFS_FILE_ACCESS_WRITE;
				break;

			case 'A':
			case 'a':
				if (mode_bitfield != 0)
					return nullptr;

				mode_bitfield = RETRO_VFS_FILE_ACCESS_WRITE | RETRO_VFS_FILE_ACCESS_UPDATE_EXISTING;
				append = true;
				break;

			case '+':
				extended = true;
				break;

			case 'B':
			case 'b':
			case 'T':
			case 't':
				// Binary mode unfortunately just gets assumed by default...
				// TODO: Do any of Flycast's text parsers require text mode?
				break;

			default:
				// Unrecognised character.
				return nullptr;
		}
	}

	if (mode_bitfield == 0)
		return nullptr;

	if (extended)
		mode_bitfield = RETRO_VFS_FILE_ACCESS_READ_WRITE;

	RFILE *rfile = filestream_open(path.c_str(), mode_bitfield, RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (rfile == nullptr)
		return nullptr;

	auto file = new LibretroFile(rfile);

	if (append)
		file->seek(0, SEEK_END);

	return file;
}

std::string hostfs::LibretroStorage::getParentPath(const std::string& path)
{
	std::string parent_path = path;

	parent_path.resize(path_parent_dir(parent_path.data(), parent_path.size()));

	return parent_path;
}

std::string hostfs::LibretroStorage::getSubPath(const std::string& reference, const std::string& relative)
{
	return reference + PATH_DEFAULT_SLASH_C() + relative;
}

hostfs::FileInfo hostfs::LibretroStorage::getFileInfo(const std::string& path)
{
	return makeFileInfo(path_basename(path.c_str()), path);
}

bool hostfs::LibretroStorage::exists(const std::string& path)
{
	return filestream_exists(path.c_str());
}

bool hostfs::LibretroStorage::addStorage(bool isDirectory, bool writeAccess, const std::string& description,
		void (*callback)(bool cancelled, std::string selectedPath), const std::string& mimeType)
{
	// TODO: Should this do anything?
	return false;
}

hostfs::CustomStorage& hostfs::customStorage()
{
	static LibretroStorage storage;

	return storage;
}
