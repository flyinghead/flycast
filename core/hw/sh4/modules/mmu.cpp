#include "mmu.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "debug/gdb_server.h"

TLB_Entry UTLB[64];
TLB_Entry ITLB[4];
static u32 ITLB_LRU_USE[64];

//SQ fast remap , mainly hackish , assumes 1MB pages
//max 64MB can be remapped on SQ
// Used when FullMMU is off
u32 sq_remap[64];

/*
MMU support code
This is mostly hacked-on as the core was never meant to have mmu support

There are two modes, one with 'full' mmu emulation (for wince/bleem/wtfever)
and a fast-hack mode for 1mb sqremaps (for katana)
*/
#include "mmu.h"
#include "hw/sh4/sh4_if.h"
#include "ccn.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_mem.h"

#include "hw/mem/_vmem.h"

//#define TRACE_WINCE_SYSCALLS

#ifdef TRACE_WINCE_SYSCALLS
#include "wince.h"
u32 unresolved_ascii_string;
u32 unresolved_unicode_string;
#endif

#define printf_mmu(...) DEBUG_LOG(SH4, __VA_ARGS__)

constexpr u32 ITLB_LRU_OR[4] =
{
	0x00,//000xxx
	0x20,//1xx00x
	0x14,//x1x1x0
	0x0B,//xx1x11
};
constexpr u32 ITLB_LRU_AND[4] =
{
	0x07,//000xxx
	0x39,//1xx00x
	0x3E,//x1x1x0
	0x3F,//xx1x11
};

#ifndef FAST_MMU
//sync mem mapping to mmu , suspend compiled blocks if needed.entry is a UTLB entry # , -1 is for full sync
bool UTLB_Sync(u32 entry)
{
	printf_mmu("UTLB MEM remap %d : 0x%X to 0x%X : %d asid %d size %d", entry, UTLB[entry].Address.VPN << 10, UTLB[entry].Data.PPN << 10, UTLB[entry].Data.V,
			UTLB[entry].Address.ASID, UTLB[entry].Data.SZ0 + UTLB[entry].Data.SZ1 * 2);
	if (UTLB[entry].Data.V == 0)
		return true;

	if ((UTLB[entry].Address.VPN & (0xFC000000 >> 10)) == (0xE0000000 >> 10))
	{
		// Used when FullMMU is off
		u32 vpn_sq = ((UTLB[entry].Address.VPN & 0x7FFFF) >> 10) & 0x3F;//upper bits are always known [0xE0/E1/E2/E3]
		sq_remap[vpn_sq] = UTLB[entry].Data.PPN << 10;

		return true;
	}
	else
	{
		return false;
	}
}
//sync mem mapping to mmu , suspend compiled blocks if needed.entry is a ITLB entry # , -1 is for full sync
void ITLB_Sync(u32 entry)
{
	printf_mmu("ITLB MEM remap %d : 0x%X to 0x%X : %d", entry, ITLB[entry].Address.VPN << 10, ITLB[entry].Data.PPN << 10, ITLB[entry].Data.V);
}
#endif

template<typename F>
static void mmuException(u32 mmu_error, u32 address, u32 am, F raise)
{
	printf_mmu("MMU exception -> pc = 0x%X : ", next_pc);
	CCN_TEA = address;
	CCN_PTEH.VPN = address >> 10;

	switch (mmu_error)
	{
		//No error
	case MMU_ERROR_NONE:
		die("Error: mmu_error == MMU_ERROR_NONE)");
		return;

		//TLB miss
	case MMU_ERROR_TLB_MISS:
		printf_mmu("MMU_ERROR_UTLB_MISS 0x%X, handled", address);
		if (am == MMU_TT_DWRITE)			//WTLBMISS - Write Data TLB Miss Exception
			raise(0x60, 0x400);
		else if (am == MMU_TT_DREAD)		//RTLBMISS - Read Data TLB Miss Exception
			raise(0x40, 0x400);
		else							//ITLBMISS - Instruction TLB Miss Exception
			raise(0x40, 0x400);
		return;

		//TLB Multihit
	case MMU_ERROR_TLB_MHIT:
		INFO_LOG(SH4, "MMU_ERROR_TLB_MHIT @ 0x%X", address);
		break;

		//Mem is read/write protected (depends on translation type)
	case MMU_ERROR_PROTECTED:
		printf_mmu("MMU_ERROR_PROTECTED 0x%X, handled", address);
		if (am == MMU_TT_DWRITE)			//WRITEPROT - Write Data TLB Protection Violation Exception
			raise(0xC0, 0x100);
		else if (am == MMU_TT_DREAD)		//READPROT - Data TLB Protection Violation Exception
			raise(0xA0, 0x100);
		else								//READPROT - Instr TLB Protection Violation Exception
			raise(0xA0, 0x100);
		return;

		//Mem is write protected , firstwrite
	case MMU_ERROR_FIRSTWRITE:
		printf_mmu("MMU_ERROR_FIRSTWRITE");
		verify(am == MMU_TT_DWRITE);
		//FIRSTWRITE - Initial Page Write Exception
		raise(0x80, 0x100);
		return;

		//data read/write missasligned
	case MMU_ERROR_BADADDR:
		if (am == MMU_TT_DWRITE)			//WADDERR - Write Data Address Error
		{
			printf_mmu("MMU_ERROR_BADADDR(dw) 0x%X", address);
			raise(0x100, 0x100);
		}
		else if (am == MMU_TT_DREAD)		//RADDERR - Read Data Address Error
		{
			printf_mmu("MMU_ERROR_BADADDR(dr) 0x%X", address);
			raise(0xE0, 0x100);
		}
		else							//IADDERR - Instruction Address Error
		{
#ifdef TRACE_WINCE_SYSCALLS
			if (!print_wince_syscall(address))
#endif
				printf_mmu("MMU_ERROR_BADADDR(i) 0x%X", address);
			raise(0xE0, 0x100);
		}
		return;

		//Can't Execute
	case MMU_ERROR_EXECPROT:
		INFO_LOG(SH4, "MMU_ERROR_EXECPROT 0x%X", address);

		//EXECPROT - Instruction TLB Protection Violation Exception
		raise(0xA0, 0x100);
		return;
	}

	die("Unknown mmu_error");
}

void mmu_raise_exception(u32 mmu_error, u32 address, u32 am)
{
	mmuException(mmu_error, address, am, [](u32 event, u32 vector) {
		debugger::debugTrap(event);	// FIXME CCN_TEA and CCN_PTEH have been updated already
		SH4ThrownException ex { next_pc - 2, event, vector };
		throw ex;
	});
}


void DoMMUException(u32 address, u32 mmu_error, u32 access_type)
{
	mmuException(mmu_error, address, access_type, [](u32 event, u32 vector) {
		Do_Exception(next_pc, event, vector);
	});
}

bool mmu_match(u32 va, CCN_PTEH_type Address, CCN_PTEL_type Data)
{
	if (Data.V == 0)
		return false;

	u32 sz = Data.SZ1 * 2 + Data.SZ0;
	u32 mask = mmu_mask[sz];

	if ((((Address.VPN << 10)&mask) == (va&mask)))
	{
		bool asid_match = (Data.SH == 0) && ((sr.MD == 0) || (CCN_MMUCR.SV == 0));

		if ((asid_match == false) || (Address.ASID == CCN_PTEH.ASID))
		{
			return true;
		}
	}

	return false;
}

#ifndef FAST_MMU
//Do a full lookup on the UTLB entry's
template<bool internal>
u32 mmu_full_lookup(u32 va, const TLB_Entry** tlb_entry_ret, u32& rv)
{
	if (!internal)
	{
		CCN_MMUCR.URC++;
		if (CCN_MMUCR.URB == CCN_MMUCR.URC)
			CCN_MMUCR.URC = 0;
	}

	u32 entry = -1;
	u32 nom = 0;

	for (u32 i = 0; i<64; i++)
	{
		//verify(sz!=0);
		TLB_Entry *tlb_entry = &UTLB[i];
		if (mmu_match(va, tlb_entry->Address, tlb_entry->Data))
		{
			entry = i;
			nom++;
			u32 sz = tlb_entry->Data.SZ1 * 2 + tlb_entry->Data.SZ0;
			u32 mask = mmu_mask[sz];
			//VPN->PPN | low bits
			rv = ((tlb_entry->Data.PPN << 10) & mask) | (va & (~mask));
		}
	}

	if (nom != 1)
	{
		if (nom)
		{
			return MMU_ERROR_TLB_MHIT;
		}
		else
		{
			return MMU_ERROR_TLB_MISS;
		}
	}

	*tlb_entry_ret = &UTLB[entry];

	return MMU_ERROR_NONE;
}

//Simple QACR translation for mmu (when AT is off)
static u32 mmu_QACR_SQ(u32 va)
{
	u32 QACR;

	//default to sq 0
	QACR = CCN_QACR_TR[0];
	//sq1 ? if so use QACR1
	if (va & 0x20)
		QACR = CCN_QACR_TR[1];
	va &= ~0x1f;
	return QACR + va;
}

template<u32 translation_type>
u32 mmu_full_SQ(u32 va, u32& rv)
{

	if ((va & 3) || (CCN_MMUCR.SQMD == 1 && sr.MD == 0))
	{
		//here, or after ?
		return MMU_ERROR_BADADDR;
	}

	if (CCN_MMUCR.AT)
	{
		//Address=Dest&0xFFFFFFE0;

		const TLB_Entry *entry;
		u32 lookup = mmu_full_lookup(va, &entry, rv);

		rv &= ~31;//lower 5 bits are forced to 0

		if (lookup != MMU_ERROR_NONE)
			return lookup;

		u32 md = entry->Data.PR >> 1;

		//Priv mode protection
		if ((md == 0) && sr.MD == 0)
		{
			return MMU_ERROR_PROTECTED;
		}

		//Write Protection (Lock or FW)
		if (translation_type == MMU_TT_DWRITE)
		{
			if ((entry->Data.PR & 1) == 0)
				return MMU_ERROR_PROTECTED;
			else if (entry->Data.D == 0)
				return MMU_ERROR_FIRSTWRITE;
		}
	}
	else
	{
		rv = mmu_QACR_SQ(va);
	}
	return MMU_ERROR_NONE;
}
template u32 mmu_full_SQ<MMU_TT_DREAD>(u32 va, u32& rv);
template u32 mmu_full_SQ<MMU_TT_DWRITE>(u32 va, u32& rv);

template<u32 translation_type, typename T>
u32 mmu_data_translation(u32 va, u32& rv)
{
	if (va & (sizeof(T) - 1))
		return MMU_ERROR_BADADDR;

	if (translation_type == MMU_TT_DWRITE)
	{
		if ((va & 0xFC000000) == 0xE0000000)
		{
			u32 lookup = mmu_full_SQ<translation_type>(va, rv);
			if (lookup != MMU_ERROR_NONE)
				return lookup;

			rv = va;	//SQ writes are not translated, only write backs are.
			return MMU_ERROR_NONE;
		}
	}

	if ((sr.MD == 0) && (va & 0x80000000) != 0)
	{
		//if on kernel, and not SQ addr -> error
		return MMU_ERROR_BADADDR;
	}

	if ((va & 0xFC000000) == 0x7C000000)
	{
		// 7C000000 to 7FFFFFFF in P0/U0 not translated
		rv = va;
		return MMU_ERROR_NONE;
	}

	if (fast_reg_lut[va >> 29] != 0)
	{
		// P1, P2 and P4 aren't translated
		rv = va;
		return MMU_ERROR_NONE;
	}

	const TLB_Entry *entry;
	u32 lookup = mmu_full_lookup(va, &entry, rv);

	if (lookup != MMU_ERROR_NONE)
		return lookup;

#ifdef TRACE_WINCE_SYSCALLS
	if (unresolved_unicode_string != 0)
	{
		if (va == unresolved_unicode_string)
		{
			unresolved_unicode_string = 0;
			INFO_LOG(SH4, "RESOLVED %s", get_unicode_string(va).c_str());
		}
	}
#endif

	u32 md = entry->Data.PR >> 1;

	//0X  & User mode-> protection violation
	//Priv mode protection
	if ((md == 0) && sr.MD == 0)
	{
		return MMU_ERROR_PROTECTED;
	}

	//X0 -> read olny
	//X1 -> read/write , can be FW

	//Write Protection (Lock or FW)
	if (translation_type == MMU_TT_DWRITE)
	{
		if ((entry->Data.PR & 1) == 0)
			return MMU_ERROR_PROTECTED;
		else if (entry->Data.D == 0)
			return MMU_ERROR_FIRSTWRITE;
	}
	if ((rv & 0x1C000000) == 0x1C000000)
		// map 1C000000-1FFFFFFF to P4 memory-mapped registers
		rv |= 0xF0000000;

	return MMU_ERROR_NONE;
}
template u32 mmu_data_translation<MMU_TT_DREAD, u8>(u32 va, u32& rv);
template u32 mmu_data_translation<MMU_TT_DREAD, u16>(u32 va, u32& rv);
template u32 mmu_data_translation<MMU_TT_DREAD, u32>(u32 va, u32& rv);
template u32 mmu_data_translation<MMU_TT_DREAD, u64>(u32 va, u32& rv);

template u32 mmu_data_translation<MMU_TT_DWRITE, u8>(u32 va, u32& rv);
template u32 mmu_data_translation<MMU_TT_DWRITE, u16>(u32 va, u32& rv);
template u32 mmu_data_translation<MMU_TT_DWRITE, u32>(u32 va, u32& rv);
template u32 mmu_data_translation<MMU_TT_DWRITE, u64>(u32 va, u32& rv);

u32 mmu_instruction_translation(u32 va, u32& rv)
{
	if (va & 1)
	{
		return MMU_ERROR_BADADDR;
	}
	if ((sr.MD == 0) && (va & 0x80000000) != 0)
	{
		//if SQ disabled , or if if SQ on but out of SQ mem then BAD ADDR ;)
		if (va >= 0xE0000000)
			return MMU_ERROR_BADADDR;
	}

	if ((CCN_MMUCR.AT == 0) || (fast_reg_lut[va >> 29] != 0))
	{
		rv = va;
		return MMU_ERROR_NONE;
	}

	const TLB_Entry *entry;
	u32 lookup = mmu_instruction_lookup(va, &entry, rv);
	if (lookup != MMU_ERROR_NONE)
		return lookup;

	u32 md = entry->Data.PR >> 1;

	//0X  & User mode-> protection violation
	//Priv mode protection
	if (md == 0 && sr.MD == 0)
		return MMU_ERROR_PROTECTED;

	return MMU_ERROR_NONE;
}
#endif

u32 mmu_instruction_lookup(u32 va, const TLB_Entry** tlb_entry_ret, u32& rv)
{
	bool mmach = false;
retry_ITLB_Match:
	u32 entry = 4;
	u32 nom = 0;
	for (u32 i = 0; i<4; i++)
	{
		if (ITLB[i].Data.V == 0)
			continue;
		u32 sz = ITLB[i].Data.SZ1 * 2 + ITLB[i].Data.SZ0;
		u32 mask = mmu_mask[sz];

		if ((((ITLB[i].Address.VPN << 10)&mask) == (va&mask)))
		{
			bool asid_match = (ITLB[i].Data.SH == 0) && ((sr.MD == 0) || (CCN_MMUCR.SV == 0));

			if ((asid_match == false) || (ITLB[i].Address.ASID == CCN_PTEH.ASID))
			{
				//verify(sz!=0);
				entry = i;
				nom++;
				//VPN->PPN | low bits
				rv = ((ITLB[i].Data.PPN << 10)&mask) | (va&(~mask));
			}
		}
	}

	if (entry == 4)
	{
		verify(mmach == false);
		const TLB_Entry *tlb_entry;
		u32 lookup = mmu_full_lookup(va, &tlb_entry, rv);

		if (lookup != MMU_ERROR_NONE)
			return lookup;

		u32 replace_index = ITLB_LRU_USE[CCN_MMUCR.LRUI];
		verify(replace_index != 0xFFFFFFFF);
		ITLB[replace_index] = *tlb_entry;
		entry = replace_index;
		ITLB_Sync(entry);
		mmach = true;
		goto retry_ITLB_Match;
	}
	else if (nom != 1)
	{
		if (nom)
		{
			return MMU_ERROR_TLB_MHIT;
		}
		else
		{
			return MMU_ERROR_TLB_MISS;
		}
	}

	CCN_MMUCR.LRUI &= ITLB_LRU_AND[entry];
	CCN_MMUCR.LRUI |= ITLB_LRU_OR[entry];
	*tlb_entry_ret = &ITLB[entry];

	return MMU_ERROR_NONE;
}

void mmu_set_state()
{
	if (CCN_MMUCR.AT == 1 && config::FullMMU)
		NOTICE_LOG(SH4, "Enabling Full MMU support");

	SetMemoryHandlers();
}

void MMU_init()
{
	memset(ITLB_LRU_USE, 0xFF, sizeof(ITLB_LRU_USE));
	for (u32 e = 0; e<4; e++)
	{
		u32 match_key = ((~ITLB_LRU_AND[e]) & 0x3F);
		u32 match_mask = match_key | ITLB_LRU_OR[e];
		for (u32 i = 0; i<64; i++)
		{
			if ((i & match_mask) == match_key)
			{
				verify(ITLB_LRU_USE[i] == 0xFFFFFFFF);
				ITLB_LRU_USE[i] = e;
			}
		}
	}
	mmu_set_state();
	// pre-fill kernel memory
	for (u32 vpn = ARRAY_SIZE(mmuAddressLUT) / 2; vpn < ARRAY_SIZE(mmuAddressLUT); vpn++)
		mmuAddressLUT[vpn] = vpn << 12;
}


void MMU_reset()
{
	memset(UTLB, 0, sizeof(UTLB));
	memset(ITLB, 0, sizeof(ITLB));
	mmu_set_state();
	mmu_flush_table();
}

void MMU_term()
{
}

#ifndef FAST_MMU
void mmu_flush_table()
{
	//printf("MMU tables flushed\n");

	ITLB[0].Data.V = 0;
	ITLB[1].Data.V = 0;
	ITLB[2].Data.V = 0;
	ITLB[3].Data.V = 0;

	for (u32 i = 0; i < 64; i++)
		UTLB[i].Data.V = 0;
	mmuAddressLUTFlush(true);
}
#endif

template<typename T>
T DYNACALL mmu_ReadMem(u32 adr)
{
	u32 addr;
	u32 rv = mmu_data_translation<MMU_TT_DREAD, T>(adr, addr);
	if (rv != MMU_ERROR_NONE)
		mmu_raise_exception(rv, adr, MMU_TT_DREAD);
	return _vmem_readt<T, T>(addr);
}
template u8 mmu_ReadMem(u32 adr);
template u16 mmu_ReadMem(u32 adr);
template u32 mmu_ReadMem(u32 adr);
template u64 mmu_ReadMem(u32 adr);

u16 DYNACALL mmu_IReadMem16(u32 vaddr)
{
	u32 addr;
	u32 rv = mmu_instruction_translation(vaddr, addr);
	if (rv != MMU_ERROR_NONE)
		mmu_raise_exception(rv, vaddr, MMU_TT_IREAD);
	return _vmem_ReadMem16(addr);
}

template<typename T>
void DYNACALL mmu_WriteMem(u32 adr, T data)
{
	u32 addr;
	u32 rv = mmu_data_translation<MMU_TT_DWRITE, T>(adr, addr);
	if (rv != MMU_ERROR_NONE)
		mmu_raise_exception(rv, adr, MMU_TT_DWRITE);
	_vmem_writet<T>(addr, data);
}
template void mmu_WriteMem(u32 adr, u8 data);
template void mmu_WriteMem(u32 adr, u16 data);
template void mmu_WriteMem(u32 adr, u32 data);
template void mmu_WriteMem(u32 adr, u64 data);

bool mmu_TranslateSQW(u32 adr, u32* out)
{
	if (!config::FullMMU)
	{
		//This will only work for 1 mb pages .. hopefully nothing else is used
		//*FIXME* to work for all page sizes ?

		*out = sq_remap[(adr >> 20) & 0x3F] | (adr & 0xFFFE0);
	}
	else
	{
		u32 addr;
		u32 tv = mmu_full_SQ<MMU_TT_DREAD>(adr, addr);
		if (tv != MMU_ERROR_NONE)
		{
			mmu_raise_exception(tv, adr, MMU_TT_DREAD);
			return false;
		}

		*out = addr;
	}

	return true;
}
