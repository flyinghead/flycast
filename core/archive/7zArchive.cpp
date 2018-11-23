/*
    Created on: Nov 22, 2018

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
#include "7zArchive.h"
#include "deps/lzma/7z.h"
#include "deps/lzma/7zCrc.h"
#include "deps/lzma/Alloc.h"

#define kInputBufSize ((size_t)1 << 18)

static bool crc_tables_generated;

bool SzArchive::Open(const char* path)
{
	SzArEx_Init(&szarchive);

	if (InFile_Open(&archiveStream.file, path))
		return false;
	FileInStream_CreateVTable(&archiveStream);
	LookToRead2_CreateVTable(&lookStream, false);
	lookStream.buf = (Byte *)ISzAlloc_Alloc(&g_Alloc, kInputBufSize);
	if (lookStream.buf == NULL)
		return false;
	lookStream.bufSize = kInputBufSize;
	lookStream.realStream = &archiveStream.vt;
	LookToRead2_Init(&lookStream);

	if (!crc_tables_generated)
	{
		CrcGenerateTable();
		crc_tables_generated = true;
	}
	SRes res = SzArEx_Open(&szarchive, &lookStream.vt, &g_Alloc, &g_Alloc);

	return (res == SZ_OK);
}

ArchiveFile* SzArchive::OpenFile(const char* name)
{
	u16 fname[512];
	for (int i = 0; i < szarchive.NumFiles; i++)
	{
		unsigned isDir = SzArEx_IsDir(&szarchive, i);
		if (isDir)
			continue;

		int len = SzArEx_GetFileNameUtf16(&szarchive, i, fname);
		char szname[512];
		int j = 0;
		for (; j < len && j < sizeof(szname) - 1; j++)
			szname[j] = fname[j];
		szname[j] = 0;
		if (strcmp(name, szname))
			continue;

		size_t offset = 0;
		size_t out_size_processed = 0;
		SRes res = SzArEx_Extract(&szarchive, &lookStream.vt, i, &block_idx, &out_buffer, &out_buffer_size, &offset, &out_size_processed, &g_Alloc, &g_Alloc);
		if (res != SZ_OK)
			return NULL;

		return new SzArchiveFile(out_buffer, offset, (u32)out_size_processed);
	}
	return NULL;
}

SzArchive::~SzArchive()
{
	if (lookStream.buf != NULL)
	{
		File_Close(&archiveStream.file);
		ISzAlloc_Free(&g_Alloc, lookStream.buf);
		if (out_buffer != NULL)
			ISzAlloc_Free(&g_Alloc, out_buffer);
		SzArEx_Free(&szarchive, &g_Alloc);
	}
}
