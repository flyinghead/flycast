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

#include <cstring>

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

void IsoFs::Directory::reset()
{
	index = 0;
	if (data.empty() && len != 0)
	{
		data.resize(len);
		fs->disc->ReadSectors(startFad, len / 2048, data.data(), 2048);
	}
}

IsoFs::Entry *IsoFs::Directory::nextEntry()
{
	if (index >= data.size())
		return nullptr;
	const iso9660_dir_t *dir = (const iso9660_dir_t *)&data[index];
	if (dir->length == 0)
	{
		if ((index & 2047) == 0)
			return nullptr;
		index = ((index + 2047) / 2048) * 2048;
		if (index >= data.size())
			return nullptr;
		dir = (const iso9660_dir_t *)&data[index];
		if (dir->length == 0)
			return nullptr;
	}
	std::string name(dir->filename.str + 1, dir->filename.str[0]);

	u32 startFad = decode_iso733(dir->extent) + 150;
	u32 len = decode_iso733(dir->size);
	Entry *entry;
	if ((dir->file_flags & ISO_DIRECTORY) == 0)
	{
		File *file = new File(fs);
		entry = file;
	}
	else
	{
		Directory *directory = new Directory(fs);
		len = ((len + 2047) / 2048) * 2048;
		entry = directory;
	}
	entry->startFad = startFad;
	entry->len = len;
	entry->name = name;
	index += dir->length;

	return entry;
}

IsoFs::Entry *IsoFs::Directory::getEntry(const std::string& name)
{
	std::string isoname = name + ';';
	reset();
	while (true)
	{
		Entry *entry = nextEntry();
		if (entry == nullptr)
			return nullptr;
		if (entry->getName().substr(0, isoname.size()) == isoname)
			return entry;
		delete entry;
	}
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

std::vector<IsoFs::Entry*> IsoFs::Directory::list()
{
	std::vector<IsoFs::Entry*> v;
	reset();
	while (true)
	{
		Entry *entry = nextEntry();
		if (entry == nullptr)
			break;
		v.push_back(entry);
	}
	return v;
}
