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

// OPTIMIZATION: Inline register access functions to avoid function call overhead
static inline u32 getRegValue(const shil_param& param) {
    if (param.is_imm()) {
        return param.imm_value();
    } else if (param.is_reg()) {
        return *param.reg_ptr();
    }
    return 0;
}

static inline void setRegValue(const shil_param& param, u32 value) {
    if (param.is_reg()) {
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

// OPTIMIZATION: Fast path for common register-to-register moves
static inline void fastMov32(const shil_param& dst, const shil_param& src) {
    if (dst.is_reg() && src.is_reg()) {
        *dst.reg_ptr() = *src.reg_ptr();
    } else if (dst.is_reg() && src.is_imm()) {
        *dst.reg_ptr() = src.imm_value();
    } else {
        setRegValue(dst, getRegValue(src));
    }
}

// OPTIMIZATION: Fast path for common arithmetic operations
static inline void fastAdd(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (dst.is_reg() && src1.is_reg() && src2.is_reg()) {
        *dst.reg_ptr() = *src1.reg_ptr() + *src2.reg_ptr();
    } else if (dst.is_reg() && src1.is_reg() && src2.is_imm()) {
        *dst.reg_ptr() = *src1.reg_ptr() + src2.imm_value();
    } else {
        setRegValue(dst, getRegValue(src1) + getRegValue(src2));
    }
}

static inline void fastSub(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (dst.is_reg() && src1.is_reg() && src2.is_reg()) {
        *dst.reg_ptr() = *src1.reg_ptr() - *src2.reg_ptr();
    } else if (dst.is_reg() && src1.is_reg() && src2.is_imm()) {
        *dst.reg_ptr() = *src1.reg_ptr() - src2.imm_value();
    } else {
        setRegValue(dst, getRegValue(src1) - getRegValue(src2));
    }
}

// OPTIMIZATION: Fast path for bitwise operations
static inline void fastAnd(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (dst.is_reg() && src1.is_reg() && src2.is_reg()) {
        *dst.reg_ptr() = *src1.reg_ptr() & *src2.reg_ptr();
    } else if (dst.is_reg() && src1.is_reg() && src2.is_imm()) {
        *dst.reg_ptr() = *src1.reg_ptr() & src2.imm_value();
    } else {
        setRegValue(dst, getRegValue(src1) & getRegValue(src2));
    }
}

static inline void fastOr(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (dst.is_reg() && src1.is_reg() && src2.is_reg()) {
        *dst.reg_ptr() = *src1.reg_ptr() | *src2.reg_ptr();
    } else if (dst.is_reg() && src1.is_reg() && src2.is_imm()) {
        *dst.reg_ptr() = *src1.reg_ptr() | src2.imm_value();
    } else {
        setRegValue(dst, getRegValue(src1) | getRegValue(src2));
    }
}

static inline void fastXor(const shil_param& dst, const shil_param& src1, const shil_param& src2) {
    if (dst.is_reg() && src1.is_reg() && src2.is_reg()) {
        *dst.reg_ptr() = *src1.reg_ptr() ^ *src2.reg_ptr();
    } else if (dst.is_reg() && src1.is_reg() && src2.is_imm()) {
        *dst.reg_ptr() = *src1.reg_ptr() ^ src2.imm_value();
    } else {
        setRegValue(dst, getRegValue(src1) ^ getRegValue(src2));
    }
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
    // Set PC if needed
    if (op.rs1.imm_value()) {
        next_pc = op.rs2.imm_value();
    }
    
    // Call SH4 instruction handler directly
    u32 opcode = op.rs3.imm_value();
    OpDesc[opcode]->oph(opcode);
    
    // Exit block after fallback
    should_exit_block = true;
}

void ShilInterpreter::handleDynamicJump(const shil_opcode& op) {
    // Set dynamic PC
    u32 target = getRegValue(op.rs1);
    if (!op.rs2.is_null()) {
        target += getRegValue(op.rs2);
    }
    *op.rd.reg_ptr() = target;
    should_exit_block = true;
}

void ShilInterpreter::handleConditionalJump(const shil_opcode& op) {
    // Set conditional jump target
    u32 target = getRegValue(op.rs2);
    *op.rd.reg_ptr() = target;
    should_exit_block = true;
}

// OPTIMIZATION: Optimized executeOpcode with fast paths and better branch prediction
void ShilInterpreter::executeOpcode(const shil_opcode& op) {
    // OPTIMIZATION: Fast paths for the most common operations
    switch (op.op) {
        case shop_mov32:
            fastMov32(op.rd, op.rs1);
            return;
            
        case shop_add:
            fastAdd(op.rd, op.rs1, op.rs2);
            return;
            
        case shop_sub:
            fastSub(op.rd, op.rs1, op.rs2);
            return;
            
        case shop_and:
            fastAnd(op.rd, op.rs1, op.rs2);
            return;
            
        case shop_or:
            fastOr(op.rd, op.rs1, op.rs2);
            return;
            
        case shop_xor:
            fastXor(op.rd, op.rs1, op.rs2);
            return;
            
        case shop_readm: {
            u32 addr = getRegValue(op.rs1);
            u32 size = op.rs2._imm;
            // OPTIMIZATION: Fast path for common 32-bit reads
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
        
        case shop_writem: {
            u32 addr = getRegValue(op.rs1);
            u32 size = op.rs2._imm;
            // OPTIMIZATION: Fast path for common 32-bit writes
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
        
        case shop_jcond:
            if (__builtin_expect(sr.T == op.rs2._imm, 0)) {
                next_pc = getRegValue(op.rs1);
                should_exit_block = true;
            }
            return;
            
        case shop_jdyn:
            next_pc = getRegValue(op.rs1);
            should_exit_block = true;
            return;
            
        case shop_pref:
            // Prefetch instruction - no-op for interpreter
            return;
            
        // Continue with remaining opcodes
        case shop_mov64:
            setRegValue(op.rd, getRegValue(op.rs1));
            setRegValue(op.rd2, getRegValue(op.rs2));
            break;
            
        case shop_mul_u16:
            setRegValue(op.rd, (u16)getRegValue(op.rs1) * (u16)getRegValue(op.rs2));
            break;
            
        case shop_mul_s16:
            setRegValue(op.rd, (s16)getRegValue(op.rs1) * (s16)getRegValue(op.rs2));
            break;
            
        case shop_mul_i32:
            setRegValue(op.rd, (s32)getRegValue(op.rs1) * (s32)getRegValue(op.rs2));
            break;
            
        case shop_mul_u64:
            {
                u64 result = (u64)getRegValue(op.rs1) * (u64)getRegValue(op.rs2);
                setRegValue(op.rd, (u32)result);
                setRegValue(op.rd2, (u32)(result >> 32));
            }
            break;
            
        case shop_mul_s64:
            {
                s64 result = (s64)(s32)getRegValue(op.rs1) * (s64)(s32)getRegValue(op.rs2);
                setRegValue(op.rd, (u32)result);
                setRegValue(op.rd2, (u32)(result >> 32));
            }
            break;
            
        case shop_not:
            setRegValue(op.rd, ~getRegValue(op.rs1));
            break;
            
        case shop_shl:
            setRegValue(op.rd, getRegValue(op.rs1) << (getRegValue(op.rs2) & 0x1F));
            break;
            
        case shop_shr:
            setRegValue(op.rd, getRegValue(op.rs1) >> (getRegValue(op.rs2) & 0x1F));
            break;
            
        case shop_sar:
            setRegValue(op.rd, (s32)getRegValue(op.rs1) >> (getRegValue(op.rs2) & 0x1F));
            break;
            
        case shop_neg:
            setRegValue(op.rd, -(s32)getRegValue(op.rs1));
            break;
            
        case shop_swaplb:
            {
                u32 val = getRegValue(op.rs1);
                setRegValue(op.rd, (val & 0xFFFF0000) | ((val & 0xFF) << 8) | ((val >> 8) & 0xFF));
            }
            break;
            
        case shop_test:
            sr.T = (getRegValue(op.rs1) & getRegValue(op.rs2)) == 0 ? 1 : 0;
            break;
            
        case shop_seteq:
            sr.T = (getRegValue(op.rs1) == getRegValue(op.rs2)) ? 1 : 0;
            break;
            
        case shop_setge:
            sr.T = ((s32)getRegValue(op.rs1) >= (s32)getRegValue(op.rs2)) ? 1 : 0;
            break;
            
        case shop_setgt:
            sr.T = ((s32)getRegValue(op.rs1) > (s32)getRegValue(op.rs2)) ? 1 : 0;
            break;
            
        case shop_setab:
            sr.T = (getRegValue(op.rs1) > getRegValue(op.rs2)) ? 1 : 0;
            break;
            
        case shop_setae:
            sr.T = (getRegValue(op.rs1) >= getRegValue(op.rs2)) ? 1 : 0;
            break;
            
        case shop_ext_s8:
            setRegValue(op.rd, (s32)(s8)getRegValue(op.rs1));
            break;
            
        case shop_ext_s16:
            setRegValue(op.rd, (s32)(s16)getRegValue(op.rs1));
            break;
            
        case shop_cvt_i2f_n:
            setFloatRegValue(op.rd, (f32)(s32)getRegValue(op.rs1));
            break;
            
        case shop_cvt_f2i_t:
            setRegValue(op.rd, (u32)(s32)getFloatRegValue(op.rs1));
            break;
            
        case shop_fadd:
            setFloatRegValue(op.rd, getFloatRegValue(op.rs1) + getFloatRegValue(op.rs2));
            break;
            
        case shop_fsub:
            setFloatRegValue(op.rd, getFloatRegValue(op.rs1) - getFloatRegValue(op.rs2));
            break;
            
        case shop_fmul:
            setFloatRegValue(op.rd, getFloatRegValue(op.rs1) * getFloatRegValue(op.rs2));
            break;
            
        case shop_fdiv:
            setFloatRegValue(op.rd, getFloatRegValue(op.rs1) / getFloatRegValue(op.rs2));
            break;
            
        case shop_fabs:
            setFloatRegValue(op.rd, fabsf(getFloatRegValue(op.rs1)));
            break;
            
        case shop_fneg:
            setFloatRegValue(op.rd, -getFloatRegValue(op.rs1));
            break;
            
        case shop_fsqrt:
            setFloatRegValue(op.rd, sqrtf(getFloatRegValue(op.rs1)));
            break;
            
        case shop_fmac:
            setFloatRegValue(op.rd, getFloatRegValue(op.rs1) * getFloatRegValue(op.rs2) + getFloatRegValue(op.rs3));
            break;
            
        case shop_fseteq:
            sr.T = (getFloatRegValue(op.rs1) == getFloatRegValue(op.rs2)) ? 1 : 0;
            break;
            
        case shop_fsetgt:
            sr.T = (getFloatRegValue(op.rs1) > getFloatRegValue(op.rs2)) ? 1 : 0;
            break;
            
        case shop_ifb:
            // Interpreter fallback - execute original SH4 instruction
            if (op.rs1._imm) {
                next_pc = op.rs2._imm;
            }
            {
                u32 opcode = op.rs3._imm;
                OpDesc[opcode]->oph(opcode);
            }
            should_exit_block = true;
            break;
            
        default:
            // Unhandled opcode - fallback to interpreter
            WARN_LOG(DYNAREC, "Unhandled SHIL opcode: %d", op.op);
            should_exit_block = true;
            break;
    }
}

// OPTIMIZATION: Optimized block execution with reduced function call overhead
void ShilInterpreter::executeBlock(RuntimeBlockInfo* block) {
    should_exit_block = false;
    
    // OPTIMIZATION: Cache block size to avoid repeated size() calls
    const size_t block_size = block->oplist.size();
    
    // OPTIMIZATION: Direct pointer access to opcodes
    const shil_opcode* opcodes = block->oplist.data();
    
    for (size_t i = 0; i < block_size && __builtin_expect(!should_exit_block, 1); i++) {
        executeOpcode(opcodes[i]);
    }
}

// OPTIMIZATION: Optimized mainloop with reduced overhead
void shil_interpreter_mainloop(void* v_cntx) {
    // Set up context similar to JIT mainloop
    p_sh4rcb = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4Context));
    
    try {
        while (__builtin_expect(Sh4cntx.CpuRunning, 1)) {
            // OPTIMIZATION: Reduce interrupt checking frequency
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
            
            // Update cycle counter
            Sh4cntx.cycle_counter -= block->guest_cycles;
        }
    } catch (const SH4ThrownException&) {
        ERROR_LOG(DYNAREC, "SH4ThrownException in SHIL interpreter mainloop");
        throw FlycastException("Fatal: Unhandled SH4 exception");
    }
} 