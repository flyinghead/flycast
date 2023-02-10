/*
	SH4/mod/intc

	Implements the register interface of the sh4 interrupt controller.
	For the actual implementation of interrupt caching/handling logic, look at sh4_interrupts.cpp

	--
*/

#include "types.h"
#include "../sh4_interrupts.h"
#include "../sh4_mmr.h"

INTCRegisters intc;

//Register writes need interrupt re-testing !

static void write_INTC_IPRA(u32 addr, u16 data)
{
	if (INTC_IPRA.reg_data != data)
	{
		INTC_IPRA.reg_data = data;
		SIIDRebuild();	//we need to rebuild the table
	}
}

static void write_INTC_IPRB(u32 addr, u16 data)
{
	if (INTC_IPRB.reg_data != data)
	{
		INTC_IPRB.reg_data = data;
		SIIDRebuild(); //we need to rebuild the table
	}
}

static void write_INTC_IPRC(u32 addr, u16 data)
{
	if (INTC_IPRC.reg_data != data)
	{
		INTC_IPRC.reg_data = data;
		SIIDRebuild(); //we need to rebuild the table
	}
}

static u16 read_INTC_IPRD(u32 addr)
{
	return 0;
}

//Init/Res/Term
void INTCRegisters::init()
{
	super::init();

	//INTC ICR 0xFFD00000 0x1FD00000 16 0x0000 0x0000 Held Held Pclk
	setRW<INTC_ICR_addr, u16, 0x4380>();

	//INTC IPRA 0xFFD00004 0x1FD00004 16 0x0000 0x0000 Held Held Pclk
	setWriteHandler<INTC_IPRA_addr>(write_INTC_IPRA);

	//INTC IPRB 0xFFD00008 0x1FD00008 16 0x0000 0x0000 Held Held Pclk
	setWriteHandler<INTC_IPRB_addr>(write_INTC_IPRB);

	//INTC IPRC 0xFFD0000C 0x1FD0000C 16 0x0000 0x0000 Held Held Pclk
	setWriteHandler<INTC_IPRC_addr>(write_INTC_IPRC);

	//INTC IPRD 0xFFD00010 0x1FD00010 16 0xDA74 0xDA74 Held Held Pclk	(SH7750S, SH7750R only)
	setReadOnly<INTC_IPRD_addr>(read_INTC_IPRD);

	interrupts_init();

	reset();
}

void INTCRegisters::reset()
{
	super::reset();
	interrupts_reset();
}

void INTCRegisters::term()
{
	super::term();
	interrupts_term();
}

