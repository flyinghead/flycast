/*
	Optimized SH4 interpreter for better performance
*/

#include "types.h"

#include "../sh4_interpreter.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "../sh4_interrupts.h"
#include "hw/sh4/sh4_mem.h"
#include "../sh4_sched.h"
#include "../sh4_cache.h"
#include "debug/gdb_server.h"
#include "../sh4_cycles.h"

// SH4 underclock factor when using the interpreter so that it's somewhat usable
#ifdef STRICT_MODE
constexpr int CPU_RATIO = 1;
#else
constexpr int CPU_RATIO = 8;
#endif

Sh4ICache icache;
Sh4OCache ocache;

// === SIMPLE INSTRUCTION CACHE FOR BETTER PERFORMANCE ===
#define SIMPLE_ICACHE_SIZE 1024
#define SIMPLE_ICACHE_MASK (SIMPLE_ICACHE_SIZE - 1)

struct SimpleInstructionCache {
    u32 pc[SIMPLE_ICACHE_SIZE];
    u16 opcode[SIMPLE_ICACHE_SIZE];
    
    void reset() {
        for (int i = 0; i < SIMPLE_ICACHE_SIZE; i++) {
            pc[i] = 0xFFFFFFFF;
        }
    }
    
    u16 fetch(u32 addr) {
        u32 index = (addr >> 1) & SIMPLE_ICACHE_MASK;
        
        if (__builtin_expect(pc[index] == addr, 1)) {
            return opcode[index];
        }
        
        // Cache miss - fetch from memory
        u16 op = IReadMem16(addr);
        pc[index] = addr;
        opcode[index] = op;
        return op;
    }
};

static SimpleInstructionCache g_simple_icache;

static inline void ExecuteOpcode(u16 op)
{
	if (__builtin_expect(sr.FD == 1 && OpDesc[op]->IsFloatingPoint(), 0))
		RaiseFPUDisableException();
	OpPtr[op](op);
	sh4cycles.executeCycles(op);
}

static inline u16 ReadNexOp()
{
	if (__builtin_expect(!mmu_enabled() && (next_pc & 1), 0))
		// address error
		throw SH4ThrownException(next_pc, Sh4Ex_AddressErrorRead);

	u32 addr = next_pc;
	next_pc += 2;

	// Use simple instruction cache for better performance
	return g_simple_icache.fetch(addr);
}

static void Sh4_int_Run()
{
	RestoreHostRoundingMode();

	// Reset instruction cache at start
	g_simple_icache.reset();

	try {
		do
		{
			try {
				// Optimized inner loop with minimal overhead
				do
				{
					u32 op = ReadNexOp();
					ExecuteOpcode(op);
				} while (__builtin_expect(p_sh4rcb->cntx.cycle_counter > 0, 1));
				
				p_sh4rcb->cntx.cycle_counter += SH4_TIMESLICE;
				UpdateSystem_INTC();
			} catch (const SH4ThrownException& ex) {
				Do_Exception(ex.epc, ex.expEvn);
				// an exception requires the instruction pipeline to drain, so approx 5 cycles
				sh4cycles.addCycles(5 * CPU_RATIO);
			}
		} while (__builtin_expect(sh4_int_bCpuRun, 1));
	} catch (const debugger::Stop&) {
	}

	sh4_int_bCpuRun = false;
}

static void Sh4_int_Start()
{
	sh4_int_bCpuRun = true;
}

static void Sh4_int_Stop()
{
	sh4_int_bCpuRun = false;
}

void Sh4_int_Step()
{
	verify(!sh4_int_bCpuRun);

	RestoreHostRoundingMode();
	try {
		u32 op = ReadNexOp();
		ExecuteOpcode(op);
	} catch (const SH4ThrownException& ex) {
		Do_Exception(ex.epc, ex.expEvn);
		// an exception requires the instruction pipeline to drain, so approx 5 cycles
		sh4cycles.addCycles(5 * CPU_RATIO);
	} catch (const debugger::Stop&) {
	}
}

static void Sh4_int_Reset(bool hard)
{
	verify(!sh4_int_bCpuRun);

	if (hard)
	{
		int schedNext = p_sh4rcb->cntx.sh4_sched_next;
		memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));
		p_sh4rcb->cntx.sh4_sched_next = schedNext;
	}
	next_pc = 0xA0000000;

	memset(r,0,sizeof(r));
	memset(r_bank,0,sizeof(r_bank));

	gbr=ssr=spc=sgr=dbr=vbr=0;
	mac.full=pr=fpul=0;

	sh4_sr_SetFull(0x700000F0);
	old_sr.status=sr.status;
	UpdateSR();

	fpscr.full = 0x00040001;
	old_fpscr = fpscr;

	icache.Reset(hard);
	ocache.Reset(hard);
	sh4cycles.reset();
	p_sh4rcb->cntx.cycle_counter = SH4_TIMESLICE;
	
	// Reset simple instruction cache
	g_simple_icache.reset();

	INFO_LOG(INTERPRETER, "Sh4 Reset");
}

static bool Sh4_int_IsCpuRunning()
{
	return sh4_int_bCpuRun;
}

//TODO : Check for valid delayslot instruction
void ExecuteDelayslot()
{
	try {
		u32 op = ReadNexOp();

		ExecuteOpcode(op);
	} catch (SH4ThrownException& ex) {
		AdjustDelaySlotException(ex);
		throw ex;
	} catch (const debugger::Stop& e) {
		next_pc -= 2;	// break on previous instruction
		throw e;
	}
}

void ExecuteDelayslot_RTE()
{
	try {
		// In an RTE delay slot, status register (SR) bits are referenced as follows.
		// In instruction access, the MD bit is used before modification, and in data access,
		// the MD bit is accessed after modification.
		// The other bits—S, T, M, Q, FD, BL, and RB—after modification are used for delay slot
		// instruction execution. The STC and STC.L SR instructions access all SR bits after modification.
		u32 op = ReadNexOp();
		// Now restore all SR bits
		sh4_sr_SetFull(ssr);
		// And execute
		ExecuteOpcode(op);
	} catch (const SH4ThrownException&) {
		throw FlycastException("Fatal: SH4 exception in RTE delay slot");
	} catch (const debugger::Stop& e) {
		next_pc -= 2;	// break on previous instruction
		throw e;
	}
}

// every SH4_TIMESLICE cycles
int UpdateSystem()
{
	Sh4cntx.sh4_sched_next -= SH4_TIMESLICE;
	if (Sh4cntx.sh4_sched_next < 0)
		sh4_sched_tick(SH4_TIMESLICE);

	return Sh4cntx.interrupt_pend;
}

int UpdateSystem_INTC()
{
	if (UpdateSystem())
		return UpdateINTC();
	else
		return 0;
}

static void sh4_int_resetcache() {
	g_simple_icache.reset();
}

static void Sh4_int_Init()
{
	static_assert(sizeof(Sh4cntx) == 448, "Invalid Sh4Cntx size");

	memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));
	g_simple_icache.reset();
}

static void Sh4_int_Term()
{
	Sh4_int_Stop();
	INFO_LOG(INTERPRETER, "Sh4 Term");
}

#ifndef ENABLE_SH4_CACHED_IR
void Get_Sh4Interpreter(sh4_if* cpu)
{
    INFO_LOG(INTERPRETER, "🚀 OPTIMIZED-INTERPRETER: Get_Sh4Interpreter called — linking optimized interpreter!");
    cpu->Start = Sh4_int_Start;
    cpu->Run = Sh4_int_Run;
    cpu->Stop = Sh4_int_Stop;
    cpu->Step = Sh4_int_Step;
    cpu->Reset = Sh4_int_Reset;
    cpu->Init = Sh4_int_Init;
    cpu->Term = Sh4_int_Term;
    cpu->IsCpuRunning = Sh4_int_IsCpuRunning;
    cpu->ResetCache = sh4_int_resetcache;
}
#endif // ENABLE_SH4_CACHED_IR
