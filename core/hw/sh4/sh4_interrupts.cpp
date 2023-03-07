/*
	Interrupt list caching and handling

	SH4 has a very flexible interrupt controller. In order to handle it efficiently, a sorted
	interrupt bitfield is build from the set interrupt priorities. Higher priorities get allocated
	into higher bits, and a simple mask is kept. In order to check for pending interrupts a simple
	!=0 test works, and to identify the pending interrupt bsr(pend) will give the sorted id. As
	this is a single cycle operation on most platforms, the interrupt checking/identification
	is very fast !
*/

#include "types.h"
#include "sh4_interrupts.h"
#include "sh4_core.h"
#include "sh4_mmr.h"
#include "oslib/oslib.h"
#include "debug/gdb_server.h"
#include "serialize.h"
#include <cassert>

//these are fixed
const u16 IRLPriority = 0x0246;
#define IRLP9 &IRLPriority,0
#define IRLP11 &IRLPriority,4
#define IRLP13 &IRLPriority,8

#define GIPA(p) &INTC_IPRA.reg_data,4*p
#define GIPB(p) &INTC_IPRB.reg_data,4*p
#define GIPC(p) &INTC_IPRC.reg_data,4*p

struct InterptSourceList_Entry
{
	const u16* PrioReg;
	u32 Shift;
	Sh4ExceptionCode IntEvnCode;

	int GetPrLvl() const { return (*PrioReg >> Shift) & 0xF; }
};

static const InterptSourceList_Entry InterruptSourceList[sh4_INT_ID_COUNT] =
{
	//IRL
	{ IRLP9, Sh4Ex_ExtInterrupt9 },		//sh4_IRL_9
	{ IRLP11, Sh4Ex_ExtInterruptB },	//sh4_IRL_11
	{ IRLP13, Sh4Ex_ExtInterruptD },	//sh4_IRL_13

	//HUDI
	{ GIPC(0), Sh4Ex_HUDI },			//sh4_HUDI_HUDI

	//GPIO (missing on dc ?)
	{ GIPC(3), Sh4Ex_GPIO },			//sh4_GPIO_GPIOI

	//DMAC
	{ GIPC(2), Sh4Ex_DMAC_DTME0 },		//sh4_DMAC_DMTE0
	{ GIPC(2), Sh4Ex_DMAC_DTME1 },		//sh4_DMAC_DMTE1
	{ GIPC(2), Sh4Ex_DMAC_DTME2 },		//sh4_DMAC_DMTE2
	{ GIPC(2), Sh4Ex_DMAC_DTME3 },		//sh4_DMAC_DMTE3
	{ GIPC(2), Sh4Ex_DMAC_DMAE },		//sh4_DMAC_DMAE

	//TMU
	{ GIPA(3), Sh4Ex_TMU0 },			//sh4_TMU0_TUNI0
	{ GIPA(2), Sh4Ex_TMU1 },			//sh4_TMU1_TUNI1
	{ GIPA(1), Sh4Ex_TMU2 },			//sh4_TMU2_TUNI2
	{ GIPA(1), Sh4Ex_TMU2_TICPI2 },		//sh4_TMU2_TICPI2

	//RTC
	{ GIPA(0), Sh4Ex_RTC_ATI },			//sh4_RTC_ATI
	{ GIPA(0), Sh4Ex_RTC_PRI },			//sh4_RTC_PRI
	{ GIPA(0), Sh4Ex_RTC_CUI },			//sh4_RTC_CUI

	//SCI
	{ GIPB(1), Sh4Ex_SCI_ERI },			//sh4_SCI1_ERI
	{ GIPB(1), Sh4Ex_SCI_RXI },			//sh4_SCI1_RXI
	{ GIPB(1), Sh4Ex_SCI_TXI },			//sh4_SCI1_TXI
	{ GIPB(1), Sh4Ex_SCI_TEI },			//sh4_SCI1_TEI

	//SCIF
	{ GIPC(1), Sh4Ex_SCIF_ERI },		//sh4_SCIF_ERI
	{ GIPC(1), Sh4Ex_SCIF_RXI },		//sh4_SCIF_RXI
	{ GIPC(1), Sh4Ex_SCIF_BRI },		//sh4_SCIF_BRI
	{ GIPC(1), Sh4Ex_SCIF_TXI },		//sh4_SCIF_TXI

	//WDT
	{ GIPB(3), Sh4Ex_WDT },				//sh4_WDT_ITI

	//REF
	{ GIPB(2), Sh4Ex_REF_RCMI },		//sh4_REF_RCMI
	{ GIPA(2), Sh4Ex_REF_ROVI },		//sh4_REF_ROVI
};

//Maps siid -> EventID
alignas(64) static Sh4ExceptionCode InterruptEnvId[32];
//Maps piid -> 1<<siid
alignas(64) static u32 InterruptBit[32];
//Maps sh4 interrupt level to inclusive bitfield
alignas(64) static u32 InterruptLevelBit[16];

static void Do_Interrupt(Sh4ExceptionCode intEvn);

static u32 interrupt_vpend; // Vector of pending interrupts
static u32 interrupt_vmask; // Vector of masked interrupts             (-1 inhibits all interrupts)
static u32 decoded_srimask; // Vector of interrupts allowed by SR.IMSK (-1 inhibits all interrupts)

//bit 0 ~ 27 : interrupt source 27:0. 0 = lowest level, 27 = highest level.
static void recalc_pending_itrs()
{
	Sh4cntx.interrupt_pend = interrupt_vpend & interrupt_vmask & decoded_srimask;
}

//Rebuild sorted interrupt id table (priorities were updated)
void SIIDRebuild()
{
	int cnt = 0;
	u32 vpend = interrupt_vpend;
	u32 vmask = interrupt_vmask;
	interrupt_vpend = 0;
	interrupt_vmask = 0;
	//rebuild interrupt table
	for (int ilevel = 0; ilevel < 16; ilevel++)
	{
		for (int isrc = 0; isrc < sh4_INT_ID_COUNT; isrc++)
		{
			if (InterruptSourceList[isrc].GetPrLvl() == ilevel)
			{
				InterruptEnvId[cnt] = InterruptSourceList[isrc].IntEvnCode;
				u32 p = InterruptBit[isrc] & vpend;
				u32 m = InterruptBit[isrc] & vmask;
				InterruptBit[isrc] = 1 << cnt;
				if (p)
					interrupt_vpend |= InterruptBit[isrc];
				if (m)
					interrupt_vmask |= InterruptBit[isrc];
				cnt++;
			}
		}
		InterruptLevelBit[ilevel] = (1 << cnt) - 1;
	}
	SRdecode();
}

//Decode SR.IMSK into a interrupt mask, update and return the interrupt state
bool SRdecode()
{
	if (sr.BL)
		decoded_srimask=~0xFFFFFFFF;
	else
		decoded_srimask=~InterruptLevelBit[sr.IMASK];

	recalc_pending_itrs();
	return Sh4cntx.interrupt_pend;
}

int UpdateINTC()
{
	if (!Sh4cntx.interrupt_pend)
		return 0;

	Do_Interrupt(InterruptEnvId[bitscanrev(Sh4cntx.interrupt_pend)]);
	return 1;
}

void SetInterruptPend(InterruptID intr)
{
	interrupt_vpend |= InterruptBit[intr];
	recalc_pending_itrs();
}
void ResetInterruptPend(InterruptID intr)
{
	interrupt_vpend &= ~InterruptBit[intr];
	recalc_pending_itrs();
}

void SetInterruptMask(InterruptID intr)
{
	interrupt_vmask |= InterruptBit[intr];
	recalc_pending_itrs();
}
void ResetInterruptMask(InterruptID intr)
{
	interrupt_vmask &= ~InterruptBit[intr];
	recalc_pending_itrs();
}

static void Do_Interrupt(Sh4ExceptionCode intEvn)
{
	CCN_INTEVT = intEvn;

	ssr = sh4_sr_GetFull();
	spc = next_pc;
	sgr = r[15];
	sr.BL = 1;
	sr.MD = 1;
	sr.RB = 1;
	UpdateSR();
	next_pc = vbr + 0x600;
	debugger::subroutineCall();
}

void Do_Exception(u32 epc, Sh4ExceptionCode expEvn)
{
	assert((expEvn >= Sh4Ex_TlbMissRead && expEvn <= Sh4Ex_SlotIllegalInstr)
			|| expEvn == Sh4Ex_FpuDisabled || expEvn == Sh4Ex_SlotFpuDisabled || expEvn == Sh4Ex_UserBreak);
	if (sr.BL != 0)
		throw FlycastException("Fatal: SH4 exception when blocked");
	CCN_EXPEVT = expEvn;

	ssr = sh4_sr_GetFull();
	spc = epc;
	sgr = r[15];
	sr.BL = 1;
	sr.MD = 1;
	sr.RB = 1;
	UpdateSR();

	next_pc = vbr + (expEvn == Sh4Ex_TlbMissRead || expEvn == Sh4Ex_TlbMissWrite ? 0x400 : 0x100);
	debugger::subroutineCall();

	//printf("RaiseException: from pc %08x to %08x, event %x\n", epc, next_pc, expEvn);
}


//Init/Res/Term
void interrupts_init()
{
}

void interrupts_reset()
{
	//reset interrupts cache
	interrupt_vpend = 0;
	interrupt_vmask = 0xFFFFFFFF;
	decoded_srimask = 0;

	for (u32 i = 0; i < sh4_INT_ID_COUNT; i++)
		InterruptBit[i] = 1 << i;

	//rebuild the interrupts table
	SIIDRebuild();
}

void interrupts_term()
{
}

void interrupts_serialize(Serializer& ser)
{
	ser << InterruptEnvId;
	ser << InterruptBit;
	ser << InterruptLevelBit;
	ser << interrupt_vpend;
	ser << interrupt_vmask;
	ser << decoded_srimask;

}

void interrupts_deserialize(Deserializer& deser)
{
	deser >> InterruptEnvId;
	deser >> InterruptBit;
	deser >> InterruptLevelBit;
	deser >> interrupt_vpend;
	deser >> interrupt_vmask;
	deser >> decoded_srimask;
}
