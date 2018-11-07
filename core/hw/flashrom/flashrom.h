/*
	bios & nvmem related code
*/

#pragma once
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
	~MemChip() { delete[] data; }

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
};
struct RomChip : MemChip
{
	RomChip(u32 sz, u32 write_protect_size = 0) : MemChip(sz, write_protect_size) {}
	void Reset()
	{
		//nothing, its permanent read only ;p
	}
	void Write(u32 addr,u32 data,u32 sz)
	{
		die("Write to RomChip is not possible, address=%x, data=%x, size=%d");
	}
};
struct SRamChip : MemChip
{
	SRamChip(u32 sz, u32 write_protect_size = 0) : MemChip(sz, write_protect_size) {}

	void Reset()
	{
		//nothing, its battery backed up storage
	}
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
	void Reset()
	{
		//reset the flash chip state
		state = FS_Normal;
	}
	
	virtual u8 Read8(u32 addr)
	{
		u32 rv=MemChip::Read8(addr);

		#if DC_PLATFORM==DC_PLATFORM_DREAMCAST
			if ((addr==0x1A002 || addr==0x1A0A2) && settings.dreamcast.region<=2)
				return '0' + settings.dreamcast.region;
			else if ((addr==0x1A004 || addr==0x1A0A4) && settings.dreamcast.broadcast<=3)
				return '0' + settings.dreamcast.broadcast;
		#endif

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
				memset(data + write_protect_size, 0xff, size - write_protect_size);
				state = FS_Normal;
			}
			else if ((val & 0xff) == 0x30)
			{
				// sector erase
				addr = max(addr, write_protect_size);
#if DC_PLATFORM != DC_PLATFORM_ATOMISWAVE
				printf("Erase Sector %08X! (%08X)\n",addr,addr&(~0x3FFF));
				memset(&data[addr&(~0x3FFF)],0xFF,0x4000);
#else
				// AtomisWave's Macronix 29L001mc has 64k blocks
				printf("Erase Sector %08X! (%08X)\n",addr,addr&(~0xFFFF));
				memset(&data[addr&(~0xFFFF)], 0xFF, 0x10000);
#endif
				state = FS_Normal;
			}
			else
			{
				printf("FlashRom: EraseAMD3 unexpected write @ %x: %x\n", addr, val);
			}
			break;
		}
	}	
};
