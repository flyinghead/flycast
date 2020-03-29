/*
	In case you wonder, the extern "C" stuff are for the assembly code on beagleboard/pandora
*/
#pragma once

#include "types.h"
#include "decoder.h"
#include "stdclass.h"

#include <memory>

typedef void (*DynarecCodeEntryPtr)();
typedef std::shared_ptr<RuntimeBlockInfo> RuntimeBlockInfoPtr;

struct RuntimeBlockInfo_Core
{
	u32 addr;
	DynarecCodeEntryPtr code;
	u32 lookups;
};

struct RuntimeBlockInfo: RuntimeBlockInfo_Core
{
	bool Setup(u32 pc,fpscr_t fpu_cfg);
	const char* hash();

	u32 vaddr;

	u32 host_code_size;	//in bytes
	u32 sh4_code_size; //in bytes

	u32 runs;
	s32 staging_runs;

	fpscr_t fpu_cfg;
	u32 guest_cycles;
	u32 guest_opcodes;
	u32 host_opcodes;
	bool has_fpu_op;
	u32 blockcheck_failures;
	bool temp_block;

	u32 BranchBlock; //if not 0xFFFFFFFF then jump target
	u32 NextBlock;   //if not 0xFFFFFFFF then next block (by position)

	//0 if not available
	RuntimeBlockInfo* pBranchBlock;
	RuntimeBlockInfo* pNextBlock; 

	u32 relink_offset;
	u32 relink_data;
	u32 csc_RetCache; //only for stats for now

	BlockEndType BlockType;
	bool has_jcond;

	std::vector<shil_opcode> oplist;

	bool contains_code(u8* ptr)
	{
		return ((unat)(ptr-(u8*)code))<host_code_size;
	}

	virtual ~RuntimeBlockInfo();

	virtual u32 Relink()=0;
	virtual void Relocate(void* dst)=0;
	
	//predecessors references
	std::vector<RuntimeBlockInfoPtr> pre_refs;

	void AddRef(RuntimeBlockInfoPtr other);
	void RemRef(RuntimeBlockInfoPtr other);

	void Discard();
	void UpdateRefs();
	void SetProtectedFlags();

	u32 memops;
	u32 linkedmemops;
	std::map<void*, u32> memory_accesses;	// key is host pc when access is made, value is opcode id
	bool read_only;
};

void bm_WriteBlockMap(const std::string& file);


extern "C" {
ATTR_USED DynarecCodeEntryPtr DYNACALL bm_GetCodeByVAddr(u32 addr);
}

RuntimeBlockInfoPtr bm_GetBlock(void* dynarec_code);
RuntimeBlockInfoPtr bm_GetStaleBlock(void* dynarec_code);
RuntimeBlockInfoPtr DYNACALL bm_GetBlock(u32 addr);

void bm_AddBlock(RuntimeBlockInfo* blk);
void bm_DiscardBlock(RuntimeBlockInfo* block);
void bm_Reset();
void bm_ResetCache();
void bm_ResetTempCache(bool full);
void bm_Periodical_1s();

void bm_Init();
void bm_Term();

void bm_vmem_pagefill(void** ptr,u32 PAGE_SZ);
bool bm_RamWriteAccess(void *p);
void bm_RamWriteAccess(u32 addr);
static inline bool bm_IsRamPageProtected(u32 addr)
{
	extern bool unprotected_pages[RAM_SIZE_MAX/PAGE_SIZE];
	addr &= RAM_MASK;
	return !unprotected_pages[addr / PAGE_SIZE];
}
