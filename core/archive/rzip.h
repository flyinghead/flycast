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
// Implementation of the RZIP stream format as defined by libretro
// https://github.com/libretro/libretro-common/blob/master/include/streams/rzip_stream.h

#pragma once
#include "types.h"

class RZipFile
{
public:
	~RZipFile() { Close(); }

	bool Open(const std::string& path, bool write);
	void Close();
	size_t Size() const { return size; }
	size_t Read(void *data, size_t length);
	size_t Write(const void *data, size_t length);
	FILE *rawFile() const { return file; }

private:
	FILE *file = nullptr;
	u64 size = 0;
	u32 maxChunkSize = 0;
	u8 *chunk = nullptr;
	u32 chunkSize = 0;
	u32 chunkIndex = 0;
};
