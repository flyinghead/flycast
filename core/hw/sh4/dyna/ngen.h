/*
	Header file for native generator interface
	Needs some cleanup


	SH4 -> Code gen -> Ram

	Ram -> link/relocate -> Staging buffer
	Ram -> link/relocate -> Steady-state buffer

	Staging      : scratch, relatively small, circular code buffer
	Steady state : 'final' code buffer. When blocks reach a steady-state, they get copied here

	When the Staging buffer is full, a reset is done on the dynarec.
	If the stating buffer is full, but re-locating everything will free enough space, it will be relocated (GC'd)

	If the stating buffer is full, then blocks in it are put into "hibernation"

	Block can be
	in Ram, only ('hibernated')
	in Ram + steady state buffer
	in Ram + staging buffer

	Changes required on the ngen/dynarecs for this to work

	- Support relocation
	- Support re-linking
	- Support hibernated blocks, or block removal

	Changes on BM

	- Block graph
	- Block removal
	- Relocation driving logic


	This will enable

	- Extensive block specialisation (Further mem opts, other things that might gain)
	- Possibility of superblock chains
*/

#pragma once
#include "blockmanager.h"
#include "oslib/host_context.h"

#define CODE_SIZE   (10*1024*1024)
#define TEMP_CODE_SIZE (1024*1024)

// When NO_RWX is enabled there's two address-spaces, one executable and
// one writtable. The emitter and most of the code in rec-* will work with
// the RW pointer. However the fpcb table and other pointers during execution
// (ie. exceptions) are RX pointers. These two macros convert between them by
// sub/add the pointer offset. CodeCache will point to the RW pointer for simplicity.
#ifdef FEAT_NO_RWX_PAGES
	extern ptrdiff_t cc_rx_offset;
	#define CC_RW2RX(ptr) (void*)(((uintptr_t)(ptr)) + cc_rx_offset)
	#define CC_RX2RW(ptr) (void*)(((uintptr_t)(ptr)) - cc_rx_offset)
#else
	#define CC_RW2RX(ptr) (ptr)
	#define CC_RX2RW(ptr) (ptr)
#endif

extern u8* CodeCache;

void emit_Skip(u32 sz);
u32 emit_FreeSpace();
void* emit_GetCCPtr();

//Called from ngen_FailedToFindBlock
DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock(u32 pc);
DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock_pc();
//Called when a block check failed, and the block needs to be invalidated
DynarecCodeEntryPtr DYNACALL rdv_BlockCheckFail(u32 addr);
//Called to compile code @pc
DynarecCodeEntryPtr rdv_CompilePC(u32 blockcheck_failures);
//Finds or compiles code @pc
DynarecCodeEntryPtr rdv_FindOrCompile();

//code -> pointer to code of block, dpc -> if dynamic block, pc. if cond, 0 for next, 1 for branch
void* DYNACALL rdv_LinkBlock(u8* code,u32 dpc);

u32 DYNACALL rdv_DoInterrupts(void* block_cpde);
u32 DYNACALL rdv_DoInterrupts_pc(u32 pc);

//Stuff to be implemented per dynarec core

void ngen_init();

//Called to compile a block
void ngen_Compile(RuntimeBlockInfo* block, bool smc_checks, bool reset, bool staging, bool optimise);

//Called when blocks are reset
void ngen_ResetBlocks();
//Value to be returned when the block manager failed to find a block,
//should call rdv_FailedToFindBlock and then jump to the return value
extern void (*ngen_FailedToFindBlock)();
// The dynarec mainloop
// cntx points right after the Sh4RCB struct,
// which corresponds to the start of the 512 MB or 4 GB virtual address space if enabled.
void ngen_mainloop(void* cntx);

void ngen_HandleException(host_context_t &context);
bool ngen_Rewrite(host_context_t &context, void *faultAddress);

//Canonical callback interface
enum CanonicalParamType
{
	CPT_u32,
	CPT_u32rv,
	CPT_u64rvL,
	CPT_u64rvH,
	CPT_f32,
	CPT_f32rv,
	CPT_ptr,
};

void ngen_CC_Start(shil_opcode* op);
void ngen_CC_Param(shil_opcode* op,shil_param* par,CanonicalParamType tp);
void ngen_CC_Call(shil_opcode* op,void* function);
void ngen_CC_Finish(shil_opcode* op);

RuntimeBlockInfo* ngen_AllocateBlock();
