/*
	bios & nvmem related code
*/

#pragma once
#include <math.h>
#include "types.h"

struct MemChip
{
	u8* data;
	u32 size;
	u32 mask;
	u32 write_protect_size;
	std::string load_filename;

	MemChip(u32 size, u32 write_protect_size = 0)
	{
		this->data=new u8[size];
		this->size=size;
		this->mask=size-1;//must be power of 2
		this->write_protect_size = write_protect_size;
	}
	virtual ~MemChip() { delete[] data; }

	virtual u8 Read8(u32 addr)
	{
		return data[addr&mask];
	}

	u32 Read(u32 addr,u32 sz) 
	{
		addr&=mask;

		u32 rv=0;

		for (u32 i=0;i<sz;i++)
			rv|=Read8(addr+i)<<(i*8);

		return rv;
	}

	bool Load(const string& file)
	{
		FILE* f=fopen(file.c_str(),"rb");
		if (f)
		{
			bool rv = fread(data + write_protect_size, 1, size - write_protect_size, f) == size - write_protect_size;
			fclose(f);
			if (rv)
				this->load_filename = file;

			return rv;
		}
		return false;
	}

	bool Reload()
	{
		return Load(this->load_filename);
	}

	void Save(const string& file)
	{
		FILE* f=fopen(file.c_str(),"wb");
		if (f)
		{
			fwrite(data + write_protect_size, 1, size - write_protect_size, f);
			fclose(f);
		}
	}

	bool Load(const string& root,const string& prefix,const string& names_ro,const string& title)
	{
		wchar base[512];
		wchar temp[512];
		wchar names[512];

		// FIXME: Data loss if buffer is too small
		strncpy(names,names_ro.c_str(), sizeof(names));
		names[sizeof(names) - 1] = '\0';

		sprintf(base,"%s",root.c_str());

		wchar* curr=names;
		wchar* next;
		do
		{
			next=strstr(curr,";");
			if(next) *next=0;
			if (curr[0]=='%')
			{
				sprintf(temp,"%s%s%s",base,prefix.c_str(),curr+1);
			}
			else
			{
				sprintf(temp,"%s%s",base,curr);
			}
			
			curr=next+1;

			if (Load(temp))
			{
				printf("Loaded %s as %s\n\n",temp,title.c_str());
				return true;
			}
		} while(next);


		return false;
	}
	void Save(const string& root,const string& prefix,const string& name_ro,const string& title)
	{
		wchar path[512];

		sprintf(path,"%s%s%s",root.c_str(),prefix.c_str(),name_ro.c_str());
		Save(path);

		printf("Saved %s as %s\n\n",path,title.c_str());
	}
	virtual void Reset() {}
};
struct RomChip : MemChip
{
	RomChip(u32 sz, u32 write_protect_size = 0) : MemChip(sz, write_protect_size) {}
	void Write(u32 addr,u32 data,u32 sz)
	{
		die("Write to RomChip is not possible, address=%x, data=%x, size=%d");
	}
};
struct SRamChip : MemChip
{
	SRamChip(u32 sz, u32 write_protect_size = 0) : MemChip(sz, write_protect_size) {}

	void Write(u32 addr,u32 val,u32 sz)
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
struct DCFlashChip : MemChip
{
	DCFlashChip(u32 sz, u32 write_protect_size = 0): MemChip(sz, write_protect_size), state(FS_Normal) { }

	enum FlashState
	{
		FS_Normal,
		FS_ReadAMDID1,
		FS_ReadAMDID2,
		FS_ByteProgram,
		FS_EraseAMD1,
		FS_EraseAMD2,
		FS_EraseAMD3
	};

	FlashState state;
	virtual void Reset() override
	{
		//reset the flash chip state
		state = FS_Normal;
	}
	
	virtual u8 Read8(u32 addr)
	{
#if DC_PLATFORM == DC_PLATFORM_DREAMCAST
		switch (addr)
		{
		case 0x1A002:
		case 0x1A0A2:
			if (settings.dreamcast.region <= 2)
				return '0' + settings.dreamcast.region;
			break;
		case 0x1A003:
		case 0x1A0A3:
			if (settings.dreamcast.language <= 5)
				return '0' + settings.dreamcast.language;
			break;
		case 0x1A004:
		case 0x1A0A4:
			if (settings.dreamcast.broadcast <= 3)
				return '0' + settings.dreamcast.broadcast;
			break;
		}
#endif

		u32 rv=MemChip::Read8(addr);

		return rv;
	}
	

	void Write(u32 addr,u32 val,u32 sz)
	{
		if (sz != 1)
			die("invalid access size");

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
				printf("Unknown FlashWrite mode: %x\n", val);
				break;
			}
			break;

		case FS_ReadAMDID1:
			if ((addr & 0xffff) == 0x2aa && (val & 0xff) == 0x55)
				state = FS_ReadAMDID2;
			else if ((addr & 0xffff) == 0x2aaa && (val & 0xff) == 0x55)
				state = FS_ReadAMDID2;
			else if ((addr & 0xfff) == 0x555 && (val & 0xff) == 0x55)
				state = FS_ReadAMDID2;
			else
			{
				printf("FlashRom: ReadAMDID1 unexpected write @ %x: %x\n", addr, val);
				state = FS_Normal;
			}
			break;

		case FS_ReadAMDID2:
			if ((addr & 0xffff) == 0x555 && (val & 0xff) == 0x80)
				state = FS_EraseAMD1;
			else if ((addr & 0xffff) == 0x5555 && (val & 0xff) == 0x80)
				state = FS_EraseAMD1;
			else if ((addr & 0xfff) == 0xaaa && (val & 0xff) == 0x80)
				state = FS_EraseAMD1;
			else if ((addr & 0xffff) == 0x555 && (val & 0xff) == 0xa0)
				state = FS_ByteProgram;
			else if ((addr & 0xffff) == 0x5555 && (val & 0xff) == 0xa0)
				state = FS_ByteProgram;
			else if ((addr & 0xfff) == 0xaaa && (val & 0xff) == 0xa0)
				state = FS_ByteProgram;
			else
			{
				printf("FlashRom: ReadAMDID2 unexpected write @ %x: %x\n", addr, val);
				state = FS_Normal;
			}
			break;
		case FS_ByteProgram:
			if (addr >= write_protect_size)
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
				printf("FlashRom: EraseAMD1 unexpected write @ %x: %x\n", addr, val);
			}
			break;

		case FS_EraseAMD2:
			if ((addr & 0xffff) == 0x2aa && (val & 0xff) == 0x55)
				state = FS_EraseAMD3;
			else if ((addr & 0xffff) == 0x2aaa && (val & 0xff) == 0x55)
				state = FS_EraseAMD3;
			else if ((addr & 0xfff) == 0x555 && (val & 0xff) == 0x55)
				state = FS_EraseAMD3;
			else
			{
				printf("FlashRom: EraseAMD2 unexpected write @ %x: %x\n", addr, val);
			}
			break;

		case FS_EraseAMD3:
			if (((addr & 0xfff) == 0x555 && (val & 0xff) == 0x10)
				|| ((addr & 0xfff) == 0xaaa && (val & 0xff) == 0x10))
			{
				// chip erase
				printf("Erasing Chip!\n");
#if DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
				u8 save[0x2000];
				// this area is write-protected on AW
				memcpy(save, data + 0x1a000, 0x2000);
#endif
				memset(data + write_protect_size, 0xff, size - write_protect_size);
#if DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
				memcpy(data + 0x1a000, save, 0x2000);
#endif
				state = FS_Normal;
			}
			else if ((val & 0xff) == 0x30)
			{
				// sector erase
				if (addr >= write_protect_size)
				{
#if DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
					u8 save[0x2000];
					// this area is write-protected on AW
					memcpy(save, data + 0x1a000, 0x2000);
#endif
					printf("Erase Sector %08X! (%08X)\n",addr,addr&(~0x3FFF));
					memset(&data[addr&(~0x3FFF)],0xFF,0x4000);
#if DC_PLATFORM == DC_PLATFORM_ATOMISWAVE
					memcpy(data + 0x1a000, save, 0x2000);
#endif
				}
				state = FS_Normal;
			}
			else
			{
				printf("FlashRom: EraseAMD3 unexpected write @ %x: %x\n", addr, val);
			}
			break;
		}
	}

	int WriteBlock(u32 part_id, u32 block_id, const void *data)
	{
		int offset, size;
		partition_info(part_id, &offset, &size);

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
		partition_info(part_id, &offset, &size);

		if (!validate_header(offset, part_id))
			return 0;

		int phys_id = lookup_block(offset, size, block_id);
		if (!phys_id)
			return 0;

		read_physical_block(offset, phys_id, data);

		return 1;
	}

private:
	void partition_info(int part_id, int *offset, int *size)
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
			die("unknown partition");
			break;
		}
	}

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

	inline int is_allocated(u8 *bitmap, u32 phys_id)
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

			phys_id++;
		}

		verify(phys_id < phys_end);

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
					printf("flash_lookup_block physical block %d has an invalid crc\n", phys_id);
				else
					result = phys_id;
			}

			phys_id++;
		}

		return result;
	}
};
