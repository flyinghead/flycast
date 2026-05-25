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
	class Semaphore
	{
		HANDLE handle;

	public:
		Semaphore()
		{
			SECURITY_ATTRIBUTES secattr{ sizeof(SECURITY_ATTRIBUTES) };
			secattr.bInheritHandle = TRUE;
			handle = CreateSemaphore(&secattr, 0, std::numeric_limits<LONG>::max(), NULL);
			if (handle == NULL)
				throw std::runtime_error("Semaphore create failed");
		}
		~Semaphore() {
			CloseHandle(handle);
		}

		bool wait(DWORD msecs = INFINITE)
		{
			DWORD rc = WaitForSingleObject(handle, msecs);
			if (rc == WAIT_ABANDONED || rc == WAIT_FAILED)
				throw std::runtime_error("Semaphore wait failure");
			return rc != WAIT_TIMEOUT;
		}

		void signal(int n = 1) {
			ReleaseSemaphore(handle, n, nullptr);
		}
	};

	Semaphore semaphore;
	IpcMutex mutex;
	int waiters = 0;
	// The unlock/wait/lock in wait() should be atomic so the h semaphore is used
	// to make sure all processes about to wait (i.e. on the waiters list) are notified before any other.
	// This race condition happens more frequently with one slave, where one process
	// notifies the other that everybody is ready and ends up notifying itself,
	// while the other process waits indefinitely.
	// See https://birrell.org/andrew/papers/ImplementingCVs.pdf
	Semaphore h;

	std::cv_status waitMs(std::unique_lock<IpcMutex>& lock, DWORD msecs)
	{
		mutex.lock();
		waiters++;
		mutex.unlock();

		lock.unlock();
		std::cv_status status = semaphore.wait(msecs) ?
				std::cv_status::no_timeout : std::cv_status::timeout;
		// must be done before re-acquiring the lock
		h.signal();
		lock.lock();
		return status;
	}

public:
	void notify_all()
	{
		mutex.lock();
		if (waiters > 0)
		{
			semaphore.signal(waiters);
			// make sure waiters are notified before moving on
			do {
				h.wait();
				waiters--;
			} while (waiters > 0);
		}
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

	u32 readG1(u32 addr, u32 size);
	void writeG1(u32 addr, u32 size, u32 data);

	u32 readG2Ext(u32 addr, u32 size);
	void writeG2Ext(u32 addr, u32 size, u32 data);

	bool dmaStart();
	void reset();
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
