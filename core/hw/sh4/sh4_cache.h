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
// SH4 instruction cache implementation
//
class sh4_icache
{
public:
	u16 ReadMem(u32 address)
	{
		if ((address & 0xE0000000) == 0xA0000000		// P2, P4: non-cacheable
				|| (address & 0xE0000000) == 0xE0000000
				|| !CCN_CCR.ICE)						// Instruction cache disabled
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
			u32 *p = (u32 *)line.data;
			for (int i = 0; i < 32; i += 4)
				*p++ = _vmem_ReadMem32(line_addr + i);
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

private:
	struct cache_line {
		bool valid;
		u32 address;
		u8 data[32];
	};

	std::array<cache_line, 256> lines;
};

extern sh4_icache icache;
