/*
	Copyright 2020 flyinghead

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
#include "rzip.h"
#include <zlib.h>

const u8 RZipHeader[8] = { '#', 'R', 'Z', 'I', 'P', 'v', 1, '#' };

bool RZipFile::Open(const std::string& path, bool write)
{
	verify(file == nullptr);

	file = fopen(path.c_str(), write ? "wb" : "rb");
	if (file == nullptr)
		return false;
	if (!write)
	{
		u8 header[sizeof(RZipHeader)];
		if (fread(header, sizeof(header), 1, file) != 1
			|| memcmp(header, RZipHeader, sizeof(header))
			|| fread(&maxChunkSize, sizeof(maxChunkSize), 1, file) != 1
			|| fread(&size, sizeof(size), 1, file) != 1)
		{
			Close();
			return false;
		}
		chunk = new u8[maxChunkSize];
		chunkIndex = 0;
		chunkSize = 0;
	}

	return true;
}

void RZipFile::Close()
{
	if (file != nullptr)
	{
		fclose(file);
		file = nullptr;
		if (chunk != nullptr)
		{
			delete [] chunk;
			chunk = nullptr;
		}
	}
}

size_t RZipFile::Read(void *data, size_t length)
{
	verify(file != nullptr);

	u8 *p = (u8 *)data;
	size_t rv = 0;
	while (rv < length)
	{
		if (chunkIndex == chunkSize)
		{
			chunkSize = 0;
			chunkIndex = 0;
			u32 zippedSize;
			if (fread(&zippedSize, sizeof(zippedSize), 1, file) != 1)
				break;
			u8 *zipped = new u8[zippedSize];
			if (fread(zipped, zippedSize, 1, file) != 1)
			{
				delete [] zipped;
				break;
			}
			uLongf tl = maxChunkSize;
			if (uncompress(chunk, &tl, zipped, zippedSize) != Z_OK)
			{
				delete [] zipped;
				break;
			}
			delete [] zipped;
			chunkSize = (u32)tl;
		}
		u32 l = std::min(chunkSize - chunkIndex, (u32)length);
		memcpy(p, chunk + chunkIndex, l);
		p += l;
		chunkIndex += l;
		rv += l;
	}

	return rv;
}

size_t RZipFile::Write(const void *data, size_t length)
{
	verify(file != nullptr);
	verify(ftell(file) == 0);

	maxChunkSize = 1024 * 1024;
	if (fwrite(RZipHeader, sizeof(RZipHeader), 1, file) != 1
		|| fwrite(&maxChunkSize, sizeof(maxChunkSize), 1, file) != 1
		|| fwrite(&length, sizeof(length), 1, file) != 1)
		return 0;
	const u8 *p = (const u8 *)data;
	u8 *zipped = new u8[maxChunkSize];
	size_t rv = 0;
	while (rv < length)
	{
		uLongf zippedSize = maxChunkSize;
		uLongf uncompressedSize = std::min(maxChunkSize, (u32)(length - rv));
		if (compress(zipped, &zippedSize, p, uncompressedSize) != Z_OK)
			break;
		u32 sz = (u32)zippedSize;
		if (fwrite(&sz, sizeof(sz), 1, file) != 1
			|| fwrite(zipped, zippedSize, 1, file) != 1)
			return 0;
		p += uncompressedSize;
		rv += uncompressedSize;
	}
	
	return rv;
}
