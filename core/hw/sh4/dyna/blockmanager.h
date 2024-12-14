#pragma once

#include "types.h"
#include "decoder.h"
#include "shil.h"
#include "stdclass.h"

#include <memory>

typedef void (*DynarecCodeEntryPtr)();
struct RuntimeBlockInfo;
typedef std::shared_ptr<RuntimeBlockInfo> RuntimeBlockInfoPtr;

struct RuntimeBlockInfo
{
	bool Setup(u32 pc,fpscr_t fpu_cfg);

	u32 addr;
	u32 vaddr;
	DynarecCodeEntryPtr code;

	u32 host_code_size;	//in bytes
	u32 sh4_code_size; //in bytes

	fpscr_t fpu_cfg;
	u32 guest_cycles;
	u32 guest_opcodes;
	u32 host_opcodes;	// set by host code generator, optional
	bool has_fpu_op;
	bool temp_block;
	u32 blockcheck_failures;

	u32 BranchBlock; //if not 0xFFFFFFFF then jump target
	u32 NextBlock;   //if not 0xFFFFFFFF then next block (by position)

	//0 if not available
	RuntimeBlockInfo* pBranchBlock;
	RuntimeBlockInfo* pNextBlock; 

	u32 relink_offset;
	u32 relink_data;

	BlockEndType BlockType;
	bool has_jcond;
	bool read_only;

	std::vector<shil_opcode> oplist;
	//predecessors references
	std::vector<RuntimeBlockInfoPtr> pre_refs;

	bool containsCode(const void *ptr)
	{
		return (u32)((const u8 *)ptr - (const u8 *)code) < host_code_size;
	}

	virtual ~RuntimeBlockInfo();

	virtual u32 Relink() {
		return 0;
	}
	
	void AddRef(const RuntimeBlockInfoPtr& other);
	void RemRef(const RuntimeBlockInfoPtr& other);

	void Discard();
	void SetProtectedFlags();
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
static inline bool bm_IsRamPageProtected(u32 addr)
{
	extern bool unprotected_pages[RAM_SIZE_MAX/PAGE_SIZE];
	addr &= RAM_MASK;
	return !unprotected_pages[addr / PAGE_SIZE];
}

#if FEAT_SHREC != DYNAREC_NONE

bool bm_RamWriteAccess(void *p);
void bm_RamWriteAccess(u32 addr);
void bm_LockPage(u32 addr, u32 size = PAGE_SIZE);
void bm_UnlockPage(u32 addr, u32 size = PAGE_SIZE);
u32 bm_getRamOffset(void *p);

#else

inline static bool bm_RamWriteAccess(void *p) {
	return false;
}
inline static void bm_RamWriteAccess(u32 addr) {}
inline static void bm_LockPage(u32 addr, u32 size = PAGE_SIZE) {}
inline static void bm_UnlockPage(u32 addr, u32 size = PAGE_SIZE) {}
inline static u32 bm_getRamOffset(void *p) {
	return 0;
}

#endif
