#pragma once

#include "types.h"
#include "decoder.h"
#include "stdclass.h"

#include <memory>

typedef void (*DynarecCodeEntryPtr)();
struct RuntimeBlockInfo;
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
	u32 host_opcodes;	// set by host code generator, optional
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

	BlockEndType BlockType;
	bool has_jcond;

	std::vector<shil_opcode> oplist;

	bool containsCode(const void *ptr)
	{
		return (u32)((const u8 *)ptr - (const u8 *)code) < host_code_size;
	}

	virtual ~RuntimeBlockInfo();

	virtual u32 Relink()=0;
	virtual void Relocate(void* dst)=0;
	
	//predecessors references
	std::vector<RuntimeBlockInfoPtr> pre_refs;

	void AddRef(const RuntimeBlockInfoPtr& other);
	void RemRef(const RuntimeBlockInfoPtr& other);

	void Discard();
	void SetProtectedFlags();

	bool read_only;
};

void bm_WriteBlockMap(const std::string& file);

DynarecCodeEntryPtr DYNACALL bm_GetCodeByVAddr(u32 addr);
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

void bm_vmem_pagefill(void** ptr,u32 size_bytes);
bool bm_RamWriteAccess(void *p);
void bm_RamWriteAccess(u32 addr);
static inline bool bm_IsRamPageProtected(u32 addr)
{
	extern bool unprotected_pages[RAM_SIZE_MAX/PAGE_SIZE];
	addr &= RAM_MASK;
	return !unprotected_pages[addr / PAGE_SIZE];
}
void bm_LockPage(u32 addr, u32 size = PAGE_SIZE);
void bm_UnlockPage(u32 addr, u32 size = PAGE_SIZE);
u32 bm_getRamOffset(void *p);

