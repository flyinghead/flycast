#pragma once

#include "ngen.h"
#include "shil_interpreter.h"

/// A dynarec backend that interprets SHIL instead of compiling to native code
class ShilDynarec : public Sh4Dynarec {
private:
    Sh4CodeBuffer* codeBuffer = nullptr;
    
public:
    // Initialize the dynarec
    void init(Sh4CodeBuffer& codeBuffer) override;
    
    // "Compile" the block by storing it for interpretation
    void compile(RuntimeBlockInfo* block, bool smc_checks, bool optimise) override;
    
    // Run the main execution loop
    void mainloop(void* cntx) override;
    
    // Handle exceptions
    void handleException(host_context_t& context) override;
    
    // Memory rewrite (not needed for interpreter)
    bool rewrite(host_context_t& context, void *faultAddress) override;
    
    // Canonical implementation callbacks (not used in interpreter mode)
    void canonStart(const shil_opcode *op) override {}
    void canonParam(const shil_opcode *op, const shil_param *param, CanonicalParamType paramType) override {}
    void canonCall(const shil_opcode *op, void *function) override {}
    void canonFinish(const shil_opcode *op) override {}
    
private:
    /// Execute a SHIL block via interpretation
    static void executeShilBlock(RuntimeBlockInfo* block);
}; 