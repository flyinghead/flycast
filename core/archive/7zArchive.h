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

#include "archive.h"
#include "deps/lzma/7z.h"
#include "deps/lzma/7zFile.h"

#include <algorithm>
#include <cstring>

class SzArchive : public Archive
{
public:
	SzArchive() : out_buffer(NULL)
	{
		memset(&archiveStream, 0, sizeof(archiveStream));
		memset(&lookStream, 0, sizeof(lookStream));
	}
	~SzArchive() override;

	ArchiveFile* OpenFile(const char* name) override;
	ArchiveFile* OpenFileByCrc(u32 crc) override;

protected:
	bool Open(FILE* file) override;

private:
	CSzArEx szarchive;
	UInt32 block_idx;		/* it can have any value before first call (if outBuffer = 0) */
	Byte* out_buffer;		/* it must be 0 before first call for each new archive. */
	size_t out_buffer_size; /* it can have any value before first call (if outBuffer = 0) */
	CFileInStream archiveStream;
	CLookToRead2 lookStream;
};

class SzArchiveFile : public ArchiveFile
{
public:
	SzArchiveFile(u8* data, u32 offset, u32 length)
		: data(data), offset(offset), _length(length) {}
	u32 Read(void* buffer, u32 length) override
	{
		length = std::min(length, this->_length);
		memcpy(buffer, data + offset, length);
		return length;
	}

	size_t length() override
	{
		return _length;
	}

private:
	u8* data;
	u32 offset;
	u32 _length;
};
