#include "shil_dynarec.h"
#include "hw/sh4/sh4_core.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_mem.h"
#include "oslib/oslib.h"

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
            
            // Get the next block to execute
            DynarecCodeEntryPtr code_ptr = rdv_FindOrCompile();
            
            if (code_ptr == ngen_FailedToFindBlock) {
                code_ptr = rdv_FailedToFindBlock(next_pc);
            }
            
            // Check if this is a SHIL interpreter block
            if (code_ptr == (DynarecCodeEntryPtr)shil_interpreter_stub) {
                // This is a SHIL block - execute via interpreter
                RuntimeBlockInfoPtr block = bm_GetBlock(next_pc);
                if (block) {
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
                    // Block not found - this shouldn't happen
                    ERROR_LOG(DYNAREC, "SHIL block not found for PC %08X", next_pc);
                    break;
                }
            } else {
                // This is a regular JIT block - this shouldn't happen in SHIL mode
                ERROR_LOG(DYNAREC, "Unexpected JIT block in SHIL mode at PC %08X", next_pc);
                break;
            }
        }
    } catch (const SH4ThrownException& ex) {
        // Handle SH4 exceptions
        Do_Exception(next_pc, ex.expEvn);
    } catch (const FlycastException& ex) {
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