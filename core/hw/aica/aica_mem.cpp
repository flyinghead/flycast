#include "aica_mem.h"
#include "aica.h"
#include "aica_if.h"
#include "dsp.h"
#include "sgc_if.h"

alignas(4) u8 aica_reg[0x8000];

//00000000~007FFFFF @DRAM_AREA* 
//00800000~008027FF @CHANNEL_DATA 
//00802800~00802FFF @COMMON_DATA 
//00803000~00807FFF @DSP_DATA 
template<u32 sz>
u32 ReadReg(u32 addr)
{
	if (addr >= 0x2800 && addr < 0x2818)
	{
		if (sz == 1)
			ReadCommonReg(addr, true);
		else
			ReadCommonReg(addr, false);
	}
	return ReadMemArr<sz>(aica_reg, addr);
}

template<u32 sz>
void WriteReg(u32 addr,u32 data)
{
	if (addr < 0x2000)
	{
		//Channel data
		u32 chan = addr >> 7;
		u32 reg = addr & 0x7F;
		WriteMemArr<sz>(aica_reg, addr, data);
		WriteChannelReg(chan, reg, sz);
		return;
	}

	if (addr<0x2800)
	{
		if (sz==1)
			WriteMemArr<1>(aica_reg, addr, data);
		else 
			WriteMemArr<2>(aica_reg, addr, data);
		return;
	}

	if (addr < 0x2818)
	{
		if (sz==1)
		{
			WriteCommonReg8(addr,data);
		}
		else
		{
			WriteCommonReg8(addr,data&0xFF);
			WriteCommonReg8(addr+1,data>>8);
		}
		return;
	}

	if (addr>=0x3000)
	{
		if (sz==1)
		{
			WriteMemArr<1>(aica_reg, addr, data);
			dsp_writenmem(addr);
		}
		else
		{
			WriteMemArr<2>(aica_reg, addr, data);
			dsp_writenmem(addr);
			dsp_writenmem(addr+1);
		}
		return;
	}
	if (sz==1)
		WriteAicaReg<1>(addr,data);
	else
		WriteAicaReg<2>(addr,data);
}
//Aica reads (both sh4&arm)
u32 libAICA_ReadReg(u32 addr, u32 size)
{
	if (size == 1)
		return ReadReg<1>(addr & 0x7FFF);
	else
		return ReadReg<2>(addr & 0x7FFF);
}

void libAICA_WriteReg(u32 addr,u32 data,u32 size)
{
	if (size==1)
		WriteReg<1>(addr & 0x7FFF,data);
	else
		WriteReg<2>(addr & 0x7FFF,data);
}

//Map using _vmem .. yay
void init_mem()
{
	memset(aica_reg,0,sizeof(aica_reg));
	aica_ram.data[ARAM_SIZE-1]=1;
	aica_ram.Zero();
}
//kill mem map & free used mem ;)
void term_mem()
{

}

