//ubc is disabled on dreamcast and can't be used ... but kos-debug uses it !...

#include "hw/hwreg.h"
#include "hw/sh4/sh4_mmr.h"
#include "modules.h"

UBCRegisters ubc;

void UBCRegisters::init()
{
	super::init();

	//UBC BARA 0xFF200000 0x1F200000 32 Undefined Held Held Held Iclk
	setRW<UBC_BARA_addr>();

	//UBC BAMRA 0xFF200004 0x1F200004 8 Undefined Held Held Held Iclk
	setRW<UBC_BAMRA_addr, u8, 0x0f>();

	//UBC BBRA 0xFF200008 0x1F200008 16 0x0000 Held Held Held Iclk
	setRW<UBC_BBRA_addr, u16, 0x007f>();

	//UBC BARB 0xFF20000C 0x1F20000C 32 Undefined Held Held Held Iclk
	setRW<UBC_BARB_addr>();

	//UBC BAMRB 0xFF200010 0x1F200010 8 Undefined Held Held Held Iclk
	setRW<UBC_BAMRB_addr, u8, 0x0f>();

	//UBC BBRB 0xFF200014 0x1F200014 16 0x0000 Held Held Held Iclk
	setRW<UBC_BBRB_addr, u16, 0x007f>();

	//UBC BDRB 0xFF200018 0x1F200018 32 Undefined Held Held Held Iclk
	setRW<UBC_BDRB_addr>();

	//UBC BDMRB 0xFF20001C 0x1F20001C 32 Undefined Held Held Held Iclk
	setRW<UBC_BDMRB_addr>();

	//UBC BRCR 0xFF200020 0x1F200020 16 0x0000 Held Held Held Iclk
	setRW<UBC_BRCR_addr, u16, 0xc4c9>();

	reset();
}
