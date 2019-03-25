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

#ifndef CORE_ARCHIVE_7ZARCHIVE_H_
#define CORE_ARCHIVE_7ZARCHIVE_H_

#include "archive.h"
#include "deps/lzma/7z.h"
#include "deps/lzma/7zFile.h"

class SzArchive : public Archive
{
public:
	SzArchive() : out_buffer(NULL) {
		memset(&archiveStream, 0, sizeof(archiveStream));
		memset(&lookStream, 0, sizeof(lookStream));
	}
	virtual ~SzArchive();

	virtual ArchiveFile* OpenFile(const char* name) override;

private:
	virtual bool Open(const char* path) override;

	CSzArEx szarchive;
	UInt32 block_idx;				/* it can have any value before first call (if outBuffer = 0) */
	Byte *out_buffer;				/* it must be 0 before first call for each new archive. */
	size_t out_buffer_size;			/* it can have any value before first call (if outBuffer = 0) */
	CFileInStream archiveStream;
	CLookToRead2 lookStream;

};

class SzArchiveFile : public ArchiveFile
{
public:
	SzArchiveFile(u8 *data, u32 offset, u32 length) : data(data), offset(offset), length(length) {}
	virtual u32 Read(void *buffer, u32 length) override
	{
		length = min(length, this->length);
		memcpy(buffer, data + offset, length);
		return length;
	}

private:
	u8 *data;
	u32 offset;
	u32 length;
};

#endif /* CORE_ARCHIVE_7ZARCHIVE_H_ */
