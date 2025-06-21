#include "sh4_ir_interpreter.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h" // for SH4ThrownException
#include "log/Log.h"
#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/mem/addrspace.h" // for ram_base fast access
#include "hw/flashrom/nvmem.h" // for BIOS pointer

namespace sh4 {
namespace ir {

Sh4IrInterpreter::Sh4IrInterpreter()
{
    ctx_ = &p_sh4rcb->cntx; // assume global struct exists as in legacy interpreter
}

void Sh4IrInterpreter::Init()
{
    emitter_.ClearCaches();
    // Zero context similar to legacy init
    memset(ctx_, 0, sizeof(*ctx_));

    // Temporarily undefine macros
#undef vbr
#undef pc
    ctx_->vbr = 0x8C000000; // exception vectors
    ctx_->pc  = 0xA0000000; // BIOS entry point in P2
    // Restore macros
#define vbr Sh4cntx.vbr
#define pc next_pc
}

void Sh4IrInterpreter::Reset(bool /*hard*/)
{
    // Set PC to reset vector; reinitialise general regs and VBR
    // Temporarily undefine macros
#undef r
#undef vbr
#undef sr
#undef pc
    for (int i = 0; i < 16; ++i)
        ctx_->r[i] = 0;
    ctx_->vbr = 0x8C000000;
    ctx_->sr.T = 0;
    ctx_->sh4_sched_next = 0; // Reset scheduler/cycle count for IR
    ctx_->pc = 0xA0000000;     // Set PC to reset vector (0xA0000000 for BIOS)
    // Restore macros
#define r Sh4cntx.r
#define vbr Sh4cntx.vbr
#define sr Sh4cntx.sr
#define pc next_pc

    ResetCache();
}

// Helper similar to Executor::FastRamPtr (local copy for interpreter)
static inline u8* FastPtr(uint32_t addr)
{
    // BIOS ROM (2 MiB) and mirrors (include P0 0x4000 0000 for ITLB-miss handler)
    if ((addr & 0xFFE00000u) == 0x00000000u ||
        (addr & 0xFFE00000u) == 0x40000000u ||
        (addr & 0xFFE00000u) == 0x80000000u ||
        (addr & 0xFFE00000u) == 0xA0000000u ||
        (addr & 0xFFE00000u) == 0xC0000000u)
        return nvmem::getBiosData() + (addr & 0x001FFFFF);

    // Main RAM 0x0C000000–0x0FFFFFFF
    if ((addr & 0xFC000000u) == 0x0C000000u)
        return addrspace::ram_base + (addr & 0x03FFFFFF);

    // P4 SDRAM mirrors
    if (addr >= 0xF8000000u && addr < 0xFF000000u)
        return addrspace::ram_base + 0x0C000000 + (addr & 0x00FFFFFF);

    return nullptr;
}

void Sh4IrInterpreter::Run()
{
    running_ = true;
    while (running_)
    {
        // Access ctx_->pc directly without macro interference
#undef pc
        uint32_t pc_val = ctx_->pc;
        uint32_t old_pc = pc_val;
        // No need to restore pc macro here

        static uint64_t step_counter = 0;
        try {
            const Block* blk = emitter_.BuildBlock(pc_val);
            executor_.ExecuteBlock(blk, ctx_);

            // Access ctx_->pc directly without macro interference
#undef pc
            if (ctx_->pc == old_pc)
                ctx_->pc = blk->pcNext;
            if (blk->code.size() == 2 && blk->code[0].op == ir::Op::NOP)
            {
                // Fast-skip over large stretches of 0x0000 instructions that
                // the BIOS uses for memory clear stubs. We only do this when
                // we have a direct pointer into either RAM or the BIOS ROM –
                // this guarantees the memory is valid and avoids hiding real
                // mapping bugs.
                u32 pc_scan = ctx_->pc;
                // No need to restore pc macro here
                if (u8* base = FastPtr(pc_scan))
                {
                    const u16* w = reinterpret_cast<const u16*>(base);
                    while (*w == 0 || *w == 0x0009)
                    {
                        ++w;
                        pc_scan += 2;
                        if ((pc_scan - ctx_->pc) >= 0x100000)
                            break;
                    }
                    ctx_->pc = pc_scan;
                }
                else
                {
                    // No direct pointer; fall back to reading via MMU
                    const uint32_t limit = pc_scan + 0x100000; // 1 MiB max
                    while (pc_scan < limit)
                    {
                        u16 iw = mmu_IReadMem16(pc_scan);
                        if (iw != 0x0000 && iw != 0x0009) break;
                        pc_scan += 2;
                    }
                    ctx_->pc = pc_scan;
                }
            }
            ++step_counter;
            if ((step_counter & 0x1FFFF) == 0) // every 131072 blocks
            {
                INFO_LOG(SH4, "PC=%08X", ctx_->pc);
            }
#define pc next_pc
        } catch (const SH4ThrownException& ex) {
            // Access ctx fields directly without macro interference
#undef pc
#undef sr
            Do_Exception(ex.epc, ex.expEvn);
            // Restore macros for rest of code
#define pc next_pc
#define sr Sh4cntx.sr
        }
    }
}

void Sh4IrInterpreter::Step()
{
    printf("[PRINTF_DEBUG_IR_STEP_ENTRY] Sh4IrInterpreter::Step() entered.\n");
    //fflush(stdout); // Temporarily removed for testing crash output behavior

    // Access ctx_->pc directly without macro interference
#undef pc
    uint32_t pc_val = ctx_->pc;
    uint32_t old_pc = pc_val;

    try {
        const Block* blk = emitter_.BuildBlock(pc_val);
        executor_.ExecuteBlock(blk, ctx_);

        if (ctx_->pc == old_pc)
            ctx_->pc = blk->pcNext;
        if (blk->code.size() == 2 && blk->code[0].op == ir::Op::NOP)
        {
            u32 pc_scan = ctx_->pc;

            if (u8* base = FastPtr(pc_scan))
            {
                const u16* w = reinterpret_cast<const u16*>(base);
                while (*w == 0 || *w == 0x0009)
                {
                    ++w;
                    pc_scan += 2;
                    if ((pc_scan - ctx_->pc) >= 0x100000) {
                        break;
                    }
                }
                ctx_->pc = pc_scan;
            }
            else
            {
                const uint32_t limit = pc_scan + 0x100000;
                while (pc_scan < limit)
                {
                    u16 iw = mmu_IReadMem16(pc_scan);
                    if (iw != 0x0000 && iw != 0x0009) break;
                    pc_scan += 2;
                }
                ctx_->pc = pc_scan;
            }
        }
        // Restore pc macro for rest of code
#define pc next_pc
    } catch (const SH4ThrownException& ex) {
#undef pc
#undef sr
        Do_Exception(ex.epc, ex.expEvn);
#define pc next_pc
#define sr Sh4cntx.sr
    }
}

// ResetCache implementation moved to header

} // namespace ir
} // namespace sh4

#ifdef ENABLE_SH4_IR
Executor* Get_Sh4Interpreter()
{
    fprintf(stderr, "[DEBUG_PRINTF] IR Get_Sh4Interpreter() called THE NEW ONE\n");
    printf("[DEBUG_PRINTF] IR Get_Sh4Interpreter() called. THE NEW ONE\n");
    fflush(stderr);
    return new sh4::ir::Sh4IrInterpreter();
}

void sh4::ir::Sh4IrInterpreter::InvalidateBlock(u32 addr)
{
    // Simple implementation: reset both emitter and executor caches for any write to code memory
    // This is inefficient but guarantees correctness for self-modifying code
    INFO_LOG(SH4, "Invalidating block at address 0x%08X", addr);

    // Clear emitter caches
    emitter_.ClearCaches();

    // Reset executor state to force re-fetching blocks
    // This ensures any stale block pointers are discarded
    executor_.ResetCachedBlocks();
}
#endif // SH4_IR_ENABLED
