/*
	Copyright 2022 flyinghead

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
#include "isofs.h"
#include "iso9660.h"

static u32 decode_iso733(iso733_t v)
{
	return ((v >> 56) & 0x000000FF)
			| ((v >> 40) & 0x0000FF00)
			| ((v >> 24) & 0x00FF0000)
			| ((v >> 8) & 0xFF000000);
}

IsoFs::IsoFs(Disc *disc) : disc(disc)
{
	baseFad = disc->GetBaseFAD();
}

IsoFs::Directory *IsoFs::getRoot()
{
	u8 temp[2048];
	disc->ReadSectors(baseFad + 16, 1, temp, 2048);
	// Primary Volume Descriptor
	const iso9660_pvd_t *pvd = (const iso9660_pvd_t *)temp;

	Directory *root = new Directory(this);
	if (pvd->type == 1 && !memcmp(pvd->id, ISO_STANDARD_ID, strlen(ISO_STANDARD_ID)) && pvd->version == 1)
	{
		u32 lba = decode_iso733(pvd->root_directory_record.extent);
		u32 len = decode_iso733(pvd->root_directory_record.size);

		len = ((len + 2047) / 2048) * 2048;
		root->data.resize(len);

		DEBUG_LOG(GDROM, "iso9660 root directory FAD: %d, len: %d", 150 + lba, len);
		disc->ReadSectors(150 + lba, len / 2048, root->data.data(), 2048);
	}
	else {
		WARN_LOG(GDROM, "iso9660 PVD NOT found");
		root->data.resize(1);
		root->data[0] = 0;
	}
	return root;
}

IsoFs::Entry *IsoFs::Directory::getEntry(const std::string& name)
{
	std::string isoname = name + ';';
	for (u32 i = 0; i < data.size(); )
	{
		const iso9660_dir_t *dir = (const iso9660_dir_t *)&data[i];
		if (dir->length == 0)
			break;

		if ((u8)dir->filename.str[0] > isoname.size()
				&& memcmp(dir->filename.str + 1, isoname.c_str(), isoname.size()) == 0)
		{
			DEBUG_LOG(GDROM, "Found %s at offset %X", name.c_str(), i);
			u32 startFad = decode_iso733(dir->extent) + 150;
			u32 len = decode_iso733(dir->size);
			if ((dir->file_flags & ISO_DIRECTORY) == 0)
			{
				File *file = new File(fs);
				file->startFad = startFad;
				file->len = len;

				return file;
			}
			else
			{
				Directory *directory = new Directory(fs);
				directory->data.resize(len);
				fs->disc->ReadSectors(startFad, len / 2048, directory->data.data(), 2048);

				return directory;
			}
		}
		i += dir->length;
	}
	return nullptr;
}

u32 IsoFs::File::read(u8 *buf, u32 size, u32 offset) const
{
	size = std::min(size, len - offset);
	u32 sectors = size / 2048;
	fs->disc->ReadSectors(startFad + offset / 2048, sectors, buf, 2048);
	size -= sectors * 2048;
	if (size > 0)
	{
		u8 temp[2048];
		fs->disc->ReadSectors(startFad + offset / 2048 + sectors, 1, temp, 2048);
		memcpy(buf + sectors * 2048, temp, size);
	}
	return sectors * 2048 + size;
}
