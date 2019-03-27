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

#ifndef CORE_ARCHIVE_ZIPARCHIVE_H_
#define CORE_ARCHIVE_ZIPARCHIVE_H_
#include "archive.h"
#include "deps/libzip/zip.h"

class ZipArchive : public Archive
{
public:
	ZipArchive() : zip(NULL) {}
	virtual ~ZipArchive();

	virtual ArchiveFile* OpenFile(const char* name) override;

private:
	virtual bool Open(const char* path) override;

	struct zip *zip;
};

class ZipArchiveFile : public ArchiveFile
{
public:
	ZipArchiveFile(struct zip_file *zip_file) : zip_file(zip_file) {}
	virtual ~ZipArchiveFile() { zip_fclose(zip_file); }
	virtual u32 Read(void* buffer, u32 length) override;

private:
	struct zip_file *zip_file;
};

#endif /* CORE_ARCHIVE_ZIPARCHIVE_H_ */
