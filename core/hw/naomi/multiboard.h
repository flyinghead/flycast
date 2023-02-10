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
#pragma once
#include "types.h"
#ifdef NAOMI_MULTIBOARD
#include "naomi_regs.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include <atomic>


#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>

class IpcMutex
{
	HANDLE mutex;

public:
	using native_handle_type = HANDLE;

	IpcMutex()
	{
		SECURITY_ATTRIBUTES secattr{ sizeof(SECURITY_ATTRIBUTES) };
		secattr.bInheritHandle = TRUE;

		mutex = CreateMutex(&secattr, FALSE, NULL);
		if (mutex == NULL)
			throw std::runtime_error("CreateMutex failed");
	}
	IpcMutex(const IpcMutex&) = delete;

	~IpcMutex() {
		CloseHandle(mutex);
	}

	void lock() {
		WaitForSingleObject(mutex, INFINITE);
	}

	void unlock() {
		ReleaseMutex(mutex);
	}

	native_handle_type native_handle() {
		return mutex;
	}

	IpcMutex& operator=(const IpcMutex&) = delete;
};

class IpcConditionVariable
{
	HANDLE semaphore;
	IpcMutex mutex;
	int waiters = 0;

	std::cv_status waitMs(std::unique_lock<IpcMutex>& lock, DWORD msecs)
	{
		mutex.lock();
		waiters++;
		mutex.unlock();

		// The unlock/wait/lock should be atomical so this implementation isn't compliant
		lock.unlock();
		DWORD rc = WaitForSingleObject(semaphore, msecs);
		lock.lock();
		if (rc == WAIT_ABANDONED || rc == WAIT_FAILED)
			throw std::runtime_error("Semaphore wait failure");
		return rc == WAIT_TIMEOUT ? std::cv_status::timeout : std::cv_status::no_timeout;
	}

public:
	IpcConditionVariable()
	{
		SECURITY_ATTRIBUTES secattr{ sizeof(SECURITY_ATTRIBUTES) };
		secattr.bInheritHandle = TRUE;
		semaphore = CreateSemaphore(&secattr, 0, std::numeric_limits<LONG>::max(), NULL);
		if (semaphore == NULL)
			throw std::runtime_error("Semaphore create failed");
	}

	~IpcConditionVariable()
	{
		CloseHandle(semaphore);
	}

	void notify_all()
	{
		mutex.lock();
		ReleaseSemaphore(semaphore, waiters, NULL);
		waiters = 0;
		mutex.unlock();
	}

	void wait(std::unique_lock<IpcMutex>& lock)
	{
		waitMs(lock, INFINITE);
	}

	template<class Rep, class Period>
	std::cv_status wait_for(std::unique_lock<IpcMutex>& lock,
			const std::chrono::duration<Rep, Period>& rel_time)
	{
		return waitMs(lock, std::chrono::duration_cast<std::chrono::milliseconds>(rel_time).count());
	}

	IpcConditionVariable& operator=(const IpcConditionVariable&) = delete;
};

#else // _!WIN32

#include <pthread.h>

class IpcMutex
{
	pthread_mutex_t mutex;

public:
	using native_handle_type = pthread_mutex_t*;

	IpcMutex()
	{
		pthread_mutexattr_t mattr;
		pthread_mutexattr_init(&mattr);
		pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
		pthread_mutex_init(&mutex, &mattr);
		pthread_mutexattr_destroy(&mattr);
	}
	IpcMutex(const IpcMutex&) = delete;

	~IpcMutex() {
		pthread_mutex_destroy(&mutex);
	}

	void lock() {
		pthread_mutex_lock(&mutex);
	}

	void unlock() {
		pthread_mutex_unlock(&mutex);
	}

	native_handle_type native_handle() {
		return &mutex;
	}

	IpcMutex& operator=(const IpcMutex&) = delete;
};

class IpcConditionVariable
{
	pthread_cond_t cond;

public:
	IpcConditionVariable()
	{
		pthread_condattr_t cvattr;
		pthread_condattr_init(&cvattr);
		pthread_condattr_setpshared(&cvattr, PTHREAD_PROCESS_SHARED);
		pthread_cond_init(&cond, &cvattr);
		pthread_condattr_destroy(&cvattr);
	}
	IpcConditionVariable(const IpcConditionVariable&) = delete;

	~IpcConditionVariable() {
		pthread_cond_destroy(&cond);
	}

	void notify_all() {
		pthread_cond_broadcast(&cond);
	}

	void wait(std::unique_lock<IpcMutex>& lock) {
		pthread_cond_wait(&cond, lock.mutex()->native_handle());
	}

	template<class Rep, class Period>
	std::cv_status wait_for(std::unique_lock<IpcMutex>& lock,
			const std::chrono::duration<Rep, Period>& rel_time)
	{
		timespec ts;
	    clock_gettime(CLOCK_REALTIME, &ts);
	    std::chrono::seconds seconds = std::chrono::duration_cast<std::chrono::seconds>(rel_time);
	    ts.tv_sec += seconds.count();
	    auto nanoTime = rel_time - seconds;
	    ts.tv_nsec += std::chrono::nanoseconds(nanoTime).count();
	    if (ts.tv_nsec >= 1000000000) {
	    	ts.tv_sec++;
	    	ts.tv_nsec -= 1000000000;
	    }
		int rc = pthread_cond_timedwait(&cond, lock.mutex()->native_handle(), &ts);
		return rc == ETIMEDOUT ? std::cv_status::timeout : std::cv_status::no_timeout;
	}

	IpcConditionVariable& operator=(const IpcConditionVariable&) = delete;
};
#endif // _!WIN32

class Multiboard
{
public:
	static constexpr u32 G1_BASE = 0x05F7080;
	static constexpr u32 G2_BASE = 0x1010000;

	Multiboard();
	~Multiboard();

	u32 readG1(u32 addr, u32 size)
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

	void writeG1(u32 addr, u32 size, u32 data)
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

	u32 readG2Ext(u32 addr, u32 size)
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

	void writeG2Ext(u32 addr, u32 size, u32 data)
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

	bool dmaStart()
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

	void reset()
	{
		if (isSlave())
			sharedMem->status.fetch_and(~(0x10 << boardId));
	}

	void syncWait();

private:
	bool isMaster() const { return boardId == 0; }
	bool isSlave() const { return boardId != 0; }

	void startSlave();

	static constexpr size_t MEM_SIZE = 0x100000 / sizeof(u16);
	struct SharedMemory
	{
		std::atomic<u16> status;
		IpcMutex mutex;
		IpcConditionVariable cond;
		std::atomic<bool> boardReady[4];
		std::atomic<bool> boardSynced[4];
		std::atomic<bool> exit;
		u16 data[MEM_SIZE];
	};
	int boardId = 0;
	u32 offset = 0;
	u16 reg74 = 0;
	u32 reg9c = 0;
	SharedMemory *sharedMem;
#ifdef _WIN32
	HANDLE mapFile;
#else
	std::string sharedMemFileName;
#endif
	int boardCount = 0;
	bool slaveStarted = false;
	int schedId;
};

#else // !NAOMI_MULTIBOARD

class Multiboard
{
public:
	u32 readG1(u32 addr, u32 size) {
		return 0;
	}
	void writeG1(u32 addr, u32 size, u32 data) { }

	u32 readG2Ext(u32 addr, u32 size) {
		return 0;
	}
	void writeG2Ext(u32 addr, u32 size, u32 data) { }

	bool dmaStart() {
		return false;
	}
	void reset() { }

	void syncWait() { }
};

#endif // !NAOMI_MULTIBOARD
