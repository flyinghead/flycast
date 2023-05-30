/*
	Created on: Mar 15, 2020

	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
//
// Optical communication board (837-13691)
// Ring topology
// 10 Mbps
// Max packet size 0x4000
//
#include "naomi_m3comm.h"
#include "naomi_regs.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "network/naomi_network.h"
#include "emulator.h"
#include "rend/gui.h"

#include <chrono>
#include <memory>

constexpr u16 COMM_CTRL_CPU_RAM = 1 << 0;
constexpr u16 COMM_CTRL_RESET = 1 << 5;		// rising edge
constexpr u16 COMM_CTRL_G1DMA = 1 << 14;	// active low

struct CommBoardStat
{
	u16 transmode;		// communication mode (0: master, positive value: slave)
	u16 totalnode;		// Total number of nodes (same value is entered in upper and lower 8 bits)
	u16 nodeID;			// Local node ID (the same value is entered in the upper and lower 8 bits)
	u16 transcnt;		// counter (value increases by 1 per frame)
	u16 cts;			// CTS timer value (for debugging)
	u16 dma_rx_addr;	// DMA receive address (for debugging)
	u16 dma_rx_size;	// DMA receive size (for debugging)
	u16 dma_tx_addr;	// DMA transmit address (for debugging)
	u16 dma_tx_size;	// DMA transmission size (for debugging)
	u16 dummy[7];
};

#if !defined(__OpenBSD__)
static inline u16 swap16(u16 w)
{
	return (w >> 8) | (w << 8);
}
#endif

static void vblankCallback(Event event, void *param) {
	((NaomiM3Comm *)param)->vblank();
}

void NaomiM3Comm::closeNetwork()
{
	EventManager::unlisten(Event::VBlank, vblankCallback, this);
	naomiNetwork.shutdown();
}

void NaomiM3Comm::connectNetwork()
{
	gui_display_notification("Network started", 5000);
	packet_number = 0;
	slot_count = naomiNetwork.getSlotCount();
	slot_id = naomiNetwork.getSlotId();
	if (slot_count >= 2)
	{
		connectedState();
		EventManager::listen(Event::VBlank, vblankCallback, this);
	}
}

bool NaomiM3Comm::receiveNetwork()
{
	const u32 slot_size = swap16(*(u16*)&m68k_ram[0x204]);
	const u32 packet_size = slot_size * slot_count;

	std::unique_ptr<u8[]> buf = std::make_unique<u8[]>(packet_size);

	u16 packetNumber;
	if (!naomiNetwork.receive(buf.get(), packet_size, &packetNumber))
		return false;

	*(u16*)&comm_ram[6] = swap16(packetNumber);
	memcpy(&comm_ram[0x100 + slot_size], buf.get(), packet_size);

	return true;
}

void NaomiM3Comm::sendNetwork()
{
	const u32 packet_size = swap16(*(u16*)&m68k_ram[0x204]) * slot_count;
	naomiNetwork.send(&comm_ram[0x100], packet_size, packet_number);
	packet_number++;
}

u32 NaomiM3Comm::ReadMem(u32 address, u32 size)
{
	switch (address)
	{
	case NAOMI_COMM2_CTRL_addr:
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL read");
		return comm_ctrl;

	case NAOMI_COMM2_OFFSET_addr:
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_OFFSET read");
		return comm_offset;

	case NAOMI_COMM2_DATA_addr:
	{
		u16 value;
		if (comm_ctrl & COMM_CTRL_CPU_RAM)
			value = *(u16*)&m68k_ram[comm_offset];
		else
			// TODO u16 *commram = (u16*)membank("comm_ram")->base();
			value = *(u16*)&comm_ram[comm_offset];
		value = swap16(value);
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_DATA %s read @ %04x: %x", (comm_ctrl & COMM_CTRL_CPU_RAM) ? "m68k ram" : "comm ram", comm_offset, value);
		comm_offset += 2;
		return value;
	}

	case NAOMI_COMM2_STATUS0_addr:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS0 read %x", comm_status0);
		return comm_status0;

	case NAOMI_COMM2_STATUS1_addr:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS1 read %x", comm_status1);
		return comm_status1;

	default:
		DEBUG_LOG(NAOMI, "NaomiM3Comm::ReadMem unmapped: %08x sz %d", address, size);
		return 0xffffffff;
	}
}

void NaomiM3Comm::connectedState()
{
	memset(&comm_ram[0xf000], 0, 16);
	comm_ram[0xf000] = 1;
	comm_ram[0xf001] = 1;
	comm_ram[0xf002] = m68k_ram[0x204];
	comm_ram[0xf003] = m68k_ram[0x205];

	u32 slot_size = swap16(*(u16*)&m68k_ram[0x204]);

	CommBoardStat& stat = *(CommBoardStat *)&comm_ram[0];
	memset(&stat, 0, sizeof(stat));
	stat.transmode = swap16(slot_id == 0 ? 0 : 1);
	stat.totalnode = slot_count | (slot_count << 8);
	stat.nodeID = slot_id | (slot_id << 8);
	stat.cts = swap16(slot_id == 0 ? 0x7830 : 0x73a2);
	stat.dma_rx_addr = swap16(0x100 + slot_size);
	stat.dma_rx_size = swap16(slot_size * slot_count);
	stat.dma_tx_addr = swap16(0x100);
	stat.dma_tx_size = swap16(slot_size * slot_count);

	comm_status0 = 0xff01;	// But 1 at connect time before f000 is read
	comm_status1 = (slot_count << 8) | slot_id;
}

void NaomiM3Comm::WriteMem(u32 address, u32 data, u32 size)
{
	switch (address)
	{
	case NAOMI_COMM2_CTRL_addr:
		// bit 0: access RAM is 0 - communication RAM / 1 - M68K RAM
		// bit 1: comm RAM bank (seems R/O for SH4)
		// bit 5: M68K Reset
		// bit 6: ???
		// bit 7: might be M68K IRQ 5 or 2 - set to 0 by nlCbIntr()
		// bit 14: G1 DMA bus master 0 - active / 1 - disabled
		// bit 15: 0 - enable / 1 - disable this device ???
		if ((comm_ctrl & COMM_CTRL_RESET) == 0 && (data & COMM_CTRL_RESET) != 0)
		{
			DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL m68k reset");
			memset(&comm_ram[0], 0, 32);
			comm_status0 = 0; // varies...
			comm_status1 = 0;
			connectNetwork();
		}
		comm_ctrl = (u16)data;
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL = %x", comm_ctrl);
		return;

	case NAOMI_COMM2_OFFSET_addr:
		comm_offset = (u16)data;
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_OFFSET set to %x", comm_offset);
		return;

	case NAOMI_COMM2_DATA_addr:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_DATA written @ %04x %04x", comm_offset, (u16)data);
		data = swap16(data);
		if (comm_ctrl & COMM_CTRL_CPU_RAM)
			*(u16*)&m68k_ram[comm_offset] = (u16)data;
		else
			*(u16*)&comm_ram[comm_offset] = (u16)data;
		comm_offset += 2;
		return;

	case NAOMI_COMM2_STATUS0_addr:
		comm_status0 = (u16)data;
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS0 set to %x", comm_status0);
		return;

	case NAOMI_COMM2_STATUS1_addr:
		comm_status1 = (u16)data;
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS1 set to %x", comm_status1);
		return;

	default:
		break;
	}
	DEBUG_LOG(NAOMI, "NaomiM3Comm::WriteMem: %x <= %x sz %d", address, data, size);
}

bool NaomiM3Comm::DmaStart(u32 addr, u32 data)
{
	if (comm_ctrl & COMM_CTRL_G1DMA)
		return false;

	DEBUG_LOG(NAOMI, "NaomiM3Comm: DMA addr %08X <-> %04x len %d %s", SB_GDSTAR, comm_offset, SB_GDLEN, SB_GDDIR == 0 ? "OUT" : "IN");
	if (SB_GDDIR == 0)
	{
		// Network write
		for (u32 i = 0; i < SB_GDLEN; i++)
			comm_ram[comm_offset++] = ReadMem8_nommu(SB_GDSTAR + i);
	}
	else
	{
		// Network read
		/*
		if (SB_GDLEN == 32 && (comm_ctrl & COMM_CTRL_CPU_RAM) == 0)
		{
			char buf[32 * 5 + 1];
			buf[0] = 0;
			for (u32 i = 0; i < SB_GDLEN; i++)
			{
				u8 value = comm_ram[comm_offset + i];
				sprintf(buf + strlen(buf), "%02x ", value);
			}
			DEBUG_LOG(NAOMI, "Comm RAM read @%x: %s", comm_offset, buf);
		}
		*/
		for (u32 i = 0; i < SB_GDLEN; i++)
			WriteMem8_nommu(SB_GDSTAR + i, comm_ram[comm_offset++]);
	}
	return true;
}

void NaomiM3Comm::vblank()
{
	if ((comm_ctrl & COMM_CTRL_RESET) == 0 || comm_status1 == 0)
		return;

	using the_clock = std::chrono::high_resolution_clock;
	the_clock::time_point start = the_clock::now();
	try {
		bool received = false;
		do {
			received = receiveNetwork();
		} while (!received && the_clock::now() - start < std::chrono::milliseconds(100));
		if (!received)
			INFO_LOG(NETWORK, "No data received");
		sendNetwork();
	} catch (const FlycastException& e) {
		comm_status0 = 0;
		comm_status1 = 0;
	}
}
