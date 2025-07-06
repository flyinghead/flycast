#pragma once
#include "types.h"
#include "stdclass.h"

enum Sh4RegType
{
	//GPRs
	reg_r0,
	reg_r1,
	reg_r2,
	reg_r3,
	reg_r4,
	reg_r5,
	reg_r6,
	reg_r7,
	reg_r8,
	reg_r9,
	reg_r10,
	reg_r11,
	reg_r12,
	reg_r13,
	reg_r14,
	reg_r15,

	//FPU, bank 0
	reg_fr_0,
	reg_fr_1,
	reg_fr_2,
	reg_fr_3,
	reg_fr_4,
	reg_fr_5,
	reg_fr_6,
	reg_fr_7,
	reg_fr_8,
	reg_fr_9,
	reg_fr_10,
	reg_fr_11,
	reg_fr_12,
	reg_fr_13,
	reg_fr_14,
	reg_fr_15,

	//FPU, bank 1
	reg_xf_0,
	reg_xf_1,
	reg_xf_2,
	reg_xf_3,
	reg_xf_4,
	reg_xf_5,
	reg_xf_6,
	reg_xf_7,
	reg_xf_8,
	reg_xf_9,
	reg_xf_10,
	reg_xf_11,
	reg_xf_12,
	reg_xf_13,
	reg_xf_14,
	reg_xf_15,

	//GPR Interrupt bank
	reg_r0_Bank,
	reg_r1_Bank,
	reg_r2_Bank,
	reg_r3_Bank,
	reg_r4_Bank,
	reg_r5_Bank,
	reg_r6_Bank,
	reg_r7_Bank,

	//Misc regs
	reg_gbr,
	reg_ssr,
	reg_spc,
	reg_sgr,
	reg_dbr,
	reg_vbr,
	reg_mach,
	reg_macl,
	reg_pr,
	reg_fpul,
	reg_nextpc,
	reg_sr_status,     //Only the status bits
	reg_sr_T,          //Only T
	reg_old_fpscr,
	reg_fpscr,
	
	reg_pc_dyn,        //Write only, for dynarec only (dynamic block exit address)
	reg_temp,

	sh4_reg_count,

	/*
		These are virtual registers, used by the dynarec decoder
	*/
	regv_dr_0,
	regv_dr_2,
	regv_dr_4,
	regv_dr_6,
	regv_dr_8,
	regv_dr_10,
	regv_dr_12,
	regv_dr_14,

	regv_xd_0,
	regv_xd_2,
	regv_xd_4,
	regv_xd_6,
	regv_xd_8,
	regv_xd_10,
	regv_xd_12,
	regv_xd_14,

	regv_fv_0,
	regv_fv_4,
	regv_fv_8,
	regv_fv_12,

	regv_xmtrx,
	regv_fmtrx,

	NoReg=-1
};

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

#define STATUS_MASK 0x700083F2

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
struct sh4_if
{
	void (*Start)();
	void (*Run)();
	void (*Stop)();
	void (*Step)();
	void (*Reset)(bool hard);
	void (*Init)();
	void (*Term)();
	void (*ResetCache)();
	bool (*IsCpuRunning)();
};

extern sh4_if sh4_cpu;

struct alignas(64) Sh4Context
{
	union
	{
		struct
		{
			f32 xffr[32];
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
		};
		u64 raw[64-8];
	};
};
static_assert(sizeof(Sh4Context) == 448, "Invalid Sh4Context size");

struct alignas(32) SQBuffer {
	u8 data[32];
};

void setSqwHandler();
void DYNACALL do_sqw_mmu(u32 dst);

typedef void DYNACALL sqw_fp(u32 dst, const SQBuffer *sqb);

#define FPCB_SIZE (RAM_SIZE_MAX/2)
#define FPCB_MASK (FPCB_SIZE -1)
#if HOST_CPU == CPU_ARM
// The arm32 dynarec context register (r8) points past the end of the Sh4RCB struct.
// To get a pointer to the fpcb table, we substract sizeof(Sh4RCB), which we
// want to be an i8r4 value that can be substracted in one op (such as 0x4100000)
#define FPCB_PAD 0x100000
#else
#define FPCB_PAD 0x10000
#endif
struct alignas(PAGE_SIZE) Sh4RCB
{
	void* fpcb[FPCB_SIZE];
	u8 _pad[FPCB_PAD - sizeof(Sh4Context) - sizeof(SQBuffer) * 2 - sizeof(void *)];
	sqw_fp* do_sqw_nommu;
	SQBuffer sq_buffer[2];
	Sh4Context cntx;
};

extern Sh4RCB* p_sh4rcb;

static inline u32 sh4_sr_GetFull()
{
	return (p_sh4rcb->cntx.sr.status & STATUS_MASK) | p_sh4rcb->cntx.sr.T;
}

static inline void sh4_sr_SetFull(u32 value)
{
	p_sh4rcb->cntx.sr.status=value & STATUS_MASK;
	p_sh4rcb->cntx.sr.T=value&1;
}

#define do_sqw_nommu sh4rcb.do_sqw_nommu

#define sh4rcb (*p_sh4rcb)
#define Sh4cntx (sh4rcb.cntx)

// #ifdef ENABLE_SH4_CACHED_IR
// namespace sh4 { namespace ir { void Get_Sh4Interpreter(sh4_if* cpu); } }
// #endif

//Get an interface to sh4 interpreter
void Get_Sh4Interpreter(sh4_if* cpu);
void Get_Sh4Recompiler(sh4_if* cpu);

u32* GetRegPtr(u32 reg);

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
