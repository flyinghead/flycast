// Stubs for dynarec-only symbols when the cached-IR interpreter is built with
// FEAT_SHREC == DYNAREC_NONE. These satisfy link dependencies that originate
// from legacy code paths (bm_getRamOffsetch, SSA optimiser, etc.) that are still
// compiled in non-dynarec builds.
//
// These functions intentionally do nothing – they should never be called when
// the dynamic recompiler is disabled. If one does get executed we abort so the
// bug is caught immediately.

#include <cstdio>
#include <cstdlib>
#include <cstdint>



struct RuntimeBlockInfo; // forward declaration for signatures
class Sh4Dynarec; // forward declaration of dynarec class

// Helper that terminates if a dynarec-only symbol is ever executed
static inline void unreachable(const char* sym)
{
    fprintf(stderr,
            "ERROR: dynarec stub %s was invoked while FEAT_SHREC == DYNAREC_NONE\n",
            sym);
    std::abort();
}

// Global flag referenced by some SHIL helpers
// Global flags/objects normally supplied by dynarec
int _sh4Dynarec = 0;
Sh4Dynarec* sh4Dynarec = nullptr;

// --------------------- SSA-optimiser helper stubs ---------------------------
bool rdv_readMemImmediate(uint32_t /*addr*/, int /*size*/, void*& /*ptr*/, bool& /*isRam*/,
                          uint32_t& /*physAddr*/, RuntimeBlockInfo* /*block*/)
{
    unreachable("rdv_readMemImmediate");
    return false;
}

bool rdv_writeMemImmediate(uint32_t /*addr*/, int /*size*/, void*& /*ptr*/, bool& /*isRam*/,
                           uint32_t& /*physAddr*/, RuntimeBlockInfo* /*block*/)
{
    unreachable("rdv_writeMemImmediate");
    return false;
}

void dec_updateBlockCycles(RuntimeBlockInfo* /*bi*/, unsigned short /*cycles*/)
{
    unreachable("dec_updateBlockCycles");
}


