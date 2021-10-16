/*
	Copyright 2021 flyinghead

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
#include "flashrom.h"
#include "oslib/oslib.h"
#include "stdclass.h"

bool MemChip::Load(const std::string& file)
{
	FILE *f = nowide::fopen(file.c_str(), "rb");
	if (f)
	{
		bool rv = std::fread(data + write_protect_size, 1, size - write_protect_size, f) == size - write_protect_size;
		std::fclose(f);
		if (rv)
			this->load_filename = file;

		return rv;
	}
	return false;
}

void WritableChip::Save(const std::string& file)
{
	FILE *f = nowide::fopen(file.c_str(), "wb");
	if (f == nullptr)
	{
		ERROR_LOG(FLASHROM, "Cannot save flash/nvmem to file '%s'", file.c_str());
		return;
	}
	if (std::fwrite(data + write_protect_size, 1, size - write_protect_size, f) != size - write_protect_size)
		ERROR_LOG(FLASHROM, "Failed or truncated write to flash file '%s'", file.c_str());
	std::fclose(f);
}

bool MemChip::Load(const std::string &prefix, const std::string &names_ro, const std::string &title)
{
	std::string fullpath = hostfs::findFlash(prefix, names_ro);
	if (!fullpath.empty() && Load(fullpath))
	{
		INFO_LOG(FLASHROM, "Loaded %s as %s", fullpath.c_str(),
				title.c_str());
		return true;
	}
	return false;
}

void WritableChip::Save(const std::string &prefix, const std::string &name_ro, const std::string &title)
{
	std::string path = hostfs::getFlashSavePath(prefix, name_ro);
	Save(path);
	INFO_LOG(FLASHROM, "Saved %s as %s", path.c_str(), title.c_str());
}

void MemChip::digest(u8 md5Digest[16])
{
	MD5Sum().add(data + write_protect_size, size - write_protect_size)
			.getDigest(md5Digest);
}
