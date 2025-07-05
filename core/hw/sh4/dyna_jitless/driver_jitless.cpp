#include "types.h"
#include <unordered_set>
#include <cmath>

#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/ir/sh4_ir_interpreter.h"
#include "hw/sh4/sh4_core.h"
extern void Sh4_int_Step();
#include "hw/sh4/sh4_interrupts.h"

#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/sh4_cycles.h"

#ifdef STRICT_MODE
constexpr int CPU_RATIO = 1;
#else
constexpr int CPU_RATIO = 8;
#endif

#include "blockmanager_jitless.h"
#include "ngen_jitless.h"
#include "decoder_jitless.h"
#include "oslib/virtmem.h"

#include "cfg/option.h"

// Global exception flag from sh4_interrupts.cpp
extern bool g_exception_was_raised;

#if FEAT_SHREC == DYNAREC_JITLESS

// Enlarged code cache for jitless path to avoid frequent clears
constexpr u32 CODE_SIZE = 32_MB;
constexpr u32 TEMP_CODE_SIZE = 2_MB;
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
    currentBlock = block;
    
    DEBUG_LOG(DYNAREC, "🔍 Executing SHIL block at 0x%08X with %zu ops", block->addr, block->oplist.size());
    
         for (size_t i = 0; i < block->oplist.size(); i++) {
         const shil_opcode& op = block->oplist[i];
        
        // Log potentially dangerous operations
        if (op.op == shop_jdyn || op.op == shop_jcond || op.op == shop_ifb) {
            DEBUG_LOG(DYNAREC, "🔍 SHIL[%zu]: Executing %s (op=%d)", i, 
                     (op.op == shop_jdyn) ? "shop_jdyn" : 
                     (op.op == shop_jcond) ? "shop_jcond" : "shop_ifb", 
                     op.op);
            
            // Log PC state before risky ops
            u32 pre_next_pc = next_pc;
            DEBUG_LOG(DYNAREC, "🔍 PRE-OP: next_pc=0x%08X r[15]=0x%08X", next_pc, r[15]);
            
            // Execute the operation
            switch (op.op) {
                case shop_jdyn:
                    // Dynamic jump
                    next_pc = getParamU32(op.rs1);
                    DEBUG_LOG(DYNAREC, "🔍 shop_jdyn: jumping to 0x%08X", next_pc);
                    return; // Exit block
                    
                case shop_jcond:
                    // Conditional jump
                    if (sr.T == 1) {
                        next_pc = op.rs1._imm;
                        DEBUG_LOG(DYNAREC, "🔍 shop_jcond: condition true, jumping to 0x%08X", next_pc);
                        return; // Exit block
                    } else {
                        DEBUG_LOG(DYNAREC, "🔍 shop_jcond: condition false, continuing");
                    }
                    break;
                    
                case shop_ifb:
                    // Interpreter fallback
                    {
                        DEBUG_LOG(DYNAREC, "🔍 shop_ifb: Fallback to interpreter, PC check needed");
                        if (op.rs1._imm) {
                            next_pc = op.rs2._imm;
                            DEBUG_LOG(DYNAREC, "🔍 shop_ifb: Set PC to 0x%08X", next_pc);
                        }
                        
                        u16 opcode = op.rs3._imm;  // Use the already decoded opcode from rs3
                        DEBUG_LOG(DYNAREC, "🔍 shop_ifb: Executing SH4 opcode 0x%04X at PC=0x%08X", opcode, next_pc);
                        
                        u32 pre_pc = next_pc;
                        OpDesc[opcode]->oph(opcode);
                        u32 post_pc = next_pc;
                        
                        DEBUG_LOG(DYNAREC, "🔍 shop_ifb: PC changed from 0x%08X to 0x%08X", pre_pc, post_pc);
                        
                        return; // Exit block after interpreter fallback
                    }
                    
                default:
                    break;
            }
            
            // Check for corruption after risky ops
            if (next_pc != pre_next_pc) {
                DEBUG_LOG(DYNAREC, "🔍 POST-OP: next_pc changed from 0x%08X to 0x%08X", pre_next_pc, next_pc);
            }
            
            if (next_pc >= 0x9F000000 && next_pc <= 0x9FFFFFFF) {
                ERROR_LOG(DYNAREC, "🚨 PC CORRUPTION in SHIL[%zu]: next_pc=0x%08X after op=%d", i, next_pc, op.op);
                die("PC corruption detected in SHIL execution");
            }
        } else {
            // Execute normal operations without extensive logging
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
                
                // Illegal instruction - throw SH4ThrownException exactly like the IR interpreter does
                case shop_illegal: {
                    u32 epc = getParamU32(op.rs1);         // Exception PC
                    u32 delaySlot = getParamU32(op.rs2);   // Is it in delay slot?
                    
                    DEBUG_LOG(DYNAREC, "🔍 shop_illegal: Exception at PC=0x%08X, delaySlot=%d", epc, delaySlot);
                    
                    // Throw SH4ThrownException exactly like the IR interpreter does
                    if (delaySlot == 1) {
                        throw SH4ThrownException(epc - 2, Sh4Ex_SlotIllegalInstr);
                    } else {
                        throw SH4ThrownException(epc, Sh4Ex_IllegalInstr);
                    }
                }
                
                // TODO: Add more opcodes as needed
                default:
                    // For unhandled opcodes, log a warning but continue
                    WARN_LOG(DYNAREC, "Unhandled SHIL opcode: %s (%d) at PC %08X", 
                            shil_opcode_name(op.op), op.op, next_pc);
                    break;
            }
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
                // Exception flag cleared at start of main loop like regular dynarec
                static u32 last_exception_pc = 0;
                if (unlikely(g_exception_was_raised)) {
                    // If we just raised an exception at the same PC again, step the interpreter once to avoid endless loop
                    if (next_pc == last_exception_pc) {
                        WARN_LOG(DYNAREC, "⚠️  Re-raised exception at 0x%08X; executing one interpreter step to advance", next_pc);
                        bool cpu_was_running = sh4_int_bCpuRun;
                        if (cpu_was_running)
                            sh4_int_bCpuRun = false;
                        try {
                            Sh4_int_Step();
                        } catch (const SH4ThrownException&) {
                            // Let outer handler process nested exception
                        }
                        if (cpu_was_running)
                            sh4_int_bCpuRun = true;
                    }
                    last_exception_pc = next_pc;
                    g_exception_was_raised = false;
                }
                
                // Find or compile block
                u32 addr = next_pc;
                DynarecCodeEntryPtr code_ptr = bm_GetCodeByVAddr(addr);
                
                if (addr == 0xFFFFFFFF) {
                    // BL=0 – inject exception
                    if (sr.BL == 0) {
                        INFO_LOG(DYNAREC, "🔧 PC==0xFFFFFFFF (BL=0) – injecting AddressErrorRead");
                        Do_Exception(0xFFFFFFFF, Sh4Ex_AddressErrorRead);
                        continue;
                    } else {
                        INFO_LOG(DYNAREC, "🔧 PC==0xFFFFFFFF (BL=1) – double fault condition");
                        // Double fault - advance PC and continue
                        next_pc = 0x8C000000;
                        continue;
                    }
                }
                
                if (code_ptr == ngen_FailedToFindBlock) {
                    // Block not found, create one
                    DEBUG_LOG(DYNAREC, "🔧 Block not found, creating jitless block...");
                    code_ptr = createJitlessBlock(addr);
                    DEBUG_LOG(DYNAREC, "🔧 createJitlessBlock returned: 0x%llX", (unsigned long long)code_ptr);
                    if (!code_ptr) {
                        ERROR_LOG(DYNAREC, "🔧 Failed to create jitless block at PC=0x%08X", addr);
                        continue;
                    }
                }
                
                DEBUG_LOG(DYNAREC, "🔧 About to execute block: next_pc=0x%08X code_ptr=0x%llX", next_pc, (unsigned long long)code_ptr);
                
                // Check if this is a jitless block (low bit set) or regular block  
                if (reinterpret_cast<uintptr_t>(code_ptr) & 0x1) {
                    // Jitless block - extract RuntimeBlockInfo and execute via SHIL interpretation
                    RuntimeBlockInfo* block = reinterpret_cast<RuntimeBlockInfo*>(reinterpret_cast<uintptr_t>(code_ptr) & ~0x1);
                    INFO_LOG(DYNAREC, "🔧 Jitless block detected: block=0x%llX addr=0x%08X oplist.size=%zu", 
                            (unsigned long long)block, addr, block->oplist.size());
                    
                    INFO_LOG(DYNAREC, "🔧 PRE-EXECUTE: PC=0x%08X next_pc=0x%08X r[15]=0x%08X", 
                            Sh4cntx.pc, next_pc, r[15]);
                    
                    executeShilBlock(block);
                    
                    INFO_LOG(DYNAREC, "🔧 POST-EXECUTE: PC=0x%08X next_pc=0x%08X r[15]=0x%08X", 
                            Sh4cntx.pc, next_pc, r[15]);
                } else {
                    // Regular JIT block - shouldn't happen in jitless mode  
                    ERROR_LOG(DYNAREC, "🔧 Regular JIT block detected in jitless mode: 0x%llX", (unsigned long long)code_ptr);
                    break;
                }
                
            } catch (const SH4ThrownException& ex) {
                // Handle SH4 exceptions exactly like the regular interpreter
                DEBUG_LOG(DYNAREC, "🔧 SH4ThrownException caught: epc=0x%08X expEvn=0x%X", ex.epc, ex.expEvn);
                
                // Call Do_Exception like the regular dynarec - let FlycastException propagate and crash
                Do_Exception(ex.epc, ex.expEvn);
                // An SH4 exception drains the pipeline (~5 cycles) – match interpreter timing
                sh4cycles.addCycles(5 * CPU_RATIO);
            }
        } while (p_sh4rcb->cntx.CpuRunning);
        
        INFO_LOG(DYNAREC, "🔧 Main loop exited: CpuRunning=%s", p_sh4rcb->cntx.CpuRunning ? "true" : "false");
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
        // Step 1: Allocate a RuntimeBlockInfo
        RuntimeBlockInfo* rbi = this->allocateBlock();
        if (!rbi) {
            return ngen_FailedToFindBlock;
        }
        
        // Step 2: Setup the block (SH4 → SHIL decoding and optimization)
        try {
            if (!rbi->Setup(pc, fpscr)) {
                delete rbi;
                return ngen_FailedToFindBlock;
            }
        } catch (const FlycastException& e) {
            // Architectural violation detected during block setup (e.g., branch in delay slot)
            WARN_LOG(DYNAREC, "🔧 Block setup failed due to architectural violation at PC=0x%08X: %s", pc, e.what());
            delete rbi;
            // Re-throw the exception to be handled by the caller
            throw;
        }
        
        // Step 3: Create a unique "code" pointer for this block
        // We'll use the block's memory address as a unique identifier
        rbi->code = reinterpret_cast<DynarecCodeEntryPtr>(reinterpret_cast<uintptr_t>(rbi) | 0x1);
        
        // Step 4: Add to block manager
        bm_AddBlock(rbi);
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

// Forward declaration so it can be called from recSh4_Run
static void recSh4_Reset(bool hard);

static void recSh4_Run()
{
    RestoreHostRoundingMode();

    u8 *sh4_dyna_rcb = (u8 *)&Sh4cntx + sizeof(Sh4cntx);
    INFO_LOG(DYNAREC, "cntx // fpcb offset: %td // pc offset: %td // pc %08X", (u8*)&sh4rcb.fpcb - sh4_dyna_rcb, (u8*)&sh4rcb.cntx.pc - sh4_dyna_rcb, sh4rcb.cntx.pc);

    // Keep the SH4 running across soft/hard resets initiated by the BIOS.
    while (true)
    {
        sh4Dynarec->mainloop(sh4_dyna_rcb);

        // mainloop returned — CPU is no longer running
        sh4_int_bCpuRun = false;

        // If PC is at the reset vector, the BIOS just issued an SH4 reset.
        // Restart the interpreter/dynarec loop instead of shutting down.
        if (((next_pc & ~1u) == 0xA0000000u) || ((next_pc & ~1u) == 0x00000000u))
        {
            INFO_LOG(DYNAREC, "🔄 SH4 reset detected – restarting dynarec mainloop (PC=0x%08X)", next_pc);
            recSh4_Reset(true);
            sh4Interp.Start(); // raises CpuRunning again
            continue; // re-enter mainloop
        }

        break; // Normal shutdown
    }
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
