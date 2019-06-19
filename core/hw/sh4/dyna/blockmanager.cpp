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
#include "hw/sh4/sh4_sched.h"


#if HOST_OS==OS_LINUX && defined(DYNA_OPROF)
#include <opagent.h>
op_agent_t          oprofHandle;
#endif

#if FEAT_SHREC != DYNAREC_NONE


typedef std::vector<RuntimeBlockInfoPtr> bm_List;
typedef std::set<RuntimeBlockInfoPtr> bm_Set;
typedef std::map<void*, RuntimeBlockInfoPtr> bm_Map;

bm_Set all_temp_blocks;
bm_List del_blocks;

bool unprotected_pages[RAM_SIZE_MAX/PAGE_SIZE];
static std::set<RuntimeBlockInfo*> blocks_per_page[RAM_SIZE_MAX/PAGE_SIZE];

bm_Map blkmap;
u32 bm_gc_luc,bm_gcf_luc;
// Stats
u32 protected_blocks;
u32 unprotected_blocks;

#define FPCA(x) ((DynarecCodeEntryPtr&)sh4rcb.fpcb[(x>>1)&FPCB_MASK])

// addr must be a physical address
// This returns an executable address
DynarecCodeEntryPtr DYNACALL bm_GetCode(u32 addr)
{
	DynarecCodeEntryPtr rv = (DynarecCodeEntryPtr)FPCA(addr);

	return rv;
}

// addr must be a virtual address
// This returns an executable address
DynarecCodeEntryPtr DYNACALL bm_GetCodeByVAddr(u32 addr)
{
#ifndef NO_MMU
	if (!mmu_enabled())
#endif
		return bm_GetCode(addr);
#ifndef NO_MMU
	else
	{
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
#endif
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
	iter--;  // Need to go back to find the potential candidate

	// However it might be out of bounds, check for that
	if ((u8*)iter->second->code + iter->second->host_code_size < dynarec_code)
		return NULL;

	verify(iter->second->contains_code((u8*)dynarecrw));
	return iter->second;
}

void bm_CleanupDeletedBlocks()
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
		it--;
		if ((*it)->contains_code((u8*)dynarecrw))
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
		printf("DUP: %08X %p %08X %p\n", iter->second->addr, iter->second->code, block->addr, block->code);
		verify(false);
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
			printf("op_write_native_code error\n");
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
	verify((void*)bm_GetCode(block_ptr->addr) == (void*)block_ptr->code);
	FPCA(block_ptr->addr) = ngen_FailedToFindBlock;

	if (block_ptr->temp_block)
		all_temp_blocks.erase(block_ptr);

	del_blocks.push_back(block_ptr);
	block_ptr->Discard();
}

#if 0
bool UDgreaterX ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{	
	return elem1->runs > elem2->runs;
}

bool UDgreaterLOC ( RuntimeBlockInfo* elem1, RuntimeBlockInfo* elem2 )
{	
	return elem1->addr < elem2->addr;
}

u32 FindPath(RuntimeBlockInfo* rbi, u32 sa,s32 mc,u32& plc)
{
	if (mc < 0 || rbi==0)
		return 0;

	plc++;
	if (rbi->BlockType==BET_Cond_0 || rbi->BlockType==BET_Cond_1)
	{
		u32 plc1=plc,plc2=plc,v1=0,v2=0;
		if (rbi->BranchBlock>sa)
		{
			v1=FindPath(bm_GetBlock(rbi->BranchBlock),rbi->addr,mc-rbi->guest_cycles,plc1);
		}
		v2=FindPath(bm_GetBlock(rbi->NextBlock),rbi->addr,mc-rbi->guest_cycles,plc2);
		if (plc1>plc2)
		{
			plc=plc1;
			return rbi->guest_cycles+v1;
		}
		else
		{
			plc=plc2;
			return rbi->guest_cycles+v2;
		}
		
	}
	else if (rbi->BlockType==BET_StaticJump)
	{
		if (rbi->BranchBlock>sa)
			return rbi->guest_cycles+FindPath(bm_GetBlock(rbi->BranchBlock),rbi->addr,mc-rbi->guest_cycles,plc);
		else
		{
			return rbi->guest_cycles;
		}
	}
	else
	{
		if (plc!=1)
			printf("Chain lost due to %d\n",rbi->BlockType);
		else
			printf("Chain fail due to %d\n",rbi->BlockType);
		return rbi->guest_cycles;
	}
}
u32 total_saved;
void FindPath(u32 start)
{
	RuntimeBlockInfo* rbi=bm_GetBlock(start);

	if (!rbi || !rbi->runs)
		return;

	u32 plen=0;
	u32 pclc=FindPath(rbi,start,SH4_TIMESLICE,plen);
	if (plen>1)
	{
		total_saved+=(plen-1)*2*rbi->runs;
		printf("%08X: %d, %d, %.2f, %.2f\n",start,pclc,plen,pclc/(float)plen,plen*2*rbi->runs/1000.f);
	}
	rbi->runs=0;
}
#endif

u32 rebuild_counter=20;
void bm_Periodical_1s()
{
	bm_CleanupDeletedBlocks();

	if (rebuild_counter>0) rebuild_counter--;
#if HOST_OS==OS_WINDOWS && 0
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreaterX);

	map<u32,u32> vmap;
	map<u32,u32> calls;

	u32 total_runs=0;
	for(int i=0;i<all_blocks.size();i++)
	{
		RuntimeBlockInfo* rbi=all_blocks[i];
		total_runs+=rbi->runs;
		if (rbi->BranchBlock!=-1 && rbi->BranchBlock < rbi->addr)
		{
			if (rbi->BlockType==BET_Cond_0 || rbi->BlockType==BET_Cond_1 || rbi->BlockType==BET_StaticJump)
			{
				RuntimeBlockInfo* bbi=bm_GetBlock(all_blocks[i]->BranchBlock);
				if (bbi && bbi->runs)
				{
					vmap[all_blocks[i]->BranchBlock]=bbi->runs;
				}
			}

			if (rbi->BlockType==BET_StaticCall)
			{
				RuntimeBlockInfo* bbi=bm_GetBlock(all_blocks[i]->BranchBlock);
				if (bbi && bbi->runs)
				{
					calls[all_blocks[i]->BranchBlock]+=rbi->runs;
				}
			}
		}
		verify(rbi->NextBlock>rbi->addr);
	}

	map<u32,u32>::iterator iter=vmap.begin();

	total_saved=0;
	u32 total_l_runs=0;
	while(iter!=vmap.end())
	{
		FindPath(iter->first);
		total_l_runs+=iter->second;
		iter++;
	}

	for(int i=0;i<all_blocks.size();i++)
		all_blocks[i]->runs=0;

	printf("Total Saved: %.2f || Total Loop Runs: %.2f  || Total Runs: %.2f\n",total_saved/1000.f,total_l_runs/1000.f,total_runs/1000.f);
#endif
}

#if 0
void constprop(RuntimeBlockInfo* blk);
void bm_Rebuild()
{
	return;

	die("this is broken in multiple levels, including compile options");

	void RASDASD();
	RASDASD();

	blkmap.clear();
	
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreaterLOC);
	
	for(size_t i=0; i<all_blocks.size(); i++)
	{
		bool do_opts=((all_blocks[i]->addr&0x3FFFFFFF)>0x0C010100);

		if (all_blocks[i]->staging_runs<0 && do_opts)
		{
//#if HOST_OS==OS_WINDOWS
			//constprop(all_blocks[i]);
//#endif
		}
		ngen_Compile(all_blocks[i],NoCheck,false,all_blocks[i]->staging_runs>0,do_opts);

		blkmap.insert(all_blocks[i]);
		verify(bm_GetBlock((RuntimeBlockInfo*)all_blocks[i]->code)==all_blocks[i]);

		FPCA(all_blocks[i]->addr)=all_blocks[i]->code;
	}

	for(size_t i=0; i<all_blocks.size(); i++)
	{
		all_blocks[i]->Relink();
	}

	rebuild_counter=30;
}
#endif

void bm_vmem_pagefill(void** ptr, u32 size_bytes)
{
	for (size_t i=0; i < size_bytes / sizeof(ptr[0]); i++)
	{
		ptr[i]=(void*)ngen_FailedToFindBlock;
	}
}

void bm_Reset()
{
	bm_ResetCache();
	bm_CleanupDeletedBlocks();
	protected_blocks = 0;
	unprotected_blocks = 0;

	mem_region_unlock(virt_ram_base + 0x0C000000, 0x10000000 - 0x0C000000);
	if (_nvmem_4gb_space())
	{
		mem_region_unlock(virt_ram_base + 0x8C000000, 0x90000000 - 0x8C000000);
		mem_region_unlock(virt_ram_base + 0xAC000000, 0xB0000000 - 0xAC000000);
	}
}

static void bm_LockPage(u32 addr)
{
	addr = addr & (RAM_MASK - PAGE_MASK);
	if (!mmu_enabled())
		mem_region_lock(virt_ram_base + 0x0C000000 + addr, PAGE_SIZE);
	if (_nvmem_4gb_space())
	{
		mem_region_lock(virt_ram_base + 0x8C000000 + addr, PAGE_SIZE);
		mem_region_lock(virt_ram_base + 0xAC000000 + addr, PAGE_SIZE);
		// TODO wraps
	}
}

static void bm_UnlockPage(u32 addr)
{
	addr = addr & (RAM_MASK - PAGE_MASK);
	if (!mmu_enabled())
		mem_region_unlock(virt_ram_base + 0x0C000000 + addr, PAGE_SIZE);
	if (_nvmem_4gb_space())
	{
		mem_region_unlock(virt_ram_base + 0x8C000000 + addr, PAGE_SIZE);
		mem_region_unlock(virt_ram_base + 0xAC000000 + addr, PAGE_SIZE);
		// TODO wraps
	}
}

void bm_ResetCache()
{
	ngen_ResetBlocks();
	_vmem_bm_reset();

	for (auto it : blkmap)
	{
		RuntimeBlockInfoPtr block = it.second;
		block->relink_data = 0;
		block->pNextBlock = 0;
		block->pBranchBlock = 0;
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

	// FIXME Grandia II doesn't like it. intermittent reset when pressing start (except if disabling SSA?? TBC)
	memset(unprotected_pages, 0, sizeof(unprotected_pages));

#ifdef DYNA_OPROF
	if (oprofHandle)
	{
		for (int i=0;i<del_blocks.size();i++)
		{
			if (op_unload_native_code(oprofHandle, (uint64_t)del_blocks[i]->code) != 0)
			{
				printf("op_unload_native_code error\n");
			}
		}
	}
#endif
}

void bm_ResetTempCache(bool full)
{
	if (!full)
	{
		for (auto& block : all_temp_blocks)
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
		printf("bm: Failed to open oprofile\n");
	else
		printf("bm: Oprofile integration enabled !\n");
#endif
	bm_Reset();
}

void bm_Term()
{
#ifdef DYNA_OPROF
	if (oprofHandle) op_close_agent(oprofHandle);
	
	oprofHandle=0;
#endif
	bm_Reset();
}

void bm_WriteBlockMap(const string& file)
{
	FILE* f=fopen(file.c_str(),"wb");
	if (f)
	{
		printf("Writing block map !\n");
		for (auto& it : blkmap)
		{
			RuntimeBlockInfoPtr& block = it.second;
			fprintf(f, "block: %d:%08X:%p:%d:%d:%d\n", block->BlockType, block->addr, block->code, block->host_code_size, block->guest_cycles, block->guest_opcodes);
			for(size_t j = 0; j < block->oplist.size(); j++)
				fprintf(f,"\top: %zd:%d:%s\n", j, block->oplist[j].guest_offs, block->oplist[j].dissasm().c_str());
		}
		fclose(f);
		printf("Finished writing block map\n");
	}
}

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

void sh4_jitsym(FILE* out)
{
	for (auto& it : blkmap)
	{
		RuntimeBlockInfoPtr& block = it.second;
		fprintf(out, "%p %d %08X\n", block->code, block->host_code_size, block->addr);
	}
}
#if 0
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

	printf("Total lookups:  %.0fKRuns, %.0fKLuops, Total cycles: %.0fMhz, Total Hops: %.0fMips, Total Sops: %.0fMips! \n",total_runs/1000,total_lups/1000,total_cycles/1000/1000,total_hops/1000/1000,total_sops/1000/1000);
	total_hops/=100;
	total_cycles/=100;
	total_runs/=100;

	double sel_hops=0;
	for (size_t i=0;i<(all_blocks.size()/100);i++)
	{
		printf("Block %08X: %p, r: %d (c: %d, s: %d, h: %d) (r: %.2f%%, c: %.2f%%, h: %.2f%%)\n",
			all_blocks[i]->addr, all_blocks[i]->code,all_blocks[i]->runs,
			all_blocks[i]->guest_cycles,all_blocks[i]->guest_opcodes,all_blocks[i]->host_opcodes,

			all_blocks[i]->runs/total_runs,
			all_blocks[i]->guest_cycles*all_blocks[i]->runs/total_cycles,
			all_blocks[i]->host_opcodes*all_blocks[i]->runs/total_hops);
		
		sel_hops+=all_blocks[i]->host_opcodes*all_blocks[i]->runs;
	}

	printf(" >-< %.2f%% covered in top 1%% blocks\n",sel_hops/total_hops);

	size_t i;
	for (i=all_blocks.size()/100;sel_hops/total_hops<50;i++)
	{
		printf("Block %08X: %p, r: %d (c: %d, s: %d, h: %d) (r: %.2f%%, c: %.2f%%, h: %.2f%%)\n",
			all_blocks[i]->addr, all_blocks[i]->code,all_blocks[i]->runs,
			all_blocks[i]->guest_cycles,all_blocks[i]->guest_opcodes,all_blocks[i]->host_opcodes,

			all_blocks[i]->runs/total_runs,
			all_blocks[i]->guest_cycles*all_blocks[i]->runs/total_cycles,
			all_blocks[i]->host_opcodes*all_blocks[i]->runs/total_hops);
		
		sel_hops+=all_blocks[i]->host_opcodes*all_blocks[i]->runs;
	}

	printf(" >-< %.2f%% covered in top %.2f%% blocks\n",sel_hops/total_hops,i*100.0/all_blocks.size());

}

void bm_Sort()
{
	printf("!!!!!!!!!!!!!!!!!!! BLK REPORT !!!!!!!!!!!!!!!!!!!!n");

	printf("     ---- Blocks: Sorted based on Runs ! ----     \n");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater);
	bm_PrintTopBlocks();

	printf("<><><><><><><><><><><><><><><><><><><><><><><><><>\n");

	printf("     ---- Blocks: Sorted based on hops ! ----     \n");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater2);
	bm_PrintTopBlocks();

	printf("<><><><><><><><><><><><><><><><><><><><><><><><><>\n");

	printf("     ---- Blocks: Sorted based on wefs ! ----     \n");
	std::sort(all_blocks.begin(),all_blocks.end(),UDgreater3);
	bm_PrintTopBlocks();

	printf("^^^^^^^^^^^^^^^^^^^ END REPORT ^^^^^^^^^^^^^^^^^^^\n");

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

void RuntimeBlockInfo::AddRef(RuntimeBlockInfoPtr other)
{ 
	pre_refs.push_back(other); 
}

void RuntimeBlockInfo::RemRef(RuntimeBlockInfoPtr other)
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
		if (ref->NextBlock == vaddr)
			ref->pNextBlock = NULL;
		if (ref->BranchBlock == vaddr)
			ref->pBranchBlock = NULL;
		ref->relink_data = 0;
		ref->Relink();
	}
	pre_refs.clear();

	if (read_only)
	{
		// Remove this block from the per-page block lists
		for (u32 addr = this->addr & ~PAGE_MASK; addr < this->addr + this->sh4_code_size; addr += PAGE_SIZE)
		{
			set<RuntimeBlockInfo*>& block_list = blocks_per_page[(addr & RAM_MASK) / PAGE_SIZE];
			block_list.erase(this);
		}
	}
}

void RuntimeBlockInfo::SetProtectedFlags()
{
	// Don't write protect rom
	if (!IsOnRam(addr))
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
		blocks_per_page[(addr & RAM_MASK) / PAGE_SIZE].insert(this);
		bm_LockPage(addr);
	}
}

void bm_RamWriteAccess(u32 addr)
{
	addr &= RAM_MASK;
	if (unprotected_pages[addr / PAGE_SIZE])
	{
		printf("Page %08x already unprotected\n", addr);
		die("Fatal error");
	}
	unprotected_pages[addr / PAGE_SIZE] = true;
	bm_UnlockPage(addr);
	set<RuntimeBlockInfo*>& block_list = blocks_per_page[addr / PAGE_SIZE];
	vector<RuntimeBlockInfo*> list_copy;
	list_copy.insert(list_copy.begin(), block_list.begin(), block_list.end());
	//if (!list_copy.empty())
	//	printf("bm_RamWriteAccess write access to %08x\n", addr);
	for (auto& block : list_copy)
	{
		bm_DiscardBlock(block);
	}
	verify(block_list.empty());
}

bool bm_RamWriteAccess(void *p)
{
	if (_nvmem_4gb_space())
	{
		if ((u8 *)p < virt_ram_base || (u8 *)p >= virt_ram_base + 0x100000000L)
			return false;
	}
	else
	{
		if ((u8 *)p < virt_ram_base || (u8 *)p >= virt_ram_base + 0x20000000)
			return false;
	}
	u32 addr = (u8*)p - virt_ram_base;
	if (mmu_enabled() && (addr & 0x80000000) == 0)
		// If mmu enabled, let vmem32 manage user space
		// shouldn't be necessary since it's called first
		return false;
	if (!IsOnRam(addr) || ((addr >> 29) > 0 && (addr >> 29) < 4))	// system RAM is not mapped to 20, 40 and 60 because of laziness
		return false;
	bm_RamWriteAccess(addr);

	return true;
}

bool print_stats;
#if 0
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
		f=fopen(get_writable_data_path("/blkmap.lst").c_str(),"w");
		print_stats=0;

		printf("Writing blocks to %p\n",f);
	}

	for (RuntimeBlockInfo *blk : blkmap)
	{

		if (f)
		{
			fprintf(f,"block: %p\n",blk);
			fprintf(f,"vaddr: %08X\n",blk->vaddr);
			fprintf(f,"paddr: %08X\n",blk->addr);
			fprintf(f,"hash: %s\n",blk->hash());
			fprintf(f,"hash_rloc: %s\n",blk->hash(false,true));
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

			u32 hcode=0;
			s32 gcode=-1;
			u8* pucode=(u8*)blk->code;

			size_t j=0;
			
			fprintf(f,"{\n");
			for (;j<blk->oplist.size();j++)
			{
				shil_opcode* op = &blk->oplist[j];
				fprint_hex(f,"//h:",pucode,hcode,op->host_offs);

				if (gcode!=op->guest_offs)
				{
					gcode=op->guest_offs;
					u32 rpc=blk->vaddr+gcode;
#ifndef NO_MMU
					try {
#endif
						u16 op=IReadMem16(rpc);

						char temp[128];
						OpDesc[op]->Disassemble(temp,rpc,op);

						fprintf(f,"//g: %04X %s\n", op, temp);
#ifndef NO_MMU
					} catch (SH4ThrownException& ex) {
						fprintf(f,"//g: ???? (page fault)\n");
					}
#endif
				}

				string s=op->dissasm();
				fprintf(f,"//il:%d:%d: %s\n",op->guest_offs,op->host_offs,s.c_str());
			}
			
			fprint_hex(f,"//h:",pucode,hcode,blk->host_code_size);

			fprintf(f,"}\n");
		}

		blk->runs=0;
	}

	if (f) fclose(f);
}
#endif
#endif

