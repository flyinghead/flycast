#pragma once
#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/sh4/dyna/ngen.h"

//Translation Types
//Opcode read
#define MMU_TT_IREAD 0
//Data write
#define MMU_TT_DWRITE 1
//Data write
#define MMU_TT_DREAD 2

enum class MmuError
{
	//Translation was successful
	NONE,
	//TLB miss
	TLB_MISS,
	//TLB Multihit
	TLB_MHIT,
	//Mem is read/write protected (depends on translation type)
	PROTECTED,
	//Mem is write protected , firstwrite
	FIRSTWRITE,
	//data-Opcode read/write misaligned
	BADADDR
};

struct TLB_Entry
{
	CCN_PTEH_type Address;
	CCN_PTEL_type Data;
	CCN_PTEA_type Assistance;
};

extern TLB_Entry UTLB[64];
extern TLB_Entry ITLB[4];

namespace mmu
{

extern bool mmuOn;

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

class MMU
{
public:
	static MmuError fullLookup(u32 va, const TLB_Entry **entry, u32& rv);
	static MmuError instructionLookup(u32 va, const TLB_Entry** tlb_entry_ret, u32& rv);
	template<u32 translation_type>
	static MmuError translateData(u32 va, u32& rv);
	static MmuError translateInstruction(u32 va, u32& rv);
	template<u32 translation_type>
	static MmuError fullSQ(u32 va, u32& rv);
	static void UTLBSync(u32 entry);
	static void ITLBSync(u32 entry);
	static void flushTable();
	static void setMemoryHandlers();
};

class WinCEMMU
{
public:
	static MmuError fullLookup(u32 va, const TLB_Entry **entry, u32& rv);
	static MmuError instructionLookup(u32 va, const TLB_Entry** tlb_entry_ret, u32& rv);
	template<u32 translation_type>
	static MmuError translateData(u32 va, u32& rv);
	static MmuError translateInstruction(u32 va, u32& rv);
	template<u32 translation_type>
	static MmuError fullSQ(u32 va, u32& rv);
	static void UTLBSync(u32 entry);
	static void ITLBSync(u32 entry);
	static void flushTable();
	static void setMemoryHandlers();
};

static inline MmuError fullLookup(u32 va, const TLB_Entry **entry, u32& rv)
{
	if (settings.content.windowsCE)
		return WinCEMMU::fullLookup(va, entry, rv);
	else
		return MMU::fullLookup(va, entry, rv);
}

template<u32 translation_type>
static MmuError translateData(u32 va, u32& rv)
{
	if (settings.content.windowsCE)
		return WinCEMMU::translateData<translation_type>(va, rv);
	else
		return MMU::translateData<translation_type>(va, rv);
}

static inline MmuError translateInstruction(u32 va, u32& rv)
{
	if (settings.content.windowsCE)
		return WinCEMMU::translateInstruction(va, rv);
	else
		return MMU::translateInstruction(va, rv);
}

void setMemoryHandlers();

static inline void UTLBSync(u32 entry)
{
	if (settings.content.windowsCE)
		WinCEMMU::UTLBSync(entry);
	else
		MMU::UTLBSync(entry);
}
static inline void ITLBSync(u32 entry)
{
	if (settings.content.windowsCE)
		WinCEMMU::ITLBSync(entry);
	else
		MMU::ITLBSync(entry);
}

bool match(u32 va, CCN_PTEH_type Address, CCN_PTEL_type Data);

void setState();

static inline void flushTable()
{
	using namespace mmu;
	if (settings.content.windowsCE)
		WinCEMMU::flushTable();
	else
		MMU::flushTable();
}

[[noreturn]] void raiseException(MmuError mmu_error, u32 address, u32 am);

static inline bool enabled()
{
	return mmuOn;
}

void doException(u32 addr, MmuError mmu_error, u32 access_type);

inline static bool isTranslated(u32 va, u32 size)
{
	if (va & (std::min(size, 4u) - 1))
		return true;

	if (fast_reg_lut[va >> 29] != 0)
		return false;

	if ((va & 0xFC000000) == 0x7C000000)
		// On-chip RAM area isn't translated
		return false;

	return true;
}

template<bool wince>
void translateSQW(u32 adr, u32* out);

#ifdef FAST_MMU
// maps 4K virtual page number to physical address
extern u32 addressLUT[0x100000];

static inline void addressLUTFlush(bool full)
{
	if (full || !settings.content.windowsCE) {
		memset(addressLUT, 0, sizeof(addressLUT) / 2);	// flush user memory (P0)
	}
	else
	{
		constexpr u32 slotPages = (32 * 1024 * 1024) >> 12;
		memset(addressLUT, 0, slotPages * sizeof(u32));		// flush slot 0 (windows CE)
	}
}
#endif

#if FEAT_SHREC != DYNAREC_NONE

#ifdef _MSC_VER
using dynlookup_t = u32 (DYNACALL *)(u32, u32);
#else
using dynlookup_t = u32 DYNACALL (*)(u32, u32);
#endif
dynlookup_t getDynarecLookupFunc(bool write);

#endif

void init();
void reset();
void term();

void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

}
