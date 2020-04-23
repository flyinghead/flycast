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
#include "naomi_m3comm.h"
#include "naomi_regs.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "network/naomi_network.h"
#include <chrono>

static inline u16 swap16(u16 w)
{
	return (w >> 8) | (w << 8);
}

void NaomiM3Comm::closeNetwork()
{
	network_stopping = true;
	naomiNetwork.shutdown();
	if (thread && thread->joinable())
		thread->join();
}

void NaomiM3Comm::connectNetwork()
{
	packet_number = 0;
	if (naomiNetwork.syncNetwork())
	{
		slot_count = naomiNetwork.slotCount();
		slot_id = naomiNetwork.slotId();
		connectedState(true);
	}
	else
	{
		connectedState(false);
		network_stopping = true;
		naomiNetwork.shutdown();
	}
}

void NaomiM3Comm::receiveNetwork()
{
	const u32 slot_size = swap16(*(u16*)&m68k_ram[0x204]);
	const u32 packet_size = slot_size * slot_count;

	std::unique_ptr<u8[]> buf(new u8[packet_size]);

	if (naomiNetwork.receive(buf.get(), packet_size))
	{
		packet_number += slot_count - 1;
		*(u16*)&comm_ram[6] = swap16(packet_number);
		std::unique_lock<std::mutex> lock(mem_mutex);
		memcpy(&comm_ram[0x100 + slot_size], buf.get(), packet_size);
	}
}

void NaomiM3Comm::sendNetwork()
{
	if (naomiNetwork.hasToken())
	{
		const u32 packet_size = swap16(*(u16*)&m68k_ram[0x204]) * slot_count;
		std::unique_lock<std::mutex> lock(mem_mutex);
		naomiNetwork.send(&comm_ram[0x100], packet_size);
		packet_number++;
		*(u16*)&comm_ram[6] = swap16(packet_number);
	}
}

u32 NaomiM3Comm::ReadMem(u32 address, u32 size)
{
	switch (address & 255)
	{
	case NAOMI_COMM2_CTRL_addr & 255:
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL read");
		return comm_ctrl;

	case NAOMI_COMM2_OFFSET_addr & 255:
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_OFFSET read");
		return comm_offset;

	case NAOMI_COMM2_DATA_addr & 255:
	{
		u16 value;
		if (comm_ctrl & 1)
			value = *(u16*)&m68k_ram[comm_offset];
		else
			// TODO u16 *commram = (u16*)membank("comm_ram")->base();
			value = *(u16*)&comm_ram[comm_offset];
		value = swap16(value);
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_DATA %s read @ %04x: %x", (comm_ctrl & 1) ? "m68k ram" : "comm ram", comm_offset, value);
		comm_offset += 2;
		return value;
	}

	case NAOMI_COMM2_STATUS0_addr & 255:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS0 read %x", comm_status0);
		return comm_status0;

	case NAOMI_COMM2_STATUS1_addr & 255:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS1 read %x", comm_status1);
		return comm_status1;

	default:
		DEBUG_LOG(NAOMI, "NaomiM3Comm::ReadMem unmapped: %08x sz %d", address, size);
		return 0xffffffff;
	}
}

void NaomiM3Comm::connectedState(bool success)
{
	if (!success)
		return;

	memset(&comm_ram[0xf000], 0, 16);
	comm_ram[0xf000] = 1;
	comm_ram[0xf001] = 1;
	comm_ram[0xf002] = m68k_ram[0x204];
	comm_ram[0xf003] = m68k_ram[0x205];

	u32 slot_size = swap16(*(u16*)&m68k_ram[0x204]);

	memset(&comm_ram[0], 0, 32);
	// 80000
	comm_ram[0] = 0;
	comm_ram[1] = slot_id == 0 ? 0 : 1;
	// 80002
	comm_ram[2] = 0x01;
	comm_ram[3] = 0x01;
	// 80004
	if (slot_id == 0)
	{
		comm_ram[4] = 0;
		comm_ram[5] = 0;
	}
	else
	{
		comm_ram[4] = 1;
		comm_ram[5] = 1;
	}
	// 80006: packet number
	comm_ram[6] = 0;
	comm_ram[7] = 0;
	// 80008
	comm_ram[8] = slot_id == 0 ? 0x78 : 0x73;
	comm_ram[9] = slot_id == 0 ? 0x30 : 0xa2;
	// 8000A
	*(u16 *)(comm_ram + 10) = 0x100 + slot_size;		// offset of recvd data
	// 8000C
	*(u16 *)(comm_ram + 12) = slot_size * slot_count;	// recvd data size
	// 8000E
	*(u16 *)(comm_ram + 14) = 0x100;					// offset of sent data
	// 80010
	*(u16 *)(comm_ram + 16) = 0x80 + slot_size * slot_count;	// sent data size
														// FIXME wrungp uses 100, others 80

	comm_status0 = 0xff01;	// But 1 at connect time before f000 is read
	comm_status1 = (slot_count << 8) | slot_id;
}

void NaomiM3Comm::WriteMem(u32 address, u32 data, u32 size)
{
	switch (address & 255)
	{
	case NAOMI_COMM2_CTRL_addr & 255:
		// bit 0: access RAM is 0 - communication RAM / 1 - M68K RAM
		// bit 1: comm RAM bank (seems R/O for SH4)
		// bit 5: M68K Reset
		// bit 6: ???
		// bit 7: might be M68K IRQ 5 or 2
		// bit 14: G1 DMA bus master 0 - active / 1 - disabled
		// bit 15: 0 - enable / 1 - disable this device ???
		if (data & (1 << 5))
		{
			DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL m68k reset");
			memset(&comm_ram[0], 0, 32);
			comm_status0 = 0; // varies...
			comm_status1 = 0;
			if (!thread || !thread->joinable())
				startThread();
		}
		comm_ctrl = (u16)(data & ~(1 << 5));
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_CTRL set to %x", comm_ctrl);
		return;

	case NAOMI_COMM2_OFFSET_addr & 255:
		comm_offset = (u16)data;
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_OFFSET set to %x", comm_offset);
		return;

	case NAOMI_COMM2_DATA_addr & 255:
		DEBUG_LOG(NAOMI, "NAOMI_COMM2_DATA written @ %04x %04x", comm_offset, (u16)data);
		data = swap16(data);
		if (comm_ctrl & 1)
			*(u16*)&m68k_ram[comm_offset] = (u16)data;
		else
			*(u16*)&comm_ram[comm_offset] = (u16)data;
		comm_offset += 2;
		return;

	case NAOMI_COMM2_STATUS0_addr & 255:
		comm_status0 = (u16)data;
		//DEBUG_LOG(NAOMI, "NAOMI_COMM2_STATUS0 set to %x", comm_status0);
		return;

	case NAOMI_COMM2_STATUS1_addr & 255:
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
	if (comm_ctrl & 0x4000)
		return false;

	DEBUG_LOG(NAOMI, "NaomiM3Comm: DMA addr %08X <-> %04x len %d %s", SB_GDSTAR, comm_offset, SB_GDLEN, SB_GDDIR == 0 ? "OUT" : "IN");
	std::unique_lock<std::mutex> lock(mem_mutex);
	if (SB_GDDIR == 0)
	{
		// Network write
		for (u32 i = 0; i < SB_GDLEN; i++)
			comm_ram[comm_offset++] = ReadMem8_nommu(SB_GDSTAR + i);
	}
	else
	{
		// Network read
		if (SB_GDLEN == 32 && (comm_ctrl & 1) == 0)
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
		for (u32 i = 0; i < SB_GDLEN; i++)
			WriteMem8_nommu(SB_GDSTAR + i, comm_ram[comm_offset++]);
	}
	return true;
}

void NaomiM3Comm::startThread()
{
	network_stopping = false;
	thread = std::unique_ptr<std::thread>(new std::thread([this]() {
		using the_clock = std::chrono::high_resolution_clock;

		connectNetwork();

		the_clock::time_point token_time = the_clock::now();

		while (!network_stopping)
		{
			naomiNetwork.pipeSlaves();
			receiveNetwork();

			if (slot_id == 0 && naomiNetwork.hasToken())
			{
				const auto target_duration = std::chrono::milliseconds(10);
				auto duration = the_clock::now() - token_time;
				if (duration < target_duration)
				{
					DEBUG_LOG(NAOMI, "Sleeping for %ld ms", std::chrono::duration_cast<std::chrono::milliseconds>(target_duration - duration).count());
					std::this_thread::sleep_for(target_duration - duration);
				}
				token_time = the_clock::now();
			}

			sendNetwork();

		}
		DEBUG_LOG(NAOMI, "Network thread exiting");
	}));
}
