#include "mmu.h"
#include "hw/mem/addrspace.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h"
#include "debug/gdb_server.h"
#include "serialize.h"

TLB_Entry UTLB[64];
TLB_Entry ITLB[4];
static u32 ITLB_LRU_USE[64];
bool mmuOn;

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
#ifndef FAST_MMU
#include "ccn.h"
#endif
#include "hw/sh4/sh4_mem.h"

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
static void mmuException(MmuError mmu_error, u32 address, u32 am, F raise)
{
	printf_mmu("MMU exception -> pc = 0x%X : ", next_pc);
	CCN_TEA = address;
	CCN_PTEH.VPN = address >> 10;

	switch (mmu_error)
	{
	case MmuError::NONE:
		die("Error: mmu_error == MmuError::NONE)");
		return;

	case MmuError::TLB_MISS:
		printf_mmu("MmuError::UTLB_MISS 0x%X, handled", address);
		if (am == MMU_TT_DWRITE)
			raise(Sh4Ex_TlbMissWrite);
		else
			raise(Sh4Ex_TlbMissRead);
		return;

	case MmuError::TLB_MHIT:
		ERROR_LOG(SH4, "MmuError::TLB_MHIT @ 0x%X", address);
		raise(Sh4Ex_TlbMultiHit);
		break;

	//Mem is read/write protected (depends on translation type)
	case MmuError::PROTECTED:
		printf_mmu("MmuError::PROTECTED 0x%X, handled", address);
		if (am == MMU_TT_DWRITE)
			raise(Sh4Ex_TlbProtViolWrite);
		else
			raise(Sh4Ex_TlbProtViolRead);
		return;

	//Mem is write protected , firstwrite
	case MmuError::FIRSTWRITE:
		printf_mmu("MmuError::FIRSTWRITE");
		verify(am == MMU_TT_DWRITE);
		raise(Sh4Ex_TlbInitPageWrite);
		return;

	//data read/write misaligned
	case MmuError::BADADDR:
		if (am == MMU_TT_DWRITE)			//WADDERR - Write Data Address Error
		{
			printf_mmu("MmuError::BADADDR(dw) 0x%X", address);
			raise(Sh4Ex_AddressErrorWrite);
		}
		else if (am == MMU_TT_DREAD)		//RADDERR - Read Data Address Error
		{
			printf_mmu("MmuError::BADADDR(dr) 0x%X", address);
			raise(Sh4Ex_AddressErrorRead);
		}
		else							//IADDERR - Instruction Address Error
		{
#ifdef TRACE_WINCE_SYSCALLS
			if (!print_wince_syscall(address))
#endif
				printf_mmu("MmuError::BADADDR(i) 0x%X", address);
			raise(Sh4Ex_AddressErrorRead);
		}
		return;

	default:
		die("Unknown mmu_error");
	}
}

[[noreturn]] void mmu_raise_exception(MmuError mmu_error, u32 address, u32 am)
{
	mmuException(mmu_error, address, am, [](Sh4ExceptionCode event) {
		debugger::debugTrap(event);	// FIXME CCN_TEA and CCN_PTEH have been updated already

		throw SH4ThrownException(next_pc - 2, event);
	});
	die("Unknown mmu_error");
}


void DoMMUException(u32 address, MmuError mmu_error, u32 access_type)
{
	mmuException(mmu_error, address, access_type, [](Sh4ExceptionCode event) {
		Do_Exception(next_pc, event);
	});
}

bool mmu_match(u32 va, CCN_PTEH_type Address, CCN_PTEL_type Data)
{
	if (Data.V == 0)
		return false;

	u32 sz = Data.SZ1 * 2 + Data.SZ0;
	u32 mask = mmu_mask[sz];

	if ((((Address.VPN << 10) & mask) == (va & mask)))
	{
		bool needAsidMatch = Data.SH == 0 && (sr.MD == 0 || CCN_MMUCR.SV == 0);

		if (!needAsidMatch || Address.ASID == CCN_PTEH.ASID)
			return true;
	}

	return false;
}

#ifndef FAST_MMU
//Do a full lookup on the UTLB entry's
MmuError mmu_full_lookup(u32 va, const TLB_Entry** tlb_entry_ret, u32& rv)
{
	CCN_MMUCR.URC++;
	if (CCN_MMUCR.URB == CCN_MMUCR.URC)
		CCN_MMUCR.URC = 0;

	*tlb_entry_ret = nullptr;
	for (const TLB_Entry& tlb_entry : UTLB)
	{
		if (mmu_match(va, tlb_entry.Address, tlb_entry.Data))
		{
			if (*tlb_entry_ret != nullptr)
				return MmuError::TLB_MHIT;
			*tlb_entry_ret = &tlb_entry;
			u32 sz = tlb_entry.Data.SZ1 * 2 + tlb_entry.Data.SZ0;
			u32 mask = mmu_mask[sz];
			//VPN->PPN | low bits
			rv = ((tlb_entry.Data.PPN << 10) & mask) | (va & ~mask);
		}
	}

	if (*tlb_entry_ret == nullptr)
		return MmuError::TLB_MISS;
	else
		return MmuError::NONE;
}

//Simple QACR translation for mmu (when AT is off)
static u32 mmu_QACR_SQ(u32 va)
{
	int sqi = (va >> 5) & 1;
	u32 addr = (sqi ? CCN_QACR1.Area : CCN_QACR0.Area) << 26;
	addr |= va & 0x03ffffe0;

	return addr;
}

template<u32 translation_type>
MmuError mmu_full_SQ(u32 va, u32& rv)
{

	if ((va & 3) || (CCN_MMUCR.SQMD == 1 && sr.MD == 0))
		//here, or after ?
		return MmuError::BADADDR;

	if (CCN_MMUCR.AT)
	{
		//Address=Dest&0xFFFFFFE0;

		const TLB_Entry *entry;
		MmuError lookup = mmu_full_lookup(va, &entry, rv);

		rv &= ~31;//lower 5 bits are forced to 0

		if (lookup != MmuError::NONE)
			return lookup;

		u32 md = entry->Data.PR >> 1;

		//Priv mode protection
		if (md == 0 && sr.MD == 0)
			return MmuError::PROTECTED;

		//Write Protection (Lock or FW)
		if (translation_type == MMU_TT_DWRITE)
		{
			if ((entry->Data.PR & 1) == 0)
				return MmuError::PROTECTED;
			else if (entry->Data.D == 0)
				return MmuError::FIRSTWRITE;
		}
	}
	else
	{
		rv = mmu_QACR_SQ(va);
	}
	return MmuError::NONE;
}
template MmuError mmu_full_SQ<MMU_TT_DREAD>(u32 va, u32& rv);
template MmuError mmu_full_SQ<MMU_TT_DWRITE>(u32 va, u32& rv);

template<u32 translation_type>
MmuError mmu_data_translation(u32 va, u32& rv)
{
	if (translation_type == MMU_TT_DWRITE)
	{
		if ((va & 0xFC000000) == 0xE0000000)
		{
			MmuError lookup = mmu_full_SQ<MMU_TT_DWRITE>(va, rv);
			if (lookup != MmuError::NONE)
				return lookup;

			rv = va;	//SQ writes are not translated, only write backs are.
			return MmuError::NONE;
		}
	}

	if (sr.MD == 0 && (va & 0x80000000) != 0)
		//if on kernel, and not SQ addr -> error
		return MmuError::BADADDR;

	if ((va & 0xFC000000) == 0x7C000000)
	{
		// 7C000000 to 7FFFFFFF in P0/U0 not translated
		rv = va;
		return MmuError::NONE;
	}

	if (fast_reg_lut[va >> 29] != 0)
	{
		// P1, P2 and P4 aren't translated
		rv = va;
		return MmuError::NONE;
	}

	const TLB_Entry *entry;
	MmuError lookup = mmu_full_lookup(va, &entry, rv);

	if (lookup != MmuError::NONE)
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
	if (md == 0 && sr.MD == 0)
		return MmuError::PROTECTED;

	//X0 -> read olny
	//X1 -> read/write , can be FW

	//Write Protection (Lock or FW)
	if (translation_type == MMU_TT_DWRITE)
	{
		if ((entry->Data.PR & 1) == 0)
			return MmuError::PROTECTED;
		else if (entry->Data.D == 0)
			return MmuError::FIRSTWRITE;
	}
	if ((rv & 0x1C000000) == 0x1C000000)
		// map 1C000000-1FFFFFFF to P4 memory-mapped registers
		rv |= 0xF0000000;

	return MmuError::NONE;
}
template MmuError mmu_data_translation<MMU_TT_DREAD>(u32 va, u32& rv);
template MmuError mmu_data_translation<MMU_TT_DWRITE>(u32 va, u32& rv);

MmuError mmu_instruction_translation(u32 va, u32& rv)
{
	if (sr.MD == 0 && (va & 0x80000000) != 0)
		// User mode on kernel address
		return MmuError::BADADDR;

	if ((va >> 29) == 7)
		// P4 not executable
		return MmuError::BADADDR;

	if (fast_reg_lut[va >> 29] != 0)
	{
		// P1 and P2 aren't translated
		rv = va;
		return MmuError::NONE;
	}

	const TLB_Entry *entry;
	MmuError lookup = mmu_instruction_lookup(va, &entry, rv);
	if (lookup != MmuError::NONE)
		return lookup;

	u32 md = entry->Data.PR >> 1;

	//0X  & User mode-> protection violation
	//Priv mode protection
	if (md == 0 && sr.MD == 0)
		return MmuError::PROTECTED;

	return MmuError::NONE;
}
#endif

MmuError mmu_instruction_lookup(u32 va, const TLB_Entry** tlb_entry_ret, u32& rv)
{
	bool mmach = false;
retry_ITLB_Match:
	*tlb_entry_ret = nullptr;
	for (const TLB_Entry& entry : ITLB)
	{
		if (entry.Data.V == 0)
			continue;
		u32 sz = entry.Data.SZ1 * 2 + entry.Data.SZ0;
		u32 mask = mmu_mask[sz];

		if ((((entry.Address.VPN << 10) & mask) == (va & mask)))
		{
			bool needAsidMatch = entry.Data.SH == 0 && (sr.MD == 0 || CCN_MMUCR.SV == 0);

			if (!needAsidMatch || entry.Address.ASID == CCN_PTEH.ASID)
			{
				if (*tlb_entry_ret != nullptr)
					return MmuError::TLB_MHIT;
				*tlb_entry_ret = &entry;
				//VPN->PPN | low bits
				rv = ((entry.Data.PPN << 10) & mask) | (va & ~mask);
			}
		}
	}

	if (*tlb_entry_ret == nullptr)
	{
#ifndef FAST_MMU
		verify(!mmach);
#else
		// the matching may be approximative
		if (mmach)
			return MmuError::TLB_MISS;
#endif
		const TLB_Entry *tlb_entry;
		MmuError lookup = mmu_full_lookup(va, &tlb_entry, rv);
		if (lookup != MmuError::NONE)
			return lookup;

		u32 replace_index = ITLB_LRU_USE[CCN_MMUCR.LRUI];
		verify(replace_index != 0xFFFFFFFF);
		ITLB[replace_index] = *tlb_entry;
		ITLB_Sync(replace_index);
		mmach = true;
		goto retry_ITLB_Match;
	}

	CCN_MMUCR.LRUI &= ITLB_LRU_AND[*tlb_entry_ret - ITLB];
	CCN_MMUCR.LRUI |= ITLB_LRU_OR[*tlb_entry_ret - ITLB];

	return MmuError::NONE;
}

void mmu_set_state()
{
	if (CCN_MMUCR.AT == 1)
	{
		// Detect if we're running Windows CE
		static const char magic[] = { 'S', 0, 'H', 0, '-', 0, '4', 0, ' ', 0, 'K', 0, 'e', 0, 'r', 0, 'n', 0, 'e', 0, 'l', 0 };
		if (memcmp(GetMemPtr(0x8c0110a8, 4), magic, sizeof(magic)) == 0
				|| memcmp(GetMemPtr(0x8c011118, 4), magic, sizeof(magic)) == 0)
		{
			mmuOn = true;
			NOTICE_LOG(SH4, "Enabling Full MMU support");
		}
	}
	else
	{
		mmuOn = false;
	}

	SetMemoryHandlers();
	setSqwHandler();
}

u32 mmuAddressLUT[0x100000];

void MMU_init()
{
	memset(ITLB_LRU_USE, 0xFF, sizeof(ITLB_LRU_USE));
	for (u32 e = 0; e<4; e++)
	{
		u32 match_key = ((~ITLB_LRU_AND[e]) & 0x3F);
		u32 match_mask = match_key | ITLB_LRU_OR[e];
		for (u32 i = 0; i < std::size(ITLB_LRU_USE); i++)
		{
			if ((i & match_mask) == match_key)
			{
				verify(ITLB_LRU_USE[i] == 0xFFFFFFFF);
				ITLB_LRU_USE[i] = e;
			}
		}
	}
	mmu_set_state();
#ifdef FAST_MMU
	// pre-fill kernel memory
	for (u32 vpn = std::size(mmuAddressLUT) / 2; vpn < std::size(mmuAddressLUT); vpn++)
		mmuAddressLUT[vpn] = vpn << 12;
#endif
}


void MMU_reset()
{
	memset(UTLB, 0, sizeof(UTLB));
	memset(ITLB, 0, sizeof(ITLB));
	mmu_set_state();
	mmu_flush_table();
	memset(sq_remap, 0, sizeof(sq_remap));
}

void MMU_term()
{
}

#ifndef FAST_MMU
void mmu_flush_table()
{
	for (TLB_Entry& entry : ITLB)
		entry.Data.V = 0;
	for (TLB_Entry& entry : UTLB)
		entry.Data.V = 0;
}
#endif

template<typename T>
T DYNACALL mmu_ReadMem(u32 adr)
{
	if (adr & (std::min((int)sizeof(T), 4) - 1))
		// Unaligned
		mmu_raise_exception(MmuError::BADADDR, adr, MMU_TT_DREAD);
	u32 addr;
	MmuError rv = mmu_data_translation<MMU_TT_DREAD>(adr, addr);
	if (rv != MmuError::NONE)
		mmu_raise_exception(rv, adr, MMU_TT_DREAD);
	return addrspace::readt<T>(addr);
}
template u8 mmu_ReadMem(u32 adr);
template u16 mmu_ReadMem(u32 adr);
template u32 mmu_ReadMem(u32 adr);
template u64 mmu_ReadMem(u32 adr);

u16 DYNACALL mmu_IReadMem16(u32 vaddr)
{
	if (vaddr & (sizeof(u16) - 1))
		// Unaligned
		mmu_raise_exception(MmuError::BADADDR, vaddr, MMU_TT_IREAD);
	u32 addr;
	MmuError rv = mmu_instruction_translation(vaddr, addr);
	if (rv != MmuError::NONE)
		mmu_raise_exception(rv, vaddr, MMU_TT_IREAD);
	return addrspace::read16(addr);
}

template<typename T>
void DYNACALL mmu_WriteMem(u32 adr, T data)
{
	if (adr & (std::min((int)sizeof(T), 4) - 1))
		// Unaligned
		mmu_raise_exception(MmuError::BADADDR, adr, MMU_TT_DWRITE);
	u32 addr;
	MmuError rv = mmu_data_translation<MMU_TT_DWRITE>(adr, addr);
	if (rv != MmuError::NONE)
		mmu_raise_exception(rv, adr, MMU_TT_DWRITE);
	addrspace::writet<T>(addr, data);
}
template void mmu_WriteMem(u32 adr, u8 data);
template void mmu_WriteMem(u32 adr, u16 data);
template void mmu_WriteMem(u32 adr, u32 data);
template void mmu_WriteMem(u32 adr, u64 data);

void mmu_TranslateSQW(u32 adr, u32 *out)
{
	if (!mmuOn)
	{
		//This will only work for 1 mb pages .. hopefully nothing else is used
		//*FIXME* to work for all page sizes ?

		*out = sq_remap[(adr >> 20) & 0x3F] | (adr & 0xFFFE0);
	}
	else
	{
		u32 addr;
		MmuError tv = mmu_full_SQ<MMU_TT_DREAD>(adr, addr);
		if (tv != MmuError::NONE)
			mmu_raise_exception(tv, adr, MMU_TT_DREAD);

		*out = addr;
	}
}

void mmu_serialize(Serializer& ser)
{
	ser << UTLB;
	ser << ITLB;
	ser << sq_remap;
}

void mmu_deserialize(Deserializer& deser)
{
	deser.skip(8, Deserializer::V33);	// CCN_QACR_TR

	deser >> UTLB;
	deser >> ITLB;

	if (deser.version() >= Deserializer::V11
			|| (deser.version() >= Deserializer::V11_LIBRETRO && deser.version() <= Deserializer::VLAST_LIBRETRO))
		deser >> sq_remap;
	deser.skip(64 * 4, Deserializer::V23); // ITLB_LRU_USE
}
