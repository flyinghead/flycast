#pragma once
#include "types.h"
#include "sh4_if.h"
#include <cmath>

int UpdateSystem_INTC();
bool UpdateSR();
void setDefaultRoundingMode();

struct SH4ThrownException
{
	SH4ThrownException(u32 epc, Sh4ExceptionCode expEvn) : epc(epc), expEvn(expEvn) { }

	u32 epc;
	Sh4ExceptionCode expEvn;
};

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
