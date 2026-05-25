/*
	Copyright 2023 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "multiboard.h"

#ifdef NAOMI_MULTIBOARD
static Multiboard *multiboard;

#include "cfg/cfg.h"
#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#endif
#include "oslib/oslib.h"
#include "naomi_cart.h"
#include "naomi_roms.h"
#include "cfg/option.h"
#include "hw/sh4/sh4_sched.h"
#include <chrono>

constexpr int SyncCycles = 500000;

static int schedCallback(int tag, int cycles, int jitter, void *arg)
{
	multiboard->syncWait();
	return SyncCycles;
}

Multiboard::Multiboard()
{
	sharedMem = nullptr;

	boardId = config::loadInt("naomi", "BoardId");
#ifdef _WIN32
	const char *FileName = "Local\\naomi_multiboard_mem";
	if (isMaster())
		mapFile = CreateFileMapping(
				INVALID_HANDLE_VALUE,    // use paging file
				NULL,                    // default security
				PAGE_READWRITE,          // read/write access
				0,                       // max. object size (high-order)
				sizeof(SharedMemory),    // max. object size (low-order)
				FileName);               // name of mapping object
	else
		mapFile = OpenFileMapping(
			FILE_MAP_ALL_ACCESS,   // read/write access
			FALSE,                 // do not inherit the name
			FileName);             // name of mapping object

	if (mapFile == NULL)
	{
		ERROR_LOG(NAOMI, "Could not open/create file mapping (%d)", GetLastError());
	}
	else
	{
		INFO_LOG(NAOMI, "Created shared memory: \"%s\"", FileName);
		sharedMem = (SharedMemory *) MapViewOfFile(mapFile,   // handle to map object
				FILE_MAP_ALL_ACCESS, // read/write permission
				0,
				0,
				sizeof(SharedMemory));

		if (sharedMem == nullptr)
		{
			ERROR_LOG(NAOMI, "Could not map view of file (%d)", GetLastError());
			CloseHandle(mapFile);
			mapFile = NULL;
		}
	}
#else

	sharedMemFileName = "/naomi_multiboard_mem" + std::to_string(isMaster() ? getpid() : getppid());
	int fd = shm_open(sharedMemFileName.c_str(), O_RDWR | (isMaster() ? O_CREAT : 0), 0644);
	if (fd < 0)
		ERROR_LOG(NAOMI, "Can't open mapped file %s: errno %d", sharedMemFileName.c_str(), errno);
	else
	{
		if (isMaster() && ftruncate(fd, sizeof(SharedMemory)))
			ERROR_LOG(NAOMI, "Can't ftruncate mapped file. errno %d", errno);
		sharedMem = (SharedMemory *)mmap(nullptr, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
	}
#endif
	if (sharedMem == nullptr)
		throw FlycastException("Cannot initialize Naomi multiboard shared memory");
	if (isMaster())
	{
		new (sharedMem) SharedMemory();
		sharedMem->boardReady[1] = sharedMem->boardReady[2] = sharedMem->boardReady[3] = true;
		sharedMem->boardSynced[1] = sharedMem->boardSynced[2] = sharedMem->boardSynced[3] = true;
	}
	multiboard = this;
	schedId = sh4_sched_register(0, schedCallback);
	sh4_sched_request(schedId, SyncCycles);
}

void Multiboard::startSlave()
{
	if (isSlave() || slaveStarted)
		return;
	int slaves;
	if (config::MultiboardSlaves >= 2)
		slaves = CurrentCartridge->game->multiboard;
	else
		slaves = 1;
	boardCount = slaves + 1;
	for (int i = 0; i < boardCount; i++)
		sharedMem->boardReady[i] = false;

	int x = config::loadInt("window", "left", (1920 - 640) / 2);
	int y = config::loadInt("window", "top", (1080 - 480) / 2);
	int width = config::loadInt("window", "width", 640);
	std::string biosFile = CurrentCartridge->game->bios == nullptr ? "naomi" : CurrentCartridge->game->bios;
	std::string biosPath = hostfs::findNaomiBios(biosFile + ".zip");
	if (biosPath.empty())
		biosPath = hostfs::findNaomiBios(biosFile + ".7z");

	for (int i = 0; i < slaves; i++)
	{
		std::string region = "config:Dreamcast.Region=" + std::to_string(config::Region);
		std::string board = "naomi:BoardId=" + std::to_string(i + 1);
		int slaveX = x;
		if (slaves == 2)
			slaveX = i == 0 ? x - width : x + width;
		else if (slaves == 3)
			slaveX = i == 1 ? x - width : i == 2 ? x + width : x;
		std::string left = "window:left=" + std::to_string(slaveX);
		std::string top = "window:top=" + std::to_string(y);
		const char *args[] = {
			"-config", board.c_str(),
			"-config", region.c_str(),
			"-config", left.c_str(),
			"-config", top.c_str(),
			biosPath.c_str()
		};
		os_RunInstance(std::size(args), args);
	}
	slaveStarted = true;
}

void Multiboard::syncWait()
{
	if (isMaster() && !slaveStarted)
		return;

	{
		std::unique_lock<IpcMutex> lock(sharedMem->mutex);
		sharedMem->boardReady[boardId] = true;
		sharedMem->boardSynced[boardId] = false;
		sharedMem->cond.notify_all();
	}
	do {
		if (isSlave() && sharedMem->exit) {
			NOTICE_LOG(NAOMI, "Slave exiting");
			throw FlycastException("Slave exit");
		}
		{
			std::unique_lock<IpcMutex> lock(sharedMem->mutex);
			bool allReady = true;
			for (const auto& ready : sharedMem->boardReady)
				if (!ready) {
					allReady = false;
					break;
				}
			if (allReady) {
				sharedMem->boardSynced[boardId] = true;
				sharedMem->cond.notify_all();
				break;
			}
			if (sharedMem->cond.wait_for(lock, std::chrono::seconds(5)) ==  std::cv_status::timeout) {
				ERROR_LOG(NAOMI, "Time out waiting for multiboard vsync. Slave %d", isSlave());
				return;
			}
		}
	} while (true);
	if (isMaster())
	{
		do {
			{
				std::unique_lock<IpcMutex> lock(sharedMem->mutex);

				bool allSynced = true;
				for (const auto& synced : sharedMem->boardSynced)
					if (!synced) {
						allSynced = false;
						break;
					}
				if (allSynced)
				{
					for (int i = 0; i < boardCount; i++)
						sharedMem->boardReady[i] = false;
					break;
				}
				if (sharedMem->cond.wait_for(lock, std::chrono::seconds(5)) ==  std::cv_status::timeout) {
					ERROR_LOG(NAOMI, "Time out waiting for multiboard vsync");
					break;
				}
			}
		} while (true);
	}
}

Multiboard::~Multiboard()
{
	if (schedId != -1)
		sh4_sched_unregister(schedId);
	multiboard = nullptr;

	if (sharedMem != nullptr)
	{
		sharedMem->exit = true;
		sharedMem->cond.notify_all();
		if (isMaster())
			sharedMem->~SharedMemory();
	}
#ifdef _WIN32
	if (sharedMem != nullptr)
		UnmapViewOfFile(sharedMem);
	if (mapFile != NULL)
		CloseHandle(mapFile);
#else
	if (sharedMem != nullptr)
		munmap(sharedMem, sizeof(SharedMemory));
	shm_unlink(sharedMemFileName.c_str());
#endif
}

u32 Multiboard::readG1(u32 addr, u32 size)
{
	switch (addr)
	{
	case NAOMI_MBOARD_OFFSET_addr:
		return offset;

	case NAOMI_MBOARD_DATA_addr:
		{
			u32 bank = (sharedMem->status & 1) ? 0 : 0x80000;
			u32 addr;
			if (isMaster())
			{
				bank = 0x80000 - bank;
				addr = offset + bank / 2;
			}
			else
			{
				addr = offset + (boardId - 1) * 0x10000 + bank / 2;
			}
			u16 data = sharedMem->data[addr & (MEM_SIZE - 1)];
			DEBUG_LOG(NAOMI, "read NAOMI_COMM_DATA[%x]: %x (pc = %x)", addr, data, p_sh4rcb->cntx.pc);
			offset++;
			return data;
		}

	case 0x5f7074:
		DEBUG_LOG(NAOMI, "5F7074 read: %d", reg74);    // loops from 0 to ff
		return reg74 & 0xff;

	case 0x5f706C:
		DEBUG_LOG(NAOMI, "5F706C read");
		return 0;    // written to C4 afterwards & 7 except if == 7

	case G1_BASE + 0x08:
		DEBUG_LOG(NAOMI, "5F7088 read");
		return 0x80;    // loops until bit 7 is set

	case G1_BASE + 0x10:
		DEBUG_LOG(NAOMI, "5F7090 read");
		return 0x60;       // ? or 0x61 or 0x62

	case G1_BASE + 0x14:
		DEBUG_LOG(NAOMI, "5F7094 read");
		return 0x43;    // set to 43 before

	default:
		DEBUG_LOG(NAOMI, "Unknown G1 register read<%d>: %x (pc = %x)", size, addr, p_sh4rcb->cntx.pc);
		return 0xFFFF;
	}
}

void Multiboard::writeG1(u32 addr, u32 size, u32 data)
{
	switch (addr)
	{
	case NAOMI_MBOARD_OFFSET_addr:
		DEBUG_LOG(NAOMI, "NAOMI_COMM_OFFSET = %x (pc = %x)", data, p_sh4rcb->cntx.pc);
		offset = data;
		break;

	case NAOMI_MBOARD_DATA_addr:
		{
			DEBUG_LOG(NAOMI, "NAOMI_COMM_DATA[%x] = %x (pc = °%x)", offset, data, p_sh4rcb->cntx.pc);
			u32 bank = (sharedMem->status & 1) ? 0 : 0x80000;
			u32 addr;
			if (isMaster())
			{
				bank = 0x80000 - bank;
				addr = offset + bank / 2;
			}
			else
			{
				addr = offset + (boardId - 1) * 0x10000 + bank / 2;
			}
			sharedMem->data[addr & (MEM_SIZE - 1)] = data;
			offset++;
		}
		break;

	case 0x5f7070:
		DEBUG_LOG(NAOMI, "5F7070 written: %d", data);
		break;

	case 0x5f7074:
		DEBUG_LOG(NAOMI, "5F7074 written: %d", data);
		reg74 = data;
		startSlave();
		break;

	case 0x5f7058:      // Set to 0 before DMA operation from multiboard
		break;

	case NAOMI_MBOARD_STATUS_addr:
		// Set to 4 after writing most packets
		if (isSlave())
		{
			if ((data & 4) != 0)
				sharedMem->status.fetch_or(0x10 << boardId);
			//else
			//	sharedMem->status.fetch_and(~(0x10 << boardId));
		}
		break;

	default:
		DEBUG_LOG(NAOMI, "Unknown G1 register written<%d>: %x = %x (pc = %x)", size, addr, data, p_sh4rcb->cntx.pc);
		break;
	}
}

u32 Multiboard::readG2Ext(u32 addr, u32 size)
{
	//DEBUG_LOG(NAOMI, "g2ext_readMem<%d> %x (pc = %x)", size, addr, p_sh4rcb->cntx.pc);
	switch (addr)
	{
	case G2_BASE + 0x08:
		return 0x80;     // loops until bit 7 is set

	case G2_BASE + 0x10: // similar to 5F7090
		return 0x60;

	case G2_BASE + 0x14: // similar to 5F7094
		return 0x43;

	case G2_BASE + 0x94:
		return 0; // ?
	case G2_BASE + 0x98:
		return 0; // ?

	case G2_BASE + 0x9c: // similar to 5F7074
		return isMaster() ? reg9c : 0;

	case G2_BASE + 0xc0: // similar to 5F706C. need to match!
		return 0;

	case 0x1008000: // status reg
		{
			verify(size == 2);
			// seems to determine if acting as master or slave
			if (isSlave())
				return 0;
			u16 v = 0xff00 | sharedMem->status;
			DEBUG_LOG(NAOMI, "g2ext_readMem status_reg %x", v);
			return v;
		}

	default:
		if ((addr - 0x1020000) < MEM_SIZE * 2)
		{
			u32 bank = (sharedMem->status & 1) ? 0x80000 : 0;
			DEBUG_LOG(NAOMI, "g2ext_readMem<%d> %x -> %x", size, addr, sharedMem->data[(addr - 0x1020000 + bank) / 2]);
			verify(size >= 2);
			u32 offset;
			if (isSlave())
			{
				return 0;
				bank = 0x80000 - bank;
				if (addr >= 0x1040000) {
					INFO_LOG(NAOMI, "Read shared mem out of bound for slave: %x", addr);
					break;
				}
				offset = (addr - 0x1020000 + bank + (boardId - 1) * 0x20000) / 2;
			}
			else
				offset = (addr - 0x1020000 + bank) / 2;

			if (size == 2)
				return sharedMem->data[offset];
			else
				return *(u32 *)&sharedMem->data[offset];
		}
		break;
	}
	return 0;
}

void Multiboard::writeG2Ext(u32 addr, u32 size, u32 data)
{
	//DEBUG_LOG(NAOMI, "g2ext_writeMem<%d> %x = %x", size, addr, data);
	switch (addr) {
	case 0x1008000: // status reg
		verify(size == 2);
		if (isMaster())
			sharedMem->status = data;
		DEBUG_LOG(NAOMI, "g2ext_writeMem status_reg %x", data);
		break;

	case G2_BASE + 0x9c:
		reg9c = data;
		break;

	case G2_BASE + 0xa0: // LEDs
		DEBUG_LOG(NAOMI, "G2 leds %x", data);
		break;

	default:
		if ((addr - 0x1020000) < MEM_SIZE * 2)
		{
			DEBUG_LOG(NAOMI, "g2ext_writeMem<%d> %x: %x", size, addr, data);
			verify(size >= 2);
			u32 bank = (sharedMem->status & 1) ? 0x80000 : 0;
			if (isSlave())
			{
				break;
				bank = 0x80000 - bank;
				if (addr >= 0x1040000) {
					INFO_LOG(NAOMI, "Write shared mem out of bound for slave: %x", addr);
					break;
				}
				u32 offset = (addr - 0x1020000 + bank + (boardId - 1) * 0x20000) / 2;
				if (size == 2)
					sharedMem->data[offset] = data;
				else
					*(u32 *)&sharedMem->data[offset] = data;
			}
			else
			{
				u32 offset = (addr - 0x1020000 + bank) / 2;
				if ((sharedMem->status & 2) != 0 || addr >= 0x1040000)
				{
					if (size == 2)
						sharedMem->data[offset] = data;
					else
						*(u32 *)&sharedMem->data[offset] = data;
				}
				if (addr < 0x1040000) // FIXME this is weird
				{
					if ((sharedMem->status & 4) != 0)
					{
						if (size == 2)
							sharedMem->data[offset + 0x10000] = data;
						else
							*(u32 *)&sharedMem->data[offset + 0x10000] = data;
					}
					if (sharedMem->status & 8)
					{
						if (size == 2)
							sharedMem->data[offset + 0x20000] = data;
						else
							*(u32 *)&sharedMem->data[offset + 0x20000] = data;
					}
				}
			}
		}
		break;
	}
}

bool Multiboard::dmaStart()
{
	if (isMaster())
		return false;

	DEBUG_LOG(NAOMI, "Multiboard DMA start addr %08X len %d", SB_GDSTAR, SB_GDLEN);
	verify(1 == SB_GDDIR);
	u32 start = SB_GDSTAR & 0x1FFFFFE0;
	u32 len = (SB_GDLEN + 31) & ~31;
	u32 bank = (sharedMem->status & 1) ? 0 : 0x80000;
	u32 *src = (u32 *)&sharedMem->data[(boardId - 1) * 0x10000 + bank / 2];

	WriteMemBlock_nommu_ptr(start, src, len);
	SB_GDSTARD = start + len;
	SB_GDLEND = len;

	return true;
}

void Multiboard::reset()
{
	if (isSlave())
		sharedMem->status.fetch_and(~(0x10 << boardId));
}

#endif // NAOMI_MULTIBOARD
