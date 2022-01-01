/*
	Tiny cute block manager. Doesn't keep block graphs or anything fancy ...
	Its based on a simple hashed-lists idea
*/

#include <algorithm>
#include <set>
#include <map>
#include "blockmanager.h"
#include "ngen.h"

#include "../sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/sh4_sched.h"


#if defined(__unix__) && defined(DYNA_OPROF)
#include <opagent.h>
op_agent_t          oprofHandle;
#endif

#if FEAT_SHREC != DYNAREC_NONE


typedef std::vector<RuntimeBlockInfoPtr> bm_List;
typedef std::set<RuntimeBlockInfoPtr> bm_Set;
typedef std::map<void*, RuntimeBlockInfoPtr> bm_Map;

static bm_Set all_temp_blocks;
static bm_List del_blocks;

bool unprotected_pages[RAM_SIZE_MAX/PAGE_SIZE];
static std::set<RuntimeBlockInfo*> blocks_per_page[RAM_SIZE_MAX/PAGE_SIZE];

static bm_Map blkmap;
// Stats
u32 protected_blocks;
u32 unprotected_blocks;

#define FPCA(x) ((DynarecCodeEntryPtr&)sh4rcb.fpcb[(x>>1)&FPCB_MASK])

// addr must be a physical address
// This returns an executable address
static DynarecCodeEntryPtr DYNACALL bm_GetCode(u32 addr)
{
	DynarecCodeEntryPtr rv = FPCA(addr);

	return rv;
}

// addr must be a virtual address
// This returns an executable address
DynarecCodeEntryPtr DYNACALL bm_GetCodeByVAddr(u32 addr)
{
	if (!mmu_enabled())
		return bm_GetCode(addr);

	if (addr & 1)
	{
		switch (addr)
		{
#ifdef USE_WINCE_HACK
		case 0xfffffde7: // GetTickCount
			// This should make this syscall faster
			r[0] = sh4_sched_now64() * 1000 / SH4_MAIN_CLOCK;
			next_pc = pr;
			break;

		case 0xfffffd05: // QueryPerformanceCounter(u64 *)
			{
				u32 paddr;
				if (mmu_data_translation<MMU_TT_DWRITE, u64>(r[4], paddr) == MMU_ERROR_NONE)
				{
					_vmem_WriteMem64(paddr, sh4_sched_now64() >> 4);
					r[0] = 1;
					next_pc = pr;
				}
				else
				{
					Do_Exception(addr, 0xE0, 0x100);
				}
			}
			break;
#endif

		default:
			Do_Exception(addr, 0xE0, 0x100);
			break;
		}
		addr = next_pc;
	}

	u32 paddr;
	u32 rv = mmu_instruction_translation(addr, paddr);
	if (rv != MMU_ERROR_NONE)
	{
		DoMMUException(addr, rv, MMU_TT_IREAD);
		mmu_instruction_translation(next_pc, paddr);
	}

	return bm_GetCode(paddr);
}

// addr must be a physical address
// This returns an executable address
RuntimeBlockInfoPtr DYNACALL bm_GetBlock(u32 addr)
{
	DynarecCodeEntryPtr cde = bm_GetCode(addr);  // Returns RX ptr

	if (cde == ngen_FailedToFindBlock)
		return NULL;
	else
		return bm_GetBlock((void*)cde);  // Returns RX pointer
}

// This takes a RX address and returns the info block ptr (RW space)
RuntimeBlockInfoPtr bm_GetBlock(void* dynarec_code)
{
	if (blkmap.empty())
		return NULL;

	void *dynarecrw = CC_RX2RW(dynarec_code);
	// Returns a block who's code addr is bigger than dynarec_code (or end)
	auto iter = blkmap.upper_bound(dynarecrw);
	if (iter == blkmap.begin())
		return NULL;
	iter--;  // Need to go back to find the potential candidate

	// However it might be out of bounds, check for that
	if (!iter->second->containsCode(dynarecrw))
		return NULL;

	return iter->second;
}

static void bm_CleanupDeletedBlocks()
{
	del_blocks.clear();
}

// Takes RX pointer and returns a RW pointer
RuntimeBlockInfoPtr bm_GetStaleBlock(void* dynarec_code)
{
	void *dynarecrw = CC_RX2RW(dynarec_code);
	if (del_blocks.empty())
		return NULL;
	// Start from the end to get the youngest one
	auto it = del_blocks.end();
	do
	{
		--it;
		if ((*it)->containsCode(dynarecrw))
			return *it;
	} while (it != del_blocks.begin());

	return NULL;
}

void bm_AddBlock(RuntimeBlockInfo* blk)
{
	RuntimeBlockInfoPtr block(blk);
	if (block->temp_block)
		all_temp_blocks.insert(block);
	auto iter = blkmap.find((void*)blk->code);
	if (iter != blkmap.end()) {
		ERROR_LOG(DYNAREC, "DUP: %08X %p %08X %p", iter->second->addr, iter->second->code, block->addr, block->code);
		die("Duplicated block");
	}
	blkmap[(void*)block->code] = block;

	verify((void*)bm_GetCode(block->addr) == (void*)ngen_FailedToFindBlock);
	FPCA(block->addr) = (DynarecCodeEntryPtr)CC_RW2RX(block->code);

#ifdef DYNA_OPROF
	if (oprofHandle)
	{
		char fname[512];

		sprintf(fname,"sh4:%08X,c:%d,s:%d,h:%d", block->addr, block->guest_cycles, block->guest_opcodes, block->host_opcodes);

		if (op_write_native_code(oprofHandle, fname, (uint64_t)block->code, (void*)block->code, block->host_code_size) != 0)
		{
			INFO_LOG(DYNAREC, "op_write_native_code error");
		}
	}
#endif

}

void bm_DiscardBlock(RuntimeBlockInfo* block)
{
	// Remove from block map
	auto it = blkmap.find((void*)block->code);
	verify(it != blkmap.end());
	RuntimeBlockInfoPtr block_ptr = it->second;

	blkmap.erase(it);

	block_ptr->pNextBlock = NULL;
	block_ptr->pBranchBlock = NULL;
	block_ptr->Relink();

	// Remove from jump table
	verify((void*)bm_GetCode(block_ptr->addr) == CC_RW2RX((void*)block_ptr->code));
	FPCA(block_ptr->addr) = ngen_FailedToFindBlock;

	if (block_ptr->temp_block)
		all_temp_blocks.erase(block_ptr);

	del_blocks.push_back(block_ptr);
	block_ptr->Discard();
}

void bm_Periodical_1s()
{
	bm_CleanupDeletedBlocks();
}

void bm_vmem_pagefill(void** ptr, u32 size_bytes)
{
	for (size_t i = 0; i < size_bytes / sizeof(ptr[0]); i++)
	{
		ptr[i]=(void*)ngen_FailedToFindBlock;
	}
}

void bm_Reset()
{
	bm_CleanupDeletedBlocks();
	protected_blocks = 0;
	unprotected_blocks = 0;

	if (_nvmem_enabled())
	{
		// Windows cannot lock/unlock a region spanning more than one VirtualAlloc or MapViewOfFile
		// so we have to unlock each region individually
		// No need for this mess in 4GB mode since windows doesn't use it
		if (settings.platform.ram_size == 16 * 1024 * 1024)
		{
			mem_region_unlock(virt_ram_base + 0x0C000000, RAM_SIZE);
			mem_region_unlock(virt_ram_base + 0x0D000000, RAM_SIZE);
			mem_region_unlock(virt_ram_base + 0x0E000000, RAM_SIZE);
			mem_region_unlock(virt_ram_base + 0x0F000000, RAM_SIZE);
		}
		else
		{
			mem_region_unlock(virt_ram_base + 0x0C000000, RAM_SIZE);
			mem_region_unlock(virt_ram_base + 0x0E000000, RAM_SIZE);
		}
		if (_nvmem_4gb_space())
		{
			mem_region_unlock(virt_ram_base + 0x8C000000u, 0x90000000u - 0x8C000000u);
			mem_region_unlock(virt_ram_base + 0xAC000000u, 0xB0000000u - 0xAC000000u);
		}
	}
	else
	{
		mem_region_unlock(&mem_b[0], RAM_SIZE);
	}
}

void bm_LockPage(u32 addr, u32 size)
{
	addr = addr & (RAM_MASK - PAGE_MASK);
	if (_nvmem_enabled())
	{
		mem_region_lock(virt_ram_base + 0x0C000000 + addr, size);
		if (_nvmem_4gb_space())
		{
			mem_region_lock(virt_ram_base + 0x8C000000 + addr, size);
			mem_region_lock(virt_ram_base + 0xAC000000 + addr, size);
			// TODO wraps
		}
	}
	else
	{
		mem_region_lock(&mem_b[addr], size);
	}
}

void bm_UnlockPage(u32 addr, u32 size)
{
	addr = addr & (RAM_MASK - PAGE_MASK);
	if (_nvmem_enabled())
	{
		mem_region_unlock(virt_ram_base + 0x0C000000 + addr, size);
		if (_nvmem_4gb_space())
		{
			mem_region_unlock(virt_ram_base + 0x8C000000 + addr, size);
			mem_region_unlock(virt_ram_base + 0xAC000000 + addr, size);
			// TODO wraps
		}
	}
	else
	{
		mem_region_unlock(&mem_b[addr], size);
	}
}

void bm_ResetCache()
{
	ngen_ResetBlocks();
	_vmem_bm_reset();

	for (const auto& it : blkmap)
	{
		RuntimeBlockInfoPtr block = it.second;
		block->relink_data = 0;
		block->pNextBlock = NULL;
		block->pBranchBlock = NULL;
		// needed for the transition to full mmu. Could perhaps limit it to the current block.
		block->Relink();
		// Avoid circular references
		block->Discard();
		del_blocks.push_back(block);
	}

	blkmap.clear();
	// blkmap includes temp blocks as well
	all_temp_blocks.clear();

	for (auto& block_list : blocks_per_page)
		block_list.clear();

	memset(unprotected_pages, 0, sizeof(unprotected_pages));

#ifdef DYNA_OPROF
	if (oprofHandle)
	{
		for (int i=0;i<del_blocks.size();i++)
		{
			if (op_unload_native_code(oprofHandle, (uint64_t)del_blocks[i]->code) != 0)
			{
				INFO_LOG(DYNAREC, "op_unload_native_code error");
			}
		}
	}
#endif
}

void bm_ResetTempCache(bool full)
{
	if (!full)
	{
		for (const auto& block : all_temp_blocks)
		{
			FPCA(block->addr) = ngen_FailedToFindBlock;
			blkmap.erase((void*)block->code);
		}
	}
	del_blocks.insert(del_blocks.begin(),all_temp_blocks.begin(),all_temp_blocks.end());
	all_temp_blocks.clear();
}

void bm_Init()
{
#ifdef DYNA_OPROF
	oprofHandle=op_open_agent();
	if (oprofHandle==0)
		INFO_LOG(DYNAREC, "bm: Failed to open oprofile");
	else
		INFO_LOG(DYNAREC, "bm: Oprofile integration enabled !");
#endif
}

void bm_Term()
{
#ifdef DYNA_OPROF
	if (oprofHandle) op_close_agent(oprofHandle);
	
	oprofHandle=0;
#endif
	bm_Reset();
}

void bm_WriteBlockMap(const std::string& file)
{
	FILE* f=fopen(file.c_str(),"wb");
	if (f)
	{
		INFO_LOG(DYNAREC, "Writing block map !");
		for (auto& it : blkmap)
		{
			RuntimeBlockInfoPtr& block = it.second;
			fprintf(f, "block: %d:%08X:%p:%d:%d:%d\n", block->BlockType, block->addr, block->code, block->host_code_size, block->guest_cycles, block->guest_opcodes);
			for(size_t j = 0; j < block->oplist.size(); j++)
				fprintf(f,"\top: %zd:%d:%s\n", j, block->oplist[j].guest_offs, block->oplist[j].dissasm().c_str());
		}
		fclose(f);
		INFO_LOG(DYNAREC, "Finished writing block map");
	}
}

void sh4_jitsym(FILE* out)
{
	for (const auto& it : blkmap)
	{
		const RuntimeBlockInfoPtr& block = it.second;
		fprintf(out, "%p %d %08X\n", block->code, block->host_code_size, block->addr);
	}
}

#if 0
u32 GetLookup(RuntimeBlockInfo* elem)
{
	return elem->lookups;
}

bool UDgreater ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{
	return elem1->runs > elem2->runs;
}

bool UDgreater2 ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{
	return elem1->runs*elem1->host_opcodes > elem2->runs*elem2->host_opcodes;
}

bool UDgreater3 ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{
	return elem1->runs*elem1->host_opcodes/elem1->guest_cycles > elem2->runs*elem2->host_opcodes/elem2->guest_cycles;
}

void bm_PrintTopBlocks()
{
	double total_lups=0;
	double total_runs=0;
	double total_cycles=0;
	double total_hops=0;
	double total_sops=0;

	for (size_t i=0;i<all_blocks.size();i++)
	{
		total_lups+=GetLookup(all_blocks[i]);
		total_cycles+=all_blocks[i]->runs*all_blocks[i]->guest_cycles;
		total_hops+=all_blocks[i]->runs*all_blocks[i]->host_opcodes;
		total_sops+=all_blocks[i]->runs*all_blocks[i]->guest_opcodes;
		total_runs+=all_blocks[i]->runs;
	}

	INFO_LOG(DYNAREC, "Total lookups:  %.0fKRuns, %.0fKLuops, Total cycles: %.0fMhz, Total Hops: %.0fMips, Total Sops: %.0fMips!",total_runs/1000,total_lups/1000,total_cycles/1000/1000,total_hops/1000/1000,total_sops/1000/1000);
	total_hops/=100;
	total_cycles/=100;
	total_runs/=100;

	double sel_hops=0;
	for (size_t i=0;i<(all_blocks.size()/100);i++)
	{
		INFO_LOG(DYNAREC, "Block %08X: %p, r: %d (c: %d, s: %d, h: %d) (r: %.2f%%, c: %.2f%%, h: %.2f%%)",
			all_blocks[i]->addr, all_blocks[i]->code,all_blocks[i]->runs,
			all_blocks[i]->guest_cycles,all_blocks[i]->guest_opcodes,all_blocks[i]->host_opcodes,

			all_blocks[i]->runs/total_runs,
			all_blocks[i]->guest_cycles*all_blocks[i]->runs/total_cycles,
			all_blocks[i]->host_opcodes*all_blocks[i]->runs/total_hops);
		
		sel_hops+=all_blocks[i]->host_opcodes*all_blocks[i]->runs;
	}

	INFO_LOG(DYNAREC, " >-< %.2f%% covered in top 1%% blocks",sel_hops/total_hops);

	size_t i;
	for (i=all_blocks.size()/100;sel_hops/total_hops<50;i++)
	{
		INFO_LOG(DYNAREC, "Block %08X: %p, r: %d (c: %d, s: %d, h: %d) (r: %.2f%%, c: %.2f%%, h: %.2f%%)",
			all_blocks[i]->addr, all_blocks[i]->code,all_blocks[i]->runs,
			all_blocks[i]->guest_cycles,all_blocks[i]->guest_opcodes,all_blocks[i]->host_opcodes,

			all_blocks[i]->runs/total_runs,
			all_blocks[i]->guest_cycles*all_blocks[i]->runs/total_cycles,
			all_blocks[i]->host_opcodes*all_blocks[i]->runs/total_hops);
		
		sel_hops+=all_blocks[i]->host_opcodes*all_blocks[i]->runs;
	}

	INFO_LOG(DYNAREC, " >-< %.2f%% covered in top %.2f%% blocks",sel_hops/total_hops,i*100.0/all_blocks.size());

}

void bm_Sort()
{
	INFO_LOG(DYNAREC, "!!!!!!!!!!!!!!!!!!! BLK REPORT !!!!!!!!!!!!!!!!!!!!");

	INFO_LOG(DYNAREC, "     ---- Blocks: Sorted based on Runs ! ----     ");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater);
	bm_PrintTopBlocks();

	INFO_LOG(DYNAREC, "<><><><><><><><><><><><><><><><><><><><><><><><><>");

	INFO_LOG(DYNAREC, "     ---- Blocks: Sorted based on hops ! ----     ");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater2);
	bm_PrintTopBlocks();

	INFO_LOG(DYNAREC, "<><><><><><><><><><><><><><><><><><><><><><><><><>");

	INFO_LOG(DYNAREC, "     ---- Blocks: Sorted based on wefs ! ----     ");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater3);
	bm_PrintTopBlocks();

	INFO_LOG(DYNAREC, "^^^^^^^^^^^^^^^^^^^ END REPORT ^^^^^^^^^^^^^^^^^^^");

	for (size_t i=0;i<all_blocks.size();i++)
	{
		all_blocks[i]->runs=0;
	}
}
#endif

RuntimeBlockInfo::~RuntimeBlockInfo()
{
	if (sh4_code_size != 0)
	{
		if (read_only)
			protected_blocks--;
		else
			unprotected_blocks--;
	}
}

void RuntimeBlockInfo::AddRef(const RuntimeBlockInfoPtr& other)
{ 
	pre_refs.push_back(other); 
}

void RuntimeBlockInfo::RemRef(const RuntimeBlockInfoPtr& other)
{
	bm_List::iterator it = std::find(pre_refs.begin(), pre_refs.end(), other);
	if (it != pre_refs.end())
		pre_refs.erase(it);
}

void RuntimeBlockInfo::Discard()
{
	// Update references
	for (RuntimeBlockInfoPtr& ref : pre_refs)
	{
		if (ref->pNextBlock == this)
			ref->pNextBlock = nullptr;
		if (ref->pBranchBlock == this)
			ref->pBranchBlock = nullptr;
		ref->relink_data = 0;
		ref->Relink();
	}
	pre_refs.clear();

	if (read_only)
	{
		// Remove this block from the per-page block lists
		for (u32 addr = this->addr & ~PAGE_MASK; addr < this->addr + this->sh4_code_size; addr += PAGE_SIZE)
		{
			auto& block_list = blocks_per_page[(addr & RAM_MASK) / PAGE_SIZE];
			block_list.erase(this);
		}
	}
}

void RuntimeBlockInfo::SetProtectedFlags()
{
#ifdef TARGET_NO_EXCEPTIONS
	this->read_only = false;
	return;
#endif
	// Don't write protect rom and BIOS/IP.BIN (Grandia II)
	if (!IsOnRam(addr) || (addr & 0x1FFF0000) == 0x0c000000)
	{
		this->read_only = false;
		unprotected_blocks++;
		return;
	}
	for (u32 addr = this->addr & ~PAGE_MASK; addr < this->addr + sh4_code_size; addr += PAGE_SIZE)
	{
		if (unprotected_pages[(addr & RAM_MASK) / PAGE_SIZE])
		{
			this->read_only = false;
			unprotected_blocks++;
			return;
		}
	}
	this->read_only = true;
	protected_blocks++;
	for (u32 addr = this->addr & ~PAGE_MASK; addr < this->addr + sh4_code_size; addr += PAGE_SIZE)
	{
		auto& block_list = blocks_per_page[(addr & RAM_MASK) / PAGE_SIZE];
		if (block_list.empty())
			bm_LockPage(addr);
		block_list.insert(this);
	}
}

void bm_RamWriteAccess(u32 addr)
{
	addr &= RAM_MASK;
	if (unprotected_pages[addr / PAGE_SIZE])
	{
		//ERROR_LOG(DYNAREC, "Page %08x already unprotected", addr);
		//die("Fatal error");
		return;
	}
	unprotected_pages[addr / PAGE_SIZE] = true;
	bm_UnlockPage(addr);
	std::set<RuntimeBlockInfo*>& block_list = blocks_per_page[addr / PAGE_SIZE];
	if (!block_list.empty())
	{
		std::vector<RuntimeBlockInfo*> list_copy;
		list_copy.insert(list_copy.begin(), block_list.begin(), block_list.end());
		if (!list_copy.empty())
			DEBUG_LOG(DYNAREC, "bm_RamWriteAccess write access to %08x pc %08x", addr, next_pc);
		for (auto& block : list_copy)
		{
			bm_DiscardBlock(block);
		}
		verify(block_list.empty());
	}
}

u32 bm_getRamOffset(void *p)
{
	if (_nvmem_enabled())
	{
		if (_nvmem_4gb_space())
		{
			if ((u8 *)p < virt_ram_base || (u8 *)p >= virt_ram_base + 0x100000000L)
				return -1;
		}
		else
		{
			if ((u8 *)p < virt_ram_base || (u8 *)p >= virt_ram_base + 0x20000000)
				return -1;
		}
		u32 addr = (u8*)p - virt_ram_base;
		if (!IsOnRam(addr) || ((addr >> 29) > 0 && (addr >> 29) < 4))	// system RAM is not mapped to 20, 40 and 60 because of laziness
			return -1;
		return addr & RAM_MASK;
	}
	else
	{
		if ((u8 *)p < &mem_b[0] || (u8 *)p >= &mem_b[RAM_SIZE])
			return -1;
		return (u32)((u8 *)p - &mem_b[0]);
	}
}

bool bm_RamWriteAccess(void *p)
{
	u32 offset = bm_getRamOffset(p);
	if (offset == (u32)-1)
		return false;
	bm_RamWriteAccess(offset);
	return true;
}

bool print_stats = true;

void fprint_hex(FILE* d,const char* init,u8* ptr, u32& ofs, u32 limit)
{
	int base=ofs;
	int cnt=0;
	while(ofs<limit)
	{
		if (cnt==32)
		{
			fputs("\n",d);
			cnt=0;
		}

		if (cnt==0)
			fprintf(d,"%s:%d:",init,ofs-base);

		fprintf(d," %02X",ptr[ofs++]);
		cnt++;
	}
	fputs("\n",d);
}



void print_blocks()
{
	FILE* f=0;

	if (print_stats)
	{
		f=fopen(get_writable_data_path("blkmap.lst").c_str(),"w");
		print_stats=false;

		INFO_LOG(DYNAREC, "Writing blocks to %p", f);
	}

	for (auto it : blkmap)
	{
		RuntimeBlockInfoPtr blk = it.second;
		if (f)
		{
			fprintf(f,"block: %p\n",blk.get());
			fprintf(f,"vaddr: %08X\n",blk->vaddr);
			fprintf(f,"paddr: %08X\n",blk->addr);
			fprintf(f,"hash: %s\n",blk->hash());
			fprintf(f,"hash_rloc: %s\n",blk->hash());
			fprintf(f,"code: %p\n",blk->code);
			fprintf(f,"runs: %d\n",blk->runs);
			fprintf(f,"BlockType: %d\n",blk->BlockType);
			fprintf(f,"NextBlock: %08X\n",blk->NextBlock);
			fprintf(f,"BranchBlock: %08X\n",blk->BranchBlock);
			fprintf(f,"pNextBlock: %p\n",blk->pNextBlock);
			fprintf(f,"pBranchBlock: %p\n",blk->pBranchBlock);
			fprintf(f,"guest_cycles: %d\n",blk->guest_cycles);
			fprintf(f,"guest_opcodes: %d\n",blk->guest_opcodes);
			fprintf(f,"host_opcodes: %d\n",blk->host_opcodes);
			fprintf(f,"il_opcodes: %zd\n",blk->oplist.size());

			s32 gcode=-1;

			size_t j=0;
			
			fprintf(f,"{\n");
			for (;j<blk->oplist.size();j++)
			{
				shil_opcode* op = &blk->oplist[j];
				//fprint_hex(f,"//h:",pucode,hcode,op->host_offs);

				if (gcode!=op->guest_offs)
				{
					gcode=op->guest_offs;
					u32 rpc=blk->vaddr+gcode;
					try {
						u16 op=IReadMem16(rpc);

						char temp[128];
						OpDesc[op]->Disassemble(temp,rpc,op);

						fprintf(f,"//g: %04X %s\n", op, temp);
					} catch (const SH4ThrownException& ex) {
						fprintf(f,"//g: ???? (page fault)\n");
					}
				}

				std::string s = op->dissasm();
				fprintf(f,"//il:%d:%d: %s\n",op->guest_offs,op->host_offs,s.c_str());
			}
			
			//fprint_hex(f,"//h:",pucode,hcode,blk->host_code_size);

			fprintf(f,"}\n");
		}

		blk->runs=0;
	}

	if (f) fclose(f);
}
#endif

