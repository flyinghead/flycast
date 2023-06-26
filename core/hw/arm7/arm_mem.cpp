#include "arm_mem.h"
#include "hw/aica/aica_mem.h"

namespace aica
{

namespace arm
{

#define REG_L (0x2D00)
#define REG_M (0x2D04)

//Set to true when aica interrupt is pending
bool aica_interr;
u32 aica_reg_L;
//Set to true when the out of the intc is 1
bool e68k_out;
u32 e68k_reg_L;
u32 e68k_reg_M; //constant ?

void update_e68k()
{
	if (!e68k_out && aica_interr)
	{
		//Set the pending signal
		//Is L register held here too ?
		e68k_out = true;
		e68k_reg_L = aica_reg_L;

		update_armintc();
	}
}

void interruptChange(u32 bits,u32 L)
{
	aica_interr = bits != 0;
	if (aica_interr)
		aica_reg_L = L;
	update_e68k();
}

void e68k_AcceptInterrupt()
{
	e68k_out = false;
	update_e68k();
	update_armintc();
}

//Reg reads from arm side ..
template <typename T>
T readReg(u32 addr)
{
	addr &= 0x7FFF;
	if (addr == REG_L)
		return (T)e68k_reg_L;
	else if (addr == REG_M)
		return (T)e68k_reg_M;	//shouldn't really happen
	else if (sizeof(T) == 4)
		return readRegInternal<u16>(addr);
	else
		return readRegInternal<T>(addr);
}

template <typename T>
void writeReg(u32 addr, T data)
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
			writeRegInternal(addr, (u16)data);
		else
			writeRegInternal(addr, data);
	}
}

//00000000~007FFFFF @DRAM_AREA* 
//00800000~008027FF @CHANNEL_DATA 
//00802800~00802FFF @COMMON_DATA 
//00803000~00807FFF @DSP_DATA 

template u8 readReg<u8>(u32 adr);
template u16 readReg<u16>(u32 adr);
template u32 readReg<u32>(u32 adr);

template void writeReg<>(u32 adr, u8 data);
template void writeReg<>(u32 adr, u16 data);
template void writeReg<>(u32 adr, u32 data);

} // namespace arm
} // namespace aica
