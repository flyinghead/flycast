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
#include "ZipArchive.h"

ZipArchive::~ZipArchive()
{
	zip_close(zip);
}

bool ZipArchive::Open(FILE *file)
{
	zip_error_t error;
	zip_source_t *source = zip_source_filep_create(file, 0, -1, &error);
	if (source == nullptr)
	{
		std::fclose(file);
		return false;
	}
	zip = zip_open_from_source(source, 0, nullptr);
	if (zip == nullptr)
		zip_source_free(source);
	return zip != nullptr;
}

ArchiveFile* ZipArchive::OpenFile(const char* name)
{
	zip_file_t *zip_file = zip_fopen(zip, name, 0);
	if (zip_file == nullptr)
		return nullptr;
	zip_stat_t stat;
	zip_stat(zip, name, 0, &stat);
	return new ZipArchiveFile(zip_file, stat.size, stat.name);
}

static zip_file *zip_fopen_by_crc(zip_t *za, u32 crc, int flags, zip_uint64_t& index)
{
	if (crc == 0)
		return nullptr;

	zip_int64_t n = zip_get_num_entries(za, 0);
	for (index = 0; index < (zip_uint64_t)n; index++)
	{
		zip_stat_t stat;
		if (zip_stat_index(za, index, flags, &stat) < -1)
			return nullptr;
		if (stat.crc == crc)
			return zip_fopen_index(za, index, flags);
	}

	return nullptr;
}

ArchiveFile* ZipArchive::OpenFileByCrc(u32 crc)
{
	zip_uint64_t index;
	zip_file_t *zip_file = zip_fopen_by_crc(zip, crc, 0, index);
	if (zip_file == nullptr)
		return nullptr;
	zip_stat_t stat;
	zip_stat_index(zip, index, 0, &stat);

	return new ZipArchiveFile(zip_file, stat.size, stat.name);
}

u32 ZipArchiveFile::Read(void* buffer, u32 length)
{
	return zip_fread(zip_file, buffer, length);
}

bool ZipArchive::Open(const void *data, size_t size)
{
	zip_error_t error;
	zip_source_t *source = zip_source_buffer_create(data, size, 0, &error);
	if (source == nullptr)
		return false;
	zip = zip_open_from_source(source, 0, nullptr);
	if (zip == nullptr)
		zip_source_free(source);
	return zip != nullptr;
}

ArchiveFile *ZipArchive::OpenFirstFile()
{
	zip_file_t *zipFile = zip_fopen_index(zip, 0, 0);
	if (zipFile == nullptr)
		return nullptr;
	zip_stat_t stat;
	zip_stat_index(zip, 0, 0, &stat);
	return new ZipArchiveFile(zipFile, stat.size, stat.name);
}

ArchiveFile *ZipArchive::OpenFileByIndex(size_t index)
{
	zip_file_t *zipFile = zip_fopen_index(zip, index, 0);
	if (zipFile == nullptr)
		return nullptr;
	zip_stat_t stat;
	zip_stat_index(zip, index, 0, &stat);
	return new ZipArchiveFile(zipFile, stat.size, stat.name);
}
