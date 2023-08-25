//Bus state controller registers

#include "types.h"
#include "hw/sh4/sh4_mmr.h"

#include "hw/naomi/naomi.h"
#include "cfg/option.h"
#include "modules.h"

BSCRegisters bsc;

//u32 port_out_data;
static void write_BSC_PDTRA_arcade(u32 addr, u16 data)
{
	BSC_PDTRA.full = data;
	NaomiBoardIDWrite(data);
}

static void write_BSC_PDTRA(u32 addr, u16 data)
{
	BSC_PDTRA.full = data;
}

static u16 read_BSC_PDTRA_arcade(u32 addr)
{
	return NaomiBoardIDRead();
}

static u16 read_BSC_PDTRA(u32 addr)
{
	/* as seen on chankast */
	u32 tpctra = BSC_PCTRA.full;
	u32 tpdtra = BSC_PDTRA.full;

	u16 tfinal = 0;
	// magic values
	if ((tpctra & 0xf) == 0x8)
		tfinal = 3;
	else if ((tpctra & 0xf) == 0xB)
		tfinal = 3;
	else
		tfinal = 0;

	if ((tpctra & 0xf) == 0xB && (tpdtra & 0xf) == 2)
		tfinal = 0;
	else if ((tpctra & 0xf) == 0xC && (tpdtra & 0xf) == 2)
		tfinal = 3;

	tfinal |= config::Cable << 8;

	return tfinal;
}

void BSCRegisters::init()
{
	super::init();

	//BSC BCR1 0xFF800000 0x1F800000 32 0x00000000 Held Held Held Bclk
	setRW<BSC_BCR1_addr, u32, 0x033efffd>();

	//BSC BCR2 0xFF800004 0x1F800004 16 0x3FFC Held Held Held Bclk
	setRW<BSC_BCR2_addr, u16, 0x3ffd>();

	//BSC WCR1 0xFF800008 0x1F800008 32 0x77777777 Held Held Held Bclk
	setRW<BSC_WCR1_addr, u32, 0x77777777>();

	//BSC WCR2 0xFF80000C 0x1F80000C 32 0xFFFEEFFF Held Held Held Bclk
	setRW<BSC_WCR2_addr, u32, 0xfffeefff>();

	//BSC WCR3 0xFF800010 0x1F800010 32 0x07777777 Held Held Held Bclk
	setRW<BSC_WCR3_addr, u32, 0x07777777>();

	//BSC MCR 0xFF800014 0x1F800014 32 0x00000000 Held Held Held Bclk
	setRW<BSC_MCR_addr, u32, 0xf8bbffff>();

	//BSC PCR 0xFF800018 0x1F800018 16 0x0000 Held Held Held Bclk
	setRW<BSC_PCR_addr, u16>();

	//BSC RTCSR 0xFF80001C 0x1F80001C 16 0x0000 Held Held Held Bclk
	setRW<BSC_RTCSR_addr, u16, 0x00ff>();

	//BSC RTCNT 0xFF800020 0x1F800020 16 0x0000 Held Held Held Bclk
	setRW<BSC_RTCNT_addr, u16, 0x00ff>();

	//BSC RTCOR 0xFF800024 0x1F800024 16 0x0000 Held Held Held Bclk
	setRW<BSC_RTCOR_addr, u16, 0x00ff>();

	//BSC RFCR 0xFF800028 0x1F800028 16 0x0000 Held Held Held Bclk
	// forced to 17 to help naomi/aw boot
	setReadOnly<BSC_RFCR_addr, u16>();

	//BSC PCTRA 0xFF80002C 0x1F80002C 32 0x00000000 Held Held Held Bclk
	// Naomi BIOS writes u16 in this register but ignoring them doesn't seem to hurt
	setRW<BSC_PCTRA_addr, u32>();

	//BSC PDTRA 0xFF800030 0x1F800030 16 Undefined Held Held Held Bclk
	setRW<BSC_PDTRA_addr, u16>();

	//BSC PCTRB 0xFF800040 0x1F800040 32 0x00000000 Held Held Held Bclk
	setRW<BSC_PCTRB_addr, u32, 0x000000ff>();

	//BSC PDTRB 0xFF800044 0x1F800044 16 Undefined Held Held Held Bclk
	setRW<BSC_PDTRB_addr, u16, 0x000f>();

	//BSC GPIOIC 0xFF800048 0x1F800048 16 0x00000000 Held Held Held Bclk
	setRW<BSC_GPIOIC_addr, u16>();

	reset();
}

void BSCRegisters::reset()
{
	super::reset();

	/*
	BSC BCR1 H'FF80 0000 H'1F80 0000 32 H'0000 0000*2 Held Held Held Bclk
	BSC BCR2 H'FF80 0004 H'1F80 0004 16 H'3FFC*2 Held Held Held Bclk
	BSC WCR1 H'FF80 0008 H'1F80 0008 32 H'7777 7777 Held Held Held Bclk
	BSC WCR2 H'FF80 000C H'1F80 000C 32 H'FFFE EFFF Held Held Held Bclk
	BSC WCR3 H'FF80 0010 H'1F80 0010 32 H'0777 7777 Held Held Held Bclk

	BSC MCR H'FF80 0014 H'1F80 0014 32 H'0000 0000 Held Held Held Bclk
	BSC PCR H'FF80 0018 H'1F80 0018 16 H'0000 Held Held Held Bclk
	BSC RTCSR H'FF80 001C H'1F80 001C 16 H'0000 Held Held Held Bclk
	BSC RTCNT H'FF80 0020 H'1F80 0020 16 H'0000 Held Held Held Bclk
	BSC RTCOR H'FF80 0024 H'1F80 0024 16 H'0000 Held Held Held Bclk
	BSC RFCR H'FF80 0028 H'1F80 0028 16 H'0000 Held Held Held Bclk
	BSC PCTRA H'FF80 002C H'1F80 002C 32 H'0000 0000 Held Held Held Bclk
	BSC PDTRA H'FF80 0030 H'1F80 0030 16 Undefined Held Held Held Bclk
	BSC PCTRB H'FF80 0040 H'1F80 0040 32 H'0000 0000 Held Held Held Bclk
	BSC PDTRB H'FF80 0044 H'1F80 0044 16 Undefined Held Held Held Bclk
	BSC GPIOIC H'FF80 0048 H'1F80 0048 16 H'0000 0000 Held Held Held Bclk
	BSC SDMR2 H'FF90 xxxx H'1F90 xxxx 8 Write-only Bclk
	BSC SDMR3 H'FF94 xxxx H'1F94 xxxx 8 Bclk
	*/
	BSC_BCR2.full = 0x3FFC;
	BSC_WCR1.full = 0x77777777;
	BSC_WCR2.full = 0xFFFEEFFF;
	BSC_WCR3.full = 0x07777777;

	BSC_RFCR.full = 17;

	if (settings.platform.isNaomi() || settings.platform.isSystemSP())
		setHandlers<BSC_PDTRA_addr>(read_BSC_PDTRA_arcade, write_BSC_PDTRA_arcade);
	else
		setHandlers<BSC_PDTRA_addr>(read_BSC_PDTRA, write_BSC_PDTRA);
}
