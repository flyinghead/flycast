#include "arm_mem.h"
#include "hw/aica/aica_mem.h"

#define REG_L (0x2D00)
#define REG_M (0x2D04)

//Set to true when aica interrupt is pending
bool aica_interr=false;
u32 aica_reg_L=0;
//Set to true when the out of the intc is 1
bool e68k_out = false;
u32 e68k_reg_L;
u32 e68k_reg_M=0; //constant ?

void update_e68k()
{
	if (!e68k_out && aica_interr)
	{
		//Set the pending signal
		//Is L register held here too ?
		e68k_out=true;
		e68k_reg_L=aica_reg_L;

		update_armintc();
	}
}

void libARM_InterruptChange(u32 bits,u32 L)
{
	aica_interr=bits!=0;
	if (aica_interr)
		aica_reg_L=L;
	update_e68k();
}

void e68k_AcceptInterrupt()
{
	e68k_out=false;
	update_e68k();
	update_armintc();
}

//Reg reads from arm side ..
template <typename T>
T arm_ReadReg(u32 addr)
{
	addr &= 0x7FFF;
	if (addr == REG_L)
		return (T)e68k_reg_L;
	else if (addr == REG_M)
		return (T)e68k_reg_M;	//shouldn't really happen
	else if (sizeof(T) == 4)
		return aicaReadReg<u16>(addr);
	else
		return aicaReadReg<T>(addr);
}

template <typename T>
void arm_WriteReg(u32 addr, T data)
{
	addr &= 0x7FFF;
	if (addr == REG_L)
	{
		return; // Shouldn't really happen (read only)
	}
	else if (addr == REG_M)
	{
		//accept interrupts
		if (data & 1)
			e68k_AcceptInterrupt();
	}
	else
	{
		if (sizeof(T) == 4)
			aicaWriteReg(addr, (u16)data);
		else
			aicaWriteReg(addr, data);
	}
}

//00000000~007FFFFF @DRAM_AREA* 
//00800000~008027FF @CHANNEL_DATA 
//00802800~00802FFF @COMMON_DATA 
//00803000~00807FFF @DSP_DATA 

template u8 arm_ReadReg<u8>(u32 adr);
template u16 arm_ReadReg<u16>(u32 adr);
template u32 arm_ReadReg<u32>(u32 adr);

template void arm_WriteReg<>(u32 adr,u8 data);
template void arm_WriteReg<>(u32 adr,u16 data);
template void arm_WriteReg<>(u32 adr,u32 data);
