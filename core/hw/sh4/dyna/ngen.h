// Header file for native generator interface

#pragma once
#include "blockmanager.h"
#include "oslib/host_context.h"

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

//Called from ngen_FailedToFindBlock
DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock(u32 pc);
DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock_pc();
//Called when a block check failed, and the block needs to be invalidated
DynarecCodeEntryPtr DYNACALL rdv_BlockCheckFail(u32 addr);
//Called to compile code @pc
DynarecCodeEntryPtr rdv_CompilePC(u32 blockcheck_failures);
//Finds or compiles code @pc
DynarecCodeEntryPtr rdv_FindOrCompile();
// Registers a custom FailedToFindBlock handler function
void rdv_SetFailedToFindBlockHandler(void (*handler)());

//code -> pointer to code of block, dpc -> if dynamic block, pc. if cond, 0 for next, 1 for branch
void* DYNACALL rdv_LinkBlock(u8* code,u32 dpc);

//Value to be returned when the block manager failed to find a block,
//should call rdv_FailedToFindBlock and then jump to the return value
// Set with rdv_SetFailedToFindBlockHandler(). Internal use only.
extern void (*ngen_FailedToFindBlock)();

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

bool rdv_readMemImmediate(u32 addr, int size, void*& ptr, bool& isRam, u32& physAddr, RuntimeBlockInfo* block = nullptr);
bool rdv_writeMemImmediate(u32 addr, int size, void*& ptr, bool& isRam, u32& physAddr, RuntimeBlockInfo* block = nullptr);

class Sh4CodeBuffer
{
public:
	// Return the current position of the code buffer, where new code should be emitted.
	void *get();
	// Advance the buffer position by 'size' bytes.
	void advance(u32 size);
	// Return the available free space in bytes.
	u32 getFreeSpace();
	// Return a pointer to the beginning of the code buffer.
	void *getBase();
	// Return the full size of the code buffer.
	// Note that the code buffer may be partitioned (long term blocks, short term blocks) so the full size
	// might be greater than what getFreeSpace() returns when the buffer is empty.
	u32 getSize();

	// Select the long term or temporary buffer (internal use)
	void useTempBuffer(bool enable) { tempBuffer = enable; }
	// Reset main or temp code buffer position to 0 (internal use)
	void reset(bool temporary);

private:
	u32 lastAddr = 0;
	u32 tempLastAddr = 0;
	bool tempBuffer = false;
};

class Sh4Dynarec
{
public:
	// Initialize the dynarec, which should keep a reference to the passed code buffer to generate code later.
	virtual void init(Sh4CodeBuffer& codeBuffer) = 0;
	// Compile the given block.
	// If smc_checks is true, add self-modifying code detection.
	// If optimize is true, use fast memory accesses if possible, that will be rewritten if they fail.
	virtual void compile(RuntimeBlockInfo* block, bool smc_checks, bool optimise) = 0;
	// Signal the dynarec that the code buffer has been cleared. It should re-generate its main loop if needed.
	virtual void reset() {}
	// Run the code.
	// cntx points right after the Sh4RCB struct, which corresponds to the start of the 512 MB virtual address space
	// (if available).
	virtual void mainloop(void* cntx) = 0;
	// An SH4 exception has occurred. The dynarec should abort execution of the current block and longjmp to its main loop
	// to execute the exception handler.
	virtual void handleException(host_context_t& context) = 0;
	// Rewrite the memory access at host PC address 'faultAddress'. This fast memory access failed and should be rewritten
	// to use mem access handlers.
	virtual bool rewrite(host_context_t& context, void *faultAddress) = 0;
	// Allocate a new block information structure.
	virtual RuntimeBlockInfo *allocateBlock() {
		return new RuntimeBlockInfo();
	}

	// Dynarec canonical implementation callback methods.
	// Used to call default implementation of shil ops that the dynarec doesn't implement.
	// Start call
	virtual void canonStart(const shil_opcode *op) = 0;
	// Pass input parameter or retrieve return parameter
	virtual void canonParam(const shil_opcode *op, const shil_param *param, CanonicalParamType paramType) = 0;
	// Call the implementation
	virtual void canonCall(const shil_opcode *op, void *function) = 0;
	// Finish call
	virtual void canonFinish(const shil_opcode *op) = 0;

	virtual ~Sh4Dynarec() = default;
};

extern Sh4Dynarec *sh4Dynarec;
