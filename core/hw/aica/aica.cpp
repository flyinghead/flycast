#include "aica.h"
#include "aica_if.h"
#include "aica_mem.h"
#include "sgc_if.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/arm7/arm7.h"
#include "hw/arm7/arm_mem.h"

namespace aica
{

#define SH4_IRQ_BIT (1 << (holly_SPU_IRQ & 31))

CommonData_struct* CommonData;
DSPData_struct* DSPData;
InterruptInfo* MCIEB;
InterruptInfo* MCIPD;
InterruptInfo* MCIRE;
InterruptInfo* SCIEB;
InterruptInfo* SCIPD;
InterruptInfo* SCIRE;
std::deque<u8> midiSendBuffer;

//Interrupts
//arm side
static u32 GetL(u32 which)
{
	if (which > 7)
		which = 7; //higher bits share bit 7

	u32 bit = 1 << which;
	u32 rv = 0;

	if (CommonData->SCILV0 & bit)
		rv = 1;

	if (CommonData->SCILV1 & bit)
		rv |= 2;
	
	if (CommonData->SCILV2 & bit)
		rv |= 4;

	return rv;
}

static void update_arm_interrupts()
{
	u32 p_ints=SCIEB->full & SCIPD->full;

	u32 Lval=0;
	if (p_ints)
	{
		u32 bit_value=1;//first bit
		//scan all interrupts , lo to hi bit.I assume low bit ints have higher priority over others
		for (u32 i=0;i<11;i++)
		{
			if (p_ints & bit_value)
			{
				//for the first one , Set the L reg & exit
				Lval=GetL(i);
				break;
			}
			bit_value<<=1; //next bit
		}
	}

	arm::interruptChange(p_ints,Lval);
}

//sh4 side
static bool UpdateSh4Ints()
{
	u32 p_ints = MCIEB->full & MCIPD->full;
	if (p_ints)
	{
		if ((SB_ISTEXT & SH4_IRQ_BIT) == 0)
		{
			// if no interrupt is already pending then raise one
			asic_RaiseInterrupt(holly_SPU_IRQ);
			return true;
		}
	}
	else
	{
		if ((SB_ISTEXT & SH4_IRQ_BIT) != 0)
			asic_CancelInterrupt(holly_SPU_IRQ);
	}
	return false;
}

AicaTimer timers[3];
int aica_schid = -1;
const int AICA_TICK = 145125;	// 44.1 KHz / 32

static int AicaUpdate(int tag, int c, int j)
{
	arm::run(32);

	return AICA_TICK;
}

//Mainloop

void timeStep()
{
	for (std::size_t i = 0; i < std::size(timers); i++)
		timers[i].StepTimer(1);

	SCIPD->SAMPLE_DONE = 1;
	MCIPD->SAMPLE_DONE = 1;

	sgc::AICA_Sample();

	//Make sure sh4/arm interrupt system is up to date
	update_arm_interrupts();
	UpdateSh4Ints();	
}

static void AicaInternalDMA()
{
	if (!CommonData->DEXE)
		return;

	// Start dma
	DEBUG_LOG(AICA, "AICA internal DMA: DGATE %d DDIR %d DLG %x", CommonData->DGATE, CommonData->DDIR, CommonData->DLG);
	if (CommonData->DGATE)
	{
		// Clear memory/registers
		if (CommonData->DDIR)
		{
			// to wave mem
			u32 addr = ((CommonData->DMEA_hi << 16) | (CommonData->DMEA_lo << 2)) & ARAM_MASK;
			u32 len = std::min(CommonData->DLG, ARAM_SIZE - addr);
			memset(&aica_ram[addr], 0, len * 4);
		}
		else
		{
			// to regs
			u32 addr = CommonData->DRGA << 2;
			for (u32 i = 0; i < CommonData->DLG; i++, addr += 4)
				writeAicaReg(addr, (u32)0);
		}
	}
	else
	{
		// Data xfer
		u32 waddr = ((CommonData->DMEA_hi << 16) | (CommonData->DMEA_lo << 2)) & ARAM_MASK;
		u32 raddr = CommonData->DRGA << 2;
		u32 len = std::min(CommonData->DLG, ARAM_SIZE - waddr);
		if (CommonData->DDIR)
		{
			// reg to wave mem
			for (u32 i = 0; i < len; i++, waddr += 4, raddr += 4)
				*(u32*)&aica_ram[waddr] = readAicaReg<u32>(raddr);
		}
		else
		{
			// wave mem to regs
			for (u32 i = 0; i < len; i++, waddr += 4, raddr += 4)
				writeAicaReg(raddr, *(u32*)&aica_ram[waddr]);
		}
	}
	CommonData->DEXE = 0;
	MCIPD->DMA_END = 1;
	UpdateSh4Ints();
	SCIPD->DMA_END = 1;
	update_arm_interrupts();
}

//Memory i/o
template<typename T>
void writeTimerAndIntReg(u32 reg, T data)
{
	switch (reg)
	{
	case SCIEB_addr:
		SCIEB->full = data & 0x7ff;
		update_arm_interrupts();
		break;

	case SCIPD_addr:
		// other bits are read-only
		if (data & (1 << 5))
		{
			SCIPD->SCPU = 1;
			update_arm_interrupts();
		}
		break;

	case SCIRE_addr:
		SCIPD->full &= ~data;
		update_arm_interrupts();
		break;

	case MCIEB_addr:
		MCIEB->full = data & 0x7ff;
		if (UpdateSh4Ints())
			arm::avoidRaceCondition();
		break;

	case MCIPD_addr:
		// other bits are read-only
		if (data & (1 << 5))
		{
			MCIPD->SCPU = 1;
			if (UpdateSh4Ints())
				arm::avoidRaceCondition();
		}
		break;

	case MCIRE_addr:
		MCIPD->full &= ~data;
		UpdateSh4Ints();
		break;

	case TIMER_A:
		WriteMemArr(aica_reg, reg, data);
		timers[0].RegisterWrite();
		break;

	case TIMER_B:
		WriteMemArr(aica_reg, reg, data);
		timers[1].RegisterWrite();
		break;

	case TIMER_C:
		WriteMemArr(aica_reg, reg, data);
		timers[2].RegisterWrite();
		break;

	// DEXE, DDIR, DLG
	case 0x288C:
		WriteMemArr(aica_reg, reg, data);
		AicaInternalDMA();
		break;

	default:
		WriteMemArr(aica_reg, reg, data);
		break;
	}
}

template void writeTimerAndIntReg<>(u32 reg, u8 data);
template void writeTimerAndIntReg<>(u32 reg, u16 data);
template void writeTimerAndIntReg<>(u32 reg, u32 data);

void midiSend(u8 data)
{
	midiSendBuffer.push_back(data);
	SCIPD->MIDI_IN = 1;
	update_arm_interrupts();
	MCIPD->MIDI_IN = 1;
	UpdateSh4Ints();
}

void init()
{
	initMem();
	initRtc();

	static_assert(sizeof(*CommonData) == 0x508, "Invalid CommonData size");
	static_assert(sizeof(*DSPData) == 0x15C8, "Invalid DSPData size");

	CommonData=(CommonData_struct*)&aica_reg[0x2800];
	DSPData=(DSPData_struct*)&aica_reg[0x3000];
	//slave cpu (arm7)

	SCIEB=(InterruptInfo*)&aica_reg[0x289C];
	SCIPD=(InterruptInfo*)&aica_reg[0x289C+4];
	SCIRE=(InterruptInfo*)&aica_reg[0x289C+8];
	//Main cpu (sh4)
	MCIEB=(InterruptInfo*)&aica_reg[0x28B4];
	MCIPD=(InterruptInfo*)&aica_reg[0x28B4+4];
	MCIRE=(InterruptInfo*)&aica_reg[0x28B4+8];

	sgc::init();
	if (aica_schid == -1)
		aica_schid = sh4_sched_register(0, &AicaUpdate);
	arm::init();
}

void reset(bool hard)
{
	if (hard)
	{
		initMem();
		sgc::term();
		sgc::init();
		sh4_sched_request(aica_schid, AICA_TICK);
	}
	for (std::size_t i = 0; i < std::size(timers); i++)
		timers[i].Init(aica_reg, i);
	resetRtc(hard);
	arm::reset();
}

void term()
{
	arm::term();
	sgc::term();
	termMem();
	sh4_sched_unregister(aica_schid);
	aica_schid = -1;
}

} // namespace aica
