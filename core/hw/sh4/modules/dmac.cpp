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

DMACRegisters dmac;

void DMAC_Ch2St()
{
	u32 dmaor = DMAC_DMAOR.full;

	u32 src = DMAC_SAR(2) & 0x1fffffe0;
	u32 dst = SB_C2DSTAT & 0x01ffffe0;
	u32 len = SB_C2DLEN;

	if (0x8201 != (dmaor & DMAOR_MASK))
	{
		INFO_LOG(SH4, "DMAC: DMAOR has invalid settings (%X) !", dmaor);
		return;
	}
	if ((src >> 26) != 3)
	{
		// Source address must be in system RAM
		WARN_LOG(SH4, "DMAC: invalid source address %x dest %x len %x", DMAC_SAR(2), SB_C2DSTAT, SB_C2DLEN);
		DMAC_DMAOR.AE = 1;
		asic_RaiseInterrupt(holly_CH2_DMA);
		return;
	}

	DEBUG_LOG(SH4, ">> DMAC: Ch2 DMA SRC=%X DST=%X LEN=%X", src, SB_C2DSTAT, SB_C2DLEN);

	// Direct DList DMA (Ch2)

	// TA FIFO - Polygon and YUV converter paths and mirror
	// 10000000 - 10FFFFE0
	// 12000000 - 12FFFFE0
	if ((dst & 0x01000000) == 0)
	{
		if ((src & RAM_MASK) + len > RAM_SIZE)
		{
			u32 newLen = RAM_SIZE - (src & RAM_MASK);
			SQBuffer *psrc = (SQBuffer *)GetMemPtr(src, newLen);
			TAWrite(dst, psrc, newLen / sizeof(SQBuffer));
			len -= newLen;
			src += newLen;
		}
		SQBuffer *psrc = (SQBuffer *)GetMemPtr(src, len);
		TAWrite(dst, psrc, len / sizeof(SQBuffer));
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
static void WriteCHCR(u32 addr, u32 data)
{
	if constexpr (ch == 0 || ch == 1)
		DMAC_CHCR(ch).full = data & 0xff0ffff7;
	else
		// no AL or RL on channels 2 and 3
		DMAC_CHCR(ch).full = data & 0xff0afff7;

	if (DMAC_CHCR(ch).TE == 0 && DMAC_CHCR(ch).DE && DMAC_DMAOR.DME)
	{
		if (DMAC_CHCR(ch).RS == 4)
		{
			DEBUG_LOG(SH4, "DMAC: Manual DMA ch:%d TS:%d src: %08X dst: %08X len: %08X SM: %d, DM: %d", ch, DMAC_CHCR(ch).TS,
					DMAC_SAR(ch), DMAC_DAR(ch), DMAC_DMATCR(ch), DMAC_CHCR(ch).SM, DMAC_CHCR(ch).DM);
			u32 src = DMAC_SAR(ch);
			u32 len = DMAC_DMATCR(ch);
			u32 dst = DMAC_DAR(ch);

			int srcIncr, dstIncr;
			switch (DMAC_CHCR(ch).SM)
			{
			case 1:
				srcIncr = 1;
				break;
			case 2:
				srcIncr = -1;
				break;
			default:
				srcIncr = 0;
				break;
			}
			switch (DMAC_CHCR(ch).DM)
			{
			case 1:
				dstIncr = 1;
				break;
			case 2:
				dstIncr = -1;
				break;
			default:
				dstIncr = 0;
				break;
			}

			switch (DMAC_CHCR(ch).TS)
			{
			case 0:	// 64 bits
				srcIncr *= sizeof(u64);
				dstIncr *= sizeof(u64);
				for (; len != 0; len--)
				{
					u64 data = addrspace::read64(src);
					addrspace::write64(dst, data);
					src += srcIncr;
					dst += dstIncr;
				}
				break;

			case 1: // 8 bits
				for (; len != 0; len--)
				{
					u8 data = addrspace::read8(src);
					addrspace::write8(dst, data);
					src += srcIncr;
					dst += dstIncr;
				}
				break;

			case 2: // 16 bits
				srcIncr *= sizeof(u16);
				dstIncr *= sizeof(u16);
				for (; len != 0; len--)
				{
					u16 data = addrspace::read16(src);
					addrspace::write16(dst, data);
					src += srcIncr;
					dst += dstIncr;
				}
                break;

			case 4: // 32-byte block
				len *= 32 / sizeof(u32);
				[[fallthrough]];

            default: // 32 bits
				srcIncr *= sizeof(u32);
				dstIncr *= sizeof(u32);
				for (; len != 0; len--)
				{
					u32 data = addrspace::read32(src);
					addrspace::write32(dst, data);
					src += srcIncr;
					dst += dstIncr;
				}
				break;
            }
            DMAC_CHCR(ch).TE = 1;
           	DMAC_SAR(ch) = src;
           	DMAC_DAR(ch) = dst;
           	DMAC_DMATCR(ch) = len;
        }

        InterruptPend(dmac_itr[ch], DMAC_CHCR(ch).TE);
        InterruptMask(dmac_itr[ch], DMAC_CHCR(ch).IE);
    }
}

//Init term res
void DMACRegisters::init()
{
	super::init();

	//DMAC SAR0 0xFFA00000 0x1FA00000 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_SAR0_addr>();

	//DMAC DAR0 0xFFA00004 0x1FA00004 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DAR0_addr>();

	//DMAC DMATCR0 0xFFA00008 0x1FA00008 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DMATCR0_addr, u32, 0x00ffffff>();

	//DMAC CHCR0 0xFFA0000C 0x1FA0000C 32 0x00000000 0x00000000 Held Held Bclk
	setWriteHandler<DMAC_CHCR0_addr>(WriteCHCR<0>);

	//DMAC SAR1 0xFFA00010 0x1FA00010 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_SAR1_addr>();

	//DMAC DAR1 0xFFA00014 0x1FA00014 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DAR1_addr>();

	//DMAC DMATCR1 0xFFA00018 0x1FA00018 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DMATCR1_addr, u32, 0x00ffffff>();

	//DMAC CHCR1 0xFFA0001C 0x1FA0001C 32 0x00000000 0x00000000 Held Held Bclk
	setWriteHandler<DMAC_CHCR1_addr>(WriteCHCR<1>);

	//DMAC SAR2 0xFFA00020 0x1FA00020 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_SAR2_addr>();

	//DMAC DAR2 0xFFA00024 0x1FA00024 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DAR2_addr>();

	//DMAC DMATCR2 0xFFA00028 0x1FA00028 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DMATCR2_addr, u32, 0x00ffffff>();

	//DMAC CHCR2 0xFFA0002C 0x1FA0002C 32 0x00000000 0x00000000 Held Held Bclk
	setWriteHandler<DMAC_CHCR2_addr>(WriteCHCR<2>);

	//DMAC SAR3 0xFFA00030 0x1FA00030 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_SAR3_addr>();

	//DMAC DAR3 0xFFA00034 0x1FA00034 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DAR3_addr>();

	//DMAC DMATCR3 0xFFA00038 0x1FA00038 32 Undefined Undefined Held Held Bclk
	setRW<DMAC_DMATCR3_addr, u32, 0x00ffffff>();

	//DMAC CHCR3 0xFFA0003C 0x1FA0003C 32 0x00000000 0x00000000 Held Held Bclk
	setWriteHandler<DMAC_CHCR3_addr>(WriteCHCR<3>);

	//DMAC DMAOR 0xFFA00040 0x1FA00040 32 0x00000000 0x00000000 Held Held Bclk
	setRW<DMAC_DMAOR_addr, u32, 0x00008307>();

	reset();
}
