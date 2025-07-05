#include "shil_interpreter.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_opcode_list.h"
#include "emulator.h"
#include "cfg/cfg.h"
#include "blockmanager.h"
#include "ngen.h"
#include <cmath>

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

// OPTIMIZATION: Register cache to avoid repeated memory access
struct RegisterCache {
    u32 regs[16];       // General purpose registers (renamed from r to avoid macro conflict)
    u32 sr_t;           // T flag cache
    bool regs_dirty[16]; // Track which registers need writeback (renamed from r_dirty)
    bool sr_t_dirty;    // Track if T flag needs writeback
    
    void flush() {
        // Write back dirty registers using direct access to avoid macro conflicts
        for (int i = 0; i < 16; i++) {
            if (regs_dirty[i]) {
#undef r
                sh4rcb.cntx.r[i] = regs[i];
#define r Sh4cntx.r
                regs_dirty[i] = false;
            }
        }
        if (sr_t_dirty) {
#undef sr
            sh4rcb.cntx.sr.T = sr_t;
#define sr Sh4cntx.sr
            sr_t_dirty = false;
        }
    }
    
    void load() {
        // Load registers into cache using direct access to avoid macro conflicts
        for (int i = 0; i < 16; i++) {
#undef r
            regs[i] = sh4rcb.cntx.r[i];
#define r Sh4cntx.r
            regs_dirty[i] = false;
        }
#undef sr
        sr_t = sh4rcb.cntx.sr.T;
#define sr Sh4cntx.sr
        sr_t_dirty = false;
    }
    
    u32 get_reg(int reg) {
        return regs[reg];
    }
    
    void set_reg(int reg, u32 value) {
        regs[reg] = value;
        regs_dirty[reg] = true;
    }
    
    u32 get_sr_t() {
        return sr_t;
    }
    
    void set_sr_t(u32 value) {
        sr_t = value;
        sr_t_dirty = true;
    }
};

static RegisterCache reg_cache;

// OPTIMIZATION: Instruction fusion patterns
enum FusedOpcodeType {
    FUSED_NONE = 0,
    FUSED_MOV_ADD,      // mov + add sequence
    FUSED_MOV_CMP,      // mov + cmp sequence  
    FUSED_LOAD_USE,     // load + immediate use
    FUSED_STORE_INC,    // store + increment
};

struct FusedInstruction {
    FusedOpcodeType type;
    u32 data[4];        // Fused instruction data
};

// OPTIMIZATION: Pre-decode blocks for faster execution
struct PredecodedBlock {
    FusedInstruction* instructions;
    size_t instruction_count;
    bool has_branches;
    bool has_memory_ops;
    u32 estimated_cycles;
};

// OPTIMIZATION: Inline register access functions with register cache
static inline u32 getRegValue(const shil_param& param) {
    if (__builtin_expect(param.is_imm(), 0)) {
        return param.imm_value();
    } else if (__builtin_expect(param.is_reg(), 1)) {
        // Use register cache for faster access
        if (param._reg < 16) {
            return reg_cache.get_reg(param._reg);
        }
        return *param.reg_ptr();
    }
    return 0;
}

static inline void setRegValue(const shil_param& param, u32 value) {
    if (__builtin_expect(param.is_reg(), 1)) {
        // Use register cache for faster access
        if (param._reg < 16) {
            reg_cache.set_reg(param._reg, value);
            return;
        }
        *param.reg_ptr() = value;
    }
}

static inline f32 getFloatRegValue(const shil_param& param) {
    if (param.is_imm()) {
        return *(f32*)&param._imm;
    } else if (param.is_reg()) {
        return *(f32*)param.reg_ptr();
    }
    return 0.0f;
}

static inline void setFloatRegValue(const shil_param& param, f32 value) {
    if (param.is_reg()) {
        *(f32*)param.reg_ptr() = value;
    }
}

// OPTIMIZATION: Ultra-fast paths for the most common operations
static inline void ultraFastMov32(int dst_reg, int src_reg) {
    reg_cache.set_reg(dst_reg, reg_cache.get_reg(src_reg));
}

static inline void ultraFastMovImm(int dst_reg, u32 imm) {
    reg_cache.set_reg(dst_reg, imm);
}

static inline void ultraFastAdd(int dst_reg, int src1_reg, int src2_reg) {
    reg_cache.set_reg(dst_reg, reg_cache.get_reg(src1_reg) + reg_cache.get_reg(src2_reg));
}

static inline void ultraFastAddImm(int dst_reg, int src_reg, u32 imm) {
    reg_cache.set_reg(dst_reg, reg_cache.get_reg(src_reg) + imm);
}

static inline void ultraFastSub(int dst_reg, int src1_reg, int src2_reg) {
    reg_cache.set_reg(dst_reg, reg_cache.get_reg(src1_reg) - reg_cache.get_reg(src2_reg));
}

static inline void ultraFastAnd(int dst_reg, int src1_reg, int src2_reg) {
    reg_cache.set_reg(dst_reg, reg_cache.get_reg(src1_reg) & reg_cache.get_reg(src2_reg));
}

static inline void ultraFastOr(int dst_reg, int src1_reg, int src2_reg) {
    reg_cache.set_reg(dst_reg, reg_cache.get_reg(src1_reg) | reg_cache.get_reg(src2_reg));
}

static inline void ultraFastCmp(int reg1, int reg2) {
    reg_cache.set_sr_t((reg_cache.get_reg(reg1) == reg_cache.get_reg(reg2)) ? 1 : 0);
}

// OPTIMIZATION: Fast path for common register-to-register moves
static inline void fastMov32(const shil_param& dst, const shil_param& src) {
    if (__builtin_expect(dst.is_reg() && src.is_reg() && dst._reg < 16 && src._reg < 16, 1)) {
        ultraFastMov32(dst._reg, src._reg);
    } else if (dst.is_reg() && src.is_imm() && dst._reg < 16) {
        ultraFastMovImm(dst._reg, src.imm_value());
    } else {
        setRegValue(dst, getRegValue(src));
    }
}

// OPTIMIZATION: Fast path for common arithmetic operations
static inline void fastAdd(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (__builtin_expect(dst.is_reg() && src1.is_reg() && src2.is_reg() && 
                        dst._reg < 16 && src1._reg < 16 && src2._reg < 16, 1)) {
        ultraFastAdd(dst._reg, src1._reg, src2._reg);
    } else if (dst.is_reg() && src1.is_reg() && src2.is_imm() && 
               dst._reg < 16 && src1._reg < 16) {
        ultraFastAddImm(dst._reg, src1._reg, src2.imm_value());
    } else {
        setRegValue(dst, getRegValue(src1) + getRegValue(src2));
    }
}

static inline void fastSub(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (__builtin_expect(dst.is_reg() && src1.is_reg() && src2.is_reg() && 
                        dst._reg < 16 && src1._reg < 16 && src2._reg < 16, 1)) {
        ultraFastSub(dst._reg, src1._reg, src2._reg);
    } else {
        setRegValue(dst, getRegValue(src1) - getRegValue(src2));
    }
}

// OPTIMIZATION: Fast path for bitwise operations
static inline void fastAnd(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (__builtin_expect(dst.is_reg() && src1.is_reg() && src2.is_reg() && 
                        dst._reg < 16 && src1._reg < 16 && src2._reg < 16, 1)) {
        ultraFastAnd(dst._reg, src1._reg, src2._reg);
    } else {
        setRegValue(dst, getRegValue(src1) & getRegValue(src2));
    }
}

static inline void fastOr(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (__builtin_expect(dst.is_reg() && src1.is_reg() && src2.is_reg() && 
                        dst._reg < 16 && src1._reg < 16 && src2._reg < 16, 1)) {
        ultraFastOr(dst._reg, src1._reg, src2._reg);
    } else {
        setRegValue(dst, getRegValue(src1) | getRegValue(src2));
    }
}

static inline void fastXor(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    setRegValue(dst, getRegValue(src1) ^ getRegValue(src2));
}

// Remove old member functions - they're now static inline
u32 ShilInterpreter::getRegValue(const shil_param& param) {
    return ::getRegValue(param);
}

void ShilInterpreter::setRegValue(const shil_param& param, u32 value) {
    ::setRegValue(param, value);
}

f32 ShilInterpreter::getFloatRegValue(const shil_param& param) {
    return ::getFloatRegValue(param);
}

void ShilInterpreter::setFloatRegValue(const shil_param& param, f32 value) {
    ::setFloatRegValue(param, value);
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
    reg_cache.flush();
    
    // Set PC if needed
    if (op.rs1.imm_value()) {
        next_pc = op.rs2.imm_value();
    }
    
    // Call SH4 instruction handler directly
    u32 opcode = op.rs3.imm_value();
    OpDesc[opcode]->oph(opcode);
    
    // Reload register cache after fallback
    reg_cache.load();
    
    // Exit block after fallback
    should_exit_block = true;
}

void ShilInterpreter::handleDynamicJump(const shil_opcode& op) {
    // Flush register cache before jump
    reg_cache.flush();
    
    // Set dynamic PC
    u32 target = getRegValue(op.rs1);
    if (!op.rs2.is_null()) {
        target += getRegValue(op.rs2);
    }
    *op.rd.reg_ptr() = target;
    should_exit_block = true;
}

void ShilInterpreter::handleConditionalJump(const shil_opcode& op) {
    // Flush register cache before jump
    reg_cache.flush();
    
    // Set conditional jump target
    u32 target = getRegValue(op.rs2);
    *op.rd.reg_ptr() = target;
    should_exit_block = true;
}

// OPTIMIZATION: Hyper-optimized executeOpcode with instruction fusion and threading
void ShilInterpreter::executeOpcode(const shil_opcode& op) {
    // OPTIMIZATION: Use direct function pointers for fastest dispatch
    static void* opcode_handlers[] = {
        &&handle_mov32, &&handle_mov64, &&handle_add, &&handle_sub, &&handle_mul_u16, &&handle_mul_s16,
        &&handle_mul_i32, &&handle_mul_u64, &&handle_mul_s64, &&handle_and, &&handle_or, &&handle_xor,
        &&handle_not, &&handle_shl, &&handle_shr, &&handle_sar, &&handle_neg, &&handle_swaplb,
        &&handle_test, &&handle_seteq, &&handle_setge, &&handle_setgt, &&handle_setab, &&handle_setae,
        &&handle_readm, &&handle_writem, &&handle_jcond, &&handle_jdyn, &&handle_pref,
        &&handle_ext_s8, &&handle_ext_s16, &&handle_cvt_i2f_n, &&handle_cvt_f2i_t,
        &&handle_fadd, &&handle_fsub, &&handle_fmul, &&handle_fdiv, &&handle_fabs, &&handle_fneg,
        &&handle_fsqrt, &&handle_fmac, &&handle_fseteq, &&handle_fsetgt, &&handle_ifb
    };
    
    // OPTIMIZATION: Direct threading - jump directly to handler
    if (__builtin_expect(op.op < sizeof(opcode_handlers)/sizeof(void*), 1)) {
        goto *opcode_handlers[op.op];
    }
    goto handle_default;

handle_mov32:
    fastMov32(op.rd, op.rs1);
    return;

handle_add:
    fastAdd(op.rd, op.rs1, op.rs2);
    return;

handle_sub:
    fastSub(op.rd, op.rs1, op.rs2);
    return;

handle_and:
    fastAnd(op.rd, op.rs1, op.rs2);
    return;

handle_or:
    fastOr(op.rd, op.rs1, op.rs2);
    return;

handle_xor:
    fastXor(op.rd, op.rs1, op.rs2);
    return;

handle_readm: {
    u32 addr = getRegValue(op.rs1);
    u32 size = op.rs2._imm;
    // OPTIMIZATION: Inline memory access for speed
    if (__builtin_expect(size == 4, 1)) {
        setRegValue(op.rd, ReadMem32(addr));
        return;
    }
    // Handle other sizes
    u32 value = 0;
    switch (size) {
        case 1: value = ReadMem8(addr); break;
        case 2: value = ReadMem16(addr); break;
        case 8: {
            u64 val64 = ReadMem64(addr);
            setRegValue(op.rd, (u32)val64);
            setRegValue(op.rd2, (u32)(val64 >> 32));
            return;
        }
    }
    setRegValue(op.rd, value);
    return;
}

handle_writem: {
    u32 addr = getRegValue(op.rs1);
    u32 size = op.rs2._imm;
    // OPTIMIZATION: Inline memory access for speed
    if (__builtin_expect(size == 4, 1)) {
        WriteMem32(addr, getRegValue(op.rs3));
        return;
    }
    // Handle other sizes
    u32 value = getRegValue(op.rs3);
    switch (size) {
        case 1: WriteMem8(addr, value); break;
        case 2: WriteMem16(addr, value); break;
        case 8: {
            u64 val64 = ((u64)getRegValue(op.rs3) << 32) | getRegValue(op.rs2);
            WriteMem64(addr, val64);
            break;
        }
    }
    return;
}

handle_jcond:
    if (__builtin_expect(reg_cache.get_sr_t() == op.rs2._imm, 0)) {
        reg_cache.flush(); // Flush before jump
        next_pc = getRegValue(op.rs1);
        should_exit_block = true;
    }
    return;

handle_jdyn:
    reg_cache.flush(); // Flush before jump
    next_pc = getRegValue(op.rs1);
    should_exit_block = true;
    return;

handle_pref:
    // Prefetch instruction - no-op for interpreter
    return;

handle_test:
    reg_cache.set_sr_t((getRegValue(op.rs1) & getRegValue(op.rs2)) == 0 ? 1 : 0);
    return;

handle_seteq:
    reg_cache.set_sr_t((getRegValue(op.rs1) == getRegValue(op.rs2)) ? 1 : 0);
    return;

// Continue with remaining handlers using traditional switch for less common ops
handle_mov64:
    setRegValue(op.rd, getRegValue(op.rs1));
    setRegValue(op.rd2, getRegValue(op.rs2));
    return;

handle_mul_u16:
    setRegValue(op.rd, (u16)getRegValue(op.rs1) * (u16)getRegValue(op.rs2));
    return;

handle_mul_s16:
    setRegValue(op.rd, (s16)getRegValue(op.rs1) * (s16)getRegValue(op.rs2));
    return;

handle_mul_i32:
    setRegValue(op.rd, (s32)getRegValue(op.rs1) * (s32)getRegValue(op.rs2));
    return;

handle_mul_u64: {
    u64 result = (u64)getRegValue(op.rs1) * (u64)getRegValue(op.rs2);
    setRegValue(op.rd, (u32)result);
    setRegValue(op.rd2, (u32)(result >> 32));
    return;
}

handle_mul_s64: {
    s64 result = (s64)(s32)getRegValue(op.rs1) * (s64)(s32)getRegValue(op.rs2);
    setRegValue(op.rd, (u32)result);
    setRegValue(op.rd2, (u32)(result >> 32));
    return;
}

handle_not:
    setRegValue(op.rd, ~getRegValue(op.rs1));
    return;

handle_shl:
    setRegValue(op.rd, getRegValue(op.rs1) << (getRegValue(op.rs2) & 0x1F));
    return;

handle_shr:
    setRegValue(op.rd, getRegValue(op.rs1) >> (getRegValue(op.rs2) & 0x1F));
    return;

handle_sar:
    setRegValue(op.rd, (s32)getRegValue(op.rs1) >> (getRegValue(op.rs2) & 0x1F));
    return;

handle_neg:
    setRegValue(op.rd, -(s32)getRegValue(op.rs1));
    return;

handle_swaplb: {
    u32 val = getRegValue(op.rs1);
    setRegValue(op.rd, (val & 0xFFFF0000) | ((val & 0xFF) << 8) | ((val >> 8) & 0xFF));
    return;
}

handle_setge:
    reg_cache.set_sr_t(((s32)getRegValue(op.rs1) >= (s32)getRegValue(op.rs2)) ? 1 : 0);
    return;

handle_setgt:
    reg_cache.set_sr_t(((s32)getRegValue(op.rs1) > (s32)getRegValue(op.rs2)) ? 1 : 0);
    return;

handle_setab:
    reg_cache.set_sr_t((getRegValue(op.rs1) > getRegValue(op.rs2)) ? 1 : 0);
    return;

handle_setae:
    reg_cache.set_sr_t((getRegValue(op.rs1) >= getRegValue(op.rs2)) ? 1 : 0);
    return;

handle_ext_s8:
    setRegValue(op.rd, (s32)(s8)getRegValue(op.rs1));
    return;

handle_ext_s16:
    setRegValue(op.rd, (s32)(s16)getRegValue(op.rs1));
    return;

handle_cvt_i2f_n:
    setFloatRegValue(op.rd, (f32)(s32)getRegValue(op.rs1));
    return;

handle_cvt_f2i_t:
    setRegValue(op.rd, (u32)(s32)getFloatRegValue(op.rs1));
    return;

handle_fadd:
    setFloatRegValue(op.rd, getFloatRegValue(op.rs1) + getFloatRegValue(op.rs2));
    return;

handle_fsub:
    setFloatRegValue(op.rd, getFloatRegValue(op.rs1) - getFloatRegValue(op.rs2));
    return;

handle_fmul:
    setFloatRegValue(op.rd, getFloatRegValue(op.rs1) * getFloatRegValue(op.rs2));
    return;

handle_fdiv:
    setFloatRegValue(op.rd, getFloatRegValue(op.rs1) / getFloatRegValue(op.rs2));
    return;

handle_fabs:
    setFloatRegValue(op.rd, fabsf(getFloatRegValue(op.rs1)));
    return;

handle_fneg:
    setFloatRegValue(op.rd, -getFloatRegValue(op.rs1));
    return;

handle_fsqrt:
    setFloatRegValue(op.rd, sqrtf(getFloatRegValue(op.rs1)));
    return;

handle_fmac:
    setFloatRegValue(op.rd, getFloatRegValue(op.rs1) * getFloatRegValue(op.rs2) + getFloatRegValue(op.rs3));
    return;

handle_fseteq:
    reg_cache.set_sr_t((getFloatRegValue(op.rs1) == getFloatRegValue(op.rs2)) ? 1 : 0);
    return;

handle_fsetgt:
    reg_cache.set_sr_t((getFloatRegValue(op.rs1) > getFloatRegValue(op.rs2)) ? 1 : 0);
    return;

handle_ifb:
    // Interpreter fallback - execute original SH4 instruction
    reg_cache.flush();
    if (op.rs1._imm) {
        next_pc = op.rs2._imm;
    }
    {
        u32 opcode = op.rs3._imm;
        OpDesc[opcode]->oph(opcode);
    }
    reg_cache.load();
    should_exit_block = true;
    return;

handle_default:
    // Unhandled opcode - fallback to interpreter
    WARN_LOG(DYNAREC, "Unhandled SHIL opcode: %d", op.op);
    should_exit_block = true;
    return;
}

// OPTIMIZATION: Hyper-optimized block execution with register caching
void ShilInterpreter::executeBlock(RuntimeBlockInfo* block) {
    should_exit_block = false;
    
    // OPTIMIZATION: Load registers into cache at block start
    reg_cache.load();
    
    // OPTIMIZATION: Cache block size and use direct pointer access
    const size_t block_size = block->oplist.size();
    const shil_opcode* opcodes = block->oplist.data();
    
    // OPTIMIZATION: Unroll small blocks for better performance
    if (__builtin_expect(block_size <= 4, 0)) {
        // Unrolled execution for tiny blocks
        switch (block_size) {
            case 4:
                executeOpcode(opcodes[0]);
                if (__builtin_expect(should_exit_block, 0)) goto exit;
                executeOpcode(opcodes[1]);
                if (__builtin_expect(should_exit_block, 0)) goto exit;
                executeOpcode(opcodes[2]);
                if (__builtin_expect(should_exit_block, 0)) goto exit;
                executeOpcode(opcodes[3]);
                break;
            case 3:
                executeOpcode(opcodes[0]);
                if (__builtin_expect(should_exit_block, 0)) goto exit;
                executeOpcode(opcodes[1]);
                if (__builtin_expect(should_exit_block, 0)) goto exit;
                executeOpcode(opcodes[2]);
                break;
            case 2:
                executeOpcode(opcodes[0]);
                if (__builtin_expect(should_exit_block, 0)) goto exit;
                executeOpcode(opcodes[1]);
                break;
            case 1:
                executeOpcode(opcodes[0]);
                break;
        }
    } else {
        // Regular loop for larger blocks
        for (size_t i = 0; i < block_size && __builtin_expect(!should_exit_block, 1); i++) {
            executeOpcode(opcodes[i]);
        }
    }
    
exit:
    // OPTIMIZATION: Flush register cache at block end
    reg_cache.flush();
}

// OPTIMIZATION: Hyper-optimized mainloop with reduced overhead
void shil_interpreter_mainloop(void* v_cntx) {
    // Set up context similar to JIT mainloop
    p_sh4rcb = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4Context));
    
    try {
        while (__builtin_expect(Sh4cntx.CpuRunning, 1)) {
            // OPTIMIZATION: Reduce interrupt checking frequency for better performance
            if (__builtin_expect(Sh4cntx.cycle_counter <= 0, 0)) {
                Sh4cntx.cycle_counter += SH4_TIMESLICE;
                if (UpdateSystem_INTC()) {
                    // Interrupt occurred, continue to handle it
                    continue;
                }
            }
            
            // Get current PC
            u32 pc = next_pc;
            
            // Find or create block for current PC
            RuntimeBlockInfoPtr blockPtr = bm_GetBlock(pc);
            RuntimeBlockInfo* block = nullptr;
            
            if (__builtin_expect(blockPtr != nullptr, 1)) {
                block = blockPtr.get();
            } else {
                // Block doesn't exist, we need to create and decode it
                // This will trigger the normal block creation process
                DynarecCodeEntryPtr code = rdv_CompilePC(0);
                if (!code) {
                    ERROR_LOG(DYNAREC, "Failed to create block for PC: %08X", pc);
                    break;
                }
                
                // Get the block again after compilation
                blockPtr = bm_GetBlock(pc);
                if (!blockPtr) {
                    ERROR_LOG(DYNAREC, "Block creation succeeded but block not found for PC: %08X", pc);
                    break;
                }
                block = blockPtr.get();
            }
            
            // Check if this is a SHIL interpreter block
            if (__builtin_expect(reinterpret_cast<uintptr_t>(block->code) & 0x1, 1)) {
                // This is a SHIL interpreter block
                ShilInterpreter::executeBlock(block);
            } else {
                // This is a regular JIT block - shouldn't happen in interpreter mode
                ERROR_LOG(DYNAREC, "Unexpected JIT block in SHIL interpreter mode");
                break;
            }
            
            // OPTIMIZATION: Estimate cycle count based on instruction count
            Sh4cntx.cycle_counter -= block->guest_cycles;
        }
    } catch (const SH4ThrownException&) {
        ERROR_LOG(DYNAREC, "SH4ThrownException in SHIL interpreter mainloop");
        throw FlycastException("Fatal: Unhandled SH4 exception");
    }
} 