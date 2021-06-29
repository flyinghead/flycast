#pragma once
#include "types.h"
#include "sh4_if.h"
#include "cfg/option.h"

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

union DoubleReg
{
	f64 dbl;
	f32 sgl[2];
};

static INLINE f64 GetDR(u32 n)
{
#ifdef TRACE
	if (n>7)
		INFO_LOG(SH4, "DR_r INDEX OVERRUN %d >7", n);
#endif
	DoubleReg t;

	t.sgl[1]=fr[(n<<1) + 0];
	t.sgl[0]=fr[(n<<1) + 1];

	return t.dbl;
}

static INLINE f64 GetXD(u32 n)
{
#ifdef TRACE
	if (n>7)
		INFO_LOG(SH4, "XD_r INDEX OVERRUN %d >7", n);
#endif
	DoubleReg t;

	t.sgl[1]=xf[(n<<1) + 0];
	t.sgl[0]=xf[(n<<1) + 1];

	return t.dbl;
}

static INLINE void SetDR(u32 n,f64 val)
{
#ifdef TRACE
	if (n>7)
		INFO_LOG(SH4, "DR_w INDEX OVERRUN %d >7", n);
#endif
	DoubleReg t;
	t.dbl=val;


	fr[(n<<1) | 1]=t.sgl[0];
	fr[(n<<1) | 0]=t.sgl[1];
}

static INLINE void SetXD(u32 n,f64 val)
{
#ifdef TRACE
	if (n>7)
		INFO_LOG(SH4, "XD_w INDEX OVERRUN %d >7", n);
#endif

	DoubleReg t;
	t.dbl=val;

	xf[(n<<1) | 1]=t.sgl[0];
	xf[(n<<1) | 0]=t.sgl[1];
}

bool Do_Exception(u32 epc, u32 expEvn, u32 CallVect);

struct SH4ThrownException {
	u32 epc;
	u32 expEvn;
	u32 callVect;
};

static INLINE void RaiseFPUDisableException()
{
	if (config::FullMMU)
	{
		SH4ThrownException ex { next_pc - 2, 0x800, 0x100 };
		throw ex;
	}
}

static INLINE void AdjustDelaySlotException(SH4ThrownException& ex)
{
	ex.epc -= 2;
	if (ex.expEvn == 0x800)	// FPU disable exception
		ex.expEvn = 0x820;	// Slot FPU disable exception
	else if (ex.expEvn == 0x180)	// Illegal instruction exception
		ex.expEvn = 0x1A0;			// Slot illegal instruction exception
}

// The SH4 sets the signaling bit to 0 for qNaN (unlike all recent CPUs). Some games rely on this.
static INLINE f32 fixNaN(f32 f)
{
#ifdef STRICT_MODE
	u32& hex = *(u32 *)&f;
#ifdef __FAST_MATH__
	// fast-math
	if ((hex & 0x7fffffff) > 0x7f800000)
		hex = 0x7fbfffff;
#else
	// no fast-math
	if (f != f)
		hex = 0x7fbfffff;
#endif
#endif
	return f;
}

static INLINE f64 fixNaN64(f64 f)
{
#ifdef STRICT_MODE
	u64& hex = *(u64 *)&f;
#ifdef __FAST_MATH__
	// fast-math
	if ((hex & 0x7fffffffffffffffll) > 0x7ff0000000000000ll)
		hex = 0x7ff7ffffffffffffll;
#else
	// no fast-math
	if (f != f)
		hex = 0x7ff7ffffffffffffll;
#endif
#endif
	return f;
}
