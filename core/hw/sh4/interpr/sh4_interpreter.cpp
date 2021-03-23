/*
	Highly inefficient and boring interpreter. Nothing special here
*/

#include "types.h"

#include "../sh4_interpreter.h"
#include "../sh4_opcode_list.h"
#include "../sh4_core.h"
#include "../sh4_interrupts.h"
#include "hw/sh4/sh4_mem.h"
#include "../sh4_sched.h"
#include "hw/holly/sb.h"
#include "../sh4_cache.h"
#include "debug/gdb_server.h"

#define CPU_RATIO      (8)

sh4_icache icache;
sh4_ocache ocache;

static s32 l;

static void ExecuteOpcode(u16 op)
{
	if (sr.FD == 1 && OpDesc[op]->IsFloatingPoint())
		RaiseFPUDisableException();
	OpPtr[op](op);
	l -= CPU_RATIO;
}

static u16 ReadNexOp()
{
	u32 addr = next_pc;
	next_pc += 2;

	return IReadMem16(addr);
}

static void Sh4_int_Run()
{
	sh4_int_bCpuRun = true;
	RestoreHostRoundingMode();

	l += SH4_TIMESLICE;

	try {
		do
		{
#if !defined(NO_MMU)
			try {
#endif
				do
				{
					u32 op = ReadNexOp();

					ExecuteOpcode(op);
				} while (l > 0);
				l += SH4_TIMESLICE;
				UpdateSystem_INTC();
#if !defined(NO_MMU)
			}
			catch (SH4ThrownException& ex) {
				Do_Exception(ex.epc, ex.expEvn, ex.callVect);
				l -= CPU_RATIO * 5;	// an exception requires the instruction pipeline to drain, so approx 5 cycles
			}
#endif
		} while (sh4_int_bCpuRun);
	} catch (const debugger::Stop& e) {
	}

	sh4_int_bCpuRun = false;
}

static void Sh4_int_Stop()
{
	sh4_int_bCpuRun = false;
}

static void Sh4_int_Step()
{
	verify(!sh4_int_bCpuRun);

	RestoreHostRoundingMode();
	try {
#if !defined(NO_MMU)
		try {
#endif
			u32 op = ReadNexOp();
			ExecuteOpcode(op);
#if !defined(NO_MMU)
		}
		catch (SH4ThrownException& ex) {
			Do_Exception(ex.epc, ex.expEvn, ex.callVect);
			l -= CPU_RATIO * 5;	// an exception requires the instruction pipeline to drain, so approx 5 cycles
		}
#endif
	} catch (const debugger::Stop& e) {
	}
}

static void Sh4_int_Reset(bool hard)
{
	verify(!sh4_int_bCpuRun);

	if (hard)
		memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));
	next_pc = 0xA0000000;

	memset(r,0,sizeof(r));
	memset(r_bank,0,sizeof(r_bank));

	gbr=ssr=spc=sgr=dbr=vbr=0;
	mac.full=pr=fpul=0;

	sh4_sr_SetFull(0x700000F0);
	old_sr.status=sr.status;
	UpdateSR();

	fpscr.full = 0x00040001;
	old_fpscr=fpscr;
	UpdateFPSCR();
	icache.Reset(hard);
	ocache.Reset(hard);

	INFO_LOG(INTERPRETER, "Sh4 Reset");
}

static bool Sh4_int_IsCpuRunning()
{
	return sh4_int_bCpuRun;
}

//TODO : Check for valid delayslot instruction
void ExecuteDelayslot()
{
#if !defined(NO_MMU)
	try {
#endif
		u32 op = ReadNexOp();

		ExecuteOpcode(op);
#if !defined(NO_MMU)
	}
	catch (SH4ThrownException& ex) {
		AdjustDelaySlotException(ex);
		throw ex;
	}
	catch (const debugger::Stop& e) {
		next_pc -= 2;	// break on previous instruction
		throw e;
	}
#endif
}

void ExecuteDelayslot_RTE()
{
#if !defined(NO_MMU)
	try {
#endif
		ExecuteDelayslot();
#if !defined(NO_MMU)
	}
	catch (SH4ThrownException& ex) {
		ERROR_LOG(INTERPRETER, "Exception in RTE delay slot");
	}
#endif
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
}

static void Sh4_int_Init()
{
	static_assert(sizeof(Sh4cntx) == 448, "Invalid Sh4Cntx size");

	memset(&p_sh4rcb->cntx, 0, sizeof(p_sh4rcb->cntx));
}

static void Sh4_int_Term()
{
	Sh4_int_Stop();
	INFO_LOG(INTERPRETER, "Sh4 Term");
}

void Get_Sh4Interpreter(sh4_if* cpu)
{
	cpu->Run = Sh4_int_Run;
	cpu->Stop = Sh4_int_Stop;
	cpu->Step = Sh4_int_Step;
	cpu->Reset = Sh4_int_Reset;
	cpu->Init = Sh4_int_Init;
	cpu->Term = Sh4_int_Term;
	cpu->IsCpuRunning = Sh4_int_IsCpuRunning;

	cpu->ResetCache = sh4_int_resetcache;
}
