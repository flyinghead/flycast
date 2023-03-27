/*
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

void MemChip::Load(const u8 *data, size_t size)
{
	verify(size == this->size - write_protect_size);
	memcpy(this->data + write_protect_size, data, this->size - write_protect_size);
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

void DCFlashChip::Write(u32 addr,u32 val,u32 sz)
{
	if (sz != 1)
	{
		INFO_LOG(FLASHROM, "invalid access size %d addr %x", sz, addr);
		return;
	}

	addr &= mask;

	switch(state)
	{
	case FS_Normal:
		switch (val & 0xff)
		{
		case 0xf0:
		case 0xff:  // reset chip mode
			state = FS_Normal;
			break;
		case 0xaa:  // AMD ID select part 1
			if ((addr & 0xfff) == 0x555 || (addr & 0xfff) == 0xaaa)
				state = FS_ReadAMDID1;
			break;
		default:
			INFO_LOG(FLASHROM, "Unknown FlashWrite mode: %x", val);
			break;
		}
		break;

	case FS_ReadAMDID1:
		if ((addr & 0xffff) == 0x02aa && (val & 0xff) == 0x55)
			state = FS_ReadAMDID2;
		else if ((addr & 0xffff) == 0x2aaa && (val & 0xff) == 0x55)
			state = FS_ReadAMDID2;
		else if ((addr & 0xfff) == 0x555 && (val & 0xff) == 0x55)
			state = FS_ReadAMDID2;
		else
		{
			if (val != 0xf0)
				WARN_LOG(FLASHROM, "FlashRom: ReadAMDID1 unexpected write @ %x: %x", addr, val);
			state = FS_Normal;
		}
		break;

	case FS_ReadAMDID2:
		if ((addr & 0xffff) == 0x0555 && (val & 0xff) == 0x80)
			state = FS_EraseAMD1;
		else if ((addr & 0xffff) == 0x5555 && (val & 0xff) == 0x80)
			state = FS_EraseAMD1;
		else if ((addr & 0xfff) == 0xaaa && (val & 0xff) == 0x80)
			state = FS_EraseAMD1;
		else if ((addr & 0xffff) == 0x0555 && (val & 0xff) == 0xa0)
			state = FS_ByteProgram;
		else if ((addr & 0xffff) == 0x5555 && (val & 0xff) == 0xa0)
			state = FS_ByteProgram;
		else if ((addr & 0xfff) == 0xaaa && (val & 0xff) == 0xa0)
			state = FS_ByteProgram;
		else if ((addr & 0xffff) == 0x5555 && (val & 0xff) == 0x90)
			state = FS_SelectMode;
		else
		{
			if (val != 0xf0)
				WARN_LOG(FLASHROM, "FlashRom: ReadAMDID2 unexpected write @ %x: %x", addr, val);
			state = FS_Normal;
		}
		break;
	case FS_ByteProgram:
		if ((addr & 0x1e000) != 0x1a000 && addr >= write_protect_size)
			data[addr] &= val;
		state = FS_Normal;
		break;

	case FS_EraseAMD1:
		if ((addr & 0xfff) == 0x555 && (val & 0xff) == 0xaa)
			state = FS_EraseAMD2;
		else if ((addr & 0xfff) == 0xaaa && (val & 0xff) == 0xaa)
			state = FS_EraseAMD2;
		else
		{
			if (val != 0xf0)
				WARN_LOG(FLASHROM, "FlashRom: EraseAMD1 unexpected write @ %x: %x", addr, val);
			state = FS_Normal;
		}
		break;

	case FS_EraseAMD2:
		if ((addr & 0xffff) == 0x02aa && (val & 0xff) == 0x55)
			state = FS_EraseAMD3;
		else if ((addr & 0xffff) == 0x2aaa && (val & 0xff) == 0x55)
			state = FS_EraseAMD3;
		else if ((addr & 0xfff) == 0x555 && (val & 0xff) == 0x55)
			state = FS_EraseAMD3;
		else
		{
			if (val != 0xf0)
				WARN_LOG(FLASHROM, "FlashRom: EraseAMD2 unexpected write @ %x: %x", addr, val);
			state = FS_Normal;
		}
		break;

	case FS_EraseAMD3:
		if (((addr & 0xfff) == 0x555 && (val & 0xff) == 0x10)
			|| ((addr & 0xfff) == 0xaaa && (val & 0xff) == 0x10))
		{
			// chip erase
			INFO_LOG(FLASHROM, "Erasing Chip!");
			u8 save[0x2000];
			// this area is write-protected
			memcpy(save, data + 0x1a000, 0x2000);
			memset(data + write_protect_size, 0xff, size - write_protect_size);
			memcpy(data + 0x1a000, save, 0x2000);
		}
		else if ((val & 0xff) == 0x30)
		{
			// sector erase
			if (addr >= write_protect_size)
			{
				void *start;
				u32 len;
				switch (addr & ~0x1FFF)
				{
				case 0x00000:	// SA0
					start = &data[0];
					len = 0x10000;
					break;
				case 0x10000:	// SA1
					start = &data[0x10000];
					len = 0x8000;
					break;
				case 0x18000:	// SA2
					start = &data[0x18000];
					len = 0x2000;
					break;
				case 0x1a000:	// SA3
					start = nullptr;
					len = 0;
					break;
				case 0x1c000:	// SA4
					start = &data[0x1c000];
					len = 0x4000;
					break;
				default:
					start = nullptr;
					len = 0;
					break;
				}
				INFO_LOG(FLASHROM, "Erase Sector %08X!", addr);
				if (start != nullptr)
					memset(start, 0xFF, len);
			}
		}
		else if (val != 0xf0)
			WARN_LOG(FLASHROM, "FlashRom: EraseAMD3 unexpected write @ %x: %x", addr, val);
		state = FS_Normal;
		break;
	default:
		WARN_LOG(FLASHROM, "FlashRom: invalid state. write @ %x: %x", addr, val);
		state = FS_Normal;
		break;
	}
}

int DCFlashChip::WriteBlock(u32 part_id, u32 block_id, const void *data)
{
	int offset, size;
	GetPartitionInfo(part_id, &offset, &size);

	if (!validate_header(offset, part_id))
		return 0;

	// the real system libraries allocate and write to a new physical block each
	// time a logical block is updated. the reason being that, flash memory can
	// only be programmed once, and after that the entire sector must be reset in
	// order to reprogram it. flash storage has a finite number of these erase
	// operations before its integrity deteriorates, so the libraries try to
	// minimize how often they occur by writing to a new physical block until the
	// partition is completely full
	//
	// this limitation of the original hardware isn't a problem for us, so try and
	// just update an existing logical block if it exists
	int phys_id = lookup_block(offset, size, block_id);
	if (!phys_id) {
		phys_id = alloc_block(offset, size);

		if (!phys_id)
			return 0;
	}

	// update the block's crc before writing it back out
	struct flash_user_block user;
	memcpy(&user, data, sizeof(user));
	user.block_id = block_id;
	user.crc = crc_block(&user);

	write_physical_block(offset, phys_id, &user);

	return 1;
}

int DCFlashChip::ReadBlock(u32 part_id, u32 block_id, void *data)
{
	int offset, size;
	GetPartitionInfo(part_id, &offset, &size);

	if (!validate_header(offset, part_id))
		return 0;

	int phys_id = lookup_block(offset, size, block_id);
	if (!phys_id)
		return 0;

	read_physical_block(offset, phys_id, data);

	return 1;
}

void DCFlashChip::Validate()
{
	// validate partition 0 (factory settings)
	bool valid = true;
	char sysinfo[16];

	for (u32 i = 0; i < sizeof(sysinfo); i++)
		sysinfo[i] = Read8(0x1a000 + i);
	valid = valid && memcmp(&sysinfo[5], "Dreamcast  ", 11) == 0;

	for (u32 i = 0; i < sizeof(sysinfo); i++)
		sysinfo[i] = Read8(0x1a0a0 + i);
	valid = valid && memcmp(&sysinfo[5], "Dreamcast  ", 11) == 0;

	if (!valid)
	{
		INFO_LOG(FLASHROM, "DCFlashChip::Validate resetting FLASH_PT_FACTORY");

		memcpy(sysinfo, "00000Dreamcast  ", sizeof(sysinfo));
		erase_partition(FLASH_PT_FACTORY);
		memcpy(data + 0x1a000, sysinfo, sizeof(sysinfo));
		memcpy(data + 0x1a0a0, sysinfo, sizeof(sysinfo));
	}

	// validate partition 1 (reserved)
	erase_partition(FLASH_PT_RESERVED);

	// validate partition 2 (user settings, block allocated)
	if (!validate_header(FLASH_PT_USER))
	{
		INFO_LOG(FLASHROM, "DCFlashChip::Validate resetting FLASH_PT_USER");

		erase_partition(FLASH_PT_USER);
		write_header(FLASH_PT_USER);
	}

	// validate partition 3 (game settings, block allocated)
	if (!validate_header(FLASH_PT_GAME))
	{
		INFO_LOG(FLASHROM, "DCFlashChip::Validate resetting FLASH_PT_GAME");

		erase_partition(FLASH_PT_GAME);
		write_header(FLASH_PT_GAME);
	}

	// validate partition 4 (unknown, block allocated)
	if (!validate_header(FLASH_PT_UNKNOWN))
	{
		INFO_LOG(FLASHROM, "DCFlashChip::Validate resetting FLASH_PT_UNKNOWN");

		erase_partition(FLASH_PT_UNKNOWN);
		write_header(FLASH_PT_UNKNOWN);
	}
}

int DCFlashChip::alloc_block(u32 offset, u32 size)
{
	u8 bitmap[FLASH_BLOCK_SIZE];
	int blocks = num_user_blocks(size);
	int bitmap_id = blocks;
	int phys_id = 1;
	int phys_end = 1 + blocks;

	while (phys_id < phys_end) {
		// read the next bitmap every FLASH_BITMAP_BLOCKS
		if (phys_id % FLASH_BITMAP_BLOCKS == 1) {
			read_physical_block(offset, ++bitmap_id, bitmap);
		}

		// use the first unallocated block
		if (!is_allocated(bitmap, phys_id)) {
			break;
		}
		// if the current block has been rewritten, use it
		if (lookup_block(offset, size, *(u16*)&this->data[offset + phys_id * FLASH_BLOCK_SIZE]) != phys_id)
			break;

		phys_id++;
	}

	if (phys_id >= phys_end)
	{
		WARN_LOG(FLASHROM, "Cannot allocate block in flash. Full?");
		return 0;
	}

	// mark the block as allocated
	set_allocated(bitmap, phys_id);
	write_physical_block(offset, bitmap_id, bitmap);

	return phys_id;
}

int DCFlashChip::lookup_block(u32 offset, u32 size, u32 block_id)
{
	u8 bitmap[FLASH_BLOCK_SIZE];
	int blocks = num_user_blocks(size);
	int bitmap_id = 1 + blocks;
	int phys_id = 1;
	int phys_end = bitmap_id;

	// in order to lookup a logical block, all physical blocks must be iterated.
	// since physical blocks are allocated linearly, the physical block with the
	// highest address takes precedence
	int result = 0;

	while (phys_id < phys_end) {
		// read the next bitmap every FLASH_BITMAP_BLOCKS
		if (phys_id % FLASH_BITMAP_BLOCKS == 1) {
			read_physical_block(offset, bitmap_id++, bitmap);
		}

		// being that physical blocks are allocated linearly, stop processing once
       	// the first unallocated block is hit
		if (!is_allocated(bitmap, phys_id))
			break;

		struct flash_user_block user;
		read_physical_block(offset, phys_id, &user);

		if (user.block_id == block_id)
		{
			if (!validate_crc(&user))
				WARN_LOG(FLASHROM, "flash_lookup_block physical block %d has an invalid crc", phys_id);
			else
				result = phys_id;
		}

		phys_id++;
	}

	return result;
}
