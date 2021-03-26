/*
	DMAC is not really emulated on nullDC. We just fake the dmas ;p
		Dreamcast uses sh4's dmac in ddt mode to multiplex ch0 and ch2 for dma access.
		nullDC just 'fakes' each dma as if it was a full channel, never bothering properly
		updating the dmac regs -- it works just fine really :|
*/
#include "types.h"
#include "hw/sh4/sh4_mmr.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/pvr/pvr_mem.h"
#include "dmac.h"
#include "hw/sh4/sh4_interrupts.h"
#include "hw/holly/holly_intc.h"

void DMAC_Ch2St()
{
	u32 dmaor = DMAC_DMAOR.full;

	u32 src = DMAC_SAR(2) & 0x1fffffe0;
	u32 dst = SB_C2DSTAT & 0x01ffffe0;
	u32 len = SB_C2DLEN & 0x00ffffe0;

	if (0x8201 != (dmaor & DMAOR_MASK))
	{
		INFO_LOG(SH4, "DMAC: DMAOR has invalid settings (%X) !", dmaor);
		return;
	}

	DEBUG_LOG(SH4, ">> DMAC: Ch2 DMA SRC=%X DST=%X LEN=%X", src, SB_C2DSTAT, SB_C2DLEN);

	// Direct DList DMA (Ch2)

	// TA FIFO - Polygon and YUV converter paths and mirror
	// 10000000 - 10FFFFE0
	// 12000000 - 12FFFFE0
	if ((dst & 0x01000000) == 0)
	{
		SQBuffer *sys_buf = (SQBuffer *)GetMemPtr(src, len);
		if ((src & RAM_MASK) + len > RAM_SIZE)
		{
			u32 newLen = RAM_SIZE - (src & RAM_MASK);
			TAWrite(dst, sys_buf, newLen / sizeof(SQBuffer));
			len -= newLen;
			src += newLen;
			sys_buf = (SQBuffer *)GetMemPtr(src, len);
		}
		TAWrite(dst, sys_buf, len / sizeof(SQBuffer));
		src += len;
	}
	// Direct Texture path and mirror
	// 11000000 - 11FFFFE0
	// 13000000 - 13FFFFE0
	else
	{
		bool path64b = SB_C2DSTAT & 0x02000000 ? SB_LMMODE1 == 0 : SB_LMMODE0 == 0;

		if (path64b)
		{
			// 64-bit path
			dst = (dst & 0x00FFFFFF) | 0xa4000000;
			if ((src & RAM_MASK) + len > RAM_SIZE)
			{
				u32 newLen = RAM_SIZE - (src & RAM_MASK);
				WriteMemBlock_nommu_dma(dst, src, newLen);
				len -= newLen;
				src += newLen;
				dst += newLen;
			}
			WriteMemBlock_nommu_dma(dst, src, len);
			src += len;
			dst += len;
		}
		else
		{
			// 32-bit path
			dst = (dst & 0xFFFFFF) | 0xa5000000;
			while (len > 0)
			{
				u32 v = ReadMem32_nommu(src);
				pvr_write32p<u32>(dst, v);
				len -= 4;
				src += 4;
				dst += 4;
			}
		}
		SB_C2DSTAT = dst;
	}

	// Setup some of the regs so it thinks we've finished DMA

	DMAC_CHCR(2).TE = 1;
	DMAC_DMATCR(2) = 0;

	SB_C2DST = 0;
	SB_C2DLEN = 0;

	asic_RaiseInterrupt(holly_CH2_DMA);
}

static const InterruptID dmac_itr[] = { sh4_DMAC_DMTE0, sh4_DMAC_DMTE1, sh4_DMAC_DMTE2, sh4_DMAC_DMTE3 };

template<u32 ch>
void WriteCHCR(u32 addr, u32 data)
{
	if (ch == 0 || ch == 1)
		DMAC_CHCR(ch).full = data & 0xff0ffff7;
	else
		// no AL or RL on channels 2 and 3
		DMAC_CHCR(ch).full = data & 0xff0afff7;

	if (DMAC_CHCR(ch).TE == 0 && DMAC_CHCR(ch).DE && DMAC_DMAOR.DME)
    {
		if (DMAC_CHCR(ch).RS == 4)
        {
			u32 len = DMAC_DMATCR(ch) * 32;

            DEBUG_LOG(SH4, "DMAC: Manual DMA ch:%d TS:%d src: %08X dst: %08X len: %08X SM: %d, DM: %d", ch, DMAC_CHCR(ch).TS,
            		DMAC_SAR(ch), DMAC_DAR(ch), DMAC_DMATCR(ch), DMAC_CHCR(ch).SM, DMAC_CHCR(ch).DM);
            verify(DMAC_CHCR(ch).TS == 4);
            for (u32 ofs = 0; ofs < len; ofs += 4)
            {
                u32 data = ReadMem32_nommu(DMAC_SAR(ch) + ofs);
                WriteMem32_nommu(DMAC_DAR(ch) + ofs, data);
            }

            DMAC_CHCR(ch).TE = 1;
            if (DMAC_CHCR(ch).SM == 1)
            	DMAC_SAR(ch) += len;
            else if (DMAC_CHCR(ch).SM == 2)
            	DMAC_SAR(ch) -= len;
            if (DMAC_CHCR(ch).DM == 1)
            	DMAC_DAR(ch) += len;
            else if (DMAC_CHCR(ch).DM == 2)
            	DMAC_DAR(ch) -= len;
        }

        InterruptPend(dmac_itr[ch], DMAC_CHCR(ch).TE);
        InterruptMask(dmac_itr[ch], DMAC_CHCR(ch).IE);
    }
}

void WriteDMAOR(u32 addr, u32 data)
{
	DMAC_DMAOR.full = data;
}

//Init term res
void dmac_init()
{
	//DMAC SAR0 0xFFA00000 0x1FA00000 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_SAR0_addr,RIO_DATA,32);

	//DMAC DAR0 0xFFA00004 0x1FA00004 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DAR0_addr,RIO_DATA,32);

	//DMAC DMATCR0 0xFFA00008 0x1FA00008 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DMATCR0_addr,RIO_DATA,32);

	//DMAC CHCR0 0xFFA0000C 0x1FA0000C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_CHCR0_addr,RIO_WF,32,0,&WriteCHCR<0>);

	//DMAC SAR1 0xFFA00010 0x1FA00010 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_SAR1_addr,RIO_DATA,32);

	//DMAC DAR1 0xFFA00014 0x1FA00014 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DAR1_addr,RIO_DATA,32);

	//DMAC DMATCR1 0xFFA00018 0x1FA00018 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DMATCR1_addr,RIO_DATA,32);

	//DMAC CHCR1 0xFFA0001C 0x1FA0001C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_CHCR1_addr,RIO_WF,32,0,&WriteCHCR<1>);

	//DMAC SAR2 0xFFA00020 0x1FA00020 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_SAR2_addr,RIO_DATA,32);

	//DMAC DAR2 0xFFA00024 0x1FA00024 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DAR2_addr,RIO_DATA,32);

	//DMAC DMATCR2 0xFFA00028 0x1FA00028 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DMATCR2_addr,RIO_DATA,32);

	//DMAC CHCR2 0xFFA0002C 0x1FA0002C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_CHCR2_addr,RIO_WF,32,0,&WriteCHCR<2>);

	//DMAC SAR3 0xFFA00030 0x1FA00030 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_SAR3_addr,RIO_DATA,32);

	//DMAC DAR3 0xFFA00034 0x1FA00034 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DAR3_addr,RIO_DATA,32);

	//DMAC DMATCR3 0xFFA00038 0x1FA00038 32 Undefined Undefined Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DMATCR3_addr,RIO_DATA,32);

	//DMAC CHCR3 0xFFA0003C 0x1FA0003C 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_CHCR3_addr,RIO_WF,32,0,&WriteCHCR<3>);

	//DMAC DMAOR 0xFFA00040 0x1FA00040 32 0x00000000 0x00000000 Held Held Bclk
	sh4_rio_reg(DMAC,DMAC_DMAOR_addr,RIO_WF,32,0,&WriteDMAOR);
}
void dmac_reset()
{
	/*
	DMAC SAR0 H'FFA0 0000 H'1FA0 0000 32 Undefined Undefined Held Held Bclk
	DMAC DAR0 H'FFA0 0004 H'1FA0 0004 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR0 H'FFA0 0008 H'1FA0 0008 32 Undefined Undefined Held Held Bclk
	DMAC CHCR0 H'FFA0 000C H'1FA0 000C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC SAR1 H'FFA0 0010 H'1FA0 0010 32 Undefined Undefined Held Held Bclk
	DMAC DAR1 H'FFA0 0014 H'1FA0 0014 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR1 H'FFA0 0018 H'1FA0 0018 32 Undefined Undefined Held Held Bclk
	DMAC CHCR1 H'FFA0 001C H'1FA0 001C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC SAR2 H'FFA0 0020 H'1FA0 0020 32 Undefined Undefined Held Held Bclk
	DMAC DAR2 H'FFA0 0024 H'1FA0 0024 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR2 H'FFA0 0028 H'1FA0 0028 32 Undefined Undefined Held Held Bclk
	DMAC CHCR2 H'FFA0 002C H'1FA0 002C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC SAR3 H'FFA0 0030 H'1FA0 0030 32 Undefined Undefined Held Held Bclk
	DMAC DAR3 H'FFA0 0034 H'1FA0 0034 32 Undefined Undefined Held Held Bclk
	DMAC DMATCR3 H'FFA0 0038 H'1FA0 0038 32 Undefined Undefined Held Held Bclk
	DMAC CHCR3 H'FFA0 003C H'1FA0 003C 32 H'0000 0000 H'0000 0000 Held Held Bclk
	DMAC DMAOR H'FFA0 0040 H'1FA0 0040 32 H'0000 0000 H'0000 0000 Held Held Bclk
	*/
	DMAC_CHCR(0).full = 0x0;
	DMAC_CHCR(1).full = 0x0;
	DMAC_CHCR(2).full = 0x0;
	DMAC_CHCR(3).full = 0x0;
	DMAC_DMAOR.full = 0x0;
}
void dmac_term()
{
}
