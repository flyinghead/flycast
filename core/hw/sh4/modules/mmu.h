#pragma once
#include "types.h"
#include "hw/sh4/sh4_mmr.h"

//Translation Types
//Opcode read
#define MMU_TT_IREAD 0
//Data write
#define MMU_TT_DWRITE 1
//Data write
#define MMU_TT_DREAD 2

struct TLB_Entry
{
	CCN_PTEH_type Address;
	CCN_PTEL_type Data;
	CCN_PTEA_type Assistance;
};

extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];
extern u32 sq_remap[64];

//These are working only for SQ remaps on ndce
bool UTLB_Sync(u32 entry);
void ITLB_Sync(u32 entry);

bool mmu_match(u32 va, CCN_PTEH_type Address, CCN_PTEL_type Data);
void mmu_set_state();
void mmu_flush_table();

static INLINE bool mmu_enabled()
{
#ifndef NO_MMU
	return settings.dreamcast.FullMMU && CCN_MMUCR.AT == 1;
#else
	return false;
#endif
}

template<bool internal = false>
u32 mmu_full_lookup(u32 va, const TLB_Entry **entry, u32& rv);
void mmu_instruction_translation(u32 va, u32& rv, bool& shared);
template<u32 translation_type, typename T>
extern void mmu_data_translation(u32 va, u32& rv);

#if defined(NO_MMU)
	bool inline mmu_TranslateSQW(u32 addr, u32* mapped) {
		*mapped = sq_remap[(addr>>20)&0x3F] | (addr & 0xFFFE0);
		return true;
	}
	void inline mmu_flush_table() {}
#else
	u8 DYNACALL mmu_ReadMem8(u32 addr);
	u16 DYNACALL mmu_ReadMem16(u32 addr);
	u16 DYNACALL mmu_IReadMem16(u32 addr);
	u32 DYNACALL mmu_ReadMem32(u32 addr);
	u64 DYNACALL mmu_ReadMem64(u32 addr);

	void DYNACALL mmu_WriteMem8(u32 addr, u8 data);
	void DYNACALL mmu_WriteMem16(u32 addr, u16 data);
	void DYNACALL mmu_WriteMem32(u32 addr, u32 data);
	void DYNACALL mmu_WriteMem64(u32 addr, u64 data);
	
	bool mmu_TranslateSQW(u32 addr, u32* mapped);
#endif
