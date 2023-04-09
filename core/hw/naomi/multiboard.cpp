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

static Multiboard *multiboard;

#ifdef NAOMI_MULTIBOARD
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
#include <thread>
#include <chrono>

constexpr int SyncCycles = 500000;

static int schedCallback(int tag, int cycles, int jitter)
{
	multiboard->syncWait();
	return SyncCycles;
}

Multiboard::Multiboard()
{
	sharedMem = nullptr;

	boardId = cfgLoadInt("naomi", "BoardId", 0);
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

	int x = cfgLoadInt("window", "left", (1920 - 640) / 2);
	int y = cfgLoadInt("window", "top", (1080 - 480) / 2);
	int width = cfgLoadInt("window", "width", 640);
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
			CurrentCartridge->game->bios == nullptr ? "naomi" : CurrentCartridge->game->bios
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

#endif // NAOMI_MULTIBOARD
