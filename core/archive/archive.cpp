/*
    Created on: Nov 23, 2018

	Copyright 2018 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "archive.h"
#include "7zArchive.h"
#include "ZipArchive.h"
#include "oslib/storage.h"

Archive *OpenArchive(const std::string& path)
{
	FILE *file = nullptr;
	hostfs::FileInfo fileInfo;
	try {
		fileInfo = hostfs::storage().getFileInfo(path);
		if (!fileInfo.isDirectory)
			file = hostfs::storage().openFile(path, "rb");
	} catch (const hostfs::StorageException& e) {
	}
	if (file == nullptr)
	{
		file = hostfs::storage().openFile(path + ".7z", "rb");
		if (file == nullptr)
			file = hostfs::storage().openFile(path + ".7Z", "rb");
	}
	if (file != nullptr)
	{
		Archive *sz_archive = new SzArchive();
		if (sz_archive->Open(file))
			return sz_archive;
		delete sz_archive;
		file = nullptr;
	}
	// Retry as a zip file
	try {
		if (!fileInfo.isDirectory)
			file = hostfs::storage().openFile(path, "rb");
	} catch (const hostfs::StorageException& e) {
	}
	if (file == nullptr)
	{
		file = hostfs::storage().openFile(path + ".zip", "rb");
		if (file == nullptr)
		{
			file = hostfs::storage().openFile(path + ".ZIP", "rb");
			if (file == nullptr)
				return nullptr;
		}
	}
	Archive *zip_archive = new ZipArchive();
	if (zip_archive->Open(file))
		return zip_archive;
	delete zip_archive;

	return nullptr;
}

bool Archive::Open(const char* path)
{
	FILE *file = nowide::fopen(path, "rb");
	if (file == nullptr)
		return false;
	return Open(file);
}

