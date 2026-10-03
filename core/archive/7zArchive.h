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
#include "lzma/7z.h"

#include <algorithm>
#include <cstring>

class SzArchive : public Archive
{
public:
	SzArchive() {
		SzArEx_Init(&szarchive);
	}
	~SzArchive() override;

	// Takes ownership of file, including when opening fails.
	bool Open(hostfs::File *file) override;
	ArchiveFile* OpenFile(const char* name) override;
	ArchiveFile *OpenFileByCrc(u32 crc) override;
	// Returned files share the extraction buffer and must be consumed before opening another file.
	ArchiveFile *OpenFileByIndex(size_t index);
	size_t GetFileCount() const { return szarchive.NumFiles; }
	std::string GetFileName(size_t index) const;
	bool IsDirectory(size_t index) const;
	u64 GetFileSize(size_t index) const;

private:
	void Close();

	struct ArchiveStream
	{
		static SRes Read(const ISeekInStream *p, void *buf, size_t *size);
		static SRes Seek(const ISeekInStream *p, Int64 *pos, ESzSeek origin);

		ISeekInStream vt;
		hostfs::File *file;
	};

	CSzArEx szarchive;
	UInt32 block_idx = 0;
	Byte *out_buffer = nullptr;
	size_t out_buffer_size = 0;
	ArchiveStream archiveStream {};
	CLookToRead2 lookStream {};

};

class SzArchiveFile : public ArchiveFile
{
public:
	SzArchiveFile(u8 *data, size_t offset, size_t length)
		: data(data), offset(offset), _length(length) {}
	u32 Read(void *buffer, u32 length) override
	{
		length = std::min<size_t>(length, _length - position);
		if (length != 0)
			memcpy(buffer, data + offset + position, length);
		position += length;
		return length;
	}

	size_t length() override {
		return _length;
	}

private:
	u8 *data;
	size_t offset;
	size_t _length;
	size_t position = 0;
};
