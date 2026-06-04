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
#include "naomi_regs.h"
#include "cfg/option.h"
#include "hw/sh4/sh4_mem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/holly/holly_intc.h"
#include "hw/holly/sb.h"
#include "ui/gui.h"
#include "oslib/i18n.h"
#include <atomic>

using namespace i18n;

constexpr int BaseSyncCycles = 836208;	// 4 times per frame
constexpr u32 G1_BASE = 0x05F7080;
constexpr u32 G2_BASE = 0x1010000;

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
#endif // !_WIN32

template<typename T, size_t SIZE>
struct PipeData
{
	std::array<T, SIZE> data;
	u32 index = 0;
	IpcMutex mutex;
};

constexpr size_t MEM_SIZE = 0x100000 / sizeof(u16);

struct SharedMemory
{
	std::atomic<u16> status;
	IpcMutex mutex;
	IpcConditionVariable cond;
	std::atomic<bool> boardReady[4];
	std::atomic<bool> boardSynced[4];
	std::atomic<bool> exit;
	u16 data[MEM_SIZE];
	PipeData<std::pair<u16, bool>, 16> keyboardEvents;
};

template<typename T, size_t SIZE>
struct Pipe
{
	using Lock = std::unique_lock<IpcMutex>;

	Pipe(PipeData<T, SIZE>& data)
		: data(data)
	{}

	void receive()
	{
		while (writeIndex != data.index) {
			localData[writeIndex] = data.data[writeIndex];
			writeIndex = (writeIndex + 1) % SIZE;
		}
	}

	T read()
	{
		if (readIndex == writeIndex)
			// underflow
			return {};
		T v = localData[readIndex];
		readIndex = (readIndex + 1) % SIZE;
		return v;
	}

	bool available() {
		receive();
		return readIndex != writeIndex;
	}

	void write(T v)
	{
		Lock _(data.mutex);
		data.data[data.index] = v;
		data.index = (data.index + 1) % SIZE;
	}

	PipeData<T, SIZE>& data;
	std::array<T, SIZE> localData;
	u32 writeIndex = 0;
	u32 readIndex = 0;
};

using KeyboardEvent = std::pair<u16, bool>;
struct MultiKeyboard : public Pipe<KeyboardEvent, 16>
{
	MultiKeyboard(PipeData<KeyboardEvent, 16>& data)
		: Pipe<KeyboardEvent, 16>(data)
	 {}
};

Multiboard *Multiboard::Instance;

Multiboard::Multiboard()
{
	Instance = this;
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
	if (settings.content.gameId.substr(0, 4) == "F355")
		syncCycles = BaseSyncCycles / 2;
	else
		syncCycles = BaseSyncCycles;
	schedId = sh4_sched_register(0, schedCallback, this);
	sh4_sched_request(schedId, syncCycles);
	keyboard = std::make_unique<MultiKeyboard>(sharedMem->keyboardEvents);
}

int Multiboard::schedCallback(int tag, int cycles, int jitter, void *arg)
{
	Multiboard *multiboard = (Multiboard *)arg;
	multiboard->syncWait();
	return multiboard->syncCycles;
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

	bool derby = settings.content.gameId.substr(0, 6) == " DERBY";
	int x = config::loadInt("window", "left", (1920 - 640) / 2);
	int y = config::loadInt("window", "top", (1080 - 480) / 2);
	int width = config::loadInt("window", "width", 640);
	std::string romPath;
	if (derby)
		// DOC slaves have the same cart as master
		romPath = settings.content.path;
	else
		romPath = CurrentCartridge->game->bios == nullptr ? "naomi" : CurrentCartridge->game->bios;

	for (int i = 0; i < slaves; i++)
	{
		std::string region = "config:Dreamcast.Region=" + std::to_string(config::Region);
		std::string board = "naomi:BoardId=" + std::to_string(i + 1);
		int slaveX = x;
		const char *location = "";
		if (derby) {
			slaveX = x + width;
			location = T("Right");
		}
		else if (slaves == 2)
		{
			if (i == 0) {
				slaveX = x - width;
				location = T("Left");
			}
			else {
				slaveX = x + width;
				location = T("Right");
			}
		}
		else if (slaves == 3)
		{
			switch (i)
			{
			case 0:
				slaveX = x;
				location = T("Center");
				break;
			case 1:
				slaveX = x - width;
				location = T("Left");
				break;
			case 2:
				slaveX = x + width;
				location = T("Right");
				break;
			}
		}
		std::string left = "window:left=" + std::to_string(slaveX);
		std::string top = "window:top=" + std::to_string(y);
		std::string title = "window:title=\"" + std::string(location) + " - " + settings.content.title + '"';
		std::string gameId = "naomi:gameId=\"" + settings.content.gameId + "-SLAVE\"";
		const char *args[] = {
			"-config", board.c_str(),
			"-config", region.c_str(),
			"-config", left.c_str(),
			"-config", top.c_str(),
			"-config", gameId.c_str(),
			"-config", title.c_str(),
			romPath.c_str()
		};
		os_RunInstance(std::size(args), args);
	}
	slaveStarted = true;
}

void sdlReceiveSlaveKeyboardEvent(u16 scancode, bool pressed);

void Multiboard::syncWait()
{
	if (isMaster() && !slaveStarted) {
		startSlave();
		return;
	}

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
		// Receive keyboard events and pipe them back to SDL. Must be done on the main/ui thread
		if (keyboard != nullptr)
		{
			while (keyboard->available())
			{
				auto pair = keyboard->read();
				if (!pair.second)
					// Ignore key up events: we always send them in pair
					continue;
				gui_runOnUiThread([pair]() {
					sdlReceiveSlaveKeyboardEvent(pair.first, true);
					sdlReceiveSlaveKeyboardEvent(pair.first, false);
				});
			}
		}
	}
	else
	{
		const u16 newStatus = sharedMem->status;
		if ((newStatus ^ g2status) & 1) {
			// Bank switch
			if ((1 << boardId) & newStatus) {
				DEBUG_LOG(NAOMI, "[%d] Slave gdrom interrupt", boardId);
				asic_RaiseInterrupt(holly_GDROM_CMD);
			}
		}
		g2status = newStatus;
	}
}

Multiboard::~Multiboard()
{
	Instance = nullptr;
	if (schedId != -1)
		sh4_sched_unregister(schedId);

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

std::pair<u32, bool> Multiboard::readG1(u32 addr, u32 size)
{
	switch (addr)
	{
	case NAOMI_MBOARD_OFFSET_addr:
		return std::make_pair(offset, true);

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
				addr = (offset & 0xffff) + (boardId - 1) * 0x10000 + bank / 2;
			}
			u16 data = sharedMem->data[addr & (MEM_SIZE - 1)];
			//DEBUG_LOG(NAOMI, "[%d] read MBOARD_DATA[%x]: %x (pc = %x)", boardId, addr, data, p_sh4rcb->cntx.pc);
			offset++;
			return std::make_pair(data, true);
		}

	case NOAMI_MBOARD_DMA_OFFSET_addr:
		DEBUG_LOG(NAOMI, "[%d] MBOARD_DMA_OFFSET read: %x", boardId, dmaOffset);
		return std::make_pair(dmaOffset, true);

	case NAOMI_MBOARD_BOARD_ID_addr:
		DEBUG_LOG(NAOMI, "[%d] MBOARD_BOARD_ID read", boardId);
		 // b3: link transport mode (0=multiboard uart, 1=sh4 scif)
		 // b4: link speed (0=38400, 1=115200)
		return std::make_pair(boardId | 0x10, true);

	case G1_BASE + 0x08:
		DEBUG_LOG(NAOMI, "[%d] 5F7088 read", boardId);
		return std::make_pair(0x80, true);    // loops until bit 7 is set

	case G1_BASE + 0x10:
		DEBUG_LOG(NAOMI, "[%d] 5F7090 read", boardId);
		return std::make_pair(0x60, true);       // ? or 0x61 or 0x62

	case G1_BASE + 0x14:
		DEBUG_LOG(NAOMI, "[%d] 5F7094 read", boardId);
		return std::make_pair(0x43, true);    // set to 43 before

	case NAOMI_MBOARD_CONFIG_SLOT_addr:
		DEBUG_LOG(NAOMI, "[%d] Multiboard config slot read", boardId);
		return std::make_pair(boardId, true);

	default:
		return std::make_pair(0, false);
	}
}

bool Multiboard::writeG1(u32 addr, u32 data, u32 size)
{
	switch (addr)
	{
	case NAOMI_MBOARD_OFFSET_addr:
		offset = data;
		return true;

	case NAOMI_MBOARD_DATA_addr:
		{
			DEBUG_LOG(NAOMI, "[%d] MBOARD_DATA[%x] = %x (pc = %x)", boardId, offset, data, p_sh4rcb->cntx.pc);
			u32 bank = (sharedMem->status & 1) ? 0 : 0x80000;
			u32 addr;
			if (isMaster())
			{
				bank = 0x80000 - bank;
				addr = offset + bank / 2;
			}
			else
			{
				addr = (offset & 0xffff) + (boardId - 1) * 0x10000 + bank / 2;
			}
			sharedMem->data[addr & (MEM_SIZE - 1)] = data;
			offset++;
		}
		return true;

	// The multiboard includes a 16550 UART.
	// It is accessed on the G1 bus through an index register at 5F7070 (to select the 16550 register [0-7])
	// and a data register at 5F7074 to read from, or write to, the selected register.
	// All 8 registers are directly mapped on the G2 bus at G2_BASE + [80-9C]
	// It doesn't seem to be used in normal operations.

	case NOAMI_MBOARD_DMA_OFFSET_addr:
		DEBUG_LOG(NAOMI, "[%d] MBOARD_DMA_OFFSET written: %x", boardId, data);
		dmaOffset = data;
		return true;

	case NAOMI_MBOARD_STATUS_addr:
		if (isSlave())
		{
			DEBUG_LOG(NAOMI, "[%d] MBOARD_STATUS = %x", boardId, data);
			switch (data)
			{
			case 3: // clear GDROM interrupt
				asic_CancelInterrupt(holly_GDROM_CMD);
				break;
			case 4: // board ready
				sharedMem->status.fetch_or(0x10 << boardId);
				break;
			default:
				break;
			}
		}
		return true;

	case NAOMI_MBOARD_CONFIG_SLOT_addr:
		DEBUG_LOG(NAOMI, "[%d] Multiboard config slot set to %d", boardId, data);
		return true;

	default:
		return false;
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

	case G2_BASE + 0xc0: // MBOARD_BOARD_ID
		return boardId | 0x10;

	case 0x1008000: // status reg
		{
			verify(size == 2);
			// seems to determine if acting as master or slave
			if (isSlave())
				return 0;
			u16 v = 0xff00 | sharedMem->status;
			//DEBUG_LOG(NAOMI, "[%d] g2ext_readMem status_reg %x", boardId, v);
			return v;
		}

	default:
		if ((addr - 0x1020000) < MEM_SIZE * 2 && isMaster())
		{
			u32 bank = (sharedMem->status & 1) ? 0x80000 : 0;
			verify(size >= 2);
			u32 offset = (addr - 0x1020000 + bank) / 2;

			if (size == 2) {
				DEBUG_LOG(NAOMI, "[%d] g2ext_readMem<%d> %x -> %x", boardId, size, addr, sharedMem->data[offset]);
				return sharedMem->data[offset];
			}
			else {
				DEBUG_LOG(NAOMI, "[%d] g2ext_readMem<%d> %x -> %x", boardId, size, addr, *(u32 *)&sharedMem->data[offset]);
				return *(u32 *)&sharedMem->data[offset];
			}
		}
		break;
	}
	return 0;
}

void Multiboard::writeG2Ext(u32 addr, u32 data, u32 size)
{
	//DEBUG_LOG(NAOMI, "g2ext_writeMem<%d> %x = %x", size, addr, data);
	switch (addr)
	{
	case 0x1008000: // status reg
		verify(size <= 2);
		DEBUG_LOG(NAOMI, "[%d] G2_STATUS write %x", boardId, data);
		if (isMaster()) {
			verify(size == 2);
			sharedMem->status = data;
		}
		break;

	case G2_BASE + 0xa0: // LEDs
		DEBUG_LOG(NAOMI, "[%d] G2 leds %x", boardId, data);
		break;

	default:
		if ((addr - 0x1020000) < MEM_SIZE * 2 && isMaster())
		{
			DEBUG_LOG(NAOMI, "[%d] g2ext_writeMem<%d> %x: %x pc %x pr %x", boardId, size, addr, data, p_sh4rcb->cntx.pc, p_sh4rcb->cntx.pr);
			verify(size >= 2);
			u32 bank = (sharedMem->status & 1) ? 0x80000 : 0;
			u32 offset = (addr - 0x1020000 + bank) / 2;
			if ((sharedMem->status & 2) != 0 || addr >= 0x1040000)
			{
				int slot = (addr - 0x1020000) / 0x20000 + 1;
				// Only connected boards have a writable shared memory area
				if (slot < boardCount)
				{
					if (size == 2)
						sharedMem->data[offset] = data;
					else
						*(u32 *)&sharedMem->data[offset] = data;
				}
			}
			if (addr < 0x1040000)
			{
				if ((sharedMem->status & 4) != 0 && boardCount >= 3)
				{
					if (size == 2)
						sharedMem->data[offset + 0x10000] = data;
					else
						*(u32 *)&sharedMem->data[offset + 0x10000] = data;
				}
				if ((sharedMem->status & 8) != 0 && boardCount >= 4)
				{
					if (size == 2)
						sharedMem->data[offset + 0x20000] = data;
					else
						*(u32 *)&sharedMem->data[offset + 0x20000] = data;
				}
			}
		}
		break;
	}
}

bool Multiboard::dmaStart()
{
	if (isMaster() || (CurrentCartridge != nullptr && CurrentCartridge->getSize() > 0))
		return false;

	DEBUG_LOG(NAOMI, "Multiboard DMA start offset %x addr %08X len %d", dmaOffset, SB_GDSTAR, SB_GDLEN);
	verify(1 == SB_GDDIR);
	u32 start = SB_GDSTAR & 0x1FFFFFE0;
	u32 len = (SB_GDLEN + 31) & ~31;
	u32 bank = (sharedMem->status & 1) ? 0 : 0x80000;
	u32 *src = (u32 *)&sharedMem->data[(boardId - 1) * 0x10000 + bank / 2 + dmaOffset];

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

void Multiboard::keyboardEvent(u16 code, bool pressed) {
	if (Instance != nullptr && Instance->keyboard != nullptr)
		Instance->keyboard->write(std::make_pair(code, pressed));
}

#endif // NAOMI_MULTIBOARD
