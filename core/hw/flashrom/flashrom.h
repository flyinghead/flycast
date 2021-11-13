/*
	bios & nvmem related code
*/

#pragma once
#include <cmath>
#include "types.h"
#include "serialize.h"

struct MemChip
{
	u8* data;
	u32 size;
	u32 mask;

protected:
	u32 write_protect_size;
	std::string load_filename;

	MemChip(u32 size, u32 write_protect_size = 0)
	{
		this->data = new u8[size]();
		this->size = size;
		this->mask = size - 1; // must be power of 2
		this->write_protect_size = write_protect_size;
	}

public:
	virtual ~MemChip()
	{
		delete[] data;
	}

	virtual u8 Read8(u32 addr)
	{
		return data[addr & mask];
	}

	u32 Read(u32 addr,u32 sz) 
	{
		addr &= mask;

		u32 rv = 0;

		for (u32 i = 0; i < sz; i++)
			rv |= Read8(addr + i) << (i * 8);

		return rv;
	}

	bool Load(const std::string& file);
	virtual bool Reload() { return true; }
	bool Load(const std::string &prefix, const std::string &names_ro,
			const std::string &title);
	void digest(u8 md5Digest[16]);

	virtual void Reset() {}
	virtual void Serialize(Serializer& ser) const { }
	virtual void Deserialize(Deserializer& deser) { }
};

struct WritableChip : MemChip
{
protected:
	WritableChip(u32 size, u32 write_protect_size = 0)
		: MemChip(size, write_protect_size) {}

public:
	virtual void Write(u32 addr, u32 data, u32 size) = 0;

	bool Reload() override
	{
		return Load(this->load_filename);
	}
	void Save(const std::string& file);
	void Save(const std::string &prefix, const std::string &name_ro,
			const std::string &title);
};

struct RomChip : MemChip
{
	RomChip(u32 sz) : MemChip(sz) {}
};

struct SRamChip : WritableChip
{
	SRamChip(u32 sz, u32 write_protect_size = 0) : WritableChip(sz, write_protect_size) {}

	void Write(u32 addr,u32 val,u32 sz) override
	{
		addr&=mask;
		if (addr < write_protect_size)
			return;
		switch (sz)
		{
		case 1:
			data[addr]=(u8)val;
			return;
		case 2:
			*(u16*)&data[addr]=(u16)val;
			return;
		case 4:
			*(u32*)&data[addr]=val;
			return;
		default:
			die("invalid access size");
		}
	}

	void Serialize(Serializer& ser) const override
	{
		ser.serialize(&this->data[write_protect_size], size - write_protect_size);
	}
	void Deserialize(Deserializer& deser) override
	{
		deser.deserialize(&this->data[write_protect_size], size - write_protect_size);
	}
};

//
// Flash block handling code borrowed from redream (https://github.com/inolen/redream)
//

// magic cookie every block-allocated partition begins with
#define FLASH_MAGIC_COOKIE "KATANA_FLASH____"

#define FLASH_BLOCK_SIZE 0x40
// each bitmap is 64 bytes in length, and each byte can record the state of 8
// physical blocks (one per bit), therefore, each bitmap can represent up to
// 512 physical blocks. these 512 blocks are each 64-bytes in length, meaning
// each partition would need partition_size / 32768 bitmap blocks to represent
// all of its physical blocks
#define FLASH_BITMAP_BLOCKS (FLASH_BLOCK_SIZE * 8)
#define FLASH_BITMAP_BYTES (FLASH_BITMAP_BLOCKS * 64)

// flash partitions
#define FLASH_PT_FACTORY 0
#define FLASH_PT_RESERVED 1
#define FLASH_PT_USER 2
#define FLASH_PT_GAME 3
#define FLASH_PT_UNKNOWN 4
#define FLASH_PT_NUM 5

// flash logical blocks
#define FLASH_USER_SYSCFG 0x05
#define FLASH_USER_INET 0x80
#define FLASH_USER_ISP1 0xC0
#define FLASH_USER_ISP2 0xC6

struct flash_syscfg_block {
  u16 block_id;
  // last set time (seconds since 1/1/1950 00:00)
  u16 time_lo;
  u16 time_hi;
  u8 unknown1;
  u8 lang;
  u8 mono;
  u8 autostart;
  u8 unknown2[4];
  u8 reserved[50];
};

struct flash_isp1_block {
	u16 block_id;
	u8 _unknown[4];
	char sega[4];
	char username[28];
	char password[16];
	char phone[8];
	u16 crc;
};

struct flash_isp2_block {
	u16 block_id;
	char sega[4];
	char username[28];
	char password[16];
	char phone[12];
	u16 crc;
};

// header block in block-allocated partition
struct flash_header_block {
  char magic[16];
  u8 part_id;
  u8 version;
  u8 reserved[46];
};

// user block in block-allocated partition
struct flash_user_block {
  u16 block_id;
  u8 data[60];
  u16 crc;
};

// Macronix 29LV160TMC
// AtomisWave uses a custom 29L001mc model
struct DCFlashChip : WritableChip
{
	DCFlashChip(u32 sz, u32 write_protect_size = 0): WritableChip(sz, write_protect_size), state(FS_Normal) { }

	enum FlashState
	{
		FS_Normal,
		FS_ReadAMDID1,
		FS_ReadAMDID2,
		FS_ByteProgram,
		FS_EraseAMD1,
		FS_EraseAMD2,
		FS_EraseAMD3,
		FS_SelectMode,
	};

	FlashState state;
	void Reset() override
	{
		//reset the flash chip state
		state = FS_Normal;
	}
	
	void Write(u32 addr,u32 val,u32 sz) override
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

	u8 Read8(u32 addr) override
	{
		if (state == FS_SelectMode)
		{
			state = FS_Normal;
			switch (addr & 0x43)
			{
			case 0:	// manufacturer's code
				return 4;		// or 0x20 or 1
			case 1:	// device code
				return 0xb0;	// or 0x40 or 0x3e
			case 2:	// sector protection verification
				// sector protection
				DEBUG_LOG(FLASHROM, "Sector protection address %x", addr);
				return (addr & 0x1e000) == 0x1a000;
			default:
				WARN_LOG(FLASHROM, "SelectMode unknown address %x", addr);
				return 0;
			}
		}
		return MemChip::Read8(addr);
	}

	int WriteBlock(u32 part_id, u32 block_id, const void *data)
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

	int ReadBlock(u32 part_id, u32 block_id, void *data)
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

	void GetPartitionInfo(int part_id, int *offset, int *size)
	{
		switch (part_id)
		{
		case FLASH_PT_FACTORY:
			*offset = 0x1a000;
			*size = 8 * 1024;
			break;
		case FLASH_PT_RESERVED:
			*offset = 0x18000;
			*size = 8 * 1024;
			break;
		case FLASH_PT_USER:
			*offset = 0x1c000;
			*size = 16 * 1024;
			break;
		case FLASH_PT_GAME:
			*offset = 0x10000;
			*size = 32 * 1024;
			break;
		case FLASH_PT_UNKNOWN:
			*offset = 0x00000;
			*size = 64 * 1024;
			break;
		default:
			*offset = 0;
			*size = 0;
			die("unknown partition");
			break;
		}
	}

	void Validate()
	{
		// validate partition 0 (factory settings)
		bool valid = true;
		char sysinfo[16];

		for (size_t i = 0; i < sizeof(sysinfo); i++)
			sysinfo[i] = Read8(0x1a000 + i);
		valid = valid && memcmp(&sysinfo[5], "Dreamcast  ", 11) == 0;

		for (size_t i = 0; i < sizeof(sysinfo); i++)
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

private:
	int crc_block(struct flash_user_block *block)
	{
		const u8 *buf = (const u8 *)block;
		int size = 62;
		int n = 0xffff;

		for (int i = 0; i < size; i++) {
			n ^= (buf[i] << 8);

			for (int c = 0; c < 8; c++) {
				if (n & 0x8000) {
					n = (n << 1) ^ 4129;
				} else {
					n = (n << 1);
				}
			}
		}

		return (~n) & 0xffff;
	}

	int validate_crc(struct flash_user_block *user)
	{
		return user->crc == crc_block(user);
	}

	inline int num_physical_blocks(u32 size)
	{
		return size / FLASH_BLOCK_SIZE;
	}

	inline int num_bitmap_blocks(u32 size)
	{
		return (int)ceil(size / (float)FLASH_BITMAP_BYTES);
	}

	inline int num_user_blocks(u32 size)
	{
		return num_physical_blocks(size) - num_bitmap_blocks(size) - 1;
	}

	inline int is_allocated(const u8 *bitmap, u32 phys_id)
	{
		int index = (phys_id - 1) % FLASH_BITMAP_BLOCKS;
		return (bitmap[index / 8] & (0x80 >> (index % 8))) == 0x0;
	}

	inline void set_allocated(u8 *bitmap, u32 phys_id)
	{
		int index = (phys_id - 1) % FLASH_BITMAP_BLOCKS;
		bitmap[index / 8] &= ~(0x80 >> (index % 8));
	}

	void write_physical_block(u32 offset, u32 phys_id, const void *data)
	{
		memcpy(&this->data[offset + phys_id * FLASH_BLOCK_SIZE], data, FLASH_BLOCK_SIZE);
	}

	void read_physical_block(u32 offset, u32 phys_id, void *data)
	{
		memcpy(data, &this->data[offset + phys_id * FLASH_BLOCK_SIZE], FLASH_BLOCK_SIZE);
	}

	int validate_header(u32 offset, u32 part_id)
	{
		struct flash_header_block header;
		read_physical_block(offset, 0, &header);

		if (memcmp(header.magic, FLASH_MAGIC_COOKIE, sizeof(header.magic)) != 0)
			return 0;

		if (header.part_id != part_id)
			return 0;

		return 1;
	}

	int validate_header(u32 part_id)
	{
		int offset, size;
		GetPartitionInfo(part_id, &offset, &size);

		return validate_header(offset, part_id);
	}

	int alloc_block(u32 offset, u32 size)
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

	int lookup_block(u32 offset, u32 size, u32 block_id)
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

	void Serialize(Serializer& ser) const override
	{
		ser << state;
		ser.serialize(&this->data[write_protect_size], size - write_protect_size);
	}

	void Deserialize(Deserializer& deser) override
	{
		deser >> state;
		deser.deserialize(&this->data[write_protect_size], size - write_protect_size);
	}

	void erase_partition(u32 part_id)
	{
		int offset, size;
		GetPartitionInfo(part_id, &offset, &size);

		memset(data + offset, 0xFF, size);
	}

	void write_header(int part_id)
	{
		int offset, size;
		GetPartitionInfo(part_id, &offset, &size);

		struct flash_header_block header;
		memset(&header, 0xff, sizeof(header));
		memcpy(header.magic, FLASH_MAGIC_COOKIE, sizeof(header.magic));
		header.part_id = part_id;
		header.version = 0;

		write_physical_block(offset, 0, &header);
	}
};
