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
#pragma once
#include "types.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

class NaomiM3Comm
{
public:
	u32 ReadMem(u32 address, u32 size);
	void WriteMem(u32 address, u32 data, u32 size);
	bool DmaStart(u32 addr, u32 data);

	void closeNetwork();

private:
	void initNetwork();
	void connectNetwork();
	void receiveNetwork();
	void sendNetwork();
	void connectedState(bool success);
	void startThread();

	u16 comm_ctrl = 0xC000;
	u16 comm_offset = 0;
	u16 comm_status0 = 0;
	u16 comm_status1 = 0;
	u8 m68k_ram[128 * 1024];
	u8 comm_ram[128 * 1024];
	u16 packet_number = 0;

	int slot_count = 0;
	int slot_id = 0;
	std::atomic<bool> network_stopping{ false };
	std::unique_ptr<std::thread> thread;
	std::mutex mem_mutex;
};
