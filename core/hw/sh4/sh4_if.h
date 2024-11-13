#pragma once
#include "types.h"
#include "stdclass.h"
#include <cassert>

// SR (status register)

union sr_status_t
{
	struct
	{
		u32 T_h     : 1;
		u32 S       : 1;
		u32         : 2;
		u32 IMASK   : 4;
		u32 Q       : 1;
		u32 M       : 1;
		u32         : 5;
		u32 FD      : 1;
		u32         : 12;
		u32 BL      : 1;
		u32 RB      : 1;
		u32 MD      : 1;
		u32         : 1;
	};
	u32 status;
};

// Status register with isolated T bit.
// Used in place of the normal SR bitfield so that the T bit can be
// handled as a regular register. This simplifies dynarec implementations.
struct sr_t
{
	union
	{
		struct
		{
			u32 T_h     : 1;
			u32 S       : 1;
			u32         : 2;
			u32 IMASK   : 4;
			u32 Q       : 1;
			u32 M       : 1;
			u32         : 5;
			u32 FD      : 1;
			u32         : 12;
			u32 BL      : 1;
			u32 RB      : 1;
			u32 MD      : 1;
			u32         : 1;
		};
		u32 status;
	};
	u32 T;

	static constexpr u32 MASK = 0x700083F2;

	u32 getFull() const {
		return (status & MASK) | T;
	}

	void setFull(u32 v) {
		status = v & MASK;
		T = v & 1;
	}
};

// FPSCR (fpu status and control register)
struct fpscr_t
{
	union
	{
		u32 full;
		struct
		{
			u32 RM         : 2;
			u32 finexact   : 1;
			u32 funderflow : 1;
			u32 foverflow  : 1;
			u32 fdivbyzero : 1;
			u32 finvalidop : 1;
			u32 einexact   : 1;
			u32 eunderflow : 1;
			u32 eoverflow  : 1;
			u32 edivbyzero : 1;
			u32 einvalidop : 1;
			u32 cinexact   : 1;
			u32 cunderflow : 1;
			u32 coverflow  : 1;
			u32 cdivbyzero : 1;
			u32 cinvalid   : 1;
			u32 cfpuerr    : 1;
			u32 DN         : 1;
			u32 PR         : 1;
			u32 SZ         : 1;
			u32 FR         : 1;
			u32            : 10;
		};
	};
};

//sh4 interface
class Sh4Executor
{
public:
	virtual ~Sh4Executor() = default;
	virtual void Run() = 0;
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void Step() = 0;
	virtual void Reset(bool hard) = 0;
	virtual void Init() = 0;
	virtual void Term() = 0;
	virtual void ResetCache() = 0;
	virtual bool IsCpuRunning() = 0;
};

struct alignas(32) SQBuffer {
	u8 data[32];
};

void setSqwHandler();
struct Sh4Context;
typedef void DYNACALL SQWriteFunc(u32 dst, Sh4Context *ctx);

struct alignas(64) Sh4Context
{
	union
	{
		struct
		{
			SQBuffer sq_buffer[2];

			float xf[16];
			float fr[16];
			u32 r[16];

			union
			{
				u64 full;
				struct
				{
					u32 l;
					u32 h;
				};
			} mac;

			u32 r_bank[8];

			u32 gbr,ssr,spc,sgr,dbr,vbr;
			u32 pr,fpul;
			u32 pc;

			u32 jdyn;

			sr_t sr;
			fpscr_t fpscr;
			sr_status_t old_sr;
			fpscr_t old_fpscr;

			u32 CpuRunning;

			int sh4_sched_next;
			u32 interrupt_pend;

			u32 temp_reg;
			int cycle_counter;

			SQWriteFunc *doSqWrite;
		};
		u64 raw[64];
	};

	u32& fr_hex(int idx) {
		assert(idx >= 0 && idx <= 15);
		return reinterpret_cast<u32&>(fr[idx]);
	}
	u64& dr_hex(int idx) {
		assert(idx >= 0 && idx <= 7);
		return *reinterpret_cast<u64 *>(&fr[idx * 2]);
	}
	u64& xd_hex(int idx) {
		assert(idx >= 0 && idx <= 7);
		return *reinterpret_cast<u64 *>(&xf[idx * 2]);
	}

	double getDR(u32 n)
	{
		assert(n <= 7);
		DoubleReg t;
		t.sgl[1] = fr[n * 2];
		t.sgl[0] = fr[n * 2 + 1];

		return t.dbl;
	}

	void setDR(u32 n, double val)
	{
		assert(n <= 7);
		DoubleReg t;
		t.dbl = val;
		fr[n * 2] = t.sgl[1];
		fr[n * 2 + 1] = t.sgl[0];
	}

	static void DYNACALL UpdateFPSCR(Sh4Context *ctx);
	void restoreHostRoundingMode();

private:
	union DoubleReg
	{
		double dbl;
		float sgl[2];
	};
};
static_assert(sizeof(Sh4Context) == 512, "Invalid Sh4Context size");

#define FPCB_SIZE (RAM_SIZE_MAX/2)
#define FPCB_MASK (FPCB_SIZE -1)
#if HOST_CPU == CPU_ARM
// The arm32 dynarec context register (r8) points past the end of the Sh4RCB struct.
// To get a pointer to the fpcb table, we substract sizeof(Sh4RCB), which we
// want to be an i8r4 value that can be substracted in one op (such as 0x4100000)
#define FPCB_PAD 0x100000
#else
// For other systems we could use PAGE_SIZE, except on windows that has a 64 KB granularity for memory mapping
#define FPCB_PAD 64_KB
#endif
struct alignas(PAGE_SIZE) Sh4RCB
{
	void* fpcb[FPCB_SIZE];
	u8 _pad[FPCB_PAD - sizeof(Sh4Context)];
	Sh4Context cntx;
};
static_assert((sizeof(Sh4RCB) % PAGE_SIZE) == 0, "sizeof(Sh4RCB) not multiple of PAGE_SIZE");

extern Sh4RCB* p_sh4rcb;
#define Sh4cntx (p_sh4rcb->cntx)

//Get an interface to sh4 interpreter
Sh4Executor *Get_Sh4Interpreter();
Sh4Executor *Get_Sh4Recompiler();

enum Sh4ExceptionCode : u16
{
	Sh4Ex_PowerOnReset = 0,
	Sh4Ex_ManualReset = 0x20,
	Sh4Ex_TlbMissRead = 0x40,
	Sh4Ex_TlbMissWrite = 0x60,
	Sh4Ex_TlbInitPageWrite = 0x80,
	Sh4Ex_TlbProtViolRead = 0xa0,
	Sh4Ex_TlbProtViolWrite = 0xc0,
	Sh4Ex_AddressErrorRead = 0xe0,
	Sh4Ex_AddressErrorWrite = 0x100,
	Sh4Ex_FpuError = 0x120,
	Sh4Ex_FpuDisabled = 0x800,
	Sh4Ex_SlotFpuDisabled = 0x820,
	Sh4Ex_TlbMultiHit = 0x140,
	Sh4Ex_Trap = 0x160,
	Sh4Ex_IllegalInstr = 0x180,
	Sh4Ex_SlotIllegalInstr = 0x1a0,
	Sh4Ex_NmiInterrupt = 0x1c0,
	Sh4Ex_UserBreak = 0x1e0,
	Sh4Ex_ExtInterrupt0 = 0x200,
	Sh4Ex_ExtInterrupt1 = 0x220,
	Sh4Ex_ExtInterrupt2 = 0x240,
	Sh4Ex_ExtInterrupt3 = 0x260,
	Sh4Ex_ExtInterrupt4 = 0x280,
	Sh4Ex_ExtInterrupt5 = 0x2a0,
	Sh4Ex_ExtInterrupt6 = 0x2c0,
	Sh4Ex_ExtInterrupt7 = 0x2e0,
	Sh4Ex_ExtInterrupt8 = 0x300,
	Sh4Ex_ExtInterrupt9 = 0x320,
	Sh4Ex_ExtInterruptA = 0x340,
	Sh4Ex_ExtInterruptB = 0x360,
	Sh4Ex_ExtInterruptC = 0x380,
	Sh4Ex_ExtInterruptD = 0x3a0,
	Sh4Ex_ExtInterruptE = 0x3c0,
	Sh4Ex_TMU0 = 0x400,
	Sh4Ex_TMU1 = 0x420,
	Sh4Ex_TMU2 = 0x440,
	Sh4Ex_TMU2_TICPI2 = 0x460,
	Sh4Ex_RTC_ATI = 0x480,
	Sh4Ex_RTC_PRI = 0x4a0,
	Sh4Ex_RTC_CUI = 0x4c0,
	Sh4Ex_SCI_ERI = 0x4e0,
	Sh4Ex_SCI_RXI = 0x500,
	Sh4Ex_SCI_TXI = 0x520,
	Sh4Ex_SCI_TEI = 0x540,
	Sh4Ex_WDT = 0x560,
	Sh4Ex_REF_RCMI = 0x580,
	Sh4Ex_REF_ROVI = 0x5a0,
	Sh4Ex_HUDI = 0x600,
	Sh4Ex_GPIO = 0x620,
	Sh4Ex_DMAC_DTME0 = 0x640,
	Sh4Ex_DMAC_DTME1 = 0x680,
	Sh4Ex_DMAC_DTME2 = 0x680,
	Sh4Ex_DMAC_DTME3 = 0x6a0,
	Sh4Ex_DMAC_DMAE = 0x6c0,
	Sh4Ex_SCIF_ERI = 0x700,
	Sh4Ex_SCIF_RXI = 0x720,
	Sh4Ex_SCIF_BRI = 0x740,
	Sh4Ex_SCIF_TXI = 0x760,
};
