#pragma once
#include "sh4_if.h"

enum InterruptID
{
	//internal interrupts
	//IRL*
	//sh4_IRL_0 -> these are not connected on the Dreamcast
	//sh4_IRL_1
	//sh4_IRL_2
	//sh4_IRL_3
	//sh4_IRL_4
	//sh4_IRL_5
	//sh4_IRL_6
	//sh4_IRL_7
	//sh4_IRL_8
	sh4_IRL_9       = 0,
	//sh4_IRL_10 -> these are not connected on the Dreamcast
	sh4_IRL_11      = 1,
	//sh4_IRL_12 -> these are not connected on the Dreamcast
	sh4_IRL_13      = 2,
	//sh4_IRL_14 -> these are not connected on the Dreamcast
	//sh4_IRL_15 -> no interrupt (masked)

	sh4_HUDI_HUDI   = 3,  // H-UDI underflow

	sh4_GPIO_GPIOI  = 4,

	//DMAC
	sh4_DMAC_DMTE0  = 5,
	sh4_DMAC_DMTE1  = 6,
	sh4_DMAC_DMTE2  = 7,
	sh4_DMAC_DMTE3  = 8,
	sh4_DMAC_DMAE   = 9,

	//TMU
	sh4_TMU0_TUNI0  =  10, // TMU0 underflow
	sh4_TMU1_TUNI1  =  11, // TMU1 underflow
	sh4_TMU2_TUNI2  =  12, // TMU2 underflow
	sh4_TMU2_TICPI2 =  13, // TMU Compare (not used in the Dreamcast)

	//RTC
	sh4_RTC_ATI     = 14,
	sh4_RTC_PRI     = 15,
	sh4_RTC_CUI     = 16,

	//SCI
	sh4_SCI1_ERI    = 17,
	sh4_SCI1_RXI    = 18,
	sh4_SCI1_TXI    = 19,
	sh4_SCI1_TEI    = 20,

	//SCIF
	sh4_SCIF_ERI    = 21,
	sh4_SCIF_RXI    = 22,
	sh4_SCIF_BRI    = 23,
	sh4_SCIF_TXI    = 24,

	//WDT
	sh4_WDT_ITI     = 25,

	//REF
	sh4_REF_RCMI    = 26,
	sh4_REF_ROVI    = 27,

	sh4_INT_ID_COUNT
};

void SetInterruptPend(InterruptID intr);
void ResetInterruptPend(InterruptID intr);
inline static void InterruptPend(InterruptID intr, bool v)
{
	if (!v)
		ResetInterruptPend(intr);
	else
		SetInterruptPend(intr);
}

void SetInterruptMask(InterruptID intr);
void ResetInterruptMask(InterruptID intr);
inline static void InterruptMask(InterruptID intr, bool v)
{
	if (!v)
		ResetInterruptMask(intr);
	else
		SetInterruptMask(intr);
}

int UpdateINTC();

void Do_Exception(u32 epc, Sh4ExceptionCode expEvn);

bool SRdecode();
void SIIDRebuild();

//Init/Res/Term
void interrupts_init();
void interrupts_reset();
void interrupts_term();
void interrupts_serialize(Serializer& ser);
void interrupts_deserialize(Deserializer& deser);
