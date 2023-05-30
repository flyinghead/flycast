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
#include "sh4_if.h"
#include "sh4_mem.h"
#include "modules/mmu.h"
#include "hw/pvr/pvr_mem.h"

static u32 CCN_QACR_TR[2];

template<bool mmu_on>
void DYNACALL do_sqw(u32 Dest, const SQBuffer *sqb)
{
	u32 Address;

	//Translate the SQ addresses as needed
	if (mmu_on)
	{
		mmu_TranslateSQW(Dest, &Address);
	}
	else
	{
		//sanity/optimisation check
		//verify(CCN_QACR_TR[0]==CCN_QACR_TR[1]);

		u32 QACR = CCN_QACR_TR[0];
		//QACR has already 0xE000_0000
		Address = QACR + (Dest & ~0x1f);
	}

	if (((Address >> 26) & 7) != 4)//Area 4
	{
		const SQBuffer *sq = &sqb[(Dest >> 5) & 1];
		WriteMemBlock_nommu_sq(Address, sq);
	}
	else
	{
		TAWriteSQ(Address, sqb);
	}
}

void DYNACALL do_sqw_mmu(u32 dst) {
	do_sqw<true>(dst, sq_both);
}

static void DYNACALL do_sqw_simplemmu(u32 dst, const SQBuffer *sqb) {
	do_sqw<true>(dst, sqb);
}

//yes, this micro optimization makes a difference
static void DYNACALL do_sqw_nommu_area_3(u32 dst, const SQBuffer *sqb)
{
	SQBuffer *pmem = (SQBuffer *)((u8 *)sqb + sizeof(Sh4RCB::sq_buffer) + sizeof(Sh4RCB::cntx) + 0x0C000000);
	pmem += (dst & (RAM_SIZE_MAX - 1)) >> 5;
	*pmem = sqb[(dst >> 5) & 1];
}

static void DYNACALL do_sqw_nommu_area_3_nonvmem(u32 dst, const SQBuffer *sqb)
{
	u8* pmem = &mem_b[0];

	memcpy((SQBuffer *)&pmem[dst & (RAM_MASK - 0x1F)], &sqb[(dst >> 5) & 1], sizeof(SQBuffer));
}

static void DYNACALL do_sqw_nommu_full(u32 dst, const SQBuffer *sqb) {
	do_sqw<false>(dst, sqb);
}

void setSqwHandler()
{
	if (CCN_MMUCR.AT == 1)
	{
		do_sqw_nommu = &do_sqw_simplemmu;
	}
	else
	{
		u32 area = CCN_QACR0.Area;

		CCN_QACR_TR[0] = (area << 26) - 0xE0000000; //-0xE0000000 because 0xE0000000 is added on the translation again ...

		switch (area)
		{
		case 3:
			if (addrspace::virtmemEnabled())
				do_sqw_nommu = &do_sqw_nommu_area_3;
			else
				do_sqw_nommu = &do_sqw_nommu_area_3_nonvmem;
			break;

		case 4:
			do_sqw_nommu = &TAWriteSQ;
			break;

		default:
			do_sqw_nommu = &do_sqw_nommu_full;
			break;
		}
	}
}
