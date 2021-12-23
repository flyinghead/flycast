#pragma once
#include "types.h"

namespace aicaarm {

void init();
void reset();
void run(u32 samples);
void enable(bool enabled);
// Called when the arm interrupts the SH4 to make sure it has enough cycles to finish what it's doing.
void avoidRaceCondition();
}

enum Arm7Reg
{
	RN_LR		 = 14,
	RN_PC		 = 15,
	RN_CPSR      = 16,
	RN_SPSR      = 17,

	R13_IRQ      = 18,
	R14_IRQ      = 19,
	SPSR_IRQ     = 20,
	R13_USR      = 26,
	R14_USR      = 27,
	R13_SVC      = 28,
	R14_SVC      = 29,
	SPSR_SVC     = 30,
	R13_ABT      = 31,
	R14_ABT      = 32,
	SPSR_ABT     = 33,
	R13_UND      = 34,
	R14_UND      = 35,
	SPSR_UND     = 36,
	R8_FIQ       = 37,
	R9_FIQ       = 38,
	R10_FIQ      = 39,
	R11_FIQ      = 40,
	R12_FIQ      = 41,
	R13_FIQ      = 42,
	R14_FIQ      = 43,
	SPSR_FIQ     = 44,
	RN_PSR_FLAGS = 45,
	R15_ARM_NEXT = 46,
	INTR_PEND    = 47,
	CYCL_CNT     = 48,
	RN_SCRATCH   = 49,

	RN_ARM_REG_COUNT,
};

typedef union
{
	struct
	{
		u8 B0;
		u8 B1;
		u8 B2;
		u8 B3;
	} B;

	struct
	{
		u16 W0;
		u16 W1;
	} W;

	union
	{
		struct
		{
			u32 _pad0 : 28;
			u32 V     : 1; //Bit 28
			u32 C     : 1; //Bit 29
			u32 Z     : 1; //Bit 30
			u32 N     : 1; //Bit 31
		};

		struct
		{
			u32 _pad1 : 28;
			u32 NZCV  : 4; //Bits [31:28]
		};
	} FLG;

	struct
	{
		u32 M     : 5;  //mode, PSR[4:0]
		u32 _pad0 : 1;  //not used / zero
		u32 F     : 1;  //FIQ disable, PSR[6]
		u32 I     : 1;  //IRQ disable, PSR[7]
		u32 _pad1 : 20; //not used / zero
		u32 NZCV  : 4;  //Bits [31:28]
	} PSR;

	u32 I;
} reg_pair;

alignas(8) extern reg_pair arm_Reg[RN_ARM_REG_COUNT];

#define ARM_CYCLES_PER_SAMPLE 256
extern int arm7ClockTicks;

void CPUFiq();
void CPUUpdateCPSR();
