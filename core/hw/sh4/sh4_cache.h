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
#include "hw/sh4/sh4_core.h"
#include "serialize.h"

static bool cachedArea(u32 area)
{
	static const bool cached_areas[8] = {
			true, true, true, true,	// P0/U0
			true,					// P1
			false,					// P2
			true,					// P3
			false					// P4
	};
	return cached_areas[area];
}

static bool translatedArea(u32 area)
{
	static const bool translated_areas[8] = {
			true, true, true, true,	// P0/U0
			false,					// P1
			false,					// P2
			true,					// P3
			false					// P4
	};
	return translated_areas[area];
}

//
// SH4 instruction cache
//
class sh4_icache
{
public:
	u16 ReadMem(u32 address)
	{
		bool cacheOn = false;
		u32 physAddr;
		u32 err = translateAddress(address, physAddr, cacheOn);
		if (err != MMU_ERROR_NONE)
			mmu_raise_exception(err, address, MMU_TT_IREAD);

		if (!cacheOn)
			return _vmem_readt<u16, u16>(physAddr);

		const u32 index = CCN_CCR.IIX ?
				((address >> 5) & 0x7f) | ((address >> (25 - 7)) & 0x80)
				: (address >> 5) & 0xff;
		cache_line& line = lines[index];
		const u32 tag = (physAddr >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
		{
			// miss
			line.valid = true;
			line.address = tag;
			const u32 line_addr = physAddr & ~0x1f;
			u8* const memPtr = GetMemPtr(line_addr, sizeof(line.data));
			if (memPtr != nullptr)
				memcpy(line.data, memPtr, sizeof(line.data));
			else
			{
				u32 *p = (u32 *)line.data;
				for (int i = 0; i < 32; i += 4)
					*p++ = _vmem_ReadMem32(line_addr + i);
			}
		}

		return *(u16*)&line.data[physAddr & 0x1f];
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

	void Serialize(Serializer& ser) {
		ser << lines;
	}
	void Deserialize(Deserializer& deser) {
		deser >> lines;
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
			const u32 vaddr = data & ~0x3ff;
			bool cached;
			u32 physAddr;
			u32 err = translateAddress(vaddr, physAddr, cached);
			if (err == MMU_ERROR_TLB_MISS)
				// Ignore the write
				return;
			if (err != MMU_ERROR_NONE)
				mmu_raise_exception(err, vaddr, MMU_TT_IREAD);

			u32 tag = (physAddr >> 10) & 0x7ffff;

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

	u32 translateAddress(u32 address, u32& physAddr, bool& cached)
	{
		// Alignment errors
		if (address & 1)
			return MMU_ERROR_BADADDR;

		const u32 area = address >> 29;
		const bool userMode = sr.MD == 0;

		if (userMode)
		{
			// kernel mem protected in user mode
			// FIXME this makes WinCE fail
			//if (address & 0x80000000)
			//	return MMU_ERROR_BADADDR;
		}
		else
		{
			// P4 not executable
			if (area == 7)
				return MMU_ERROR_BADADDR;
		}
		cached = CCN_CCR.ICE == 1 && cachedArea(area);

		if (CCN_MMUCR.AT == 0 || !translatedArea(area)
				// 7C000000 to 7FFFFFFF in P0/U0 not translated
				|| (address & 0xFC000000) == 0x7C000000)
		{
			physAddr = address;
		}
		else
		{
			const TLB_Entry *entry;
			u32 err = mmu_instruction_lookup(address, &entry, physAddr);

			if (err != MMU_ERROR_NONE)
				return err;

			//0X  & User mode-> protection violation
			//Priv mode protection
			if (userMode)
			{
				u32 md = entry->Data.PR >> 1;
				if (md == 0)
					return MMU_ERROR_PROTECTED;
			}
			cached = cached && entry->Data.C == 1;
		}
		return MMU_ERROR_NONE;
	}

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
		u32 physAddr;
		bool cacheOn = false;
		bool copyBack;
		u32 err = translateAddress<T, MMU_TT_DREAD>(address, physAddr, cacheOn, copyBack);
		if (err != MMU_ERROR_NONE)
			mmu_raise_exception(err, address, MMU_TT_DREAD);

		if (!cacheOn)
			return _vmem_readt<T, T>(physAddr);

		const u32 index = lineIndex(address);
		cache_line& line = lines[index];
		const u32 tag = (physAddr >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
		{
			// miss
			if (line.dirty && line.valid)
				// write-back needed
				doWriteBack(index, line);
			line.address = tag;
			readCacheLine(physAddr, line);
		}

		return *(T*)&line.data[physAddr & 0x1f];
	}

	template<class T>
	void WriteMem(u32 address, T data)
	{
		u32 physAddr = 0;
		bool cacheOn = false;
		bool copyBack = false;
		u32 err = translateAddress<T, MMU_TT_DWRITE>(address, physAddr, cacheOn, copyBack);
		if (err != MMU_ERROR_NONE)
			mmu_raise_exception(err, address, MMU_TT_DWRITE);

		if (!cacheOn)
		{
			_vmem_writet<T>(physAddr, data);
			return;
		}

		const u32 index = lineIndex(address);
		cache_line& line = lines[index];
		const u32 tag = (physAddr >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
		{
			// miss and copy-back => read cache line
			if (copyBack)
			{
				if (line.dirty && line.valid)
					// write-back needed
					doWriteBack(index, line);
				line.address = tag;
				readCacheLine(physAddr, line);
			}
		}
		else if (!copyBack)
		{
			// hit and write-through => update cache
			*(T*)&line.data[physAddr & 0x1f] = data;
		}
		if (copyBack)
		{
			// copy-back => update cache and mark line as dirty
			line.dirty = true;
			*(T*)&line.data[physAddr & 0x1f] = data;
		}
		else
		{
			// write-through => update main ram
			_vmem_writet<T>(physAddr, data);
		}
	}

	void WriteBack(u32 address, bool write_back, bool invalidate)
	{
		u32 physAddr;
		bool cached = false;
		bool copyBack;
		u32 err = translateAddress<u8, MMU_TT_DWRITE>(address, physAddr, cached, copyBack);
		if (err != MMU_ERROR_NONE)
			mmu_raise_exception(err, address, MMU_TT_DWRITE);

		if (!cached)
			return;

		const u32 index = lineIndex(address);
		cache_line& line = lines[index];
		const u32 tag = (physAddr >> 10) & 0x7ffff;
		if (!line.valid || tag != line.address)
			return;
		if (write_back && line.dirty)
			doWriteBack(index, line);
		line.valid = !invalidate;
		line.dirty = false;
	}

	void Prefetch(u32 address)
	{
		address &= ~0x1F;
		u32 physAddr;
		bool cached;
		bool copyBack;
		u32 err = translateAddress<u8, MMU_TT_DREAD>(address, physAddr, cached, copyBack);
		if (err != MMU_ERROR_NONE || !cached)
			// ignore address translation errors
			return;

		const u32 index = lineIndex(address);
		cache_line& line = lines[index];
		const u32 tag = (physAddr >> 10) & 0x7ffff;
		if (line.valid && tag == line.address)
			return;
		if (line.valid && line.dirty)
			doWriteBack(index, line);
		line.address = tag;
		readCacheLine(physAddr, line);
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

	void Serialize(Serializer& ser) {
		ser << lines;
	}
	void Deserialize(Deserializer& deser) {
		deser >> lines;
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
			u32 physAddr;
			bool cached = false;
			bool copyBack;
			u32 err = translateAddress<u8, MMU_TT_DREAD>(data & ~0x3ff, physAddr, cached, copyBack);
			if (err == MMU_ERROR_TLB_MISS)
				// Ignore the write
				return;
			if (err != MMU_ERROR_NONE)
				mmu_raise_exception(err, data & ~0x3ff, MMU_TT_DREAD);

			u32 tag = (physAddr >> 10) & 0x7ffff;

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

	void WriteBackAll()
	{
		for (cache_line& line : lines)
		{
			if (line.valid && line.dirty)
				doWriteBack((u32)(&line - &lines[0]), line);
			line.valid = false;
			line.dirty = false;
		}
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

	template<class T, u32 ACCESS>
	u32 translateAddress(u32 address, u32& physAddr, bool& cached, bool& copyBack)
	{
		// Alignment errors
		if (address & (sizeof(T) - 1))
			return MMU_ERROR_BADADDR;
		if (ACCESS == MMU_TT_DWRITE && (address & 0xFC000000) == 0xE0000000)
		{
			// Store queues
			u32 rv;
			u32 lookup = mmu_full_SQ<MMU_TT_DWRITE>(address, rv);

			physAddr = address;
			return lookup;
		}
		const u32 area = address >> 29;
		const bool userMode = sr.MD == 0;

		// kernel mem protected in user mode
		if (userMode && (address & 0x80000000))
			return MMU_ERROR_BADADDR;

		cached = CCN_CCR.OCE == 1 && cachedArea(area);
		if (ACCESS == MMU_TT_DWRITE)
			// Use CCR.CB if P1 otherwise use !CCR.WT
			copyBack = area == 4 ? CCN_CCR.CB : !CCN_CCR.WT;

		if (CCN_MMUCR.AT == 0 || !translatedArea(area)
				// 7C000000 to 7FFFFFFF in P0/U0 not translated
				|| (address & 0xFC000000) == 0x7C000000)
		{
			physAddr = address;
		}
		else
		{
			const TLB_Entry *entry;
			u32 lookup = mmu_full_lookup(address, &entry, physAddr);

			if (lookup != MMU_ERROR_NONE)
				return lookup;

			//0X  & User mode-> protection violation
			//Priv mode protection
			if (userMode)
			{
				u32 md = entry->Data.PR >> 1;
				if (md == 0)
					return MMU_ERROR_PROTECTED;
			}
			//X0 -> read only
			//X1 -> read/write , can be FW
			if (ACCESS == MMU_TT_DWRITE)
			{
				if ((entry->Data.PR & 1) == 0)
					return MMU_ERROR_PROTECTED;
				if (entry->Data.D == 0)
					return MMU_ERROR_FIRSTWRITE;
				copyBack = copyBack && entry->Data.WT == 0;
			}
			cached = cached && entry->Data.C == 1;
			if ((physAddr & 0x1C000000) == 0x1C000000)
				// map 1C000000-1FFFFFFF to P4 memory-mapped registers
				physAddr |= 0xF0000000;

		}
		return MMU_ERROR_NONE;
	}

	std::array<cache_line, 512> lines;
};

extern sh4_ocache ocache;

template<class T>
T ReadCachedMem(u32 address)
{
	return ocache.ReadMem<T>(address);
}

template<class T>
void WriteCachedMem(u32 address, T data)
{
	ocache.WriteMem<T>(address, data);
}

static inline u16 IReadCachedMem(u32 address)
{
	return icache.ReadMem(address);
}
