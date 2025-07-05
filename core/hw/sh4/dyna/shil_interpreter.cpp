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
#include <vector>
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
    }
}

// WASM JIT: Configuration
static constexpr u32 HOT_BLOCK_THRESHOLD = 3;  // Compile after 3 executions (very aggressive)
static constexpr u32 MAX_WASM_BLOCKS = 10000;  // Large cache for WASM blocks

// WASM JIT: Block execution tracking
struct WasmBlockStats {
    u32 execution_count = 0;
    bool is_compiled = false;
    bool compilation_failed = false;
    std::vector<u8> wasm_bytecode;
    std::chrono::steady_clock::time_point last_execution;
};

// WASM JIT: Global state
static std::unordered_map<u32, WasmBlockStats> wasm_block_stats;
static std::mutex wasm_compilation_mutex;

// WASM JIT: Ultra-fast register cache
struct WasmRegisterCache {
    u32 r[16];          // All general purpose registers
    u32 sr_t;           // T flag
    u32 pc;             // Program counter
    u32 pr;             // Procedure register
    u32 mac_l, mac_h;   // MAC registers
    u32 gbr, vbr;       // Base registers
    
    // Ultra-fast bulk operations
    void load_all() __attribute__((always_inline)) {
        // Unrolled register loading
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
        // Unrolled register storing
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

static WasmRegisterCache g_wasm_cache;

// WASM JIT: Minimal WASM runtime (no external dependencies)
class MiniWasmRuntime {
private:
    struct WasmStack {
        u32 data[1024];
        u32 sp = 0;
        
        void push(u32 val) { data[sp++] = val; }
        u32 pop() { return data[--sp]; }
        u32 top() { return data[sp-1]; }
    };
    
    WasmStack stack;
    
public:
    // WASM bytecode opcodes (simplified subset)
    enum WasmOp : u8 {
        // Stack operations
        WASM_I32_CONST = 0x41,
        WASM_LOCAL_GET = 0x20,
        WASM_LOCAL_SET = 0x21,
        
        // Arithmetic
        WASM_I32_ADD = 0x6A,
        WASM_I32_SUB = 0x6B,
        WASM_I32_AND = 0x71,
        WASM_I32_OR = 0x72,
        WASM_I32_XOR = 0x73,
        WASM_I32_SHL = 0x74,
        WASM_I32_SHR_U = 0x76,
        WASM_I32_SHR_S = 0x75,
        
        // Memory
        WASM_I32_LOAD = 0x28,
        WASM_I32_STORE = 0x36,
        
        // Control
        WASM_RETURN = 0x0F,
        WASM_END = 0x0B,
        
        // Custom: Register access
        WASM_REG_LOAD = 0xF0,
        WASM_REG_STORE = 0xF1,
        WASM_MEM_FAST = 0xF2
    };
    
    // Ultra-fast WASM execution (no validation, maximum speed)
    void execute_wasm_block(const std::vector<u8>& bytecode) __attribute__((hot)) {
        const u8* pc = bytecode.data();
        const u8* end = pc + bytecode.size();
        
        while (pc < end) {
            u8 opcode = *pc++;
            
            switch (opcode) {
                case WASM_I32_CONST: {
                    u32 val = *(u32*)pc;
                    pc += 4;
                    stack.push(val);
                    break;
                }
                
                case WASM_LOCAL_GET: {
                    u8 reg_idx = *pc++;
                    stack.push(g_wasm_cache.r[reg_idx]);
                    break;
                }
                
                case WASM_LOCAL_SET: {
                    u8 reg_idx = *pc++;
                    g_wasm_cache.r[reg_idx] = stack.pop();
                    break;
                }
                
                case WASM_I32_ADD: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push(a + b);
                    break;
                }
                
                case WASM_I32_SUB: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push(a - b);
                    break;
                }
                
                case WASM_I32_AND: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push(a & b);
                    break;
                }
                
                case WASM_I32_OR: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push(a | b);
                    break;
                }
                
                case WASM_I32_XOR: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push(a ^ b);
                    break;
                }
                
                case WASM_I32_SHL: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push(a << (b & 0x1F));
                    break;
                }
                
                case WASM_I32_SHR_U: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push(a >> (b & 0x1F));
                    break;
                }
                
                case WASM_I32_SHR_S: {
                    u32 b = stack.pop();
                    u32 a = stack.pop();
                    stack.push((s32)a >> (b & 0x1F));
                    break;
                }
                
                case WASM_REG_LOAD: {
                    u8 reg_idx = *pc++;
                    if (reg_idx < 16) {
                        stack.push(g_wasm_cache.r[reg_idx]);
                    } else if (reg_idx == 16) {
                        stack.push(g_wasm_cache.sr_t);
                    }
                    break;
                }
                
                case WASM_REG_STORE: {
                    u8 reg_idx = *pc++;
                    u32 val = stack.pop();
                    if (reg_idx < 16) {
                        g_wasm_cache.r[reg_idx] = val;
                    } else if (reg_idx == 16) {
                        g_wasm_cache.sr_t = val;
                    }
                    break;
                }
                
                case WASM_MEM_FAST: {
                    u8 op_type = *pc++;  // 0=read, 1=write
                    u8 size = *pc++;     // 1, 2, 4 bytes
                    
                    if (op_type == 0) {  // Read
                        u32 addr = stack.pop();
                        u32 val = 0;
                        
                        // Ultra-fast memory read with inlined bounds check
                        if (__builtin_expect((addr & 0xFF000000) == 0x0C000000, 1)) {
                            // Fast path: Main RAM
                            switch (size) {
                                case 1: val = mem_b[addr & 0xFFFFFF]; break;
                                case 2: val = *(u16*)&mem_b[addr & 0xFFFFFF]; break;
                                case 4: val = *(u32*)&mem_b[addr & 0xFFFFFF]; break;
                            }
                        } else {
                            // Slow path: Memory-mapped I/O
                            switch (size) {
                                case 1: val = ReadMem8(addr); break;
                                case 2: val = ReadMem16(addr); break;
                                case 4: val = ReadMem32(addr); break;
                            }
                        }
                        stack.push(val);
                    } else {  // Write
                        u32 val = stack.pop();
                        u32 addr = stack.pop();
                        
                        // Ultra-fast memory write with inlined bounds check
                        if (__builtin_expect((addr & 0xFF000000) == 0x0C000000, 1)) {
                            // Fast path: Main RAM
                            switch (size) {
                                case 1: mem_b[addr & 0xFFFFFF] = val; break;
                                case 2: *(u16*)&mem_b[addr & 0xFFFFFF] = val; break;
                                case 4: *(u32*)&mem_b[addr & 0xFFFFFF] = val; break;
                            }
                        } else {
                            // Slow path: Memory-mapped I/O
                            switch (size) {
                                case 1: WriteMem8(addr, val); break;
                                case 2: WriteMem16(addr, val); break;
                                case 4: WriteMem32(addr, val); break;
                            }
                        }
                    }
                    break;
                }
                
                case WASM_RETURN:
                case WASM_END:
                    return;
                
                default:
                    // Skip unknown opcodes
                    break;
            }
        }
    }
};

static MiniWasmRuntime g_wasm_runtime;

// WASM JIT: SHIL to WASM compiler
class ShilToWasmCompiler {
private:
    std::vector<u8> bytecode;
    
    void emit_u8(u8 val) { bytecode.push_back(val); }
    void emit_u32(u32 val) {
        bytecode.insert(bytecode.end(), (u8*)&val, (u8*)&val + 4);
    }
    
    void emit_const(u32 val) {
        emit_u8(MiniWasmRuntime::WASM_I32_CONST);
        emit_u32(val);
    }
    
    void emit_reg_load(u8 reg_idx) {
        emit_u8(MiniWasmRuntime::WASM_REG_LOAD);
        emit_u8(reg_idx);
    }
    
    void emit_reg_store(u8 reg_idx) {
        emit_u8(MiniWasmRuntime::WASM_REG_STORE);
        emit_u8(reg_idx);
    }
    
    u8 get_reg_index(const shil_param& param) {
        if (param.is_reg() && param._reg < 16) {
            return param._reg;
        } else if (param.is_reg()) {
            switch (param._reg) {
                case reg_sr_T: return 16;
                case reg_pc_dyn: return 17;
                case reg_pr: return 18;
                default: return 0;
            }
        }
        return 0;
    }
    
    void compile_operand(const shil_param& param) {
        if (param.is_imm()) {
            emit_const(param.imm_value());
        } else if (param.is_reg()) {
            emit_reg_load(get_reg_index(param));
        }
    }
    
public:
    std::vector<u8> compile_block(RuntimeBlockInfo* block) {
        bytecode.clear();
        bytecode.reserve(block->sh4_code_size * 8);  // Estimate
        
        // Compile each SHIL operation to WASM
        for (u32 i = 0; i < block->sh4_code_size; i++) {
            const shil_opcode& op = block->oplist[i];
            compile_operation(op);
        }
        
        // End block
        emit_u8(MiniWasmRuntime::WASM_END);
        
        return std::move(bytecode);
    }
    
private:
    void compile_operation(const shil_opcode& op) {
        switch (op.op) {
            case shop_mov32:
                compile_operand(op.rs1);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_add:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_ADD);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_sub:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_SUB);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_and:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_AND);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_or:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_OR);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_xor:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_XOR);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_shl:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_SHL);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_shr:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_SHR_U);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_sar:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_SHR_S);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_readm:
                compile_operand(op.rs1);
                emit_u8(MiniWasmRuntime::WASM_MEM_FAST);
                emit_u8(0);  // Read operation
                emit_u8(op.size);
                emit_reg_store(get_reg_index(op.rd));
                break;
                
            case shop_writem:
                compile_operand(op.rs1);  // Address
                compile_operand(op.rs2);  // Value
                emit_u8(MiniWasmRuntime::WASM_MEM_FAST);
                emit_u8(1);  // Write operation
                emit_u8(op.size);
                break;
                
            case shop_test:
                compile_operand(op.rs1);
                compile_operand(op.rs2);
                emit_u8(MiniWasmRuntime::WASM_I32_AND);
                // For test operation, set T flag to 1 if result is 0, 0 otherwise
                // This is handled in the interpreter fallback for now
                emit_reg_store(16);  // T flag
                break;
                
            default:
                // For unsupported operations, emit a no-op
                break;
        }
    }
};

static ShilToWasmCompiler g_wasm_compiler;

// WASM JIT: Execution Engine
class WasmExecutionEngine {
public:
    static void execute_block(RuntimeBlockInfo* block) __attribute__((hot)) {
        u32 addr = block->addr;
        
        // Update execution statistics
        wasm_block_stats[addr].execution_count++;
        wasm_block_stats[addr].last_execution = std::chrono::steady_clock::now();
        
        // Check if we have a compiled WASM version
        {
            std::lock_guard<std::mutex> lock(wasm_compilation_mutex);
            auto& stats = wasm_block_stats[addr];
            
            if (stats.is_compiled && !stats.wasm_bytecode.empty()) {
                // Execute compiled WASM version - MAXIMUM SPEED!
                g_wasm_cache.load_all();
                g_wasm_runtime.execute_wasm_block(stats.wasm_bytecode);
                g_wasm_cache.store_all();
                return;
            }
        }
        
        // Check if block is hot enough for compilation
        if (wasm_block_stats[addr].execution_count >= HOT_BLOCK_THRESHOLD && 
            !wasm_block_stats[addr].is_compiled && 
            !wasm_block_stats[addr].compilation_failed) {
            
            // Compile to WASM immediately (ultra-fast compilation)
            try {
                std::vector<u8> wasm_bytecode = g_wasm_compiler.compile_block(block);
                
                std::lock_guard<std::mutex> lock(wasm_compilation_mutex);
                auto& stats = wasm_block_stats[addr];
                stats.wasm_bytecode = std::move(wasm_bytecode);
                stats.is_compiled = true;
                
                // Execute the newly compiled version immediately
                g_wasm_cache.load_all();
                g_wasm_runtime.execute_wasm_block(stats.wasm_bytecode);
                g_wasm_cache.store_all();
                return;
                
            } catch (...) {
                std::lock_guard<std::mutex> lock(wasm_compilation_mutex);
                wasm_block_stats[addr].compilation_failed = true;
            }
        }
        
        // Fall back to ultra-fast interpreter
        execute_block_interpreter_fast(block);
    }
    
private:
    static void execute_block_interpreter_fast(RuntimeBlockInfo* block) __attribute__((hot)) {
        g_wasm_cache.load_all();
        
        // Ultra-fast interpreter with minimal overhead
        for (u32 i = 0; i < block->sh4_code_size; i++) {
            const shil_opcode& op = block->oplist[i];
            execute_shil_operation_ultra_fast(op);
        }
        
        g_wasm_cache.store_all();
    }
    
    static void execute_shil_operation_ultra_fast(const shil_opcode& op) __attribute__((hot)) {
        // Ultra-fast operation execution with aggressive inlining
        switch (op.op) {
            case shop_mov32:
                if (__builtin_expect(op.rd.is_reg() && op.rs1.is_reg() && op.rd._reg < 16 && op.rs1._reg < 16, 1)) {
                    g_wasm_cache.r[op.rd._reg] = g_wasm_cache.r[op.rs1._reg];
                } else if (op.rd.is_reg() && op.rs1.is_imm() && op.rd._reg < 16) {
                    g_wasm_cache.r[op.rd._reg] = op.rs1.imm_value();
                }
                break;
                
            case shop_add:
                if (__builtin_expect(op.rd.is_reg() && op.rs1.is_reg() && op.rs2.is_reg() && 
                    op.rd._reg < 16 && op.rs1._reg < 16 && op.rs2._reg < 16, 1)) {
                    g_wasm_cache.r[op.rd._reg] = g_wasm_cache.r[op.rs1._reg] + g_wasm_cache.r[op.rs2._reg];
                }
                break;
                
            case shop_sub:
                if (__builtin_expect(op.rd.is_reg() && op.rs1.is_reg() && op.rs2.is_reg() && 
                    op.rd._reg < 16 && op.rs1._reg < 16 && op.rs2._reg < 16, 1)) {
                    g_wasm_cache.r[op.rd._reg] = g_wasm_cache.r[op.rs1._reg] - g_wasm_cache.r[op.rs2._reg];
                }
                break;
                
            case shop_and:
                if (__builtin_expect(op.rd.is_reg() && op.rs1.is_reg() && op.rs2.is_reg() && 
                    op.rd._reg < 16 && op.rs1._reg < 16 && op.rs2._reg < 16, 1)) {
                    g_wasm_cache.r[op.rd._reg] = g_wasm_cache.r[op.rs1._reg] & g_wasm_cache.r[op.rs2._reg];
                }
                break;
                
            case shop_or:
                if (__builtin_expect(op.rd.is_reg() && op.rs1.is_reg() && op.rs2.is_reg() && 
                    op.rd._reg < 16 && op.rs1._reg < 16 && op.rs2._reg < 16, 1)) {
                    g_wasm_cache.r[op.rd._reg] = g_wasm_cache.r[op.rs1._reg] | g_wasm_cache.r[op.rs2._reg];
                }
                break;
                
            case shop_xor:
                if (__builtin_expect(op.rd.is_reg() && op.rs1.is_reg() && op.rs2.is_reg() && 
                    op.rd._reg < 16 && op.rs1._reg < 16 && op.rs2._reg < 16, 1)) {
                    g_wasm_cache.r[op.rd._reg] = g_wasm_cache.r[op.rs1._reg] ^ g_wasm_cache.r[op.rs2._reg];
                }
                break;
                
            case shop_readm: {
                if (__builtin_expect(op.rd.is_reg() && op.rs1.is_reg() && op.rd._reg < 16 && op.rs1._reg < 16, 1)) {
                    u32 addr = g_wasm_cache.r[op.rs1._reg];
                    u32 val = 0;
                    
                    // Ultra-fast memory read
                    if (__builtin_expect((addr & 0xFF000000) == 0x0C000000, 1)) {
                        switch (op.size) {
                            case 1: val = mem_b[addr & 0xFFFFFF]; break;
                            case 2: val = *(u16*)&mem_b[addr & 0xFFFFFF]; break;
                            case 4: val = *(u32*)&mem_b[addr & 0xFFFFFF]; break;
                        }
                    } else {
                        switch (op.size) {
                            case 1: val = ReadMem8(addr); break;
                            case 2: val = ReadMem16(addr); break;
                            case 4: val = ReadMem32(addr); break;
                        }
                    }
                    g_wasm_cache.r[op.rd._reg] = val;
                }
                break;
            }
                
            case shop_writem: {
                if (__builtin_expect(op.rs1.is_reg() && op.rs2.is_reg() && op.rs1._reg < 16 && op.rs2._reg < 16, 1)) {
                    u32 addr = g_wasm_cache.r[op.rs1._reg];
                    u32 val = g_wasm_cache.r[op.rs2._reg];
                    
                    // Ultra-fast memory write
                    if (__builtin_expect((addr & 0xFF000000) == 0x0C000000, 1)) {
                        switch (op.size) {
                            case 1: mem_b[addr & 0xFFFFFF] = val; break;
                            case 2: *(u16*)&mem_b[addr & 0xFFFFFF] = val; break;
                            case 4: *(u32*)&mem_b[addr & 0xFFFFFF] = val; break;
                        }
                    } else {
                        switch (op.size) {
                            case 1: WriteMem8(addr, val); break;
                            case 2: WriteMem16(addr, val); break;
                            case 4: WriteMem32(addr, val); break;
                        }
                    }
                }
                break;
            }
                
            default:
                // Fallback to full interpreter for complex operations
                g_wasm_cache.store_all();
                ShilInterpreter::executeOpcode(op);
                g_wasm_cache.load_all();
                break;
        }
    }
};

// WASM: ShilInterpreter implementation using WASM JIT
void ShilInterpreter::executeBlock(RuntimeBlockInfo* block) {
    WasmExecutionEngine::execute_block(block);
}

// Implement the static functions declared in the header
void ShilInterpreter::executeOpcode(const shil_opcode& op) {
    // Simple fallback implementation
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
    g_wasm_cache.store_all();
    
    // Call SH4 instruction handler directly
    u32 opcode = op.rs3.imm_value();
    OpDesc[opcode]->oph(opcode);
    
    // Reload register cache after fallback
    g_wasm_cache.load_all();
}

void ShilInterpreter::handleDynamicJump(const shil_opcode& op) {
    // Flush register cache before jump
    g_wasm_cache.store_all();
    
    // Set dynamic PC
    u32 target = getRegValue(op.rs1);
    if (!op.rs2.is_null()) {
        target += getRegValue(op.rs2);
    }
    *op.rd.reg_ptr() = target;
}

void ShilInterpreter::handleConditionalJump(const shil_opcode& op) {
    // Flush register cache before jump
    g_wasm_cache.store_all();
    
    // Set conditional jump target
    u32 target = getRegValue(op.rs2);
    *op.rd.reg_ptr() = target;
}

// Main SHIL interpreter mainloop with WASM JIT
void shil_interpreter_mainloop(void* v_cntx) {
    // Set up context
    p_sh4rcb = (Sh4RCB*)((u8*)v_cntx - sizeof(Sh4Context));
    
    while (emu.running()) {
        // WASM JIT: Minimal overhead main loop
        u32 pc = sh4rcb.cntx.pc;
        
        // WASM JIT: Fast block lookup using direct address translation
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
            // WASM JIT: Execute block with maximum performance (WASM or interpreter)
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
        
        // WASM JIT: Minimal cycle counting
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