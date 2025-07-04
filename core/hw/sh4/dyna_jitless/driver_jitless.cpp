#include "types.h"
#include <unordered_set>
#include <cmath>

#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/ir/sh4_ir_interpreter.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/modules/mmu.h"

#include "blockmanager_jitless.h"
#include "ngen_jitless.h"
#include "decoder_jitless.h"
#include "oslib/virtmem.h"

#if FEAT_SHREC == DYNAREC_JITLESS

constexpr u32 CODE_SIZE = 10_MB;
constexpr u32 TEMP_CODE_SIZE = 1_MB;
constexpr u32 FULL_SIZE = CODE_SIZE + TEMP_CODE_SIZE;

#if defined(_WIN32) || FEAT_SHREC != DYNAREC_JIT || defined(TARGET_IPHONE) || defined(TARGET_ARM_MAC)
static u8 *SH4_TCB;
#else
alignas(4096) static u8 SH4_TCB[FULL_SIZE]
#if defined(__OpenBSD__)
	__attribute__((section(".openbsd.mutable")));
#elif defined(__unix__) || defined(__SWITCH__)
	__attribute__((section(".text")));
#elif defined(__APPLE__)
	__attribute__((section("__TEXT,.text")));
#else
	#error SH4_TCB ALLOC
#endif
#endif

static u8* CodeCache;
static u8* TempCodeCache;
ptrdiff_t cc_rx_offset;

static std::unordered_set<u32> smc_hotspots;

static sh4_if sh4Interp;
static Sh4CodeBuffer codeBuffer;
Sh4Dynarec *sh4Dynarec;

// Global pointer to track the current block being executed
static RuntimeBlockInfo* currentBlock = nullptr;

// Forward declaration for interpreter function
static void DYNACALL executeShilInterpreter();
static void executeShilBlock(RuntimeBlockInfo* block);

// Helper functions for parameter access
static u32 getParamU32(const shil_param& param) {
    if (param.is_imm()) {
        return param.imm_value();
    } else if (param.is_reg()) {
        return *param.reg_ptr();
    }
    die("Invalid parameter type for u32");
    return 0;
}

static f32 getParamF32(const shil_param& param) {
    if (param.is_reg()) {
        return *(f32*)param.reg_ptr();
    }
    die("Invalid parameter type for f32");
    return 0.0f;
}

static void setParamU32(const shil_param& param, u32 value) {
    if (param.is_reg()) {
        *param.reg_ptr() = value;
    } else {
        die("Cannot set value to non-register parameter");
    }
}

static void setParamF32(const shil_param& param, f32 value) {
    if (param.is_reg()) {
        *(f32*)param.reg_ptr() = value;
    } else {
        die("Cannot set value to non-register parameter");
    }
}

static void setParamU64(const shil_param& rd1, const shil_param& rd2, u64 value) {
    if (rd1.is_reg() && rd2.is_reg()) {
        *rd1.reg_ptr() = (u32)value;        // Low 32 bits
        *rd2.reg_ptr() = (u32)(value >> 32); // High 32 bits
    } else {
        die("Cannot set u64 value to non-register parameters");
    }
}

// Execute a SHIL block using interpretation
static void executeShilBlock(RuntimeBlockInfo* block) {
    // Execute each SHIL opcode in the block
    for (const shil_opcode& op : block->oplist) {
        // Simple opcode dispatcher - expand this as needed
        switch (op.op) {
            // Memory operations (simplified for now)
            case shop_mov32:
                if (!op.rd.is_null() && !op.rs1.is_null()) {
                    setParamU32(op.rd, getParamU32(op.rs1));
                }
                break;
                
            case shop_mov64:
                // 64-bit move - copy two 32-bit values
                if (!op.rd.is_null() && !op.rd2.is_null() && !op.rs1.is_null() && !op.rs2.is_null()) {
                    setParamU32(op.rd, getParamU32(op.rs1));   // Low part
                    setParamU32(op.rd2, getParamU32(op.rs2));  // High part
                }
                break;
                
            // Arithmetic operations
            case shop_add: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 + r2);
                break;
            }
            
            case shop_sub: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 - r2);
                break;
            }
            
            case shop_and: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 & r2);
                break;
            }
            
            case shop_or: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 | r2);
                break;
            }
            
            case shop_xor: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 ^ r2);
                break;
            }
            
            case shop_not: {
                u32 r1 = getParamU32(op.rs1);
                setParamU32(op.rd, ~r1);
                break;
            }
            
            case shop_neg: {
                u32 r1 = getParamU32(op.rs1);
                setParamU32(op.rd, -(s32)r1);
                break;
            }
            
            // Shift operations
            case shop_shl: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 << r2);
                break;
            }
            
            case shop_shr: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 >> r2);
                break;
            }
            
            case shop_sar: {
                s32 r1 = (s32)getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, r1 >> r2);
                break;
            }
            
            // Compare and test operations
            case shop_test: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, (r1 & r2) ? 0 : 1); // T flag result
                break;
            }
            
            case shop_seteq: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, (r1 == r2) ? 1 : 0);
                break;
            }
            
            case shop_setge: {
                s32 r1 = (s32)getParamU32(op.rs1);
                s32 r2 = (s32)getParamU32(op.rs2);
                setParamU32(op.rd, (r1 >= r2) ? 1 : 0);
                break;
            }
            
            case shop_setgt: {
                s32 r1 = (s32)getParamU32(op.rs1);
                s32 r2 = (s32)getParamU32(op.rs2);
                setParamU32(op.rd, (r1 > r2) ? 1 : 0);
                break;
            }
            
            case shop_setae: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, (r1 >= r2) ? 1 : 0);
                break;
            }
            
            case shop_setab: {
                u32 r1 = getParamU32(op.rs1);
                u32 r2 = getParamU32(op.rs2);
                setParamU32(op.rd, (r1 > r2) ? 1 : 0);
                break;
            }
            
            // System operations
            case shop_sync_sr:
                UpdateSR();
                break;
                
            case shop_sync_fpscr:
                UpdateFPSCR();
                break;
                
            // Basic floating point operations
            case shop_fadd: {
                f32 r1 = getParamF32(op.rs1);
                f32 r2 = getParamF32(op.rs2);
                setParamF32(op.rd, r1 + r2);
                break;
            }
            
            case shop_fsub: {
                f32 r1 = getParamF32(op.rs1);
                f32 r2 = getParamF32(op.rs2);
                setParamF32(op.rd, r1 - r2);
                break;
            }
            
            case shop_fmul: {
                f32 r1 = getParamF32(op.rs1);
                f32 r2 = getParamF32(op.rs2);
                setParamF32(op.rd, r1 * r2);
                break;
            }
            
            case shop_fdiv: {
                f32 r1 = getParamF32(op.rs1);
                f32 r2 = getParamF32(op.rs2);
                setParamF32(op.rd, r1 / r2);
                break;
            }
            
            case shop_fabs: {
                f32 r1 = getParamF32(op.rs1);
                setParamF32(op.rd, fabsf(r1));
                break;
            }
            
            case shop_fneg: {
                f32 r1 = getParamF32(op.rs1);
                setParamF32(op.rd, -r1);
                break;
            }
            
            // Control flow
            case shop_jdyn:
                // Dynamic jump - get target from register
                next_pc = getParamU32(op.rs1);
                return; // Exit block execution
                
            case shop_jcond: {
                // Conditional jump based on T flag
                u32 condition = getParamU32(op.rs2);
                if (condition != 0) {
                    next_pc = op.rs1.imm_value(); // True branch
                } else {
                    next_pc = op.rs3.imm_value(); // False branch  
                }
                return; // Exit block execution
            }
            
            case shop_ifb:
                // Simple block terminator - continue to next block
                next_pc = op.rs1.imm_value();
                return; // Exit block execution
            
            // Memory operations (simplified - real implementation would need proper address translation)
            case shop_readm: {
                u32 addr = getParamU32(op.rs1);
                u32 value;
                switch (op.size) {
                    case 1: value = ReadMem8(addr); break;
                    case 2: value = ReadMem16(addr); break;
                    case 4: value = ReadMem32(addr); break;
                    default: 
                        WARN_LOG(DYNAREC, "Unsupported read size: %d", op.size);
                        value = 0;
                        break;
                }
                setParamU32(op.rd, value);
                break;
            }
            
            case shop_writem: {
                u32 addr = getParamU32(op.rs1);
                u32 value = getParamU32(op.rs2);
                switch (op.size) {
                    case 1: WriteMem8(addr, value); break;
                    case 2: WriteMem16(addr, value); break;
                    case 4: WriteMem32(addr, value); break;
                    default:
                        WARN_LOG(DYNAREC, "Unsupported write size: %d", op.size);
                        break;
                }
                break;
            }
            
            // Extension operations
            case shop_ext_s8: {
                u32 r1 = getParamU32(op.rs1);
                setParamU32(op.rd, (s32)(s8)r1);
                break;
            }
            
            case shop_ext_s16: {
                u32 r1 = getParamU32(op.rs1);
                setParamU32(op.rd, (s32)(s16)r1);
                break;
            }
            
            // TODO: Add more opcodes as needed
            default:
                // For unhandled opcodes, log a warning but continue
                WARN_LOG(DYNAREC, "Unhandled SHIL opcode: %s (%d) at PC %08X", 
                        shil_opcode_name(op.op), op.op, next_pc);
                break;
        }
    }
    
    // If we reach here without a jump, continue to the next block
    next_pc = block->NextBlock;
    
    // Advance cycles based on the block's guest cycles
    sh4rcb.cntx.cycle_counter += block->guest_cycles;
}

// Placeholder for future memory operation helpers

// Simple jitless dynarec class that initializes the global pointer
class JitlessDynarec : public Sh4Dynarec {
public:
    JitlessDynarec() {
        sh4Dynarec = this;
    }
    
    void init(Sh4CodeBuffer& codeBuffer) override {
        // Simple initialization - just keep a reference
        this->codeBuffer = &codeBuffer;
    }
    
    void compile(RuntimeBlockInfo* block, bool smc_checks, bool optimise) override {
        // For the jitless dynarec, we don't generate machine code
        // Instead, we store the SHIL opcodes and execute them via interpreter
        
        // We need unique code addresses for the block manager to work correctly
        u8* code_ptr = (u8*)codeBuffer->get();
        block->code = (DynarecCodeEntryPtr)code_ptr;  // Each block gets unique address
        
        // Store the block pointer at this location so we can retrieve it later
        *(RuntimeBlockInfo**)code_ptr = block;
        
        // Advance the code buffer to ensure next block gets different address
        codeBuffer->advance(sizeof(RuntimeBlockInfo*));
        block->host_code_size = sizeof(RuntimeBlockInfo*);
        
        // The SHIL opcodes are already stored in block->oplist by the decoder
        // The interpreter will retrieve the block using bm_GetBlock(code_ptr)
    }
    
    void mainloop(void* cntx) override {
        // Main loop for jitless dynarec using SHIL interpretation
        // Don't reassign p_sh4rcb - use the existing context to avoid FPCA table mismatch
        
        do {
            try {
                // Find or compile the block for the current PC
                INFO_LOG(DYNAREC, "🔧 Looking for block at PC=0x%08X", next_pc);
                DynarecCodeEntryPtr code_ptr = bm_GetCodeByVAddr(next_pc);
                INFO_LOG(DYNAREC, "🔧 bm_GetCodeByVAddr returned: %p", code_ptr);
                
                if (unlikely(code_ptr == ngen_FailedToFindBlock)) {
                    INFO_LOG(DYNAREC, "🔧 Block not found, creating jitless block...");
                    code_ptr = createJitlessBlock(next_pc);
                    INFO_LOG(DYNAREC, "🔧 createJitlessBlock returned: %p", code_ptr);
                }
                
                INFO_LOG(DYNAREC, "🔧 About to execute block: next_pc=0x%08X code_ptr=%p", next_pc, code_ptr);
                
                // Check if this is a jitless block (low bit set) or regular JIT block
                if (reinterpret_cast<uintptr_t>(code_ptr) & 0x1) {
                    // This is a jitless block - extract the RuntimeBlockInfo pointer
                    RuntimeBlockInfo* block = reinterpret_cast<RuntimeBlockInfo*>(reinterpret_cast<uintptr_t>(code_ptr) & ~0x1);
                    
                    INFO_LOG(DYNAREC, "🔧 Jitless block detected: block=%p addr=0x%08X oplist.size=%zu", block, block->addr, block->oplist.size());
                    
                    // Execute the block using SHIL interpretation
                    executeShilBlock(block);
                } else {
                    // This should not happen in jitless mode, but handle it gracefully
                    ERROR_LOG(DYNAREC, "🔧 WARNING: Regular JIT block found in jitless mode: %p", code_ptr);
                    
                    // Get the block from the code pointer (regular JIT format)
                    RuntimeBlockInfo* block = *(RuntimeBlockInfo**)code_ptr;
                    
                    INFO_LOG(DYNAREC, "🔧 Fallback block info: block=%p addr=0x%08X oplist.size=%zu", block, block->addr, block->oplist.size());
                    
                    // Execute the block using SHIL interpretation
                    executeShilBlock(block);
                }
                
            } catch (const SH4ThrownException&) {
                // Handle SH4 exceptions by breaking out of loop
                break;
            }
        } while (p_sh4rcb->cntx.CpuRunning);
    }
    
    void handleException(host_context_t& context) override {
        // Simple exception handling - do nothing for now
    }
    
    bool rewrite(host_context_t& context, void *faultAddress) override {
        // Simple rewrite - always return false (no rewriting)
        return false;
    }
    
    void canonStart(const shil_opcode *op) override {
        // Canonical function start - do nothing
    }
    
    void canonParam(const shil_opcode *op, const shil_param *param, CanonicalParamType paramType) override {
        // Canonical function parameter - do nothing
    }
    
    void canonCall(const shil_opcode *op, void *function) override {
        // Canonical function call - do nothing
    }
    
    void canonFinish(const shil_opcode *op) override {
        // Canonical function finish - do nothing
    }
    
    DynarecCodeEntryPtr createJitlessBlock(u32 pc) {
        ERROR_LOG(DYNAREC, "🔧 createJitlessBlock: Creating block for PC=0x%08X", pc);
        
        // Step 1: Allocate a RuntimeBlockInfo
        RuntimeBlockInfo* rbi = this->allocateBlock();
        if (!rbi) {
            ERROR_LOG(DYNAREC, "🔧 createJitlessBlock: Failed to allocate block");
            return ngen_FailedToFindBlock;
        }
        
        // Step 2: Setup the block (SH4 → SHIL decoding and optimization)
        ERROR_LOG(DYNAREC, "🔧 createJitlessBlock: Calling rbi->Setup()");
        if (!rbi->Setup(pc, fpscr)) {
            ERROR_LOG(DYNAREC, "🔧 createJitlessBlock: rbi->Setup() failed");
            delete rbi;
            return ngen_FailedToFindBlock;
        }
        
        ERROR_LOG(DYNAREC, "🔧 createJitlessBlock: Block setup successful, addr=0x%08X oplist.size=%zu", rbi->addr, rbi->oplist.size());
        
        // Step 3: Create a unique "code" pointer for this block
        // We'll use the block's memory address as a unique identifier
        rbi->code = reinterpret_cast<DynarecCodeEntryPtr>(reinterpret_cast<uintptr_t>(rbi) | 0x1);
        
        ERROR_LOG(DYNAREC, "🔧 createJitlessBlock: Assigned code pointer: %p", rbi->code);
        
        // Step 4: Add to block manager
        bm_AddBlock(rbi);
        
        ERROR_LOG(DYNAREC, "🔧 createJitlessBlock: Block added to manager, returning %p", rbi->code);
        return rbi->code;
    }

private:
    Sh4CodeBuffer *codeBuffer = nullptr;
};

static JitlessDynarec instance;

void *Sh4CodeBuffer::get()
{
	return tempBuffer ? &TempCodeCache[tempLastAddr] : &CodeCache[lastAddr];
}

void Sh4CodeBuffer::advance(u32 size)
{
	if (tempBuffer)
		tempLastAddr += size;
	else
		lastAddr += size;
}

u32 Sh4CodeBuffer::getFreeSpace()
{
	if (tempBuffer)
		return TEMP_CODE_SIZE - tempLastAddr;
	else
		return CODE_SIZE - lastAddr;
}

void *Sh4CodeBuffer::getBase()
{
	return CodeCache;
}

u32 Sh4CodeBuffer::getSize()
{
	return FULL_SIZE;
}

void Sh4CodeBuffer::reset(bool temporary)
{
	if (temporary)
		tempLastAddr = 0;
	else
		lastAddr = 0;
}

static void clear_temp_cache(bool full)
{
	//printf("recSh4:Temp Code Cache clear at %08X\n", curr_pc);
	codeBuffer.reset(true);
	bm_ResetTempCache(full);
}

static void recSh4_ClearCache()
{
	INFO_LOG(DYNAREC, "recSh4:Dynarec Cache clear at %08X free space %d", next_pc, codeBuffer.getFreeSpace());
	codeBuffer.reset(false);
	bm_ResetCache();
	smc_hotspots.clear();
	clear_temp_cache(true);
}

static void recSh4_Run()
{
	RestoreHostRoundingMode();

	u8 *sh4_dyna_rcb = (u8 *)&Sh4cntx + sizeof(Sh4cntx);
	INFO_LOG(DYNAREC, "cntx // fpcb offset: %td // pc offset: %td // pc %08X", (u8*)&sh4rcb.fpcb - sh4_dyna_rcb, (u8*)&sh4rcb.cntx.pc - sh4_dyna_rcb, sh4rcb.cntx.pc);

	sh4Dynarec->mainloop(sh4_dyna_rcb);

	sh4_int_bCpuRun = false;
}

void AnalyseBlock(RuntimeBlockInfo* blk);

bool RuntimeBlockInfo::Setup(u32 rpc,fpscr_t rfpu_cfg)
{
	addr = host_code_size = 0;
	guest_cycles = guest_opcodes = host_opcodes = 0;
	sh4_code_size = 0;
	pBranchBlock=pNextBlock=0;
	code=0;
	has_jcond=false;
	BranchBlock = NullAddress;
	NextBlock = NullAddress;
	BlockType = BET_SCL_Intr;
	has_fpu_op = false;
	temp_block = false;

	vaddr = rpc;
	if (vaddr & 1)
	{
		// read address error
		Do_Exception(vaddr, Sh4Ex_AddressErrorRead);
		return false;
	}
	else if (mmu_enabled())
	{
		MmuError rv = mmu_instruction_translation(vaddr, addr);
		if (rv != MmuError::NONE)
		{
			DoMMUException(vaddr, rv, MMU_TT_IREAD);
			return false;
		}
	}
	else
	{
		addr = vaddr;
	}
	fpu_cfg=rfpu_cfg;

	oplist.clear();

	try {
		if (!dec_DecodeBlock(this, SH4_TIMESLICE / 2))
			return false;
	}
	catch (const SH4ThrownException& ex) {
		Do_Exception(rpc, ex.expEvn);
		return false;
	}
	SetProtectedFlags();

	AnalyseBlock(this);

	return true;
}

DynarecCodeEntryPtr rdv_CompilePC(u32 blockcheck_failures)
{
	const u32 pc = next_pc;

	if (codeBuffer.getFreeSpace() < 32_KB || pc == 0x8c0000e0 || pc == 0xac010000 || pc == 0xac008300)
		recSh4_ClearCache();

	RuntimeBlockInfo* rbi = sh4Dynarec->allocateBlock();

	if (!rbi->Setup(pc, fpscr))
	{
		delete rbi;
		return nullptr;
	}
	rbi->blockcheck_failures = blockcheck_failures;
	if (smc_hotspots.find(rbi->addr) != smc_hotspots.end())
	{
		codeBuffer.useTempBuffer(true);
		if (codeBuffer.getFreeSpace() < 32_KB)
			clear_temp_cache(false);
		rbi->temp_block = true;
		if (rbi->read_only)
			INFO_LOG(DYNAREC, "WARNING: temp block %x (%x) is protected!", rbi->vaddr, rbi->addr);
	}
	bool do_opts = !rbi->temp_block;
	bool block_check = !rbi->read_only;
	sh4Dynarec->compile(rbi, block_check, do_opts);
	verify(rbi->code != nullptr);

	bm_AddBlock(rbi);

	codeBuffer.useTempBuffer(false);

	return rbi->code;
}

DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock_pc()
{
	return rdv_FailedToFindBlock(next_pc);
}

DynarecCodeEntryPtr DYNACALL rdv_FailedToFindBlock(u32 pc)
{
	//DEBUG_LOG(DYNAREC, "rdv_FailedToFindBlock %08x", pc);
	next_pc=pc;
	DynarecCodeEntryPtr code = rdv_CompilePC(0);
	if (code == NULL)
		code = bm_GetCodeByVAddr(next_pc);
	else
		code = (DynarecCodeEntryPtr)CC_RW2RX(code);
	return code;
}

static void ngen_FailedToFindBlock_internal() {
	rdv_FailedToFindBlock(Sh4cntx.pc);
}

void (*ngen_FailedToFindBlock)() = &ngen_FailedToFindBlock_internal;

// addr must be the physical address of the start of the block
DynarecCodeEntryPtr DYNACALL rdv_BlockCheckFail(u32 addr)
{
	DEBUG_LOG(DYNAREC, "rdv_BlockCheckFail @ %08x", addr);
	u32 blockcheck_failures = 0;
	if (mmu_enabled())
	{
		RuntimeBlockInfoPtr block = bm_GetBlock(addr);
		if (block)
		{
			blockcheck_failures = block->blockcheck_failures + 1;
			if (blockcheck_failures > 5)
			{
				bool inserted = smc_hotspots.insert(addr).second;
				if (inserted)
					DEBUG_LOG(DYNAREC, "rdv_BlockCheckFail SMC hotspot @ %08x fails %d", addr, blockcheck_failures);
			}
			bm_DiscardBlock(block.get());
		}
	}
	else
	{
		next_pc = addr;
		recSh4_ClearCache();
	}
	return (DynarecCodeEntryPtr)CC_RW2RX(rdv_CompilePC(blockcheck_failures));
}

DynarecCodeEntryPtr rdv_FindOrCompile()
{
	DynarecCodeEntryPtr rv = bm_GetCodeByVAddr(next_pc);  // Returns exec addr
	if (rv == ngen_FailedToFindBlock)
		rv = (DynarecCodeEntryPtr)CC_RW2RX(rdv_CompilePC(0));  // Returns rw addr

	return rv;
}

void* DYNACALL rdv_LinkBlock(u8* code,u32 dpc)
{
	// code is the RX addr to return after, however bm_GetBlock returns RW
	//DEBUG_LOG(DYNAREC, "rdv_LinkBlock %p pc %08x", code, dpc);
	RuntimeBlockInfoPtr rbi = bm_GetBlock(code);
	bool stale_block = false;
	if (!rbi)
	{
		stale_block = true;
		rbi = bm_GetStaleBlock(code);
	}

	verify(rbi != NULL);

	u32 bcls = BET_GET_CLS(rbi->BlockType);

	if (bcls == BET_CLS_Static)
	{
		if (rbi->BlockType == BET_StaticIntr)
			next_pc = rbi->NextBlock;
		else
			next_pc = rbi->BranchBlock;
	}
	else if (bcls == BET_CLS_Dynamic)
	{
		next_pc = dpc;
	}
	else if (bcls == BET_CLS_COND)
	{
		if (dpc)
			next_pc = rbi->BranchBlock;
		else
			next_pc = rbi->NextBlock;
	}

	DynarecCodeEntryPtr rv = rdv_FindOrCompile();  // Returns rx ptr

	if (!mmu_enabled() && !stale_block)
	{
		if (bcls == BET_CLS_Dynamic)
		{
			verify(rbi->relink_data == 0 || rbi->pBranchBlock == NULL);

			if (rbi->pBranchBlock != NULL)
			{
				rbi->pBranchBlock->RemRef(rbi);
				rbi->pBranchBlock = NULL;
				rbi->relink_data = 1;
			}
			else if (rbi->relink_data == 0)
			{
				rbi->pBranchBlock = bm_GetBlock(next_pc).get();
				rbi->pBranchBlock->AddRef(rbi);
			}
		}
		else
		{
			RuntimeBlockInfo* nxt = bm_GetBlock(next_pc).get();

			if (rbi->BranchBlock == next_pc)
				rbi->pBranchBlock = nxt;
			if (rbi->NextBlock == next_pc)
				rbi->pNextBlock = nxt;

			nxt->AddRef(rbi);
		}
		u32 ncs = rbi->relink_offset + rbi->Relink();
		verify(rbi->host_code_size >= ncs);
		rbi->host_code_size = ncs;
	}
	else
	{
		INFO_LOG(DYNAREC, "null RBI: from %08X to %08X -- unlinked stale block -- code %p next %p", rbi->vaddr, next_pc, code, rv);
	}

	return (void*)rv;
}

static void recSh4_Start()
{
	sh4Interp.Start();
}

static void recSh4_Stop()
{
	sh4Interp.Stop();
}

static void recSh4_Step()
{
	sh4Interp.Step();
}

static void recSh4_Reset(bool hard)
{
	sh4Interp.Reset(hard);
	recSh4_ClearCache();
	if (hard)
		bm_Reset();
}

static void recSh4_Init()
{
	INFO_LOG(DYNAREC, "recSh4 Init");
#ifdef ENABLE_SH4_CACHED_IR
	sh4::ir::Get_Sh4Interpreter(&sh4Interp);
#else
	Get_Sh4Interpreter(&sh4Interp);
#endif
	sh4Interp.Init();
	bm_Init();

	if (addrspace::virtmemEnabled())
		verify(&mem_b[0] == ((u8*)p_sh4rcb->sq_buffer + 512 + 0x0C000000));

	// Call the platform-specific magic to make the pages RWX
	CodeCache = nullptr;
#ifdef FEAT_NO_RWX_PAGES
	bool rc = virtmem::prepare_jit_block(SH4_TCB, FULL_SIZE, (void**)&CodeCache, &cc_rx_offset);
#else
	bool rc = virtmem::prepare_jit_block(SH4_TCB, FULL_SIZE, (void**)&CodeCache);
#endif
	verify(rc);
	// Ensure the pointer returned is non-null
	verify(CodeCache != nullptr);

	TempCodeCache = CodeCache + CODE_SIZE;
	sh4Dynarec->init(codeBuffer);
	bm_ResetCache();
}

static void recSh4_Term()
{
	INFO_LOG(DYNAREC, "recSh4 Term");
#ifdef FEAT_NO_RWX_PAGES
	if (CodeCache != nullptr)
		virtmem::release_jit_block(CodeCache, (u8 *)CodeCache + cc_rx_offset, FULL_SIZE);
#else
	if (CodeCache != nullptr && CodeCache != SH4_TCB)
		virtmem::release_jit_block(CodeCache, FULL_SIZE);
#endif
	CodeCache = nullptr;
	TempCodeCache = nullptr;
	bm_Term();
	sh4Interp.Term();
}

static bool recSh4_IsCpuRunning()
{
	return sh4Interp.IsCpuRunning();
}

void Get_Sh4Recompiler(sh4_if* cpu)
{
	cpu->Run = recSh4_Run;
	cpu->Start = recSh4_Start;
	cpu->Stop = recSh4_Stop;
	cpu->Step = recSh4_Step;
	cpu->Reset = recSh4_Reset;
	cpu->Init = recSh4_Init;
	cpu->Term = recSh4_Term;
	cpu->IsCpuRunning = recSh4_IsCpuRunning;
	cpu->ResetCache = recSh4_ClearCache;
}

static bool translateAddress(u32 addr, int size, u32 access, u32& outAddr, RuntimeBlockInfo* block)
{
	if (mmu_enabled() && mmu_is_translated(addr, size))
	{
		if (addr & (size - 1))
			// Unaligned
			return false;
		if (block != nullptr
				&& (addr >> 12) != (block->vaddr >> 12)
				&& (addr >> 12) != ((block->vaddr + block->sh4_code_size - 1) >> 12))
			// When full mmu is on, only consider addresses in the same 4k page
			return false;

		u32 paddr;
		MmuError rv = access == MMU_TT_DREAD ?
				mmu_data_translation<MMU_TT_DREAD>(addr, paddr)
				: mmu_data_translation<MMU_TT_DWRITE>(addr, paddr);
		if (rv != MmuError::NONE)
			return false;

		addr = paddr;
	}
	outAddr = addr;

	return true;
}

bool rdv_readMemImmediate(u32 addr, int size, void*& ptr, bool& isRam, u32& physAddr, RuntimeBlockInfo* block)
{
	size = std::min(size, 4);
	if (!translateAddress(addr, size, MMU_TT_DREAD, physAddr, block))
		return false;
	ptr = addrspace::readConst(physAddr, isRam, size);

	return true;
}

bool rdv_writeMemImmediate(u32 addr, int size, void*& ptr, bool& isRam, u32& physAddr, RuntimeBlockInfo* block)
{
	size = std::min(size, 4);
	if (!translateAddress(addr, size, MMU_TT_DWRITE, physAddr, block))
		return false;
	ptr = addrspace::writeConst(physAddr, isRam, size);

	return true;
}

void rdv_SetFailedToFindBlockHandler(void (*handler)())
{
	ngen_FailedToFindBlock = handler;
}
#endif  // FEAT_SHREC == DYNAREC_JITLESS
