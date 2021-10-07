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

static void ExecuteOpcode(u16 op)
{
	if (sr.FD == 1 && OpDesc[op]->IsFloatingPoint())
		RaiseFPUDisableException();
	OpPtr[op](op);
	p_sh4rcb->cntx.cycle_counter -= CPU_RATIO;
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

	try {
		do
		{
			try {
				do
				{
					u32 op = ReadNexOp();

					ExecuteOpcode(op);
				} while (p_sh4rcb->cntx.cycle_counter > 0);
				p_sh4rcb->cntx.cycle_counter += SH4_TIMESLICE;
				UpdateSystem_INTC();
			} catch (const SH4ThrownException& ex) {
				Do_Exception(ex.epc, ex.expEvn, ex.callVect);
				p_sh4rcb->cntx.cycle_counter -= CPU_RATIO * 5;	// an exception requires the instruction pipeline to drain, so approx 5 cycles
			}
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
		u32 op = ReadNexOp();
		ExecuteOpcode(op);
	} catch (const SH4ThrownException& ex) {
		Do_Exception(ex.epc, ex.expEvn, ex.callVect);
		p_sh4rcb->cntx.cycle_counter -= CPU_RATIO * 5;	// an exception requires the instruction pipeline to drain, so approx 5 cycles
	} catch (const debugger::Stop& e) {
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
	old_fpscr=fpscr;
	UpdateFPSCR();
	icache.Reset(hard);
	ocache.Reset(hard);
	p_sh4rcb->cntx.cycle_counter = SH4_TIMESLICE;

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
		ExecuteDelayslot();
	} catch (const SH4ThrownException& ex) {
		throw FlycastException("Fatal: SH4 exception in RTE delay slot");
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
