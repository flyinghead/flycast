#pragma once

#include "types.h"
#include "shil.h"
#include "blockmanager.h"

/// SHIL Interpreter - executes SHIL opcodes directly without JIT compilation
class ShilInterpreter {
public:
    /// Execute a single SHIL opcode
    static void executeOpcode(const shil_opcode& op);
    
    /// Execute an entire SHIL block
    static void executeBlock(RuntimeBlockInfo* block);
    
    /// Get register value for SHIL parameter
    static u32 getRegValue(const shil_param& param);
    
    /// Set register value for SHIL parameter  
    static void setRegValue(const shil_param& param, u32 value);
    
    /// Get float register value for SHIL parameter
    static f32 getFloatRegValue(const shil_param& param);
    
    /// Set float register value for SHIL parameter
    static void setFloatRegValue(const shil_param& param, f32 value);
    
    /// Get 64-bit register value for SHIL parameter
    static u64 getReg64Value(const shil_param& param);
    
    /// Set 64-bit register value for SHIL parameter
    static void setReg64Value(const shil_param& param, u64 value);
    
    /// Get pointer for SHIL parameter
    static void* getPointer(const shil_param& param);
    
private:
    /// Handle memory read operations
    static u32 handleMemoryRead(const shil_param& addr, u32 size);
    
    /// Handle memory write operations  
    static void handleMemoryWrite(const shil_param& addr, const shil_param& value, u32 size);
    
    /// Handle interpreter fallback (shop_ifb)
    static void handleInterpreterFallback(const shil_opcode& op);
    
    /// Handle dynamic jumps (shop_jdyn)
    static void handleDynamicJump(const shil_opcode& op);
    
    /// Handle conditional jumps (shop_jcond)
    static void handleConditionalJump(const shil_opcode& op);
    
    /// Handle illegal instructions (shop_illegal)
    static void handleIllegalInstruction(const shil_opcode& op);
    
    /// LLVM JIT: Fast interpreter fallback
    static void execute_block_interpreter_fast(RuntimeBlockInfo* block);
    
    /// LLVM JIT: Ultra-fast operation execution
    static void execute_shil_operation_ultra_fast(const shil_opcode& op);
};

/// Simple flag to enable SHIL interpretation mode
extern bool enable_shil_interpreter;

/// Initialize SHIL interpreter setting from config
void init_shil_interpreter_setting();

/// SHIL interpretation mainloop
void shil_interpreter_mainloop(void* v_cntx);

/// SHIL cache management functions
void shil_interpreter_clear_cache();
void shil_interpreter_print_stats();

/// SHIL cache-friendly wrapper functions
bool shil_should_clear_cache_on_compile(u32 pc, u32 free_space);
DynarecCodeEntryPtr shil_handle_block_check_fail(u32 addr);
void shil_print_block_check_stats(); 