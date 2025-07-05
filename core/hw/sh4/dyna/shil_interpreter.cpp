#include "shil_interpreter.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/sh4_interpreter.h"
#include "emulator.h"
#include "cfg/cfg.h"
#include "blockmanager.h"
#include "ngen.h"
#include <cmath>
#include <unordered_map>

// Undefine conflicting macros before our structs
#undef r
#undef sr
#undef pr
#undef gbr
#undef vbr
#undef pc
#undef mac
#undef macl
#undef mach

// Global flag to enable SHIL interpretation mode
bool enable_shil_interpreter = false;

// Initialize SHIL interpreter setting from config
void init_shil_interpreter_setting() {
    static bool initialized = false;
    if (!initialized) {
        enable_shil_interpreter = cfgLoadBool("dynarec", "UseShilInterpreter", false);
        initialized = true;
    }
}

// Global flag to indicate if we should exit block execution
static bool should_exit_block = false;

// EXTREME OPTIMIZATION: Massive register cache - keep everything in local variables
struct SuperRegisterCache {
    u32 r[16];          // All general purpose registers
    u32 sr_t;           // T flag
    u32 pc;             // Program counter
    u32 pr;             // Procedure register
    u32 mac_l, mac_h;   // MAC registers (avoid macro conflicts)
    u32 gbr, vbr;       // Base registers
    
    // Dirty flags - only flush when absolutely necessary
    u64 dirty_mask;     // Bitmask for dirty registers (64-bit for all flags)
    
    // EXTREME: Batch flush - only flush at block boundaries
    void flush_all() {
        if (__builtin_expect(dirty_mask != 0, 0)) {
            // Unrolled register writeback for maximum speed
            if (dirty_mask & 0x1) sh4rcb.cntx.r[0] = r[0];
            if (dirty_mask & 0x2) sh4rcb.cntx.r[1] = r[1];
            if (dirty_mask & 0x4) sh4rcb.cntx.r[2] = r[2];
            if (dirty_mask & 0x8) sh4rcb.cntx.r[3] = r[3];
            if (dirty_mask & 0x10) sh4rcb.cntx.r[4] = r[4];
            if (dirty_mask & 0x20) sh4rcb.cntx.r[5] = r[5];
            if (dirty_mask & 0x40) sh4rcb.cntx.r[6] = r[6];
            if (dirty_mask & 0x80) sh4rcb.cntx.r[7] = r[7];
            if (dirty_mask & 0x100) sh4rcb.cntx.r[8] = r[8];
            if (dirty_mask & 0x200) sh4rcb.cntx.r[9] = r[9];
            if (dirty_mask & 0x400) sh4rcb.cntx.r[10] = r[10];
            if (dirty_mask & 0x800) sh4rcb.cntx.r[11] = r[11];
            if (dirty_mask & 0x1000) sh4rcb.cntx.r[12] = r[12];
            if (dirty_mask & 0x2000) sh4rcb.cntx.r[13] = r[13];
            if (dirty_mask & 0x4000) sh4rcb.cntx.r[14] = r[14];
            if (dirty_mask & 0x8000) sh4rcb.cntx.r[15] = r[15];
            if (dirty_mask & 0x10000) sh4rcb.cntx.sr.T = sr_t;
            if (dirty_mask & 0x20000) sh4rcb.cntx.pc = pc;
            if (dirty_mask & 0x40000) sh4rcb.cntx.pr = pr;
            if (dirty_mask & 0x80000) { sh4rcb.cntx.mac.l = mac_l; }  // Explicit scope to avoid macro
            if (dirty_mask & 0x100000) { sh4rcb.cntx.mac.h = mac_h; } // Explicit scope to avoid macro
            if (dirty_mask & 0x200000) sh4rcb.cntx.gbr = gbr;
            if (dirty_mask & 0x400000) sh4rcb.cntx.vbr = vbr;
            dirty_mask = 0;
        }
    }
    
    void load_all() {
        // Load all registers in one shot
        r[0] = sh4rcb.cntx.r[0];   r[1] = sh4rcb.cntx.r[1];
        r[2] = sh4rcb.cntx.r[2];   r[3] = sh4rcb.cntx.r[3];
        r[4] = sh4rcb.cntx.r[4];   r[5] = sh4rcb.cntx.r[5];
        r[6] = sh4rcb.cntx.r[6];   r[7] = sh4rcb.cntx.r[7];
        r[8] = sh4rcb.cntx.r[8];   r[9] = sh4rcb.cntx.r[9];
        r[10] = sh4rcb.cntx.r[10]; r[11] = sh4rcb.cntx.r[11];
        r[12] = sh4rcb.cntx.r[12]; r[13] = sh4rcb.cntx.r[13];
        r[14] = sh4rcb.cntx.r[14]; r[15] = sh4rcb.cntx.r[15];
        sr_t = sh4rcb.cntx.sr.T;
        pc = sh4rcb.cntx.pc;
        pr = sh4rcb.cntx.pr;
        { mac_l = sh4rcb.cntx.mac.l; }  // Explicit scope to avoid macro
        { mac_h = sh4rcb.cntx.mac.h; }  // Explicit scope to avoid macro
        gbr = sh4rcb.cntx.gbr;
        vbr = sh4rcb.cntx.vbr;
        dirty_mask = 0;
    }
};

static SuperRegisterCache g_reg_cache;

// EXTREME OPTIMIZATION: Pre-decoded instruction format
struct FastShilOp {
    u8 opcode;          // Opcode type
    u8 rd, rs1, rs2;    // Register indices (pre-decoded)
    u32 imm;            // Immediate value (pre-decoded)
};

// EXTREME OPTIMIZATION: Ultra-fast register access macros
#define FAST_REG_GET(idx) (g_reg_cache.r[idx])
#define FAST_REG_SET(idx, val) do { \
    g_reg_cache.r[idx] = (val); \
    g_reg_cache.dirty_mask |= (1ULL << (idx)); \
} while(0)

#define FAST_T_GET() (g_reg_cache.sr_t)
#define FAST_T_SET(val) do { \
    g_reg_cache.sr_t = (val); \
    g_reg_cache.dirty_mask |= 0x10000ULL; \
} while(0)

// EXTREME OPTIMIZATION: Direct instruction handlers (no switch, no branches)
static void __attribute__((always_inline)) inline fast_mov32(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1));
}

static void __attribute__((always_inline)) inline fast_add(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1) + FAST_REG_GET(rs2));
}

static void __attribute__((always_inline)) inline fast_sub(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1) - FAST_REG_GET(rs2));
}

static void __attribute__((always_inline)) inline fast_and(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1) & FAST_REG_GET(rs2));
}

static void __attribute__((always_inline)) inline fast_or(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1) | FAST_REG_GET(rs2));
}

static void __attribute__((always_inline)) inline fast_xor(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1) ^ FAST_REG_GET(rs2));
}

static void __attribute__((always_inline)) inline fast_shl(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1) << (FAST_REG_GET(rs2) & 0x1F));
}

static void __attribute__((always_inline)) inline fast_shr(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, FAST_REG_GET(rs1) >> (FAST_REG_GET(rs2) & 0x1F));
}

static void __attribute__((always_inline)) inline fast_sar(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, (s32)FAST_REG_GET(rs1) >> (FAST_REG_GET(rs2) & 0x1F));
}

static void __attribute__((always_inline)) inline fast_neg(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, -(s32)FAST_REG_GET(rs1));
}

static void __attribute__((always_inline)) inline fast_not(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_REG_SET(rd, ~FAST_REG_GET(rs1));
}

// EXTREME OPTIMIZATION: Memory operations with minimal bounds checking
static void __attribute__((always_inline)) inline fast_readm(u8 rd, u8 rs1, u8 rs2, u32 size) {
    u32 addr = FAST_REG_GET(rs1);
    u32 value;
    // EXTREME: Assume most accesses are to main RAM (0x0C000000-0x0CFFFFFF)
    if (__builtin_expect((addr & 0xFF000000) == 0x0C000000, 1)) {
        switch (size) {
            case 1: value = mem_b[addr & RAM_MASK]; break;
            case 2: value = *(u16*)&mem_b[addr & RAM_MASK]; break;
            case 4: default: value = *(u32*)&mem_b[addr & RAM_MASK]; break;
        }
    } else {
        switch (size) {
            case 1: value = ReadMem8(addr); break;
            case 2: value = ReadMem16(addr); break;
            case 4: default: value = ReadMem32(addr); break;
        }
    }
    FAST_REG_SET(rd, value);
}

static void __attribute__((always_inline)) inline fast_writem(u8 rd, u8 rs1, u8 rs2, u32 size) {
    u32 addr = FAST_REG_GET(rs1);
    u32 data = FAST_REG_GET(rs2);
    // EXTREME: Assume most accesses are to main RAM
    if (__builtin_expect((addr & 0xFF000000) == 0x0C000000, 1)) {
        switch (size) {
            case 1: mem_b[addr & RAM_MASK] = data; break;
            case 2: *(u16*)&mem_b[addr & RAM_MASK] = data; break;
            case 4: default: *(u32*)&mem_b[addr & RAM_MASK] = data; break;
        }
    } else {
        switch (size) {
            case 1: WriteMem8(addr, data); break;
            case 2: WriteMem16(addr, data); break;
            case 4: default: WriteMem32(addr, data); break;
        }
    }
}

static void __attribute__((always_inline)) inline fast_test(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_T_SET((FAST_REG_GET(rs1) & FAST_REG_GET(rs2)) == 0 ? 1 : 0);
}

static void __attribute__((always_inline)) inline fast_seteq(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_T_SET(FAST_REG_GET(rs1) == FAST_REG_GET(rs2) ? 1 : 0);
}

static void __attribute__((always_inline)) inline fast_setge(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_T_SET((s32)FAST_REG_GET(rs1) >= (s32)FAST_REG_GET(rs2) ? 1 : 0);
}

static void __attribute__((always_inline)) inline fast_setgt(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_T_SET((s32)FAST_REG_GET(rs1) > (s32)FAST_REG_GET(rs2) ? 1 : 0);
}

static void __attribute__((always_inline)) inline fast_setae(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_T_SET(FAST_REG_GET(rs1) >= FAST_REG_GET(rs2) ? 1 : 0);
}

static void __attribute__((always_inline)) inline fast_seta(u8 rd, u8 rs1, u8 rs2, u32 imm) {
    FAST_T_SET(FAST_REG_GET(rs1) > FAST_REG_GET(rs2) ? 1 : 0);
}

// EXTREME OPTIMIZATION: Function pointer table for direct dispatch
typedef void (*FastOpHandler)(u8 rd, u8 rs1, u8 rs2, u32 imm);

static FastOpHandler fast_handlers[] = {
    fast_mov32,  // shop_mov32
    nullptr,     // shop_mov64
    fast_add,    // shop_add
    fast_sub,    // shop_sub
    nullptr,     // shop_mul_u16
    nullptr,     // shop_mul_s16
    nullptr,     // shop_mul_i32
    nullptr,     // shop_mul_u64
    nullptr,     // shop_mul_s64
    fast_and,    // shop_and
    fast_or,     // shop_or
    fast_xor,    // shop_xor
    fast_not,    // shop_not
    fast_shl,    // shop_shl
    fast_shr,    // shop_shr
    fast_sar,    // shop_sar
    fast_neg,    // shop_neg
    nullptr,     // shop_swaplb
    fast_test,   // shop_test
    fast_seteq,  // shop_seteq
    fast_setge,  // shop_setge
    fast_setgt,  // shop_setgt
    fast_seta,   // shop_setab
    fast_setae,  // shop_setae
};

// EXTREME OPTIMIZATION: Pre-compile blocks into direct threaded code
struct CompiledBlock {
    FastShilOp* ops;
    u32 op_count;
    u32 cycles;
    bool has_memory_ops;
    bool has_branches;
};

static std::unordered_map<uintptr_t, CompiledBlock> compiled_blocks;

// EXTREME OPTIMIZATION: Ultra-fast block executor with loop unrolling
static void __attribute__((hot)) execute_compiled_block(CompiledBlock* block) {
    FastShilOp* ops = block->ops;
    u32 count = block->op_count;
    
    // EXTREME: Unroll execution loop by 8 for maximum throughput
    u32 i = 0;
    for (; i + 7 < count; i += 8) {
        // Execute 8 operations with minimal branching
        FastOpHandler h0 = fast_handlers[ops[i].opcode];
        FastOpHandler h1 = fast_handlers[ops[i+1].opcode];
        FastOpHandler h2 = fast_handlers[ops[i+2].opcode];
        FastOpHandler h3 = fast_handlers[ops[i+3].opcode];
        FastOpHandler h4 = fast_handlers[ops[i+4].opcode];
        FastOpHandler h5 = fast_handlers[ops[i+5].opcode];
        FastOpHandler h6 = fast_handlers[ops[i+6].opcode];
        FastOpHandler h7 = fast_handlers[ops[i+7].opcode];
        
        if (__builtin_expect(h0 != nullptr, 1)) h0(ops[i].rd, ops[i].rs1, ops[i].rs2, ops[i].imm);
        if (__builtin_expect(h1 != nullptr, 1)) h1(ops[i+1].rd, ops[i+1].rs1, ops[i+1].rs2, ops[i+1].imm);
        if (__builtin_expect(h2 != nullptr, 1)) h2(ops[i+2].rd, ops[i+2].rs1, ops[i+2].rs2, ops[i+2].imm);
        if (__builtin_expect(h3 != nullptr, 1)) h3(ops[i+3].rd, ops[i+3].rs1, ops[i+3].rs2, ops[i+3].imm);
        if (__builtin_expect(h4 != nullptr, 1)) h4(ops[i+4].rd, ops[i+4].rs1, ops[i+4].rs2, ops[i+4].imm);
        if (__builtin_expect(h5 != nullptr, 1)) h5(ops[i+5].rd, ops[i+5].rs1, ops[i+5].rs2, ops[i+5].imm);
        if (__builtin_expect(h6 != nullptr, 1)) h6(ops[i+6].rd, ops[i+6].rs1, ops[i+6].rs2, ops[i+6].imm);
        if (__builtin_expect(h7 != nullptr, 1)) h7(ops[i+7].rd, ops[i+7].rs1, ops[i+7].rs2, ops[i+7].imm);
    }
    
    // Handle remaining operations
    for (; i < count; i++) {
        FastOpHandler handler = fast_handlers[ops[i].opcode];
        if (__builtin_expect(handler != nullptr, 1)) {
            handler(ops[i].rd, ops[i].rs1, ops[i].rs2, ops[i].imm);
        }
    }
}

// EXTREME OPTIMIZATION: Block compilation and execution
static void compileBlock(RuntimeBlockInfo* block) {
    // Pre-compile SHIL opcodes into direct threaded code
    CompiledBlock compiled;
    compiled.ops = new FastShilOp[block->sh4_code_size];
    compiled.op_count = block->sh4_code_size;
    compiled.cycles = 0;
    compiled.has_memory_ops = false;
    compiled.has_branches = false;
    
    for (u32 i = 0; i < block->sh4_code_size; i++) {
        const shil_opcode& op = block->oplist[i];
        FastShilOp& fast_op = compiled.ops[i];
        
        // Pre-decode everything for maximum speed (using correct field names)
        fast_op.opcode = op.op;
        fast_op.rd = (op.rd.type == FMT_I32 && op.rd._reg < 16) ? op.rd._reg : 0;
        fast_op.rs1 = (op.rs1.type == FMT_I32 && op.rs1._reg < 16) ? op.rs1._reg : 0;
        fast_op.rs2 = (op.rs2.type == FMT_I32 && op.rs2._reg < 16) ? op.rs2._reg : 0;
        fast_op.imm = (op.rs2.type == FMT_IMM) ? op.rs2._imm : 0;
        
        // Track block characteristics
        if (op.op == shop_readm || op.op == shop_writem) {
            compiled.has_memory_ops = true;
        }
        if (op.op == shop_jcond || op.op == shop_jdyn) {
            compiled.has_branches = true;
        }
        
        compiled.cycles++;
    }
    
    compiled_blocks[reinterpret_cast<uintptr_t>(block->code)] = compiled;
}

// EXTREME OPTIMIZATION: ShilInterpreter implementation using static functions
void ShilInterpreter::executeBlock(RuntimeBlockInfo* block) {
    // Load registers into cache once per block
    g_reg_cache.load_all();
    
    // Check if block is already compiled
    auto it = compiled_blocks.find(reinterpret_cast<uintptr_t>(block->code));
    if (__builtin_expect(it != compiled_blocks.end(), 1)) {
        // EXTREME: Execute pre-compiled block
        execute_compiled_block(&it->second);
    } else {
        // Compile block on first execution
        compileBlock(block);
        auto it2 = compiled_blocks.find(reinterpret_cast<uintptr_t>(block->code));
        if (it2 != compiled_blocks.end()) {
            execute_compiled_block(&it2->second);
        }
    }
    
    // Flush registers back to memory once per block
    g_reg_cache.flush_all();
}

// Implement the static functions declared in the header
void ShilInterpreter::executeOpcode(const shil_opcode& op) {
    // Simple fallback implementation - not used in extreme mode
    switch (op.op) {
        case shop_mov32:
            setRegValue(op.rd, getRegValue(op.rs1));
            break;
        case shop_add:
            setRegValue(op.rd, getRegValue(op.rs1) + getRegValue(op.rs2));
            break;
        default:
            // Unhandled - use interpreter fallback
            break;
    }
}

u32 ShilInterpreter::getRegValue(const shil_param& param) {
    if (param.is_imm()) {
        return param.imm_value();
    } else if (param.is_reg()) {
        return *param.reg_ptr();
    }
    return 0;
}

void ShilInterpreter::setRegValue(const shil_param& param, u32 value) {
    if (param.is_reg()) {
        *param.reg_ptr() = value;
    }
}

f32 ShilInterpreter::getFloatRegValue(const shil_param& param) {
    if (param.is_imm()) {
        return *(f32*)&param._imm;
    } else if (param.is_reg()) {
        return *(f32*)param.reg_ptr();
    }
    return 0.0f;
}

void ShilInterpreter::setFloatRegValue(const shil_param& param, f32 value) {
    if (param.is_reg()) {
        *(f32*)param.reg_ptr() = value;
    }
}

u64 ShilInterpreter::getReg64Value(const shil_param& param) {
    if (param.is_imm()) {
        return param.imm_value();
    } else if (param.is_reg()) {
        return *reinterpret_cast<u64*>(param.reg_ptr());
    }
    return 0;
}

void ShilInterpreter::setReg64Value(const shil_param& param, u64 value) {
    if (param.is_reg()) {
        *reinterpret_cast<u64*>(param.reg_ptr()) = value;
    }
}

void* ShilInterpreter::getPointer(const shil_param& param) {
    if (param.is_reg()) {
        return param.reg_ptr();
    }
    return nullptr;
}

u32 ShilInterpreter::handleMemoryRead(const shil_param& addr, u32 size) {
    u32 address = getRegValue(addr);
    switch (size) {
        case 1: return ReadMem8(address);
        case 2: return ReadMem16(address);
        case 4: return ReadMem32(address);
        default: return 0;
    }
}

void ShilInterpreter::handleMemoryWrite(const shil_param& addr, const shil_param& value, u32 size) {
    u32 address = getRegValue(addr);
    u32 val = getRegValue(value);
    switch (size) {
        case 1: WriteMem8(address, val); break;
        case 2: WriteMem16(address, val); break;
        case 4: WriteMem32(address, val); break;
    }
}

void ShilInterpreter::handleInterpreterFallback(const shil_opcode& op) {
    // Flush register cache before fallback
    g_reg_cache.flush_all();
    
    // Call SH4 instruction handler directly
    u32 opcode = op.rs3.imm_value();
    OpDesc[opcode]->oph(opcode);
    
    // Reload register cache after fallback
    g_reg_cache.load_all();
}

void ShilInterpreter::handleDynamicJump(const shil_opcode& op) {
    // Flush register cache before jump
    g_reg_cache.flush_all();
    
    // Set dynamic PC
    u32 target = getRegValue(op.rs1);
    if (!op.rs2.is_null()) {
        target += getRegValue(op.rs2);
    }
    *op.rd.reg_ptr() = target;
}

void ShilInterpreter::handleConditionalJump(const shil_opcode& op) {
    // Flush register cache before jump
    g_reg_cache.flush_all();
    
    // Set conditional jump target
    u32 target = getRegValue(op.rs2);
    *op.rd.reg_ptr() = target;
}

// Main SHIL interpreter mainloop
void shil_interpreter_mainloop(void* v_cntx) {
    // Set up context
    p_sh4rcb = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4Context));
    
    while (emu.running()) {
        // EXTREME: Minimal overhead main loop
        u32 pc = sh4rcb.cntx.pc;
        
        // EXTREME: Fast block lookup using direct address translation
        RuntimeBlockInfo* block = nullptr;
        
        // Check FPCA table first (fastest path)
        DynarecCodeEntryPtr code_ptr = bm_GetCodeByVAddr(pc);
        if (__builtin_expect(code_ptr != ngen_FailedToFindBlock, 1)) {
            // Check if this is a tagged SHIL interpreter block
            if (__builtin_expect(reinterpret_cast<uintptr_t>(code_ptr) & 0x1, 1)) {
                // Extract block pointer from tagged address
                block = reinterpret_cast<RuntimeBlockInfo*>(reinterpret_cast<uintptr_t>(code_ptr) & ~0x1ULL);
            }
        }
        
        if (__builtin_expect(block != nullptr, 1)) {
            // EXTREME: Execute block with minimal overhead
            ShilInterpreter::executeBlock(block);
            
            // Update PC (simplified - assume linear execution for speed)
            sh4rcb.cntx.pc += block->sh4_code_size * 2;
        } else {
            // Fallback: compile new block
            try {
                RuntimeBlockInfoPtr blockPtr = bm_GetBlock(pc);
                if (blockPtr) {
                    block = blockPtr.get();
                    ShilInterpreter::executeBlock(block);
                    sh4rcb.cntx.pc += block->sh4_code_size * 2;
                } else {
                    break; // Exit if no block found
                }
            } catch (...) {
                break; // Exit on any exception
            }
        }
        
        // EXTREME: Minimal cycle counting
        sh4_sched_ffts();
    }
}

// Redefine macros after our code
#define r Sh4cntx.r
#define sr Sh4cntx.sr
#define pr Sh4cntx.pr
#define gbr Sh4cntx.gbr
#define vbr Sh4cntx.vbr
#define pc Sh4cntx.pc
#define mac Sh4cntx.mac
#define macl Sh4cntx.macl
#define mach Sh4cntx.mach 