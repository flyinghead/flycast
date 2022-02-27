#pragma once
#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/mem/_vmem.h"
#include "cfg/option.h"
#include "hw/sh4/dyna/ngen.h"

//Translation Types
//Opcode read
#define MMU_TT_IREAD 0
//Data write
#define MMU_TT_DWRITE 1
//Data write
#define MMU_TT_DREAD 2

//Return Values
//Translation was successful
#define MMU_ERROR_NONE	   0
//TLB miss
#define MMU_ERROR_TLB_MISS 1
//TLB Multihit
#define MMU_ERROR_TLB_MHIT 2
//Mem is read/write protected (depends on translation type)
#define MMU_ERROR_PROTECTED 3
//Mem is write protected , firstwrite
#define MMU_ERROR_FIRSTWRITE 4
//data-Opcode read/write misaligned
#define MMU_ERROR_BADADDR 5
//Can't Execute
#define MMU_ERROR_EXECPROT 6

struct TLB_Entry
{
	CCN_PTEH_type Address;
	CCN_PTEL_type Data;
	CCN_PTEA_type Assistance;
};

extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];
extern u32 sq_remap[64];

constexpr u32 fast_reg_lut[8] =
{
	0, 0, 0, 0	//P0-U0
	, 1		//P1
	, 1		//P2
	, 0		//P3
	, 1		//P4
};

constexpr u32 mmu_mask[4] =
{
	((0xFFFFFFFF) >> 10) << 10,	//1 kb page
	((0xFFFFFFFF) >> 12) << 12,	//4 kb page
	((0xFFFFFFFF) >> 16) << 16,	//64 kb page
	((0xFFFFFFFF) >> 20) << 20	//1 MB page
};

bool UTLB_Sync(u32 entry);
void ITLB_Sync(u32 entry);

bool mmu_match(u32 va, CCN_PTEH_type Address, CCN_PTEL_type Data);
void mmu_set_state();
void mmu_flush_table();
void mmu_raise_exception(u32 mmu_error, u32 address, u32 am);

static inline bool mmu_enabled()
{
	return config::FullMMU && CCN_MMUCR.AT == 1;
}

template<bool internal = false>
u32 mmu_full_lookup(u32 va, const TLB_Entry **entry, u32& rv);
u32 mmu_instruction_lookup(u32 va, const TLB_Entry **entry, u32& rv);
template<u32 translation_type>
u32 mmu_full_SQ(u32 va, u32& rv);

#ifdef FAST_MMU
static inline u32 mmu_instruction_translation(u32 va, u32& rv)
{
	if (va & 1)
		return MMU_ERROR_BADADDR;
	if (fast_reg_lut[va >> 29] != 0)
	{
		rv = va;
		return MMU_ERROR_NONE;
	}

	return mmu_full_lookup(va, nullptr, rv);
}
#else
u32 mmu_instruction_translation(u32 va, u32& rv);
#endif

template<u32 translation_type, typename T>
u32 mmu_data_translation(u32 va, u32& rv);
void DoMMUException(u32 addr, u32 mmu_error, u32 access_type);

inline static bool mmu_is_translated(u32 va, u32 size)
{
#ifndef FAST_MMU
	if (va & (size - 1))
		return true;
#endif

	if (fast_reg_lut[va >> 29] != 0)
		return false;

	if ((va & 0xFC000000) == 0x7C000000)
		// On-chip RAM area isn't translated
		return false;

	return true;
}

template<typename T> T DYNACALL mmu_ReadMem(u32 adr);
u16 DYNACALL mmu_IReadMem16(u32 addr);

template<typename T> void DYNACALL mmu_WriteMem(u32 adr, T data);

bool mmu_TranslateSQW(u32 adr, u32* out);

// maps 4K virtual page number to physical address
extern u32 mmuAddressLUT[0x100000];

static inline void mmuAddressLUTFlush(bool full) {
	if (full)
		memset(mmuAddressLUT, 0, sizeof(mmuAddressLUT) / 2);	// flush user memory
	else
	{
		constexpr u32 slotPages = (32 * 1024 * 1024) >> 12;
		memset(mmuAddressLUT, 0, slotPages * sizeof(u32));		// flush slot 0
	}
}

static inline u32 mmuDynarecLookup(u32 vaddr, u32 write, u32 pc)
{
	u32 paddr;
	u32 rv;
	if (write)
		rv = mmu_data_translation<MMU_TT_DWRITE, u32>(vaddr, paddr);
	else
		rv = mmu_data_translation<MMU_TT_DREAD, u32>(vaddr, paddr);
	if (unlikely(rv != MMU_ERROR_NONE))
	{
		Sh4cntx.pc = pc;
		DoMMUException(vaddr, rv, write ? MMU_TT_DWRITE : MMU_TT_DREAD);
		host_context_t ctx;
		ngen_HandleException(ctx);
		((void (*)())ctx.pc)();
		// not reached
		return 0;
	}
	if (vaddr >> 31 == 0)
		mmuAddressLUT[vaddr >> 12] = paddr & ~0xfff;

	return paddr;
}

void MMU_init();
void MMU_reset();
void MMU_term();
