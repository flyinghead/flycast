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
#pragma once

#include <streams/file_stream.h>
#include "../../core/oslib/storage.h"

namespace hostfs {

class LibretroFile : public File
{
protected:
	RFILE* const file;

public:
	LibretroFile(RFILE* file)
		: file(file)
	{}

	~LibretroFile() override
	{
		filestream_close(file);
	}

	size_t read(void* buffer, size_t size, size_t count) override
	{
		if (size == 0 || count == 0)
			return 0;

		return filestream_read(file, buffer, size * count) / size;
	}

	size_t write(const void* buffer, size_t size, size_t count) override
	{
		if (size == 0 || count == 0)
			return 0;

		return filestream_write(file, buffer, size * count) / size;
	}

	s64 tell() override
	{
		return filestream_tell(file);
	}

	int seek(s64 offset, int whence) override
	{
		// TODO: This needs to be changed when libretro-common is updated!
		// See: https://github.com/libretro/RetroArch/pull/18607
		return filestream_seek(file, offset, whence) == -1;
	}

	char* gets(char* str, int count) override
	{
		return filestream_gets(file, str, count);
	}

	s64 size() override
	{
		return filestream_get_size(file);
	}

	int eof() override
	{
		return filestream_eof(file);
	}

	int error() override
	{
		return filestream_error(file);
	}
};

class LibretroStorage : public CustomStorage
{
protected:
	static struct retro_vfs_interface* vfs_interface;

	static bool isFileWriteable(const std::string& path);
	static FileInfo makeFileInfo(std::string name, std::string path);

public:
	static void initialise(retro_environment_t environ_cb);

	bool isKnownPath(const std::string& path) override;
	std::vector<FileInfo> listContent(const std::string& path) override;
	File *openFile(const std::string& path, const std::string& mode) override;
	std::string getParentPath(const std::string& path) override;
	std::string getSubPath(const std::string& reference, const std::string& relative) override;
	FileInfo getFileInfo(const std::string& path) override;
	bool exists(const std::string& path) override;
	bool addStorage(bool isDirectory, bool writeAccess, const std::string& description,
			void (*callback)(bool cancelled, std::string selectedPath), const std::string& mimeType) override;
};

}
