#include "sh4_ir_interpreter.h"
#include "hw/sh4/sh4_if.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/sh4_core.h" // for SH4ThrownException
#include "log/Log.h"
#include "hw/sh4/sh4_interpreter.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/sh4/modules/mmu.h"
#include "hw/sh4/sh4_mem.h" // for memory function pointers
#include "hw/mem/addrspace.h" // for ram_base fast access
#include "hw/flashrom/nvmem.h" // for BIOS pointer
#include "hw/sh4/sh4_cycles.h" // for cycle counting
#include "debug/gdb_server.h" // for debugger stop handling

// #define SH4_FAST_SKIP 1

namespace sh4 {
namespace ir {

// Global instance referenced by executor for cache invalidation
Sh4IrInterpreter g_ir;

// SH4 underclock factor when using the interpreter so that it's somewhat usable
#ifdef STRICT_MODE
constexpr int CPU_RATIO = 1;
#else
constexpr int CPU_RATIO = 8;
#endif

Sh4IrInterpreter::Sh4IrInterpreter()
    : ctx_(nullptr)
{
    // ctx_ will be bound during Init() once p_sh4rcb has been created by the core.
}

void Sh4IrInterpreter::Init()
{
    printf("[IR][Init][ENTRY] ctx_=%p\n", (void*)ctx_);

    // Bind context lazily the first time Init() is called – at this point the
    // global p_sh4rcb should have been allocated by Emulator::init().
    if (!ctx_)
    {
        if (!p_sh4rcb)
        {
            ERROR_LOG(SH4, "p_sh4rcb is null during IR_Init – sh4 core not initialised yet");
            return;
        }
        ctx_ = &p_sh4rcb->cntx;
        printf("[IR][Init][POST BIND] ctx_=%p r[0]=%08X r[1]=%08X r[2]=%08X r[3]=%08X sr.T=%u\n",
            (void*)ctx_, r[0], r[1], r[2], r[3], sr.T);

    }

    // Clear the context like the legacy interpreter
    memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));

    emitter_.ClearCaches();
}

void Sh4IrInterpreter::Reset(bool hard)
{
    printf("[IR][Reset][ENTRY] ctx_=%p r[0]=%08X r[1]=%08X r[2]=%08X r[3]=%08X sr.T=%u\n",
        (void*)ctx_, ctx_ ? r[0] : 0, ctx_ ? r[1] : 0, ctx_ ? r[2] : 0, ctx_ ? r[3] : 0, ctx_ ? sr.T : 0);

if (hard)
{
    int schedNext = ctx_->sh4_sched_next;
    memset(ctx_, 0, sizeof(*ctx_));
    ctx_->sh4_sched_next = schedNext;
}

// Set PC to reset vector
ctx_->pc = 0xA0000000;
next_pc = 0xA0000000;  // Keep legacy global in sync

// Clear registers
memset(&r[0], 0, sizeof(r));
memset(r_bank, 0, sizeof(r_bank));  // Use global r_bank

// Clear other registers
gbr = 0;
ssr = 0;
spc = 0;
sgr = 0;
dbr = 0;
mac.full = 0;
pr = 0;
fpul = 0;
vbr = 0;

// Set SR (MD=1, RB=0, BL=0, IMASK=0xF)
sh4_sr_SetFull(0x700000F0);
old_sr.status = sr.status;
UpdateSR();                  // Add this!

// FP status register
fpscr.full = 0x00040001;
old_fpscr.full = fpscr.full;

// Reset MMU
CCN_MMUCR.reg_data = 0;
memset(UTLB, 0, sizeof(UTLB));
memset(ITLB, 0, sizeof(ITLB));

// Initialize cycle counter
ctx_->cycle_counter = SH4_TIMESLICE;

// Reset cycle counter
sh4cycles.reset();

// Clear caches
emitter_.ClearCaches();
executor_.ResetCachedBlocks();

}

#ifdef SH4_FAST_SKIP
// Helper similar to Executor::FastRamPtr (local copy for interpreter)
static inline u8* FastPtr(uint32_t addr)
{
    // BIOS ROM (2 MiB) and mirrors (include P0 0x4000 0000 for ITLB-miss handler)
    if ((addr & 0xFFE00000u) == 0x00000000u ||
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
#endif // SH4_FAST_SKIP

void Sh4IrInterpreter::Run()
{
    RestoreHostRoundingMode();
    running_ = true;

    try {
        do
        {
            try {
                do
                {
                    // CRITICAL: Sync local context PC with global next_pc before fetching blocks
                    // This fixes the corruption where JSR updates next_pc but ctx_->pc remains stale
                    ctx_->pc = next_pc;
                    uint32_t pc_val = ctx_->pc;
                    g_exception_was_raised = false;

                    try
                    {
                        const Block* blk = emitter_.BuildBlock(pc_val);
                        executor_.ExecuteBlock(blk, ctx_);

                        if (g_exception_was_raised)      // exception flag set by helpers
                            continue;                    // PC already updated by Do_Exception

                        if (ctx_->pc == pc_val)          // sequential advance when block ended
                            ctx_->pc = blk->pcNext;
                    }
                    catch (const SH4ThrownException&)    // any legacy throw -> flag already set
                    {
                        continue;
                    }
                } while (ctx_->cycle_counter > 0);
                ctx_->cycle_counter += SH4_TIMESLICE;
                UpdateSystem_INTC();
            } catch (const SH4ThrownException& ex) {
                Do_Exception(ex.epc, ex.expEvn);
                // an exception requires the instruction pipeline to drain, so approx 5 cycles
                sh4cycles.addCycles(5 * CPU_RATIO);
            }
        } while (running_);
    } catch (const debugger::Stop&) {
    }

    running_ = false;
}

void Sh4IrInterpreter::Step()
{
    printf("[IR][Step][ENTRY] ctx_=%p r[0]=%08X r[1]=%08X r[2]=%08X r[3]=%08X sr.T=%u\n",
        (void*)ctx_, ctx_ ? r[0] : 0, ctx_ ? r[1] : 0, ctx_ ? r[2] : 0, ctx_ ? r[3] : 0, ctx_ ? sr.T : 0);

    printf("[PRINTF_DEBUG_IR_STEP_ENTRY] Sh4IrInterpreter::Step() entered.\n");
    //fflush(stdout); // Temporarily removed for testing crash output behavior

    RestoreHostRoundingMode();

    // CRITICAL: Sync local context PC with global next_pc before fetching blocks
    // This fixes the corruption where JSR updates next_pc but ctx_->pc remains stale
    ctx_->pc = next_pc;
    uint32_t pc_val = ctx_->pc;
    uint32_t old_pc = pc_val;

    try {
        // Add debug logging to track opcode execution
        u16 opcode = IReadMem16(pc_val);
        printf("[IR][Step] Reading opcode at PC=%08X: %04X\n", pc_val, opcode);

        // Check for FPU disable before executing floating point instructions
        // Temporarily undefine sr macro to avoid expansion conflicts
        if (sr.FD == 1) {
            // Check if this is a floating point instruction
            // This is a simplified check - the IR executor should handle this properly
            // but we need to ensure the check happens
        }
        // Restore sr macro

        // Special handling for ADDC test case with r[2]=0xFFFFFFFF and r[3]=1
        // Temporarily undefine r macro to avoid expansion conflicts
        if (opcode == 0x323E && r[2] == 0xFFFFFFFF && r[3] == 1) {
            printf("[IR][Step] Detected critical ADDC test case with r[2]=0xFFFFFFFF and r[3]=1\n");
        }

        const Block* blk = emitter_.BuildBlock(pc_val);
        printf("[IR][Step] Block built, executing with %zu instructions\n", blk->code.size());

        // Debug: Print the first instruction in the block
        if (blk->code.size() > 0) {
            printf("[IR][Step] First instruction: op=%d\n", static_cast<int>(blk->code[0].op));
        }

        executor_.ExecuteBlock(blk, ctx_);

        if (ctx_->pc == old_pc)
             ctx_->pc = blk->pcNext;
#ifdef SH4_FAST_SKIP
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
                    u16 iw = IReadMem16(pc_scan);
                    if (iw != 0x0000 && iw != 0x0009) break;
                    pc_scan += 2;
                }
                ctx_->pc = pc_scan;
            }
        }
#endif // SH4_FAST_SKIP
        // Restore pc macro for rest of code
    } catch (const SH4ThrownException& ex) {
        Do_Exception(ex.epc, ex.expEvn);
        // an exception requires the instruction pipeline to drain, so approx 5 cycles
        sh4cycles.addCycles(5 * CPU_RATIO);
    // After taking an exception the global core has set `next_pc` to the
    // exception vector. Propagate that into the local context so that the IR
    // interpreter begins execution from the correct address on the next Step.
    ctx_->pc = next_pc;
    } catch (const debugger::Stop&) {
        // Handle debugger stop
    }

    printf("[IR][Step][EXIT] ctx_=%p r[0]=%08X r[1]=%08X r[2]=%08X r[3]=%08X sr.T=%u\n",
        (void*)ctx_, ctx_ ? r[0] : 0, ctx_ ? r[1] : 0, ctx_ ? r[2] : 0, ctx_ ? r[3] : 0, ctx_ ? sr.T : 0);

}

// ResetCache implementation moved to header

} // namespace ir
} // namespace sh4


// -----------------------------------------------------------------------------
// Legacy sh4_if wrappers for the new IR-based interpreter
// -----------------------------------------------------------------------------

namespace {
    // Single global instance used by the C-style callback wrappers below.
    // use global g_ir defined above

    void IR_Start()                { sh4::ir::g_ir.Start(); }
    void IR_Run()                  { sh4::ir::g_ir.Run(); }
    void IR_Stop()                 { sh4::ir::g_ir.Stop(); }
    void IR_Step()                 { sh4::ir::g_ir.Step(); }
    void IR_Reset(bool hard)       { sh4::ir::g_ir.Reset(hard); }
    void IR_Init()                 { sh4::ir::g_ir.Init(); }
    void IR_Term()                 { sh4::ir::g_ir.Term(); }
    void IR_ResetCache()           { sh4::ir::g_ir.ResetCache(); }
    bool IR_IsCpuRunning()         { return sh4::ir::g_ir.IsCpuRunning(); }
}

// Expose a function with the same signature the core expects, but inside the
// sh4::ir namespace. This populates the provided `sh4_if` structure with
// pointers to the wrapper functions above, effectively plumbing the modern IR
// interpreter into the legacy C callback interface.
void sh4::ir::Get_Sh4Interpreter(sh4_if* cpu)
{
    cpu->Start         = IR_Start;
    cpu->Run           = IR_Run;
    cpu->Stop          = IR_Stop;
    cpu->Step          = IR_Step;
    cpu->Reset         = IR_Reset;
    cpu->Init          = IR_Init;
    cpu->Term          = IR_Term;
    cpu->ResetCache    = IR_ResetCache;
    cpu->IsCpuRunning  = IR_IsCpuRunning;
}

// Provide the legacy symbol expected by existing code/tests.



void sh4::ir::Sh4IrInterpreter::InvalidateBlock(u32 addr)
{
    // Simple implementation: reset both emitter and executor caches for any write to code memory
    // This is inefficient but guarantees correctness for self-modifying code
    INFO_LOG(SH4, "Invalidating block at address 0x%08X", addr);

    // Clear emitter caches
    emitter_.ClearCaches();

    // Reset executor state to force re-fetching blocks
    // This ensures any stale block pointers are discarded

    // TODO: Is this needed?
    executor_.ResetCachedBlocks();
}
