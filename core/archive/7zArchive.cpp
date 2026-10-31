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
#include "lzma/7z.h"
#include "lzma/7zCrc.h"
#include "lzma/Alloc.h"
#include "nowide/utf/convert.hpp"

#include <mutex>
#include <vector>

#define kInputBufSize ((size_t)1 << 18)

static std::once_flag crc_tables_generated;

SRes SzArchive::ArchiveStream::Read(const ISeekInStream *p, void *buf, size_t *size)
{
	auto file_archive = CONTAINER_FROM_VTBL(p, ArchiveStream, vt);
	*size = file_archive->file->read(buf, 1, *size);
	return file_archive->file->error() ? SZ_ERROR_READ : SZ_OK;
}

SRes SzArchive::ArchiveStream::Seek(const ISeekInStream *p, Int64 *pos, ESzSeek origin)
{
	auto file_archive = CONTAINER_FROM_VTBL(p, ArchiveStream, vt);
	if (file_archive->file->seek(*pos, origin) == 0) {
		*pos = file_archive->file->tell();
		return SZ_OK;
	}
	else {
		return SZ_ERROR_FAIL;
	}
}

bool SzArchive::Open(hostfs::File *file)
{
	Close();
	if (file == nullptr)
		return false;
	archiveStream.vt.Read = ArchiveStream::Read;
	archiveStream.vt.Seek = ArchiveStream::Seek;
	archiveStream.file = file;

	LookToRead2_CreateVTable(&lookStream, 0);
	lookStream.buf = (Byte *)ISzAlloc_Alloc(&g_Alloc, kInputBufSize);
	if (lookStream.buf == nullptr)
	{
		Close();
		return false;
	}
	lookStream.bufSize = kInputBufSize;
	lookStream.realStream = &archiveStream.vt;
	LookToRead2_Init(&lookStream);

	std::call_once(crc_tables_generated, CrcGenerateTable);
	SRes res = SzArEx_Open(&szarchive, &lookStream.vt, &g_Alloc, &g_Alloc);
	if (res != SZ_OK)
		Close();

	return (res == SZ_OK);
}

std::string SzArchive::GetFileName(size_t index) const
{
	if (index >= szarchive.NumFiles)
		return {};
	size_t len = SzArEx_GetFileNameUtf16(&szarchive, index, nullptr);
	if (len == 0)
		return {};
	std::vector<UInt16> name(len);
	SzArEx_GetFileNameUtf16(&szarchive, index, name.data());
	return nowide::utf::convert_string<char>(name.data(), name.data() + len - 1);
}

bool SzArchive::IsDirectory(size_t index) const
{
	return index < szarchive.NumFiles && SzArEx_IsDir(&szarchive, index);
}

u64 SzArchive::GetFileSize(size_t index) const
{
	return index < szarchive.NumFiles ? SzArEx_GetFileSize(&szarchive, index) : 0;
}

ArchiveFile* SzArchive::OpenFileByIndex(size_t index)
{
	if (index >= szarchive.NumFiles || IsDirectory(index))
		return nullptr;

	size_t offset = 0;
	size_t out_size_processed = 0;
	SRes res = SzArEx_Extract(&szarchive, &lookStream.vt, static_cast<UInt32>(index), &block_idx, &out_buffer, &out_buffer_size, &offset, &out_size_processed, &g_Alloc, &g_Alloc);
	if (res != SZ_OK)
		return nullptr;

	return new SzArchiveFile(out_buffer, offset, out_size_processed);
}

ArchiveFile* SzArchive::OpenFile(const char* name)
{
	for (size_t i = 0; i < GetFileCount(); i++)
		if (!IsDirectory(i) && GetFileName(i) == name)
			return OpenFileByIndex(i);
	return nullptr;
}

ArchiveFile* SzArchive::OpenFileByCrc(u32 crc)
{
	if (crc == 0)
		return nullptr;
	for (size_t i = 0; i < GetFileCount(); i++)
	{
		if (IsDirectory(i) || !SzBitWithVals_Check(&szarchive.CRCs, i))
			continue;

		if (crc != szarchive.CRCs.Vals[i])
			continue;

		return OpenFileByIndex(i);
	}
	return nullptr;
}

SzArchive::~SzArchive()
{
	Close();
}

void SzArchive::Close()
{
	delete archiveStream.file;
	archiveStream.file = nullptr;
	ISzAlloc_Free(&g_Alloc, lookStream.buf);
	lookStream.buf = nullptr;
	ISzAlloc_Free(&g_Alloc, out_buffer);
	out_buffer = nullptr;
	out_buffer_size = 0;
	block_idx = 0;
	SzArEx_Free(&szarchive, &g_Alloc);
}
