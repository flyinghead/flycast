#include "maple_if.h"
#include "maple_cfg.h"
#include "maple_helper.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "network/ggpo.h"
#include "hw/naomi/card_reader.h"

#include <memory>

enum MaplePattern
{
	MP_Start,
	MP_SDCKBOccupy = 2,
	MP_Reset,
	MP_SDCKBOccupyCancel,
	MP_NOP = 7
};

std::shared_ptr<maple_device> MapleDevices[MAPLE_PORTS][6];

int maple_schid;

/*
	Maple host controller
	Direct processing, async interrupt handling
	Device code is on maple_devs.cpp/h, config&management is on maple_cfg.cpp/h

	This code is missing many of the hardware details, like proper trigger handling,
	DMA continuation on suspect, etc ...
*/

static void maple_DoDma();
static void maple_handle_reconnect();
static int maple_schd(int tag, int cycles, int jitter, void *arg);

//really hackish
//misses delay , and stop/start implementation
//ddt/etc are just hacked for wince to work
//now with proper maple delayed DMA maybe its time to look into it ?
bool maple_ddt_pending_reset;
// pending DMA xfers
std::vector<std::pair<u32, std::vector<u32>>> mapleDmaOut;
bool SDCKBOccupied;

void maple_vblank()
{
	if (SB_MDEN & 1)
	{
		if (SB_MDTSEL == 1)
		{
			// Hardware trigger on vblank
			if (maple_ddt_pending_reset) {
				DEBUG_LOG(MAPLE, "DDT vblank ; reset pending");
			}
			else
			{
				//DEBUG_LOG(MAPLE, "DDT vblank");
				SB_MDST = 1;
				maple_DoDma();
				// if trigger reset is manual, mark it as pending
				if ((SB_MSYS >> 12) & 1)
					maple_ddt_pending_reset = true;
			}
		}
		else
		{
			maple_ddt_pending_reset = false;
			if (SDCKBOccupied)
				maple_schd(0, 0, 0, nullptr);
		}
		SDCKBOccupied = false;
	}
	if (settings.platform.isConsole())
		maple_handle_reconnect();
}

static void maple_SB_MSHTCL_Write(u32 addr, u32 data)
{
	if (data & 1)
		maple_ddt_pending_reset = false;
}

static void maple_SB_MDST_Write(u32 addr, u32 data)
{
	if (data & 1)
	{
		if (SB_MDEN & 1)
		{
			SB_MDST = 1;
			maple_DoDma();
		}
	}
}

static void maple_SB_MDEN_Write(u32 addr, u32 data)
{
	SB_MDEN = data & 1;

	if ((data & 1) == 0 && SB_MDST)
		INFO_LOG(MAPLE, "Maple DMA abort ?");
}

#ifdef STRICT_MODE
static bool check_mdapro(u32 addr)
{
	u32 area = (addr >> 26) & 7;
	u32 bottom = ((((SB_MDAPRO >> 8) & 0x7f) << 20) | 0x08000000);
	u32 top = (((SB_MDAPRO & 0x7f) << 20) | 0x080fffe0);

	if (area != 3 || addr < bottom || addr > top)
	{
		INFO_LOG(MAPLE, "MAPLE ERROR : Invalid address: %08x. SB_MDAPRO: %x %x", addr, (SB_MDAPRO >> 8) & 0x7f, SB_MDAPRO & 0x7f);
		return false;
	}
	return true;
}

static void maple_SB_MDSTAR_Write(u32 addr, u32 data)
{
	SB_MDSTAR = data & 0x1fffffe0;
	if (!check_mdapro(SB_MDSTAR))
		asic_RaiseInterrupt(holly_MAPLE_ILLADDR);
}
#endif

static u32 getPort(u32 addr)
{
	for (int i = 0; i < 6; i++)
		if ((1 << i) & addr)
			return i;
	return 5;
}

static void maple_DoDma()
{
	verify(SB_MDEN & 1);
	verify(SB_MDST & 1);

	DEBUG_LOG(MAPLE, "Maple: DoMapleDma SB_MDSTAR=%x", SB_MDSTAR);
	u32 addr = SB_MDSTAR;
#ifdef STRICT_MODE
	if (!check_mdapro(addr))
	{
		asic_RaiseInterrupt(holly_MAPLE_ILLADDR);
		SB_MDST = 0;
		return;
	}
#endif

	ggpo::getInput(mapleInputState);
	// TODO put this elsewhere and let the card readers handle being called multiple times
	if (settings.platform.isNaomi())
	{
		static u32 last_kcode[std::size(mapleInputState)];
		for (size_t i = 0; i < std::size(mapleInputState); i++)
		{
			if ((mapleInputState[i].kcode & DC_BTN_INSERT_CARD) == 0
					&& (last_kcode[i] & DC_BTN_INSERT_CARD) != 0)
				card_reader::insertCard(i);
			last_kcode[i] = mapleInputState[i].kcode;
		}
	}

	const bool swap_msb = (SB_MMSEL == 0);
	u32 xferOut = 0;
	u32 xferIn = 0;
	bool last = false;
	while (!last)
	{
		u32 header_1 = ReadMem32_nommu(addr);
		u32 header_2 = ReadMem32_nommu(addr + 4) & 0x1FFFFFE0;

		last = (header_1 >> 31) == 1;		// is last transfer ?
		u32 plen = (header_1 & 0xFF) + 1;	// transfer length (32-bit unit)
		u32 maple_op = (header_1 >> 8) & 7;	// Pattern selection: 0 - START, 2 - SDCKB occupy permission, 3 - RESET, 4 - SDCKB occupy cancel, 7 - NOP

		//this is kinda wrong .. but meh
		//really need to properly process the commands at some point
		switch (maple_op)
		{
		case MP_Start:
		{
#ifdef STRICT_MODE
			if (!check_mdapro(header_2) || !check_mdapro(addr + 8 + plen * sizeof(u32) - 1))
			{
#else
			if (GetMemPtr(header_2, 1) == nullptr)
			{
				INFO_LOG(MAPLE, "DMA Error: destination not in system ram: %x", header_2);
#endif
				header_2 = 0;
			}

			u32* p_data = (u32 *)GetMemPtr(addr + 8, plen * sizeof(u32));
			if (p_data == nullptr)
			{
				WARN_LOG(MAPLE, "MAPLE ERROR : INVALID SB_MDSTAR value 0x%X", addr);
				SB_MDST = 0;
				mapleDmaOut.clear();
				return;
			}
			const u32 frame_header = swap_msb ? SWAP32(p_data[0]) : p_data[0];

			//Command code
			u32 command = frame_header & 0xFF;
			//Recipient address
			u32 reci = (frame_header >> 8) & 0xFF;//0-5;
			//Sender address
			//u32 send = (frame_header >> 16) & 0xFF;
			//Number of additional words in frame
			u32 inlen = (frame_header >> 24) & 0xFF;

			u32 port = getPort(reci);
			u32 bus = reci >> 6;

			if (MapleDevices[bus][5] && MapleDevices[bus][port])
			{
				if (swap_msb)
				{
					static u32 maple_in_buf[1024 / 4];
					maple_in_buf[0] = frame_header;
					for (u32 i = 1; i < inlen; i++)
						maple_in_buf[i] = SWAP32(p_data[i]);
					p_data = maple_in_buf;
				}
				inlen = (inlen + 1) * 4;
				u32 outbuf[1024 / 4];
				u32 outlen = MapleDevices[bus][port]->RawDma(&p_data[0], inlen, outbuf);
				xferIn += inlen + 3; // start, parity and stop bytes
				xferOut += outlen + 3;
#ifdef STRICT_MODE
				if (!check_mdapro(header_2 + outlen - 1))
				{
					asic_RaiseInterrupt(holly_MAPLE_OVERRUN);
					SB_MDST = 0;
					mapleDmaOut.clear();
					return;
				}
#endif
				if (swap_msb)
					for (u32 i = 0; i < outlen / 4; i++)
						outbuf[i] = SWAP32(outbuf[i]);
				mapleDmaOut.emplace_back(header_2, std::vector<u32>(outbuf, outbuf + outlen / 4));
			}
			else
			{
				if (port != 5 && command != 1)
					INFO_LOG(MAPLE, "MAPLE: Unknown device bus %d port %d cmd %d reci %d", bus, port, command, reci);
				mapleDmaOut.emplace_back(header_2, std::vector<u32>(1, 0xFFFFFFFF));
			}

			//goto next command
			addr += 2 * 4 + plen * 4;
		}
		break;

		case MP_SDCKBOccupy:
		{
			u32 bus = (header_1 >> 16) & 3;
			if (MapleDevices[bus][5]) {
				SDCKBOccupied = SDCKBOccupied || MapleDevices[bus][5]->get_lightgun_pos();
				xferIn++;
			}
			addr += 1 * 4;
		}
		break;

		case MP_SDCKBOccupyCancel:
			SDCKBOccupied = false;
			addr += 1 * 4;
			break;

		case MP_Reset:
			addr += 1 * 4;
			xferIn++;
			break;

		case MP_NOP:
			addr += 1 * 4;
			break;

		default:
			INFO_LOG(MAPLE, "MAPLE: Unknown maple_op == %d length %d", maple_op, plen * 4);
			addr += 1 * 4;
		}
	}

	// Maple bus max speed: 2 Mb/s, actual speed: 1 Mb/s
	// actual measured speed with protocol analyzer for devices (vmu?) is 724-738Kb/s
	// See https://github.com/OrangeFox86/DreamcastControllerUsbPico/blob/main/measurements/Dreamcast-Power-Up-Digital-and-Analog-Player1-Controller-VMU-JumpPack.sal
	if (!SDCKBOccupied)
	{
		// 2 Mb/s from console
		u32 cycles = sh4CyclesForXfer(xferIn, 2'000'000 / 8);
		// 740 Kb/s from devices
		cycles += sh4CyclesForXfer(xferOut, 740'000 / 8);
		cycles = std::min<u32>(cycles, SH4_MAIN_CLOCK);
		sh4_sched_request(maple_schid, cycles);
	}
}

static int maple_schd(int tag, int cycles, int jitter, void *arg)
{
	if (SB_MDEN & 1)
	{
		for (const auto& pair : mapleDmaOut)
		{
			if (pair.first == 0)
			{
				asic_RaiseInterrupt(holly_MAPLE_OVERRUN);
				continue;
			}
			size_t size = pair.second.size() * sizeof(u32);
			u32 *p = (u32 *)GetMemPtr(pair.first, size);
			memcpy(p, pair.second.data(), size);
		}
		SB_MDST = 0;
		asic_RaiseInterrupt(holly_MAPLE_DMA);
	}
	else
	{
		INFO_LOG(MAPLE, "WARNING: MAPLE DMA ABORT");
		SB_MDST = 0; //I really wonder what this means, can the DMA be continued ?
	}
	mapleDmaOut.clear();

	return 0;
}

static void maple_SB_MDAPRO_Write(u32 addr, u32 data)
{
	if ((data >> 16) == 0x6155)
		SB_MDAPRO = data & 0x00007f7f;
}

//Init registers :)
void maple_Init()
{
	hollyRegs.setWriteHandler<SB_MDST_addr>(maple_SB_MDST_Write);
	hollyRegs.setWriteHandler<SB_MDEN_addr>(maple_SB_MDEN_Write);
	hollyRegs.setWriteHandler<SB_MSHTCL_addr>(maple_SB_MSHTCL_Write);
	hollyRegs.setWriteOnly<SB_MDAPRO_addr>(maple_SB_MDAPRO_Write);
#ifdef STRICT_MODE
	hollyRegs.setWriteHandler<SB_MDSTAR_addr>(maple_SB_MDSTAR_Write);
#endif

	maple_schid = sh4_sched_register(0, maple_schd);
}

static u64 reconnect_time;

void maple_Reset(bool hard)
{
	maple_ddt_pending_reset = false;
	SB_MDTSEL = 0;
	SB_MDEN   = 0;
	SB_MDST   = 0;
	SB_MSYS   = 0x3A980000;
	SB_MSHTCL = 0;
	SB_MDAPRO = 0x00007F00;
	SB_MMSEL  = 1;
	mapleDmaOut.clear();
	reconnect_time = 0;
}

void maple_Term()
{
	mcfg_DestroyDevices();
	sh4_sched_unregister(maple_schid);
	maple_schid = -1;
}

void maple_ReconnectDevices()
{
	mcfg_DestroyDevices();
	reconnect_time = sh4_sched_now64() + SH4_MAIN_CLOCK / 10;
}

static void maple_handle_reconnect()
{
	if (reconnect_time != 0 && reconnect_time <= sh4_sched_now64())
	{
		reconnect_time = 0;
		mcfg_CreateDevices();
	}
}
