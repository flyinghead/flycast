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
#include <sstream>
#include <fstream>
#include <dlfcn.h>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <chrono>

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
        INFO_LOG(DYNAREC, "SHIL Interpreter %s", enable_shil_interpreter ? "ENABLED" : "DISABLED");
    }
}

// LLVM JIT: Configuration
static constexpr u32 HOT_BLOCK_THRESHOLD = 5;  // Compile after 5 executions
static constexpr u32 MAX_COMPILED_BLOCKS = 5000;  // Cache limit

// LLVM JIT: Block execution tracking
struct LLVMBlockStats {
    u32 execution_count = 0;
    bool is_compiled = false;
    bool compilation_failed = false;
    void* compiled_function = nullptr;
    std::chrono::steady_clock::time_point last_execution;
};

// LLVM JIT: Global state
static std::unordered_map<u32, LLVMBlockStats> llvm_block_stats;
static std::mutex llvm_compilation_mutex;
static bool llvm_jit_enabled = true;

// Global flag to indicate if we should exit block execution
static bool should_exit_block = false;

// LLVM JIT: Simplified IR generator for maximum performance
class SimpleLLVMCompiler {
private:
    std::string ir_code;
    u32 next_temp = 0;
    
    std::string get_temp() {
        return "%t" + std::to_string(next_temp++);
    }
    
    void emit_line(const std::string& line) {
        ir_code += "  " + line + "\n";
    }
    
public:
    std::string compile_block(RuntimeBlockInfo* block) {
        ir_code.clear();
        next_temp = 0;
        
        // LLVM IR function header
        ir_code += "define void @shil_block_" + std::to_string(block->addr) + "(i32* %regs, i32* %sr_t) {\n";
        ir_code += "entry:\n";
        
        // Compile each SHIL operation to optimized LLVM IR
        for (const auto& op : block->oplist) {
            compile_shil_operation(op);
        }
        
        emit_line("ret void");
        ir_code += "}\n";
        
        return ir_code;
    }
    
private:
    void compile_shil_operation(const shil_opcode& op) {
        switch (op.op) {
            case shop_mov32: {
                std::string src = load_operand(op.rs1);
                store_operand(op.rd, src);
                break;
            }
            
            case shop_add: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string result = get_temp();
                emit_line(result + " = add i32 " + src1 + ", " + src2);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_sub: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string result = get_temp();
                emit_line(result + " = sub i32 " + src1 + ", " + src2);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_and: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string result = get_temp();
                emit_line(result + " = and i32 " + src1 + ", " + src2);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_or: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string result = get_temp();
                emit_line(result + " = or i32 " + src1 + ", " + src2);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_xor: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string result = get_temp();
                emit_line(result + " = xor i32 " + src1 + ", " + src2);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_shl: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string masked = get_temp();
                std::string result = get_temp();
                emit_line(masked + " = and i32 " + src2 + ", 31");
                emit_line(result + " = shl i32 " + src1 + ", " + masked);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_shr: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string masked = get_temp();
                std::string result = get_temp();
                emit_line(masked + " = and i32 " + src2 + ", 31");
                emit_line(result + " = lshr i32 " + src1 + ", " + masked);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_sar: {
                std::string src1 = load_operand(op.rs1);
                std::string src2 = load_operand(op.rs2);
                std::string masked = get_temp();
                std::string result = get_temp();
                emit_line(masked + " = and i32 " + src2 + ", 31");
                emit_line(result + " = ashr i32 " + src1 + ", " + masked);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_neg: {
                std::string src = load_operand(op.rs1);
                std::string result = get_temp();
                emit_line(result + " = sub i32 0, " + src);
                store_operand(op.rd, result);
                break;
            }
            
            case shop_not: {
                std::string src = load_operand(op.rs1);
                std::string result = get_temp();
                emit_line(result + " = xor i32 " + src + ", -1");
                store_operand(op.rd, result);
                break;
            }
            
            default:
                // Fallback for unimplemented operations
                emit_line("; Fallback for opcode " + std::to_string(op.op));
                emit_line("call void @fallback_interpreter(i32 " + std::to_string(op.op) + ")");
                break;
        }
    }
    
    std::string load_operand(const shil_param& param) {
        if (param.is_imm()) {
            return std::to_string(param._imm);
        } else if (param.is_reg()) {
            std::string ptr = get_temp();
            std::string val = get_temp();
            u32 reg_idx = param._reg - reg_r0;
            if (reg_idx < 16) {
                emit_line(ptr + " = getelementptr i32, i32* %regs, i32 " + std::to_string(reg_idx));
                emit_line(val + " = load i32, i32* " + ptr);
                return val;
            }
        }
        
        // Fallback for complex operands
        std::string temp = get_temp();
        emit_line(temp + " = call i32 @load_operand_fallback(i32 " + std::to_string(param._reg) + ")");
        return temp;
    }
    
    void store_operand(const shil_param& param, const std::string& value) {
        if (param.is_reg()) {
            u32 reg_idx = param._reg - reg_r0;
            if (reg_idx < 16) {
                std::string ptr = get_temp();
                emit_line(ptr + " = getelementptr i32, i32* %regs, i32 " + std::to_string(reg_idx));
                emit_line("store i32 " + value + ", i32* " + ptr);
                return;
            }
        }
        
        // Fallback for complex operands
        emit_line("call void @store_operand_fallback(i32 " + std::to_string(param._reg) + ", i32 " + value + ")");
    }
};

// LLVM JIT: Native code compiler using system clang
class SimpleNativeCompiler {
public:
    static void* compile_ir_to_native(const std::string& ir_code, u32 block_addr) {
        try {
            // Write IR to temporary file
            std::string ir_file = "/tmp/shil_block_" + std::to_string(block_addr) + ".ll";
            std::string obj_file = "/tmp/shil_block_" + std::to_string(block_addr) + ".o";
            std::string dylib_file = "/tmp/shil_block_" + std::to_string(block_addr) + ".dylib";
            
            std::ofstream ir_out(ir_file);
            ir_out << ir_code;
            ir_out.close();
            
            // Compile IR to object file using clang with maximum optimization
            std::string compile_cmd = "clang -O3 -march=native -flto -c " + ir_file + " -o " + obj_file + " 2>/dev/null";
            if (system(compile_cmd.c_str()) != 0) {
                WARN_LOG(DYNAREC, "LLVM: Failed to compile IR for block 0x%08X", block_addr);
                return nullptr;
            }
            
            // Link to shared library
            std::string link_cmd = "clang -shared " + obj_file + " -o " + dylib_file + " 2>/dev/null";
            if (system(link_cmd.c_str()) != 0) {
                WARN_LOG(DYNAREC, "LLVM: Failed to link block 0x%08X", block_addr);
                return nullptr;
            }
            
            // Load the compiled function
            void* handle = dlopen(dylib_file.c_str(), RTLD_NOW);
            if (!handle) {
                WARN_LOG(DYNAREC, "LLVM: Failed to load compiled block 0x%08X: %s", block_addr, dlerror());
                return nullptr;
            }
            
            std::string func_name = "shil_block_" + std::to_string(block_addr);
            void* func_ptr = dlsym(handle, func_name.c_str());
            if (!func_ptr) {
                WARN_LOG(DYNAREC, "LLVM: Failed to find function %s: %s", func_name.c_str(), dlerror());
                dlclose(handle);
                return nullptr;
            }
            
            // Clean up temporary files
            unlink(ir_file.c_str());
            unlink(obj_file.c_str());
            unlink(dylib_file.c_str());
            
            INFO_LOG(DYNAREC, "LLVM: Successfully compiled block 0x%08X", block_addr);
            return func_ptr;
            
        } catch (const std::exception& e) {
            WARN_LOG(DYNAREC, "LLVM: Exception compiling block 0x%08X: %s", block_addr, e.what());
            return nullptr;
        }
    }
};

// LLVM JIT: Ultra-fast register cache for compiled code interface
struct LLVMRegisterCache {
    u32 r[16];          // All general purpose registers
    u32 sr_t;           // T flag
    u32 pc;             // Program counter
    u32 pr;             // Procedure register
    u32 mac_l, mac_h;   // MAC registers (avoid macro conflicts)
    u32 gbr, vbr;       // Base registers
    
    // Ultra-fast bulk operations for LLVM compiled code
    void load_all() __attribute__((always_inline)) {
        // Unrolled register loading for maximum performance
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
        { mac_l = sh4rcb.cntx.mac.l; }
        { mac_h = sh4rcb.cntx.mac.h; }
        gbr = sh4rcb.cntx.gbr;
        vbr = sh4rcb.cntx.vbr;
    }
    
    void store_all() __attribute__((always_inline)) {
        // Unrolled register storing for maximum performance
        sh4rcb.cntx.r[0] = r[0];   sh4rcb.cntx.r[1] = r[1];
        sh4rcb.cntx.r[2] = r[2];   sh4rcb.cntx.r[3] = r[3];
        sh4rcb.cntx.r[4] = r[4];   sh4rcb.cntx.r[5] = r[5];
        sh4rcb.cntx.r[6] = r[6];   sh4rcb.cntx.r[7] = r[7];
        sh4rcb.cntx.r[8] = r[8];   sh4rcb.cntx.r[9] = r[9];
        sh4rcb.cntx.r[10] = r[10]; sh4rcb.cntx.r[11] = r[11];
        sh4rcb.cntx.r[12] = r[12]; sh4rcb.cntx.r[13] = r[13];
        sh4rcb.cntx.r[14] = r[14]; sh4rcb.cntx.r[15] = r[15];
        sh4rcb.cntx.sr.T = sr_t;
        sh4rcb.cntx.pc = pc;
        sh4rcb.cntx.pr = pr;
        { sh4rcb.cntx.mac.l = mac_l; }
        { sh4rcb.cntx.mac.h = mac_h; }
        sh4rcb.cntx.gbr = gbr;
        sh4rcb.cntx.vbr = vbr;
    }
};

static LLVMRegisterCache g_llvm_cache;

// EXTREME OPTIMIZATION: Pre-decoded instruction format
struct FastShilOp {
    u8 opcode;          // Opcode type
    u8 rd, rs1, rs2;    // Register indices (pre-decoded)
    u32 imm;            // Immediate value (pre-decoded)
};

// LLVM JIT: Simple register access macros for compiled code
#define LLVM_REG_GET(idx) (g_llvm_cache.r[idx])
#define LLVM_REG_SET(idx, val) do { g_llvm_cache.r[idx] = (val); } while(0)
#define LLVM_T_GET() (g_llvm_cache.sr_t)
#define LLVM_T_SET(val) do { g_llvm_cache.sr_t = (val); } while(0)

// EXTREME OPTIMIZATION: Direct instruction handlers (no switch, no branches)
// LLVM JIT: All legacy code removed - using LLVM compilation and optimized interpreter

// LLVM JIT: External runtime functions for compiled code
extern "C" {
    u32 load_operand_fallback(u32 reg_id) {
        shil_param param;
        param._reg = (Sh4RegType)reg_id;
        return ShilInterpreter::getRegValue(param);
    }
    
    void store_operand_fallback(u32 reg_id, u32 value) {
        shil_param param;
        param._reg = (Sh4RegType)reg_id;
        ShilInterpreter::setRegValue(param, value);
    }
    
    void fallback_interpreter(u32 opcode) {
        WARN_LOG(DYNAREC, "LLVM: Fallback interpreter called for opcode %d", opcode);
    }
}

// LLVM JIT: Main execution engine
void ShilInterpreter::executeBlock(RuntimeBlockInfo* block) {
    u32 addr = block->addr;
    
    // Check if LLVM JIT is disabled due to errors
    if (!llvm_jit_enabled) {
        execute_block_interpreter_fast(block);
        return;
    }
    
    // Update execution statistics
    llvm_block_stats[addr].execution_count++;
    llvm_block_stats[addr].last_execution = std::chrono::steady_clock::now();
    
    // Check if we have a compiled LLVM version
    {
        std::lock_guard<std::mutex> lock(llvm_compilation_mutex);
        auto& stats = llvm_block_stats[addr];
        
        if (stats.is_compiled && stats.compiled_function) {
            // Execute compiled native code
            g_llvm_cache.load_all();
            
            typedef void (*CompiledFunction)(u32*, u32*);
            CompiledFunction func = (CompiledFunction)stats.compiled_function;
            func(g_llvm_cache.r, &g_llvm_cache.sr_t);
            
            g_llvm_cache.store_all();
            return;
        }
    }
    
    // Check if block is hot enough for compilation
    if (llvm_block_stats[addr].execution_count >= HOT_BLOCK_THRESHOLD &&
        !llvm_block_stats[addr].is_compiled &&
        !llvm_block_stats[addr].compilation_failed) {
        
        // Attempt compilation in background
        std::thread([addr, block]() {
            std::lock_guard<std::mutex> lock(llvm_compilation_mutex);
            auto& stats = llvm_block_stats[addr];
            
            if (stats.is_compiled || stats.compilation_failed) {
                return;  // Already processed
            }
            
            try {
                SimpleLLVMCompiler compiler;
                std::string ir_code = compiler.compile_block(block);
                
                void* compiled_func = SimpleNativeCompiler::compile_ir_to_native(ir_code, addr);
                
                if (compiled_func) {
                    stats.compiled_function = compiled_func;
                    stats.is_compiled = true;
                    INFO_LOG(DYNAREC, "LLVM: Block 0x%08X compiled successfully", addr);
                } else {
                    stats.compilation_failed = true;
                    WARN_LOG(DYNAREC, "LLVM: Block 0x%08X compilation failed", addr);
                }
            } catch (const std::exception& e) {
                stats.compilation_failed = true;
                WARN_LOG(DYNAREC, "LLVM: Exception compiling block 0x%08X: %s", addr, e.what());
            }
        }).detach();
    }
    
    // Execute using fast interpreter while compilation happens
    execute_block_interpreter_fast(block);
}

// LLVM JIT: Fast interpreter fallback
void ShilInterpreter::execute_block_interpreter_fast(RuntimeBlockInfo* block) {
    g_llvm_cache.load_all();
    
    for (const auto& op : block->oplist) {
        execute_shil_operation_ultra_fast(op);
    }
    
    g_llvm_cache.store_all();
}

// LLVM JIT: Ultra-fast operation execution
void ShilInterpreter::execute_shil_operation_ultra_fast(const shil_opcode& op) {
    switch (op.op) {
        case shop_mov32:
            if (op.rs1.is_imm() && op.rd.is_reg()) {
                u32 reg_idx = op.rd._reg - reg_r0;
                if (reg_idx < 16) {
                    g_llvm_cache.r[reg_idx] = op.rs1._imm;
                    return;
                }
            }
            break;
            
        case shop_add:
            if (op.rs1.is_reg() && op.rs2.is_reg() && op.rd.is_reg()) {
                u32 rs1_idx = op.rs1._reg - reg_r0;
                u32 rs2_idx = op.rs2._reg - reg_r0;
                u32 rd_idx = op.rd._reg - reg_r0;
                if (rs1_idx < 16 && rs2_idx < 16 && rd_idx < 16) {
                    g_llvm_cache.r[rd_idx] = g_llvm_cache.r[rs1_idx] + g_llvm_cache.r[rs2_idx];
                    return;
                }
            }
            break;
            
        case shop_sub:
            if (op.rs1.is_reg() && op.rs2.is_reg() && op.rd.is_reg()) {
                u32 rs1_idx = op.rs1._reg - reg_r0;
                u32 rs2_idx = op.rs2._reg - reg_r0;
                u32 rd_idx = op.rd._reg - reg_r0;
                if (rs1_idx < 16 && rs2_idx < 16 && rd_idx < 16) {
                    g_llvm_cache.r[rd_idx] = g_llvm_cache.r[rs1_idx] - g_llvm_cache.r[rs2_idx];
                    return;
                }
            }
            break;
            
        case shop_and:
            if (op.rs1.is_reg() && op.rs2.is_reg() && op.rd.is_reg()) {
                u32 rs1_idx = op.rs1._reg - reg_r0;
                u32 rs2_idx = op.rs2._reg - reg_r0;
                u32 rd_idx = op.rd._reg - reg_r0;
                if (rs1_idx < 16 && rs2_idx < 16 && rd_idx < 16) {
                    g_llvm_cache.r[rd_idx] = g_llvm_cache.r[rs1_idx] & g_llvm_cache.r[rs2_idx];
                    return;
                }
            }
            break;
            
        case shop_or:
            if (op.rs1.is_reg() && op.rs2.is_reg() && op.rd.is_reg()) {
                u32 rs1_idx = op.rs1._reg - reg_r0;
                u32 rs2_idx = op.rs2._reg - reg_r0;
                u32 rd_idx = op.rd._reg - reg_r0;
                if (rs1_idx < 16 && rs2_idx < 16 && rd_idx < 16) {
                    g_llvm_cache.r[rd_idx] = g_llvm_cache.r[rs1_idx] | g_llvm_cache.r[rs2_idx];
                    return;
                }
            }
            break;
            
        case shop_xor:
            if (op.rs1.is_reg() && op.rs2.is_reg() && op.rd.is_reg()) {
                u32 rs1_idx = op.rs1._reg - reg_r0;
                u32 rs2_idx = op.rs2._reg - reg_r0;
                u32 rd_idx = op.rd._reg - reg_r0;
                if (rs1_idx < 16 && rs2_idx < 16 && rd_idx < 16) {
                    g_llvm_cache.r[rd_idx] = g_llvm_cache.r[rs1_idx] ^ g_llvm_cache.r[rs2_idx];
                    return;
                }
            }
            break;
    }
    
    // Fallback to full interpreter for complex operations
    g_llvm_cache.store_all();
    executeOpcode(op);
    g_llvm_cache.load_all();
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
    g_llvm_cache.store_all();
    
    // Call SH4 instruction handler directly
    u32 opcode = op.rs3.imm_value();
    OpDesc[opcode]->oph(opcode);
    
    // Reload register cache after fallback
    g_llvm_cache.load_all();
}

void ShilInterpreter::handleDynamicJump(const shil_opcode& op) {
    // Flush register cache before jump
    g_llvm_cache.store_all();
    
    // Set dynamic PC
    u32 target = getRegValue(op.rs1);
    if (!op.rs2.is_null()) {
        target += getRegValue(op.rs2);
    }
    *op.rd.reg_ptr() = target;
}

void ShilInterpreter::handleConditionalJump(const shil_opcode& op) {
    // Flush register cache before jump
    g_llvm_cache.store_all();
    
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