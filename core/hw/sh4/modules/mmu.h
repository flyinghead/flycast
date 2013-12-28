#pragma once
#include "types.h"
#include "hw/sh4/sh4_mmr.h"

struct TLB_Entry
{
	CCN_PTEH_type Address;
	CCN_PTEL_type Data;
};

extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];
extern u32 sq_remap[64];

//These are working only for SQ remaps on ndce
void UTLB_Sync(u32 entry);
void ITLB_Sync(u32 entry);

#define mmu_TranslateSQW(adr) (sq_remap[(adr>>20)&0x3F] | (adr & 0xFFFE0))
