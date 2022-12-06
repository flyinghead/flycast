#include "types.h"
#include "hw/sh4/sh4_mmr.h"

//Init term res
void cpg_init()
{
	//CPG FRQCR H'FFC0 0000 H'1FC0 0000 16 *2 Held Held Held Pclk
	sh4_rio_reg_wmask<CPG, CPG_FRQCR_addr, 0x0fff>();

	//CPG STBCR H'FFC0 0004 H'1FC0 0004 8 H'00 Held Held Held Pclk
	sh4_rio_reg(CPG, CPG_STBCR_addr, RIO_DATA);

	//CPG WTCNT H'FFC0 0008 H'1FC0 0008 8/16*3 H'00 Held Held Held Pclk
	// Need special pattern 0x5A in upper 8 bits on write. Not currently checked
	sh4_rio_reg8<CPG, CPG_WTCNT_addr>();

	//CPG WTCSR H'FFC0 000C H'1FC0 000C 8/16*3 H'00 Held Held Held Pclk
	// Need special pattern 0x5A in upper 8 bits on write. Not currently checked
	sh4_rio_reg8<CPG, CPG_WTCSR_addr>();

	//CPG STBCR2 H'FFC0 0010 H'1FC0 0010 8 H'00 Held Held Held Pclk
	sh4_rio_reg_wmask<CPG, CPG_STBCR2_addr, 0x80>();
}

void cpg_reset()
{
	/*
	CPG FRQCR H'FFC0 0000 H'1FC0 0000 16 *2 Held Held Held Pclk
	CPG STBCR H'FFC0 0004 H'1FC0 0004 8 H'00 Held Held Held Pclk
	CPG WTCNT H'FFC0 0008 H'1FC0 0008 8/16*3 H'00 Held Held Held Pclk
	CPG WTCSR H'FFC0 000C H'1FC0 000C 8/16*3 H'00 Held Held Held Pclk
	CPG STBCR2 H'FFC0 0010 H'1FC0 0010 8 H'00 Held Held Held Pclk
	*/
	CPG_FRQCR = 0;
	CPG_STBCR = 0;
	CPG_WTCNT = 0;
	CPG_WTCSR = 0;
	CPG_STBCR2 = 0;
}

void cpg_term()
{
}
