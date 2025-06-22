#include "types.h"
#include "hw/mem/addrspace.h"


// Stub implementations for blockmanager functions when using IR interpreter without dynarec
// These are minimal implementations to satisfy the linker when TARGET_NO_REC/NO_JIT are defined

// From blockmanager.h/cpp
void bm_LockPage(u32 addr, u32 size)
{
    // No-op in IR-only mode
}

void bm_UnlockPage(u32 addr, u32 size)
{
    // No-op in IR-only mode
}

u32 bm_getRamOffset(void *p)
{
    // Calculate offset from RAM base
    u8 *ram_base = addrspace::ram_base;
    return (u32)((u8*)p - ram_base);
}

bool bm_RamWriteAccess(void *p)
{
    // In IR mode, all RAM writes are allowed
    return true;
}

void bm_RamWriteAccess(u32 addr)
{
    // No-op in IR-only mode
}

// From addrspace.h/cpp
namespace addrspace {
    bool bm_lockedWrite(u8* address)
    {
        // In IR mode, all writes are allowed
        return true;
    }
}

// From decoder_opcodes.h and decoder.cpp
void dec_illegalOp(u32 op)
{
    // Simple implementation for illegal opcodes in IR mode
    printf("IR: Illegal opcode %08x\n", op);
}

// ---------------------------------------------------------------------------
// Additional stub implementations required by SSAOptimizer when dynarec is
// disabled (FEAT_SHREC == DYNAREC_NONE / NO_JIT / TARGET_NO_REC).
// These functions are normally provided by dynarec-related translation units
// but are only needed at link-time to satisfy references. They should never be
// executed in IR-only builds, so they contain minimal no-op behaviour.
// ---------------------------------------------------------------------------

