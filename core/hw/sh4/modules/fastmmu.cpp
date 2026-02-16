/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "mmu.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_core.h"
#include "types.h"
#include "stdclass.h"

#ifdef FAST_MMU

#include "hw/sh4/sh4_mem.h"

extern TLB_Entry UTLB[64];
// Used when FullMMU is off
extern u32 sq_remap[64];

//#define TRACE_WINCE_SYSCALLS

#include "wince.h"

static TLB_Entry const *lru_entry;
static u32 lru_mask;
static u32 lru_address;

struct TLB_LinkedEntry {
	TLB_Entry entry;
	TLB_LinkedEntry *next_entry;
};
#define NBUCKETS 4096
static TLB_LinkedEntry full_table[65536];
static u32 full_table_size;
static TLB_LinkedEntry *entry_buckets[NBUCKETS];

static u16 bucket_index(u32 address, int size, u32 asid)
{
	return ((address >> 20) ^ (address >> 12) ^ (address | asid | (size << 8))) & (NBUCKETS - 1);
}

static void cache_entry(const TLB_Entry &entry)
{
	if (entry.Data.SZ0 == 0 && entry.Data.SZ1 == 0)
		return;
	if (full_table_size >= std::size(full_table))
		return;

	full_table[full_table_size].entry = entry;

	u16 bucket = bucket_index(entry.Address.VPN << 10, entry.Data.SZ1 * 2 + entry.Data.SZ0, entry.Address.ASID);
	full_table[full_table_size].next_entry = entry_buckets[bucket];
	entry_buckets[bucket] = &full_table[full_table_size];
	full_table_size++;
}

static void flush_cache()
{
	full_table_size = 0;
	memset(entry_buckets, 0, sizeof(entry_buckets));
}

template<u32 size>
bool find_entry_by_page_size(u32 address, const TLB_Entry **ret_entry)
{
	u32 shift = size == 1 ? 2 :
			size == 2 ? 6 :
			size == 3 ? 10 : 0;
	u32 vpn = (address >> (10 + shift)) << shift;
	u16 bucket = bucket_index(vpn << 10, size, CCN_PTEH.ASID);
	TLB_LinkedEntry *pEntry = entry_buckets[bucket];
	while (pEntry != NULL)
	{
		if (pEntry->entry.Address.VPN == vpn && (size >> 1) == pEntry->entry.Data.SZ1 && (size & 1) == pEntry->entry.Data.SZ0)
		{
			if (pEntry->entry.Data.SH == 1 || pEntry->entry.Address.ASID == CCN_PTEH.ASID)
			{
				*ret_entry = &pEntry->entry;
				return true;
			}
		}
		pEntry = pEntry->next_entry;
	}

	return false;
}

static bool find_entry(u32 address, const TLB_Entry **ret_entry)
{
	// 4k
	if (find_entry_by_page_size<1>(address, ret_entry))
		return true;
	// 64k
	if (find_entry_by_page_size<2>(address, ret_entry))
		return true;
	// 1m
	if (find_entry_by_page_size<3>(address, ret_entry))
		return true;
	return false;
}

#if 0
static void dump_table()
{
	static int iter = 1;
	char filename[128];
	snprintf(filename, sizeof(filename), "mmutable%03d", iter++);
	FILE *f = fopen(filename, "wb");
	if (f == NULL)
		return;
	fwrite(full_table, sizeof(full_table[0]), full_table_size, f);
	fclose(f);
}

int main(int argc, char *argv[])
{
	FILE *f = fopen(argv[1], "rb");
	if (f == NULL)
	{
		perror(argv[1]);
		return 1;
	}
	full_table_size = fread(full_table, sizeof(full_table[0]), std::size(full_table), f);
	fclose(f);
	printf("Loaded %d entries\n", full_table_size);
	std::vector<u32> addrs;
	std::vector<u32> asids;
	for (int i = 0; i < full_table_size; i++)
	{
		u32 sz = full_table[i].entry.Data.SZ1 * 2 + full_table[i].entry.Data.SZ0;
		u32 mask = sz == 3 ? 1_MB : sz == 2 ? 64_KB : sz == 1 ? 4_KB : 1_KB;
		mask--;
		addrs.push_back(((full_table[i].entry.Address.VPN << 10) & mmu_mask[sz]) | (random() * mask / RAND_MAX));
		asids.push_back(full_table[i].entry.Address.ASID);
//		printf("%08x -> %08x sz %d ASID %d SH %d\n", full_table[i].entry.Address.VPN << 10, full_table[i].entry.Data.PPN << 10,
//				full_table[i].entry.Data.SZ1 * 2 + full_table[i].entry.Data.SZ0,
//				full_table[i].entry.Address.ASID, full_table[i].entry.Data.SH);
		u16 bucket = bucket_index(full_table[i].entry.Address.VPN << 10, full_table[i].entry.Data.SZ1 * 2 + full_table[i].entry.Data.SZ0);
		full_table[i].next_entry = entry_buckets[bucket];
		entry_buckets[bucket] = &full_table[i];
	}
	for (int i = 0; i < full_table_size / 10; i++)
	{
		addrs.push_back(random());
		asids.push_back(666);
	}
	u64 start = getTimeMs();
	int success = 0;
	const int loops = 100000;
	for (int i = 0; i < loops; i++)
	{
		for (int j = 0; j < addrs.size(); j++)
		{
			u32 addr = addrs[j];
			CCN_PTEH.ASID = asids[j];
			const TLB_Entry *p;
			if (find_entry(addr, &p))
				success++;
		}
	}
	u64 end = getTimeMs();
	printf("Lookup time: %f ms. Success rate %f max_len %d\n", ((double)end - start) / addrs.size(), (double)success / addrs.size() / loops, 0/*max_length*/);
}
#endif

bool UTLB_Sync(u32 entry)
{
	TLB_Entry& tlb_entry = UTLB[entry];
	u32 sz = tlb_entry.Data.SZ1 * 2 + tlb_entry.Data.SZ0;

	tlb_entry.Address.VPN &= mmu_mask[sz] >> 10;
	tlb_entry.Data.PPN &= mmu_mask[sz] >> 10;

	lru_entry = &tlb_entry;
	lru_mask = mmu_mask[sz];
	lru_address = tlb_entry.Address.VPN << 10;

	cache_entry(tlb_entry);

	if (!mmu_enabled())
	{
		if ((tlb_entry.Address.VPN & (0xFC000000 >> 10)) == (0xE0000000 >> 10))
		{
			// Used when FullMMU is off
			u32 vpn_sq = ((tlb_entry.Address.VPN & 0x7FFFF) >> 10) & 0x3F;//upper bits are always known [0xE0/E1/E2/E3]
			sq_remap[vpn_sq] = tlb_entry.Data.PPN << 10;
		}
		else if (CCN_MMUCR.AT == 1 && tlb_entry.Address.VPN != 0x30040 && tlb_entry.Address.VPN != 0x30000 && tlb_entry.Data.V == 1)
		{
			// Enable Full MMU if not an expected store queue mapping
			// VPN checks to ignore many arcade and Visual Concepts games presumably bogus mappings
			mmuOn = true;
			NOTICE_LOG(SH4, "Enabling on-demand Full MMU support");
			mmu_set_state();
		}
	}
	return true;
}

void ITLB_Sync(u32 entry)
{
}

//Do a full lookup on the UTLB entry's
MmuError mmu_full_lookup(u32 va, const TLB_Entry** tlb_entry_ret, u32& rv)
{
	if (lru_entry != NULL)
	{
		if (/*lru_entry->Data.V == 1 && */
				lru_address == (va & lru_mask)
				&& (lru_entry->Address.ASID == CCN_PTEH.ASID
						|| lru_entry->Data.SH == 1
						/*|| (sr.MD == 1 && CCN_MMUCR.SV == 1)*/))	// SV=1 not handled
		{
			//VPN->PPN | low bits
			rv = (lru_entry->Data.PPN << 10) | (va & ~lru_mask);
			if (tlb_entry_ret != nullptr)
				*tlb_entry_ret = lru_entry;

			return MmuError::NONE;
		}
	}
	const TLB_Entry *localEntry;
	if (tlb_entry_ret == nullptr)
		tlb_entry_ret = &localEntry;

	if (find_entry(va, tlb_entry_ret))
	{
		u32 mask = mmu_mask[(*tlb_entry_ret)->Data.SZ1 * 2 + (*tlb_entry_ret)->Data.SZ0];
		rv = ((*tlb_entry_ret)->Data.PPN << 10) | (va & ~mask);
		lru_entry = *tlb_entry_ret;
		lru_mask = mask;
		lru_address = ((*tlb_entry_ret)->Address.VPN << 10);

		return MmuError::NONE;
	}

#ifdef USE_WINCE_HACK
	// WinCE hack
	TLB_Entry& entry = UTLB[CCN_MMUCR.URC];
	if (wince_resolve_address(va, entry))
	{
		CCN_PTEL.reg_data = entry.Data.reg_data;
		CCN_PTEA.reg_data = entry.Assistance.reg_data;
		CCN_PTEH.reg_data = entry.Address.reg_data;

		lru_entry = *tlb_entry_ret = &entry;

		u32 sz = entry.Data.SZ1 * 2 + entry.Data.SZ0;
		lru_mask = mmu_mask[sz];
		lru_address = va & mmu_mask[sz];
		entry.Data.PPN &= mmu_mask[sz] >> 10;

		rv = (entry.Data.PPN << 10) | (va & ~mmu_mask[sz]);

		cache_entry(entry);

		p_sh4rcb->cntx.cycle_counter -= 164;

		return MmuError::NONE;
	}
#endif

	return MmuError::TLB_MISS;
}

template<u32 translation_type>
MmuError mmu_full_SQ(u32 va, u32& rv)
{
	MmuError lookup = mmu_full_lookup(va, nullptr, rv);

	if (lookup != MmuError::NONE)
		return lookup;

	rv &= ~31;//lower 5 bits are forced to 0

	return MmuError::NONE;
}
template MmuError mmu_full_SQ<MMU_TT_DREAD>(u32 va, u32& rv);
template MmuError mmu_full_SQ<MMU_TT_DWRITE>(u32 va, u32& rv);

template<u32 translation_type>
MmuError mmu_data_translation(u32 va, u32& rv)
{
	if (fast_reg_lut[va >> 29] != 0)
	{
		rv = va;
		return MmuError::NONE;
	}

	if ((va & 0xFC000000) == 0x7C000000)
	{
		// On-chip RAM area isn't translated
		rv = va;
		return MmuError::NONE;
	}

	MmuError lookup = mmu_full_lookup(va, nullptr, rv);
	if (lookup == MmuError::NONE && (rv & 0x1C000000) == 0x1C000000)
		// map 1C000000-1FFFFFFF to P4 memory-mapped registers
		rv |= 0xF0000000;
#ifdef TRACE_WINCE_SYSCALLS
	if (unresolved_unicode_string != 0 && lookup == MmuError::NONE)
	{
		if (va == unresolved_unicode_string)
		{
			unresolved_unicode_string = 0;
			printf("RESOLVED %s\n", get_unicode_string(va).c_str());
		}
	}
#endif

	return lookup;
}
template MmuError mmu_data_translation<MMU_TT_DREAD>(u32 va, u32& rv);
template MmuError mmu_data_translation<MMU_TT_DWRITE>(u32 va, u32& rv);

void mmu_flush_table()
{
	lru_entry = nullptr;
	flush_cache();
	mmuAddressLUTFlush(true);
}
#endif 	// FAST_MMU
