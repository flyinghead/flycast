#include "aica_mem.h"
#include "aica.h"
#include "aica_if.h"
#include "dsp.h"
#include "sgc_if.h"
#include "hw/hwreg.h"

namespace aica
{

alignas(4) u8 aica_reg[0x8000];

static void (*midiReceiver)(u8 data);

//Aica read/write (both sh4 & arm)

//00000000~007FFFFF @DRAM_AREA*
//00800000~008027FF @CHANNEL_DATA 
//00802800~00802FFF @COMMON_DATA 
//00803000~00807FFF @DSP_DATA 
template<typename T>
T readRegInternal(u32 addr)
{
	addr &= 0x7FFF;

	if (addr >= 0x2800 && addr < 0x2818)
	{
		sgc::ReadCommonReg(addr, sizeof(T) == 1);
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
		if constexpr (sizeof(T) == 1)
		{
			if (addr & 1)
				v >>= 8;
			else
				v &= 0xff;
		}
		return v;
	}
	return ReadMemArr<T>(aica_reg, addr);
}
template u8 readRegInternal<u8>(u32 addr);
template u16 readRegInternal<u16>(u32 addr);
template u32 readRegInternal<u32>(u32 addr);

static void writeCommonReg8(u32 reg, u8 data)
{
	WriteMemArr(aica_reg, reg, data);
	if (reg == 0x2804 || reg == 0x2805)
	{
		using namespace dsp;
		state.RBL = (8192 << CommonData->RBL) - 1;
		state.RBP = (CommonData->RBP * 2048) & ARAM_MASK;
		state.dirty = true;
	}
	else if (reg == 0x280c) {	// MOBUF
		if (midiReceiver != nullptr)
			midiReceiver(data);
	}
}

template<typename T>
void writeRegInternal(u32 addr, T data)
{
	constexpr size_t sz = sizeof(T);
	addr &= 0x7FFF;

	if (addr < 0x2000)
	{
		//Channel data
		u32 chan = addr >> 7;
		u32 reg = addr & 0x7F;
		WriteMemArr(aica_reg, addr, data);
		sgc::WriteChannelReg(chan, reg, sz);
		return;
	}

	if (addr < 0x2800)
	{
		WriteMemArr(aica_reg, addr, data);
		return;
	}

	if (addr < 0x2818)
	{
		writeCommonReg8(addr, data & 0xFF);
		if constexpr (sz == 2)
			writeCommonReg8(addr + 1, data >> 8);
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
					if constexpr (sz == 1)
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
				DEBUG_LOG(AICA, "DSP TEMP/MEMS register write<%d> @ %x = %d", (int)sz, addr, v);
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
				DEBUG_LOG(AICA, "DSP MIXS register write<%d> @ %x = %d", (int)sz, addr, v);
			}
			return;
		}

		WriteMemArr(aica_reg, addr, data);
		dsp::writeProg(addr);
		if constexpr (sz == 2)
			dsp::writeProg(addr + 1);
		return;
	}
	writeTimerAndIntReg(addr, data);
}
template void writeRegInternal<>(u32 addr, u8 data);
template void writeRegInternal<>(u32 addr, u16 data);
template void writeRegInternal<>(u32 addr, u32 data);

void initMem()
{
	memset(aica_reg, 0, sizeof(aica_reg));
	aica_ram[ARAM_SIZE - 1] = 1;
	aica_ram.zero();
	midiReceiver = nullptr;
}

void termMem()
{
}

void setMidiReceiver(void (*handler)(u8 data)) {
	midiReceiver = handler;
}

} // namespace aica
