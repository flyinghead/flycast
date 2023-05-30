#include "holly_intc.h"
#include "sb.h"
#include "hw/sh4/sh4_interrupts.h"

/*
	ASIC Interrupt controller
	part of the holly block on dc
*/

//asic_RLXXPending: Update the intc flags for pending interrupts
static void asic_RL6Pending()
{
	bool t1 = (SB_ISTNRM & SB_IML6NRM) != 0;
	bool t2 = (SB_ISTERR & SB_IML6ERR) != 0;
	bool t3 = (SB_ISTEXT & SB_IML6EXT) != 0;
	bool t4 = (SB_ISTNRM1 & SB_IML6NRM) != 0;

	InterruptPend(sh4_IRL_9, t1 || t2 || t3 || t4);
}

static void asic_RL4Pending()
{
	bool t1 = (SB_ISTNRM & SB_IML4NRM) != 0;
	bool t2 = (SB_ISTERR & SB_IML4ERR) != 0;
	bool t3 = (SB_ISTEXT & SB_IML4EXT) != 0;
	bool t4 = (SB_ISTNRM1 & SB_IML4NRM) != 0;

	InterruptPend(sh4_IRL_11, t1 || t2 || t3 || t4);
}

static void asic_RL2Pending()
{
	bool t1 = (SB_ISTNRM & SB_IML2NRM) != 0;
	bool t2 = (SB_ISTERR & SB_IML2ERR) != 0;
	bool t3 = (SB_ISTEXT & SB_IML2EXT) != 0;
	bool t4 = (SB_ISTNRM1 & SB_IML2NRM) != 0;

	InterruptPend(sh4_IRL_13, t1 || t2 || t3 || t4);
}

void asic_RaiseInterrupt(HollyInterruptID inter)
{
	u8 type = inter >> 8;
	u32 mask = 1 << (u8)inter;
	switch(type)
	{
	case 0:
		SB_ISTNRM |= mask;
		break;
	case 1:
		SB_ISTEXT |= mask;
		break;
	case 2:
		SB_ISTERR |= mask;
		break;
	}
	asic_RL2Pending();
	asic_RL4Pending();
	asic_RL6Pending();
}

void asic_RaiseInterruptBothCLX(HollyInterruptID inter)
{
	u8 type = inter >> 8;
	u32 mask = 1 << (u8)inter;
	switch(type)
	{
	case 0:
		SB_ISTNRM1 |= mask;
		SB_ISTNRM |= mask;
		break;
	case 1:
		SB_ISTEXT |= mask;
		break;
	case 2:
		SB_ISTERR |= mask;
		break;
	}
	asic_RL2Pending();
	asic_RL4Pending();
	asic_RL6Pending();
}

template<bool Naomi2>
static u32 Read_SB_ISTNRM(u32 addr)
{
	/* Note that the two highest bits indicate
	 * the OR'ed result of all the bits in
	 * SB_ISTEXT and SB_ISTERR, respectively,
	 * and writes to these two bits are ignored. */
	u32 tmp = (Naomi2 && (addr & 0x02000000) != 0 ? SB_ISTNRM1 : SB_ISTNRM) & 0x3FFFFFFF;

	if (SB_ISTEXT)
		tmp|=0x40000000;

	if (SB_ISTERR)
		tmp|=0x80000000;

	return tmp;
}

template<bool Naomi2>
static void Write_SB_ISTNRM(u32 addr, u32 data)
{
	/* writing a 1 clears the interrupt */
	if (Naomi2 && (addr & 0x02000000) != 0)
		SB_ISTNRM1 &= ~data;
	else
		SB_ISTNRM &= ~data;

	asic_RL2Pending();
	asic_RL4Pending();
	asic_RL6Pending();
}

void asic_CancelInterrupt(HollyInterruptID inter)
{
	u8 type = inter >> 8;
	u32 mask = ~(1 << (u8)inter);
	switch (type)
	{
	case 0:
		SB_ISTNRM &= mask;
		break;
	case 1:
		SB_ISTEXT &= mask;
		break;
	case 2:
		SB_ISTERR &= mask;
		break;
	}
	asic_RL2Pending();
	asic_RL4Pending();
	asic_RL6Pending();
}

static void Write_SB_ISTEXT(u32 addr, u32 data)
{
	//nothing happens -- asic_CancelInterrupt is used instead
}

static void Write_SB_ISTERR(u32 addr, u32 data)
{
	SB_ISTERR &= ~data;

	asic_RL2Pending();
	asic_RL4Pending();
	asic_RL6Pending();
}

template<bool Naomi2>
static void Write_SB_IML6NRM(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML6NRM = data;

	asic_RL6Pending();
}

template<bool Naomi2>
static void Write_SB_IML4NRM(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML4NRM = data;

	asic_RL4Pending();
}

template<bool Naomi2>
static void Write_SB_IML2NRM(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML2NRM = data;

	asic_RL2Pending();
}

template<bool Naomi2>
static void Write_SB_IML6EXT(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML6EXT = data;

	asic_RL6Pending();
}

template<bool Naomi2>
static void Write_SB_IML4EXT(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML4EXT = data;

	asic_RL4Pending();
}

template<bool Naomi2>
static void Write_SB_IML2EXT(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML2EXT = data;

	asic_RL2Pending();
}

template<bool Naomi2>
static void Write_SB_IML6ERR(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML6ERR = data;

	asic_RL6Pending();
}

template<bool Naomi2>
static void Write_SB_IML4ERR(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML4ERR = data;

	asic_RL4Pending();
}

template<bool Naomi2>
static void Write_SB_IML2ERR(u32 addr, u32 data)
{
	if (Naomi2 && (addr & 0x2000000) != 0)
		// Ignore CLXB settings
		return;
	SB_IML2ERR = data;

	asic_RL2Pending();
}

void asic_reg_Init()
{
}

void asic_reg_Term()
{

}
//Reset -> Reset - Initialise to default values
void asic_reg_Reset(bool hard)
{
	if (hard)
	{
		hollyRegs.setWriteHandler<SB_ISTEXT_addr>(Write_SB_ISTEXT);
		hollyRegs.setWriteHandler<SB_ISTERR_addr>(Write_SB_ISTERR);

		if (settings.platform.isNaomi2())
		{
			hollyRegs.setHandlers<SB_ISTNRM_addr>(Read_SB_ISTNRM<true>, Write_SB_ISTNRM<true>);

			//NRM
			//6
			hollyRegs.setWriteHandler<SB_IML6NRM_addr>(Write_SB_IML6NRM<true>);
			//4
			hollyRegs.setWriteHandler<SB_IML4NRM_addr>(Write_SB_IML4NRM<true>);
			//2
			hollyRegs.setWriteHandler<SB_IML2NRM_addr>(Write_SB_IML2NRM<true>);
			//EXT
			//6
			hollyRegs.setWriteHandler<SB_IML6EXT_addr>(Write_SB_IML6EXT<true>);
			//4
			hollyRegs.setWriteHandler<SB_IML4EXT_addr>(Write_SB_IML4EXT<true>);
			//2
			hollyRegs.setWriteHandler<SB_IML2EXT_addr>(Write_SB_IML2EXT<true>);
			//ERR
			//6
			hollyRegs.setWriteHandler<SB_IML6ERR_addr>(Write_SB_IML6ERR<true>);
			//4
			hollyRegs.setWriteHandler<SB_IML4ERR_addr>(Write_SB_IML4ERR<true>);
			//2
			hollyRegs.setWriteHandler<SB_IML2ERR_addr>(Write_SB_IML2ERR<true>);
		}
		else
		{
			hollyRegs.setHandlers<SB_ISTNRM_addr>(Read_SB_ISTNRM<false>, &Write_SB_ISTNRM<false>);

			//NRM
			//6
			hollyRegs.setWriteHandler<SB_IML6NRM_addr>(Write_SB_IML6NRM<false>);
			//4
			hollyRegs.setWriteHandler<SB_IML4NRM_addr>(Write_SB_IML4NRM<false>);
			//2
			hollyRegs.setWriteHandler<SB_IML2NRM_addr>(Write_SB_IML2NRM<false>);
			//EXT
			//6
			hollyRegs.setWriteHandler<SB_IML6EXT_addr>(Write_SB_IML6EXT<false>);
			//4
			hollyRegs.setWriteHandler<SB_IML4EXT_addr>(Write_SB_IML4EXT<false>);
			//2
			hollyRegs.setWriteHandler<SB_IML2EXT_addr>(Write_SB_IML2EXT<false>);
			//ERR
			//6
			hollyRegs.setWriteHandler<SB_IML6ERR_addr>(Write_SB_IML6ERR<false>);
			//4
			hollyRegs.setWriteHandler<SB_IML4ERR_addr>(Write_SB_IML4ERR<false>);
			//2
			hollyRegs.setWriteHandler<SB_IML2ERR_addr>(Write_SB_IML2ERR<false>);
		}
	}
}

