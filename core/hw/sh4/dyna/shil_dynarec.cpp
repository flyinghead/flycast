#include "shil_dynarec.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_mem.h"
#include "oslib/oslib.h"
#include "shil_interpreter.h"

// Forward declarations for cache-friendly functions
extern "C" void CacheFriendlyShil_on_block_compiled();
extern "C" void shil_print_block_check_stats_wrapper();

// Simple stub function that will be used as the "compiled" code pointer
static void shil_interpreter_stub() {
    // This function should never actually be called
    // The mainloop will detect SHIL blocks and call the interpreter directly
}

void ShilDynarec::init(Sh4CodeBuffer& buffer) {
    this->codeBuffer = &buffer;
}

void ShilDynarec::compile(RuntimeBlockInfo* block, bool smc_checks, bool optimise) {
    // For SHIL interpreter, we don't actually compile anything
    // We just set the code pointer to our stub function
    // The real magic happens in mainloop() which detects this and calls the interpreter
    
    block->code = (DynarecCodeEntryPtr)shil_interpreter_stub;
    block->host_code_size = 4; // Minimal size
    block->host_opcodes = block->oplist.size(); // Number of SHIL opcodes
}

void ShilDynarec::executeShilBlock(RuntimeBlockInfo* block) {
    ShilInterpreter::executeBlock(block);
}

void ShilDynarec::mainloop(void* cntx) {
    // Set up context
    p_sh4rcb = (Sh4RCB*)((u8*)cntx - sizeof(Sh4cntx));
    
    try {
        while (sh4_int_bCpuRun) {
            // Check for exceptions first
            if (UpdateSystem()) {
                // Exception occurred, continue with exception PC
                continue;
            }
            
            // **CACHE-FRIENDLY BLOCK LOOKUP**: Avoid using rdv_FindOrCompile which can trigger cache clears
            RuntimeBlockInfoPtr block = bm_GetBlock(next_pc);
            
            if (block) {
                // Block exists - execute it via SHIL interpreter
                executeShilBlock(block.get());
                
                // After executing the block, determine next PC based on block type
                switch (BET_GET_CLS(block->BlockType)) {
                    case BET_CLS_Static:
                        if (block->BlockType == BET_StaticIntr) {
                            next_pc = block->NextBlock;
                        } else {
                            next_pc = block->BranchBlock;
                        }
                        break;
                        
                    case BET_CLS_Dynamic:
                        // PC should have been set by the block execution
                        next_pc = Sh4cntx.pc;
                        break;
                        
                    case BET_CLS_COND:
                        // Conditional branch - check the condition
                        if (sr.T) {
                            next_pc = block->BranchBlock;
                        } else {
                            next_pc = block->NextBlock;
                        }
                        break;
                }
            } else {
                // **CACHE-FRIENDLY BLOCK COMPILATION**: Don't use rdv_FindOrCompile
                // Compile new block using cache-friendly approach
                
                RuntimeBlockInfo* new_block = allocateBlock();
                if (new_block->Setup(next_pc, fpscr)) {
                    // Compile the block for SHIL interpretation
                    compile(new_block, !new_block->read_only, true);
                    
                    // Add to block manager
                    bm_AddBlock(new_block);
                    
                    // Track compilation for cache management
                    CacheFriendlyShil_on_block_compiled();
                    
                    // Execute the newly compiled block
                    executeShilBlock(new_block);
                    
                    // Update PC based on block type
                    switch (BET_GET_CLS(new_block->BlockType)) {
                        case BET_CLS_Static:
                            if (new_block->BlockType == BET_StaticIntr) {
                                next_pc = new_block->NextBlock;
                            } else {
                                next_pc = new_block->BranchBlock;
                            }
                            break;
                            
                        case BET_CLS_Dynamic:
                            next_pc = Sh4cntx.pc;
                            break;
                            
                        case BET_CLS_COND:
                            if (sr.T) {
                                next_pc = new_block->BranchBlock;
                            } else {
                                next_pc = new_block->NextBlock;
                            }
                            break;
                    }
                } else {
                    // Block setup failed - this shouldn't happen often
                    ERROR_LOG(DYNAREC, "SHIL: Block setup failed for PC %08X", next_pc);
                    delete new_block;
                    break;
                }
            }
            
            // Print statistics periodically
            static u32 stats_counter = 0;
            if (++stats_counter % 50000 == 0) {
                shil_interpreter_print_stats();
                shil_print_block_check_stats_wrapper();
            }
        }
    } catch (const SH4ThrownException& ex) {
        // Handle SH4 exceptions
        Do_Exception(next_pc, ex.expEvn);
    } catch (const std::exception& ex) {
        // Handle other exceptions
        ERROR_LOG(DYNAREC, "Exception in SHIL mainloop: %s", ex.what());
        sh4_int_bCpuRun = false;
    }
}

void ShilDynarec::handleException(host_context_t& context) {
    // For the interpreter, we don't need to do anything special
    // Exceptions are handled normally in the mainloop
}

bool ShilDynarec::rewrite(host_context_t& context, void *faultAddress) {
    // Memory rewriting is not needed for the interpreter
    return false;
} 