/*
	Dreamcast 'area 0' emulation
	Pretty much all peripheral registers are mapped here

	Routing is mostly handled here, as well as flash/SRAM emulation
*/
#include "sb_mem.h"
#include "sb.h"
#include "hw/aica/aica_if.h"
#include "hw/flashrom/nvmem.h"
#include "hw/gdrom/gdrom_if.h"
#include "hw/modem/modem.h"
#include "hw/naomi/naomi.h"
#include "hw/naomi/systemsp.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/mem/addrspace.h"
#include "hw/bba/bba.h"
#include "cfg/option.h"

//Area 0 mem map
//0x00000000- 0x001FFFFF	:MPX	System/Boot ROM
//0x00200000- 0x0021FFFF	:Flash Memory
//0x00400000- 0x005F67FF	:Unassigned
//0x005F6800- 0x005F69FF	:System Control Reg.
//0x005F6C00- 0x005F6CFF	:Maple i/f Control Reg.
//0x005F7000- 0x005F70FF	:GD-ROM / NAOMI BD Reg.
//0x005F7400- 0x005F74FF	:G1 i/f Control Reg.
//0x005F7800- 0x005F78FF	:G2 i/f Control Reg.
//0x005F7C00- 0x005F7CFF	:PVR i/f Control Reg.
//0x005F8000- 0x005F9FFF	:TA / PVR Core Reg.
//0x00600000- 0x006007FF	:MODEM
//0x00600800- 0x006FFFFF	:G2 (Reserved)
//0x00700000- 0x00707FFF	:AICA- Sound Cntr. Reg.
//0x00710000- 0x0071000B	:AICA- RTC Cntr. Reg.
//0x00800000- 0x00FFFFFF	:AICA- Wave Memory
//0x01000000- 0x01FFFFFF	:Ext. Device
//0x02000000- 0x03FFFFFF*	:Image Area*	2MB
// Naomi 2:
//0x025F6800- 0x025F69FF    :PVR#2 system registers
//0x025F7C00- 0x025F7CFF	:PVR#2 PVR i/f Control Reg.
//0x025F8000- 0x025F9FFF	:PVR#2 TA / PVR Core Reg.

template<typename T, u32 System, bool Mirror>
T DYNACALL ReadMem_area0(u32 paddr)
{
	constexpr u32 sz = (u32)sizeof(T);
	u32 addr = paddr & 0x01FFFFFF;
	const u32 base = addr >> 21;

	switch (expected(base, 2))
	{
	case 0:
		// System/Boot ROM
		if (addr < (System == DC_PLATFORM_ATOMISWAVE ? 0x20000 : 0x200000))
		{
			if constexpr (Mirror)
			{
				INFO_LOG(MEMORY, "Read from area0 BIOS mirror [Unassigned], addr=%x", addr);
				return 0;
			}
			return nvmem::readBios(addr, sz);
		}
		break;
	case 1:
		// Flash memory
		if constexpr (System != DC_PLATFORM_SYSTEMSP)
			if (addr < 0x00200000 + settings.platform.flash_size)
			{
				if constexpr (Mirror)
				{
					INFO_LOG(MEMORY, "Read from area0 Flash mirror [Unassigned], addr=%x", addr);
					return 0;
				}
				return nvmem::readFlash(addr, sz);
			}
		break;
	case 2:
		// GD-ROM / Naomi/AW cart
		if (addr >= 0x005F7000 && addr <= 0x005F70FF)
		{
			if constexpr (System == DC_PLATFORM_DREAMCAST)
				return (T)ReadMem_gdrom(addr, sz);
			else
				return (T)ReadMem_naomi(addr, sz);
		}
		// All SB registers
		if (addr >= 0x005F6800 && addr <= 0x005F7CFF)
			return (T)sb_ReadMem(paddr);
		// TA / PVR core registers
		if (addr >= 0x005F8000 && addr <= 0x005F9FFF)
		{
			if constexpr (sz != 4)
				// House of the Dead 2
				return 0;
			return (T)pvr_ReadReg(paddr);
		}
		break;
	case 3:
		// MODEM
		if (addr <= 0x006007FF)
		{
			if constexpr (System == DC_PLATFORM_DREAMCAST)
			{
				if (!config::EmulateBBA)
					return (T)ModemReadMem_A0_006(addr, sz);
				else
					return (T)0;
			}
			else
			{
				return (T)libExtDevice_ReadMem_A0_006(addr, sz);
			}
		}
		// AICA sound registers
		if (addr >= 0x00700000 && addr <= 0x00707FFF)
			return aica::readAicaReg<T>(addr);
		// AICA RTC registers
		if (addr >= 0x00710000 && addr <= 0x0071000B)
			return aica::readRtcReg<T>(addr);
		break;

	case 4:
	case 5:
	case 6:
	case 7:
		// AICA ram
		return ReadMemArr<T>(&aica::aica_ram[0], addr & ARAM_MASK);

	default:
		// G2 Ext area
		if constexpr (System == DC_PLATFORM_NAOMI || System == DC_PLATFORM_NAOMI2)
			return (T)g2ext_readMem(addr, sz);
		else if constexpr (System == DC_PLATFORM_SYSTEMSP)
			return systemsp::readMemArea0<T>(addr);
		else
		{
			if (config::EmulateBBA)
				return (T)bba_ReadMem(addr, sz);
			else
				return (T)0;
		}
	}
	INFO_LOG(MEMORY, "Read from area0<%d> not implemented [Unassigned], addr=%x", sz, addr);
	return 0;
}

template<typename T, u32 System, bool Mirror>
void DYNACALL WriteMem_area0(u32 paddr, T data)
{
	constexpr u32 sz = (u32)sizeof(T);
	u32 addr = paddr & 0x01FFFFFF;//to get rid of non needed bits

	const u32 base = addr >> 21;

	switch (expected(base, 4))
	{
	case 0:
		 // System/Boot ROM
		if constexpr (!Mirror)
		{
			if constexpr (System == DC_PLATFORM_ATOMISWAVE)
			{
				if (addr < 0x20000)
				{
					nvmem::writeAWBios(addr, data, sz);
					return;
				}
			}
			else
			{
				if (addr < 0x200000)
				{
					INFO_LOG(MEMORY, "Write to [Boot ROM] is not possible, addr=%x, data=%x, size=%d", addr, data, sz);
					return;
				}
			}
		}
		break;
	case 1:
		// Flash memory
		if (!Mirror && addr < 0x00200000 + settings.platform.flash_size)
		{
			nvmem::writeFlash(addr, data, sz);
			return;
		}
		break;
	case 2:
		 // GD-ROM / Naomi/AW cart
		if (addr >= 0x005F7000 && addr <= 0x005F70FF)
		{
			if (System == DC_PLATFORM_DREAMCAST)
				WriteMem_gdrom(addr, data, sz);
			else
				WriteMem_naomi(addr, data, sz);
			return;
		}
		// All SB registers
		if (addr >= 0x005F6800 && addr <= 0x005F7CFF)
		{
			sb_WriteMem(paddr, data);
			return;
		}
		// TA / PVR core registers
		if (addr >= 0x005F8000 && addr <= 0x005F9FFF)
		{
			if (sz == 4) {
				pvr_WriteReg(paddr, data);
				return;
			}
		}
		break;
	case 3:
		// MODEM
		if (/* addr >= 0x00600000) && */ addr <= 0x006007FF)
		{
			if constexpr (System == DC_PLATFORM_DREAMCAST)
			{
				if (!config::EmulateBBA)
					ModemWriteMem_A0_006(addr, data, sz);
			}
			else
			{
				libExtDevice_WriteMem_A0_006(addr, data, sz);
			}
			return;
		}
		// AICA sound registers
		if (addr >= 0x00700000 && addr <= 0x00707FFF)
		{
			aica::writeAicaReg(addr, data);
			return;
		}
		// AICA RTC registers
		if (addr >= 0x00710000 && addr <= 0x0071000B)
		{
			aica::writeRtcReg(addr, data);
			return;
		}
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		// AICA ram
		WriteMemArr(&aica::aica_ram[0], addr & ARAM_MASK, data);
		return;

	default:
		// G2 Ext area
		if constexpr (System == DC_PLATFORM_NAOMI || System == DC_PLATFORM_NAOMI2)
			g2ext_writeMem(addr, data, sz);
		else if constexpr (System == DC_PLATFORM_SYSTEMSP)
			systemsp::writeMemArea0(addr, data);
		else
		{
			if (config::EmulateBBA)
				bba_WriteMem(addr, data, sz);
		}
		return;
	}
	INFO_LOG(MEMORY, "Write to area0_32 not implemented [Unassigned], addr=%x,data=%x,size=%d", addr, data, sz);
}

//Init/Res/Term
void sh4_area0_Init()
{
	sb_Init();
	nvmem::init();
}

void sh4_area0_Reset(bool hard)
{
	if (hard)
	{
		nvmem::term();
		nvmem::init();
	}
	else
	{
		nvmem::reset();
	}
	sb_Reset(hard);
}

void sh4_area0_Term()
{
	nvmem::term();
	sb_Term();
}


//AREA 0
static addrspace::handler area0_handler;
static addrspace::handler area0_mirror_handler;

void map_area0_init()
{
#define registerHandler(system, mirror) addrspace::registerHandler \
		(ReadMem_area0<u8, system, mirror>, ReadMem_area0<u16, system, mirror>, ReadMem_area0<u32, system, mirror>,	\
		 WriteMem_area0<u8, system, mirror>, WriteMem_area0<u16, system, mirror>, WriteMem_area0<u32, system, mirror>)

	switch (settings.platform.system)
	{
	case DC_PLATFORM_DREAMCAST:
	default:
		area0_handler = registerHandler(DC_PLATFORM_DREAMCAST, false);
		area0_mirror_handler = registerHandler(DC_PLATFORM_DREAMCAST, true);
		break;
	case DC_PLATFORM_NAOMI:
		area0_handler = registerHandler(DC_PLATFORM_NAOMI, false);
		area0_mirror_handler = registerHandler(DC_PLATFORM_NAOMI, true);
		break;
	case DC_PLATFORM_NAOMI2:
		area0_handler = registerHandler(DC_PLATFORM_NAOMI2, false);
		area0_mirror_handler = registerHandler(DC_PLATFORM_NAOMI2, true);
		break;
	case DC_PLATFORM_ATOMISWAVE:
		area0_handler = registerHandler(DC_PLATFORM_ATOMISWAVE, false);
		area0_mirror_handler = registerHandler(DC_PLATFORM_ATOMISWAVE, true);
		break;
	case DC_PLATFORM_SYSTEMSP:
		area0_handler = registerHandler(DC_PLATFORM_SYSTEMSP, false);
		area0_mirror_handler = registerHandler(DC_PLATFORM_SYSTEMSP, true);
		break;
	}
#undef registerHandler
}
void map_area0(u32 base)
{
	verify(base<0xE0);

	addrspace::mapHandler(area0_handler, 0x00 | base, 0x01 | base);
	addrspace::mapHandler(area0_mirror_handler, 0x02 | base, 0x03 | base);

	//0x0240 to 0x03FF mirrors 0x0040 to 0x01FF (no flashrom or bios)
	//0x0200 to 0x023F are unused
}
