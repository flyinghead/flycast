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
	bool t1=(SB_ISTNRM & SB_IML6NRM)!=0;
	bool t2=(SB_ISTERR & SB_IML6ERR)!=0;
	bool t3=(SB_ISTEXT & SB_IML6EXT)!=0;

	InterruptPend(sh4_IRL_9,t1|t2|t3);
}

static void asic_RL4Pending()
{
	bool t1=(SB_ISTNRM & SB_IML4NRM)!=0;
	bool t2=(SB_ISTERR & SB_IML4ERR)!=0;
	bool t3=(SB_ISTEXT & SB_IML4EXT)!=0;

	InterruptPend(sh4_IRL_11,t1|t2|t3);
}

static void asic_RL2Pending()
{
	bool t1=(SB_ISTNRM & SB_IML2NRM)!=0;
	bool t2=(SB_ISTERR & SB_IML2ERR)!=0;
	bool t3=(SB_ISTEXT & SB_IML2EXT)!=0;

	InterruptPend(sh4_IRL_13,t1|t2|t3);
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

static u32 Read_SB_ISTNRM(u32 addr)
{
	/* Note that the two highest bits indicate
	 * the OR'ed result of all the bits in
	 * SB_ISTEXT and SB_ISTERR, respectively,
	 * and writes to these two bits are ignored. */
	u32 tmp = SB_ISTNRM & 0x3FFFFFFF;

	if (SB_ISTEXT)
		tmp|=0x40000000;

	if (SB_ISTERR)
		tmp|=0x80000000;

	return tmp;
}

static void Write_SB_ISTNRM(u32 addr, u32 data)
{
	/* writing a 1 clears the interrupt */
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

static void Write_SB_IML6NRM(u32 addr, u32 data)
{
	SB_IML6NRM=data;

	asic_RL6Pending();
}

static void Write_SB_IML4NRM(u32 addr, u32 data)
{
	SB_IML4NRM=data;

	asic_RL4Pending();
}
static void Write_SB_IML2NRM(u32 addr, u32 data)
{
	SB_IML2NRM=data;

	asic_RL2Pending();
}

static void Write_SB_IML6EXT(u32 addr, u32 data)
{
	SB_IML6EXT=data;

	asic_RL6Pending();
}
static void Write_SB_IML4EXT(u32 addr, u32 data)
{
	SB_IML4EXT=data;

	asic_RL4Pending();
}
static void Write_SB_IML2EXT(u32 addr, u32 data)
{
	SB_IML2EXT=data;

	asic_RL2Pending();
}

static void Write_SB_IML6ERR(u32 addr, u32 data)
{
	SB_IML6ERR=data;

	asic_RL6Pending();
}
static void Write_SB_IML4ERR(u32 addr, u32 data)
{
	SB_IML4ERR=data;

	asic_RL4Pending();
}
static void Write_SB_IML2ERR(u32 addr, u32 data)
{
	SB_IML2ERR=data;

	asic_RL2Pending();
}

void asic_reg_Init()
{
	sb_rio_register(SB_ISTNRM_addr,RIO_FUNC,&Read_SB_ISTNRM,&Write_SB_ISTNRM);
	sb_rio_register(SB_ISTEXT_addr,RIO_WF,0,&Write_SB_ISTEXT);
	sb_rio_register(SB_ISTERR_addr,RIO_WF,0,&Write_SB_ISTERR);

	//NRM
	//6
	sb_rio_register(SB_IML6NRM_addr,RIO_WF,0,&Write_SB_IML6NRM);
	//4
	sb_rio_register(SB_IML4NRM_addr,RIO_WF,0,&Write_SB_IML4NRM);
	//2
	sb_rio_register(SB_IML2NRM_addr,RIO_WF,0,&Write_SB_IML2NRM);
	//EXT
	//6
	sb_rio_register(SB_IML6EXT_addr,RIO_WF,0,&Write_SB_IML6EXT);
	//4
	sb_rio_register(SB_IML4EXT_addr,RIO_WF,0,&Write_SB_IML4EXT);
	//2
	sb_rio_register(SB_IML2EXT_addr,RIO_WF,0,&Write_SB_IML2EXT);
	//ERR
	//6
	sb_rio_register(SB_IML6ERR_addr,RIO_WF,0,&Write_SB_IML6ERR);
	//4
	sb_rio_register(SB_IML4ERR_addr,RIO_WF,0,&Write_SB_IML4ERR);
	//2
	sb_rio_register(SB_IML2ERR_addr,RIO_WF,0,&Write_SB_IML2ERR);
}

void asic_reg_Term()
{

}
//Reset -> Reset - Initialise to default values
void asic_reg_Reset(bool hard)
{

}

