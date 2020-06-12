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
#pragma once
#include <array>
#include "types.h"
#include "sh4_mem.h"
#include "modules/mmu.h"

//
// SH4 instruction cache
//
class sh4_icache
{
public:
	u16 ReadMem(u32 address)
	{
		const u32 area = address >> 29;
		if (area == 5 || area == 7	// P2, P4: non-cacheable
				|| !CCN_CCR.ICE)	// Instruction cache disabled
			return IReadMem16(address);

		u32 index = CCN_CCR.IIX ?
				((address >> 5) & 0x7f) | ((address >> (25 - 7)) & 0x80)
				: (address >> 5) & 0xff;

#ifndef NO_MMU
		if (mmu_enabled())
		{
			u32 paddr;
			u32 rv = mmu_instruction_translation(address, paddr);
			if (rv != MMU_ERROR_NONE)
				mmu_raise_exception(rv, address, MMU_TT_IREAD);
			address = paddr;
		}
#endif

		cache_line& line = lines[index];
		const u32 tag = (address >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
		{
			// miss
			line.valid = true;
			line.address = tag;
			const u32 line_addr = address & ~0x1f;
			u8* memPtr = GetMemPtr(line_addr, sizeof(line.data));
			if (memPtr != nullptr)
				memcpy(line.data, memPtr, sizeof(line.data));
			else
			{
				u32 *p = (u32 *)line.data;
				for (int i = 0; i < 32; i += 4)
					*p++ = _vmem_ReadMem32(line_addr + i);
			}
		}

		return *(u16*)&line.data[address & 0x1f];
	}

	void Invalidate()
	{
		for (auto& line : lines)
			line.valid = false;
	}

	void Reset(bool hard)
	{
		if (hard)
			memset(&lines[0], 0, sizeof(lines));
	}

	bool Serialize(void **data, unsigned int *total_size)
	{
		REICAST_S(lines);

		return true;
	}

	bool Unserialize(void **data, unsigned int *total_size)
	{
		REICAST_US(lines);

		return true;
	}

	u32 ReadAddressArray(u32 addr)
	{
		u32 index = (addr >> 5) & 0xFF;
		return lines[index].valid | (lines[index].address << 10);
	}

	void WriteAddressArray(u32 addr, u32 data)
	{
		u32 index = (addr >> 5) & 0xFF;
		cache_line& line = lines[index];
		bool associative = (addr & 8) != 0;
		if (!associative)
		{
			line.valid = data & 1;
			line.address = (data >> 10) & 0x7ffff;
		}
		else
		{
			u32 tag;
#ifndef NO_MMU
			if (mmu_enabled())
			{
				u32 vaddr = data & ~0x3ff;
				u32 paddr;
				u32 rv = mmu_instruction_translation(vaddr, paddr);
				if (rv == MMU_ERROR_TLB_MISS)
					// Ignore the write
					return;
				if (rv != MMU_ERROR_NONE)
					mmu_raise_exception(rv, vaddr, MMU_TT_IREAD);
				tag = (paddr >> 10) & 0x7ffff;
			}
			else
#endif
			{
				tag = (data >> 10) & 0x7ffff;
			}
			if (!line.valid || tag != line.address)
				// Ignore the write
				return;
			line.valid = data & 1;
		}
	}

	u32 ReadDataArray(u32 addr)
	{
		u32 index = (addr >> 5) & 0xFF;
		cache_line& line = lines[index];
		return *(u32 *)&line.data[addr & 0x1C];
	}

	void WriteDataArray(u32 addr, u32 data)
	{
		u32 index = (addr >> 5) & 0xFF;
		cache_line& line = lines[index];
		*(u32 *)&line.data[addr & 0x1C] = data;
	}

private:
	struct cache_line {
		bool valid;
		u32 address;
		u8 data[32];
	};

	std::array<cache_line, 256> lines;
};

extern sh4_icache icache;

//
// SH4 operand cache
//
class sh4_ocache
{
public:
	template<class T>
	T ReadMem(u32 address)
	{
		const u32 area = address >> 29;
		if (area == 5 || area == 7	// P2, P4: non-cacheable
				|| !CCN_CCR.OCE)	// Operand cache disabled
		{
			return readMem<T>(address);
		}

		u32 index = lineIndex(address);

#ifndef NO_MMU
		if (mmu_enabled())
		{
			u32 paddr;
			u32 rv = mmu_data_translation<MMU_TT_DREAD, T>(address, paddr);
			if (rv != MMU_ERROR_NONE)
				mmu_raise_exception(rv, address, MMU_TT_DREAD);
			address = paddr;
		}
#endif

		cache_line& line = lines[index];
		const u32 tag = (address >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
		{
			// miss
			if (line.dirty && line.valid)
				// write-back needed
				doWriteBack(index, line);
			line.address = tag;
			readCacheLine(address, line);
		}

		return *(T*)&line.data[address & 0x1f];
	}

	template<class T>
	void WriteMem(u32 address, T data)
	{
		const u32 area = address >> 29;
		if (area == 5 || area == 7	// P2, P4: non-cacheable
				|| !CCN_CCR.OCE)	// Operand cache disabled
		{
			writeMem(address, data);
			return;
		}

		u32 index = lineIndex(address);
		// Use CCR.CB if P1 otherwise use !CCR.WT
		bool copy_back = area == 4 ? CCN_CCR.CB : !CCN_CCR.WT;

#ifndef NO_MMU
		if (mmu_enabled())
		{
			u32 paddr;
			u32 rv = mmu_data_translation<MMU_TT_DWRITE, T>(address, paddr);
			if (rv != MMU_ERROR_NONE)
				mmu_raise_exception(rv, address, MMU_TT_DWRITE);
			address = paddr;
			// FIXME need WT bit of page
		}
#endif

		cache_line& line = lines[index];
		const u32 tag = (address >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
		{
			// miss and copy-back => read cache line
			if (copy_back)
			{
				if (line.dirty && line.valid)
					// write-back needed
					doWriteBack(index, line);
				line.address = tag;
				readCacheLine(address, line);
			}
		}
		else if (!copy_back)
		{
			// hit and write-through => update cache
			*(T*)&line.data[address & 0x1f] = data;
		}
		if (copy_back)
		{
			// copy-back => update cache and mark line as dirty
			line.dirty = true;
			*(T*)&line.data[address & 0x1f] = data;
		}
		else
		{
			// write-through => update main ram
			writeMem(address, data);
		}
	}

	void WriteBack(u32 address, bool write_back, bool invalidate)
	{
		const u32 area = address >> 29;
		if (area == 5 || area == 7)	// P2, P4: non-cacheable
			return;
		address &= ~0x1F;
		u32 index = lineIndex(address);

#ifndef NO_MMU
		if (mmu_enabled())
		{
			u32 paddr;
			u32 rv = mmu_data_translation<MMU_TT_DWRITE, u32>(address, paddr);
			if (rv != MMU_ERROR_NONE)
				mmu_raise_exception(rv, address, MMU_TT_DWRITE);
			address = paddr;
		}
#endif
		cache_line& line = lines[index];
		const u32 tag = (address >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
			return;
		if (write_back && line.dirty)
			doWriteBack(index, line);
		line.valid = !invalidate;
		line.dirty = false;
	}

	void Prefetch(u32 address)
	{
		const u32 area = address >> 29;
		if (area == 5 || area == 7)	// P2, P4: non-cacheable
			return;
		address &= ~0x1F;
		u32 index = lineIndex(address);

#ifndef NO_MMU
		if (mmu_enabled())
		{
			u32 paddr;
			u32 rv = mmu_data_translation<MMU_TT_DREAD, u8>(address, paddr);
			if (rv != MMU_ERROR_NONE)
				// ignore address translation errors
				return;
			address = paddr;
		}
#endif
		cache_line& line = lines[index];
		const u32 tag = (address >> 10) & 0x7ffff;
		if (line.valid && tag == line.address)
			return;
		if (line.valid && line.dirty)
			doWriteBack(index, line);
		line.address = tag;
		readCacheLine(address, line);
	}

	void Invalidate()
	{
		for (auto& line : lines)
		{
			line.dirty = false;
			line.valid = false;
		}
	}

	void Reset(bool hard)
	{
		if (hard)
			memset(&lines[0], 0, sizeof(lines));
	}

	bool Serialize(void **data, unsigned int *total_size)
	{
		REICAST_S(lines);

		return true;
	}

	bool Unserialize(void **data, unsigned int *total_size)
	{
		REICAST_US(lines);

		return true;
	}

	u32 ReadAddressArray(u32 addr)
	{
		u32 index = (addr >> 5) & 0x1FF;
		return lines[index].valid | (lines[index].dirty << 1) | (lines[index].address << 10);
	}

	void WriteAddressArray(u32 addr, u32 data)
	{
		u32 index = (addr >> 5) & 0x1FF;
		cache_line& line = lines[index];
		bool associative = (addr & 8) != 0;
		if (!associative)
		{
			if (line.valid && line.dirty)
				doWriteBack(index, line);
			line.address = (data >> 10) & 0x7ffff;
		}
		else
		{
			u32 tag;
#ifndef NO_MMU
			if (mmu_enabled())
			{
				u32 vaddr = data & ~0x3ff;
				u32 paddr;
				u32 rv = mmu_data_translation<MMU_TT_DREAD, u8>(vaddr, paddr);
				if (rv == MMU_ERROR_TLB_MISS)
					// Ignore the write
					return;
				if (rv != MMU_ERROR_NONE)
					mmu_raise_exception(rv, vaddr, MMU_TT_DREAD);
				tag = (paddr >> 10) & 0x7ffff;
			}
			else
#endif
			{
				tag = (data >> 10) & 0x7ffff;
			}
			if (!line.valid || tag != line.address)
				// Ignore the write
				return;
			if ((data & 3) != 0 && line.dirty)
				doWriteBack(index, line);
		}
		line.valid = data & 1;
		line.dirty = (data >> 1) & 1;
	}

	u32 ReadDataArray(u32 addr)
	{
		u32 index = (addr >> 5) & 0x1FF;
		cache_line& line = lines[index];
		return *(u32 *)&line.data[addr & 0x1C];
	}

	void WriteDataArray(u32 addr, u32 data)
	{
		u32 index = (addr >> 5) & 0x1FF;
		cache_line& line = lines[index];
		*(u32 *)&line.data[addr & 0x1C] = data;
	}

private:
	struct cache_line {
		bool valid;
		bool dirty;
		u32 address;
		u8 data[32];
	};

	u32 lineIndex(u32 address)
	{
		u32 index = CCN_CCR.OIX ?
				((address >> (25 - 8)) & 0x100) | ((address >> 5) & (CCN_CCR.ORA ? 0x7f : 0xff))
				: (address >> 5) & (CCN_CCR.ORA ? 0x17f : 0x1ff);
		if (CCN_CCR.ORA && (address >> 29) == 3)
			index |= 0x80;
		return index;
	}

	void readCacheLine(u32 address, cache_line& line)
	{
		line.valid = true;
		line.dirty = false;
		const u32 line_addr = address & ~0x1f;
		u8* memPtr = GetMemPtr(line_addr, sizeof(line.data));
		if (memPtr != nullptr)
			memcpy(line.data, memPtr, sizeof(line.data));
		else
		{
			u32 *p = (u32 *)line.data;
			for (int i = 0; i < 32; i += 4)
				*p++ = _vmem_ReadMem32(line_addr + i);
		}
	}

	void doWriteBack(u32 index, cache_line& line)
	{
		if (CCN_CCR.ORA && (index & 0x80))
			return;
		u32 line_addr = (line.address << 10) | ((index & 0x1F) << 5);
		u8* memPtr = GetMemPtr(line_addr, sizeof(line.data));
		if (memPtr != nullptr)
			memcpy(memPtr, line.data, sizeof(line.data));
		else
		{
			u32 *p = (u32 *)line.data;
			for (int i = 0; i < 32; i += 4)
				_vmem_WriteMem32(line_addr + i, *p++);
		}
	}

	template<class T>
	T readMem(u32 address)
	{
		switch (sizeof(T))
		{
		case 1:
			return ReadMem8(address);
		case 2:
			return ReadMem16(address);
		case 4:
			return ReadMem32(address);
		case 8:
			return ReadMem64(address);
		default:
			die("Invalid data size");
			return 0;
		}
	}
	template<class T>
	void writeMem(u32 address, T data)
	{
		switch (sizeof(T))
		{
		case 1:
			WriteMem8(address, data);
			break;
		case 2:
			WriteMem16(address, data);
			break;
		case 4:
			WriteMem32(address, data);
			break;
		case 8:
			WriteMem64(address, data);
			break;
		default:
			die("Invalid data size");
			break;
		}
	}

	std::array<cache_line, 512> lines;
};

extern sh4_ocache ocache;
