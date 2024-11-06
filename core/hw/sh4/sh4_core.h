#pragma once
#include "types.h"
#include "sh4_if.h"

#define r Sh4cntx.r
#define r_bank Sh4cntx.r_bank
#define gbr Sh4cntx.gbr
#define ssr Sh4cntx.ssr
#define spc Sh4cntx.spc
#define sgr Sh4cntx.sgr
#define dbr Sh4cntx.dbr
#define vbr Sh4cntx.vbr
#define mac Sh4cntx.mac
#define pr Sh4cntx.pr
#define fpul Sh4cntx.fpul
#define next_pc Sh4cntx.pc
#define curr_pc (next_pc-2)
#define sr Sh4cntx.sr
#define fpscr Sh4cntx.fpscr
#define old_sr Sh4cntx.old_sr
#define old_fpscr Sh4cntx.old_fpscr
#define fr (&Sh4cntx.xffr[16])
#define xf Sh4cntx.xffr
#define fr_hex ((u32*)fr)
#define xf_hex ((u32*)xf)
#define dr_hex ((u64*)fr)
#define xd_hex ((u64*)xf)
#define sh4_int_bCpuRun Sh4cntx.CpuRunning

void UpdateFPSCR();
bool UpdateSR();
void RestoreHostRoundingMode();
void setDefaultRoundingMode();

struct SH4ThrownException
{
	SH4ThrownException(u32 epc, Sh4ExceptionCode expEvn) : epc(epc), expEvn(expEvn) { }

	u32 epc;
	Sh4ExceptionCode expEvn;
};

static inline void RaiseFPUDisableException()
{
	throw SH4ThrownException(next_pc - 2, Sh4Ex_FpuDisabled);
}

static inline void AdjustDelaySlotException(SH4ThrownException& ex)
{
	ex.epc -= 2;
	if (ex.expEvn == Sh4Ex_FpuDisabled)
		ex.expEvn = Sh4Ex_SlotFpuDisabled;
	else if (ex.expEvn == Sh4Ex_IllegalInstr)
		ex.expEvn = Sh4Ex_SlotIllegalInstr;
}

// The SH4 sets the signaling bit to 0 for qNaN (unlike all recent CPUs).
static inline f32 fixNaN(f32 f)
{
#ifdef STRICT_MODE
	u32& hex = *(u32 *)&f;
	if (std::isnan(f))
		hex = 0x7fbfffff;
#endif
	return f;
}

static inline f64 fixNaN64(f64 f)
{
#ifdef STRICT_MODE
	u64& hex = *(u64 *)&f;
	if (std::isnan(f))
		hex = 0x7ff7ffffffffffffll;
#endif
	return f;
}
