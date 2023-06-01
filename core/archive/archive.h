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
#pragma once

#include "types.h"

class ArchiveFile
{
public:
	virtual ~ArchiveFile() = default;
	virtual u32 Read(void *buffer, u32 length) = 0;
};

class Archive
{
public:
	virtual ~Archive() = default;
	virtual ArchiveFile *OpenFile(const char *name) = 0;
	virtual ArchiveFile *OpenFileByCrc(u32 crc) = 0;

protected:
	virtual bool Open(FILE *file) = 0;

private:
	bool Open(const char *name);

	friend Archive *OpenArchive(const std::string& path);
};

Archive *OpenArchive(const std::string& path);
