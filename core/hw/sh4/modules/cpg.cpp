#include "hw/hwreg.h"
#include "hw/sh4/sh4_mmr.h"
#include "modules.h"

CPGRegisters cpg;

void CPGRegisters::init()
{
	super::init();

	//CPG FRQCR H'FFC0 0000 H'1FC0 0000 16 *2 Held Held Held Pclk
	setRW<CPG_FRQCR_addr, u16, 0x0fff>();

	//CPG STBCR H'FFC0 0004 H'1FC0 0004 8 H'00 Held Held Held Pclk
	setRW<CPG_STBCR_addr, u8>();

	//CPG WTCNT H'FFC0 0008 H'1FC0 0008 8/16*3 H'00 Held Held Held Pclk
	// Need special pattern 0x5A in upper 8 bits on write. Not currently checked
	setRW<CPG_WTCNT_addr, u8>();

	//CPG WTCSR H'FFC0 000C H'1FC0 000C 8/16*3 H'00 Held Held Held Pclk
	// Need special pattern 0x5A in upper 8 bits on write. Not currently checked
	setRW<CPG_WTCSR_addr, u16, 0xff>();

	//CPG STBCR2 H'FFC0 0010 H'1FC0 0010 8 H'00 Held Held Held Pclk
	setRW<CPG_STBCR2_addr, u8, 0x80>();

	reset();
}
