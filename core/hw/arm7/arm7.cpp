#include "arm7.h"
#include "arm_mem.h"
#include "arm7_rec.h"

#define CPUReadMemoryQuick(addr) (*(u32*)&aica_ram[(addr) & ARAM_MASK])
#define CPUReadByte arm_ReadMem8
#define CPUReadMemory arm_ReadMem32
#define CPUReadHalfWord arm_ReadMem16
#define CPUReadHalfWordSigned(addr) ((s16)arm_ReadMem16(addr))

#define CPUWriteMemory arm_WriteMem32
#define CPUWriteHalfWord arm_WriteMem16
#define CPUWriteByte arm_WriteMem8

#define reg arm_Reg
#define armNextPC reg[R15_ARM_NEXT].I

#define CPUUpdateTicksAccesint(a) 1
#define CPUUpdateTicksAccessSeq32(a) 1
#define CPUUpdateTicksAccesshort(a) 1
#define CPUUpdateTicksAccess32(a) 1
#define CPUUpdateTicksAccess16(a) 1

alignas(8) reg_pair arm_Reg[RN_ARM_REG_COUNT];

static void CPUSwap(u32 *a, u32 *b)
{
	u32 c = *b;
	*b = *a;
	*a = c;
}

#define N_FLAG (reg[RN_PSR_FLAGS].FLG.N)
#define Z_FLAG (reg[RN_PSR_FLAGS].FLG.Z)
#define C_FLAG (reg[RN_PSR_FLAGS].FLG.C)
#define V_FLAG (reg[RN_PSR_FLAGS].FLG.V)

bool armIrqEnable;
bool armFiqEnable;
int armMode;

bool Arm7Enabled = false;

static u8 cpuBitsSet[256];

static void CPUSwitchMode(int mode, bool saveState);
static void CPUUpdateFlags();
static void CPUSoftwareInterrupt(int comment);
static void CPUUndefinedException();

//
// ARM7 interpreter
//
int arm7ClockTicks;

#if FEAT_AREC == DYNAREC_NONE

static void runInterpreter(u32 CycleCount)
{
	if (!Arm7Enabled)
		return;

	arm7ClockTicks -= CycleCount;
	while (arm7ClockTicks < 0)
	{
		if (reg[INTR_PEND].I)
			CPUFiq();

		reg[15].I = armNextPC + 8;

		int& clockTicks = arm7ClockTicks;
		#include "arm-new.h"
	}
}

void aicaarm::avoidRaceCondition()
{
	arm7ClockTicks = std::min(arm7ClockTicks, -50);
}

void aicaarm::run(u32 samples)
{
	for (u32 i = 0; i < samples; i++)
	{
		runInterpreter(ARM_CYCLES_PER_SAMPLE);
		libAICA_TimeStep();
	}
}
#endif

void aicaarm::init()
{
#if FEAT_AREC != DYNAREC_NONE
	recompiler::init();
#endif
	aicaarm::reset();

	for (int i = 0; i < 256; i++)
	{
		int count = 0;
		for (int j = 0; j < 8; j++)
			if (i & (1 << j))
				count++;

		cpuBitsSet[i] = count;
	}
}

static void CPUSwitchMode(int mode, bool saveState)
{
	CPUUpdateCPSR();

	switch(armMode)
	{
	case 0x10:
	case 0x1F:
		reg[R13_USR].I = reg[13].I;
		reg[R14_USR].I = reg[14].I;
		reg[RN_SPSR].I = reg[RN_CPSR].I;
		break;
	case 0x11:
		CPUSwap(&reg[R8_FIQ].I, &reg[8].I);
		CPUSwap(&reg[R9_FIQ].I, &reg[9].I);
		CPUSwap(&reg[R10_FIQ].I, &reg[10].I);
		CPUSwap(&reg[R11_FIQ].I, &reg[11].I);
		CPUSwap(&reg[R12_FIQ].I, &reg[12].I);
		reg[R13_FIQ].I = reg[13].I;
		reg[R14_FIQ].I = reg[14].I;
		reg[SPSR_FIQ].I = reg[RN_SPSR].I;
		break;
	case 0x12:
		reg[R13_IRQ].I  = reg[13].I;
		reg[R14_IRQ].I  = reg[14].I;
		reg[SPSR_IRQ].I =  reg[RN_SPSR].I;
		break;
	case 0x13:
		reg[R13_SVC].I  = reg[13].I;
		reg[R14_SVC].I  = reg[14].I;
		reg[SPSR_SVC].I =  reg[RN_SPSR].I;
		break;
	case 0x17:
		reg[R13_ABT].I  = reg[13].I;
		reg[R14_ABT].I  = reg[14].I;
		reg[SPSR_ABT].I =  reg[RN_SPSR].I;
		break;
	case 0x1b:
		reg[R13_UND].I  = reg[13].I;
		reg[R14_UND].I  = reg[14].I;
		reg[SPSR_UND].I =  reg[RN_SPSR].I;
		break;
	}

	u32 CPSR = reg[RN_CPSR].I;
	u32 SPSR = reg[RN_SPSR].I;

	switch(mode)
	{
	case 0x10:
	case 0x1F:
		reg[13].I = reg[R13_USR].I;
		reg[14].I = reg[R14_USR].I;
		reg[RN_CPSR].I = SPSR;
		break;
	case 0x11:
		CPUSwap(&reg[8].I, &reg[R8_FIQ].I);
		CPUSwap(&reg[9].I, &reg[R9_FIQ].I);
		CPUSwap(&reg[10].I, &reg[R10_FIQ].I);
		CPUSwap(&reg[11].I, &reg[R11_FIQ].I);
		CPUSwap(&reg[12].I, &reg[R12_FIQ].I);
		reg[13].I = reg[R13_FIQ].I;
		reg[14].I = reg[R14_FIQ].I;
		if(saveState)
			reg[RN_SPSR].I = CPSR;
		else
			reg[RN_SPSR].I = reg[SPSR_FIQ].I;
		break;
	case 0x12:
		reg[13].I = reg[R13_IRQ].I;
		reg[14].I = reg[R14_IRQ].I;
		reg[RN_CPSR].I = SPSR;
		if(saveState)
			reg[RN_SPSR].I = CPSR;
		else
			reg[RN_SPSR].I = reg[SPSR_IRQ].I;
		break;
	case 0x13:
		reg[13].I = reg[R13_SVC].I;
		reg[14].I = reg[R14_SVC].I;
		reg[RN_CPSR].I = SPSR;
		if(saveState)
			reg[RN_SPSR].I = CPSR;
		else
			reg[RN_SPSR].I = reg[SPSR_SVC].I;
		break;
	case 0x17:
		reg[13].I = reg[R13_ABT].I;
		reg[14].I = reg[R14_ABT].I;
		reg[RN_CPSR].I = SPSR;
		if(saveState)
			reg[RN_SPSR].I = CPSR;
		else
			reg[RN_SPSR].I = reg[SPSR_ABT].I;
		break;
	case 0x1b:
		reg[13].I = reg[R13_UND].I;
		reg[14].I = reg[R14_UND].I;
		reg[RN_CPSR].I = SPSR;
		if(saveState)
			reg[RN_SPSR].I = CPSR;
		else
			reg[RN_SPSR].I = reg[SPSR_UND].I;
		break;
	default:
		ERROR_LOG(AICA_ARM, "Unsupported ARM mode %02x", mode);
		die("Arm error..");
		break;
	}
	armMode = mode;
	CPUUpdateFlags();
	CPUUpdateCPSR();
}

void CPUUpdateCPSR()
{
	reg_pair CPSR;

	CPSR.I = reg[RN_CPSR].I & 0x40;

	CPSR.PSR.NZCV = reg[RN_PSR_FLAGS].FLG.NZCV;

	if (!armFiqEnable)
		CPSR.I |= 0x40;
	if(!armIrqEnable)
		CPSR.I |= 0x80;

	CPSR.PSR.M = armMode;
	
	reg[RN_CPSR].I = CPSR.I;
}

static void CPUUpdateFlags()
{
	u32 CPSR = reg[RN_CPSR].I;

	reg[RN_PSR_FLAGS].FLG.NZCV = reg[RN_CPSR].PSR.NZCV;

	armIrqEnable = (CPSR & 0x80) ? false : true;
	armFiqEnable = (CPSR & 0x40) ? false : true;
	update_armintc();
}

static void CPUSoftwareInterrupt(int comment)
{
	u32 PC = reg[R15_ARM_NEXT].I+4;
	CPUSwitchMode(0x13, true);
	reg[14].I = PC;
	
	armIrqEnable = false;
	armNextPC = 0x08;
}

static void CPUUndefinedException()
{
	WARN_LOG(AICA_ARM, "arm7: CPUUndefinedException(). SOMETHING WENT WRONG");
	u32 PC = reg[R15_ARM_NEXT].I+4;
	CPUSwitchMode(0x1b, true);
	reg[14].I = PC;
	armIrqEnable = false;
	armNextPC = 0x04;
}

void aicaarm::reset()
{
	INFO_LOG(AICA_ARM, "AICA ARM Reset");
#if FEAT_AREC != DYNAREC_NONE
	recompiler::flush();
#endif
	aica_interr = false;
	aica_reg_L = 0;
	e68k_out = false;
	e68k_reg_L = 0;
	e68k_reg_M = 0;

	Arm7Enabled = false;
	// clean registers
	memset(&arm_Reg[0], 0, sizeof(arm_Reg));

	armMode = 0x13;

	reg[13].I = 0x03007F00;
	reg[15].I = 0x0000000;
	reg[RN_CPSR].I = 0x00000000;
	reg[R13_IRQ].I = 0x03007FA0;
	reg[R13_SVC].I = 0x03007FE0;
	armIrqEnable = true;      
	armFiqEnable = false;
	update_armintc();

	C_FLAG = V_FLAG = N_FLAG = Z_FLAG = false;

	// disable FIQ
	reg[RN_CPSR].I |= 0x40;

	CPUUpdateCPSR();

	armNextPC = reg[15].I;
	reg[15].I += 4;
}

NOINLINE
void CPUFiq()
{
	u32 PC = reg[R15_ARM_NEXT].I+4;
	CPUSwitchMode(0x11, true);
	reg[14].I = PC;
	armIrqEnable = false;
	armFiqEnable = false;
	update_armintc();

	armNextPC = 0x1c;
}

/*
	--Seems like aica has 3 interrupt controllers actualy (damn lazy sega ..)
	The "normal" one (the one that exists on scsp) , one to emulate the 68k intc , and , 
	of course , the arm7 one

	The output of the sci* bits is input to the e68k , and the output of e68k is inputed into the FIQ
	pin on arm7
*/


void aicaarm::enable(bool enabled)
{
	if(!Arm7Enabled && enabled)
		aicaarm::reset();
	
	Arm7Enabled=enabled;
}

void update_armintc()
{
	reg[INTR_PEND].I=e68k_out && armFiqEnable;
}

#if FEAT_AREC != DYNAREC_NONE
//
// Used by ARM7 Recompiler
//
namespace aicaarm {

namespace recompiler {

//Emulate a single arm op, passed in opcode

void DYNACALL interpret(u32 opcode)
{
	u32 clockTicks = 0;

#define NO_OPCODE_READ
#include "arm-new.h"
#undef NO_OPCODE_READ

	reg[CYCL_CNT].I -= clockTicks;
}

template<u32 Pd>
void DYNACALL MSR_do(u32 v)
{
	if (Pd)
	{
		if(armMode > 0x10 && armMode < 0x1f) /* !=0x10 ?*/
		{
			reg[RN_SPSR].I = (reg[RN_SPSR].I & 0x00FFFF00) | (v & 0xFF0000FF);
		}
	}
	else
	{
		CPUUpdateCPSR();
	
		u32 newValue = reg[RN_CPSR].I;
		if(armMode > 0x10)
		{
			newValue = (newValue & 0xFFFFFF00) | (v & 0x000000FF);
		}

		newValue = (newValue & 0x00FFFFFF) | (v & 0xFF000000);
		newValue |= 0x10;
		if(armMode > 0x10)
		{
			CPUSwitchMode(newValue & 0x1f, false);
		}
		reg[RN_CPSR].I = newValue;
		CPUUpdateFlags();
	}
}
template void DYNACALL MSR_do<0>(u32 v);
template void DYNACALL MSR_do<1>(u32 v);

}
}
#endif	// FEAT_AREC != DYNAREC_NONE

