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

bool ZipArchive::Open(const char* path)
{
	zip = zip_open(path, 0, NULL);
	return (zip != NULL);
}

ArchiveFile* ZipArchive::OpenFile(const char* name)
{
	zip_file *zip_file = zip_fopen(zip, name, 0);
	if (zip_file == NULL)
		return NULL;

	return new ZipArchiveFile(zip_file);
}

u32 ZipArchiveFile::Read(void* buffer, u32 length)
{
	return zip_fread(zip_file, buffer, length);
}
