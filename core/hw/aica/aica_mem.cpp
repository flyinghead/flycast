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
	else if (addr >= 0x4000 && addr < 0x4580)
	{
		if (addr & 2)
		{
			INFO_LOG(AICA, "Unaligned DSP register read @ %x", addr);
			return 0;
		}
		DEBUG_LOG(AICA, "DSP register read @ %x", addr);
		// DSP TEMP/MEMS
		u32 v;
		if (addr < 0x4500)
		{
			v = addr < 0x4400 ? dsp::state.TEMP[(addr - 0x4000) / 8] : dsp::state.MEMS[(addr - 0x4400) / 8];
			if (addr & 4)
				v = (v >> 8) & 0xffff;
			else
				v &= 0xff;
		}
		// DSP MIXS
		else
		{
			v = dsp::state.MIXS[(addr - 0x4500) / 8];
			if (addr & 4)
				v = (v >> 4) & 0xffff;
			else
				v &= 0xf;
		}
		if (sz == 1)
		{
			if (addr & 1)
				v >>= 8;
			else
				v &= 0xff;
		}
		return v;
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

	if (addr < 0x2800)
	{
		if (sz == 1)
			WriteMemArr<1>(aica_reg, addr, data);
		else 
			WriteMemArr<2>(aica_reg, addr, data);
		return;
	}

	if (addr < 0x2818)
	{
		if (sz == 1)
		{
			WriteCommonReg8(addr, data);
		}
		else
		{
			WriteCommonReg8(addr, data & 0xFF);
			WriteCommonReg8(addr + 1, data >> 8);
		}
		return;
	}

	if (addr >= 0x3000)
	{
		if (addr & 2)
		{
			INFO_LOG(AICA, "Unaligned DSP register write @ %x", addr);
			return;
		}
		if (addr >= 0x4000 && addr < 0x4580)
		{
			// DSP TEMP/MEMS
			if (addr < 0x4500)
			{
				s32 &v = addr < 0x4400 ? dsp::state.TEMP[(addr - 0x4000) / 8] : dsp::state.MEMS[(addr - 0x4400) / 8];
				if (addr & 4)
				{
					if (sz == 1)
					{
						if (addr & 1)
							v = (v & 0x0000ffff) | (((s32)data << 24) >> 8);
						else
							v = (v & 0xffff00ff) | ((data & 0xff) << 8);
					}
					else
					{
						v = (v & 0xff) | (((s32)data << 16) >> 8);
					}
				}
				else
				{
					if (sz != 1 || (addr & 1) == 0)
						v = (v & ~0xff) | (data & 0xff);
					// else ignored
				}
				DEBUG_LOG(AICA, "DSP TEMP/MEMS register write<%d> @ %x = %d", sz, addr, v);
			}
			// DSP MIXS
			else
			{
				s32 &v = dsp::state.MIXS[(addr - 0x4500) / 8];
				if (addr & 4)
				{
					if (sz == 1)
					{
						if (addr & 1)
							v = (v & 0x00000fff) | (((s32)data << 24) >> 12);
						else
							v = (v & 0xfffff00f) | ((data & 0xff) << 4);
					}
					else
					{
						v = (v & 0xf) | (((s32)data << 16) >> 12);
					}
				}
				else
				{
					if (sz != 1 || (addr & 1) == 0)
						v = (v & ~0xf) | (data & 0xf);
					// else ignored
				}
				DEBUG_LOG(AICA, "DSP MIXS register write<%d> @ %x = %d", sz, addr, v);
			}
			return;
		}

		if (sz == 1)
		{
			WriteMemArr<1>(aica_reg, addr, data);
			dsp::writeProg(addr);
		}
		else
		{
			WriteMemArr<2>(aica_reg, addr, data);
			dsp::writeProg(addr);
			dsp::writeProg(addr + 1);
		}
		return;
	}
	if (sz == 1)
		WriteAicaReg<1>(addr, data);
	else
		WriteAicaReg<2>(addr, data);
}

//Aica reads (both sh4&arm)
u32 libAICA_ReadReg(u32 addr, u32 size)
{
	if (size == 1)
		return ReadReg<1>(addr & 0x7FFF);
	else
		return ReadReg<2>(addr & 0x7FFF);
}

void libAICA_WriteReg(u32 addr, u32 data, u32 size)
{
	if (size == 1)
		WriteReg<1>(addr & 0x7FFF, data);
	else
		WriteReg<2>(addr & 0x7FFF, data);
}

void init_mem()
{
	memset(aica_reg, 0, sizeof(aica_reg));
	aica_ram.data[ARAM_SIZE - 1] = 1;
	aica_ram.Zero();
}

void term_mem()
{
}

