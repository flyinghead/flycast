#pragma once
#include "types.h"
#include <cstdlib>
#include <vector>
#include <cstring>

#ifndef _WIN32
#include <pthread.h>
#else
#include <windows.h>
#endif


#ifdef __ANDROID__
#include <sys/mman.h>
#undef PAGE_MASK
#define PAGE_MASK (PAGE_SIZE-1)
#else
#define PAGE_SIZE 4096
#define PAGE_MASK (PAGE_SIZE-1)
#endif

//Threads

#if !defined(HOST_NO_THREADS)
typedef  void* ThreadEntryFP(void* param);

class cThread {
private:
	ThreadEntryFP* entry;
	void* param;
public :
	#ifdef _WIN32
	HANDLE hThread;
	#else
	pthread_t *hThread;
	#endif

	cThread(ThreadEntryFP* function, void* param)
		:entry(function), param(param), hThread(NULL) {}
	~cThread() { WaitToEnd(); }
	void Start();
	void WaitToEnd();
};
#endif


//Wait Events
typedef void* EVENTHANDLE;
class cResetEvent
{
private:
#ifdef _WIN32
	EVENTHANDLE hEvent;
#else
	pthread_mutex_t mutx;
	pthread_cond_t cond;
	bool state;
#endif

public :
	cResetEvent();
	~cResetEvent();
	void Set();		//Set state to signaled
	void Reset();	//Set state to non signaled
	bool Wait(u32 msec);//Wait for signal , then reset[if auto]. Returns false if timed out
	void Wait();	//Wait for signal , then reset[if auto]
};

class cMutex
{
private:
#ifdef _WIN32
	CRITICAL_SECTION cs;
#else
	pthread_mutex_t mutx;
#endif

public :
	bool state;
	cMutex()
	{
#ifdef _WIN32
		InitializeCriticalSection(&cs);
#else
		pthread_mutex_init ( &mutx, NULL);
#endif
	}
	~cMutex()
	{
#ifdef _WIN32
		DeleteCriticalSection(&cs);
#else
		pthread_mutex_destroy(&mutx);
#endif
	}
	void Lock()
	{
#ifdef _WIN32
		EnterCriticalSection(&cs);
#else
		pthread_mutex_lock(&mutx);
#endif
	}
	bool TryLock()
	{
#ifdef _WIN32
		return TryEnterCriticalSection(&cs);
#else
		return pthread_mutex_trylock(&mutx)==0;
#endif
	}
	void Unlock()
	{
#ifdef _WIN32
		LeaveCriticalSection(&cs);
#else
		pthread_mutex_unlock(&mutx);
#endif
	}
	// std::BasicLockable so we can use std::lock_guard
	void lock() { Lock(); }
	void unlock() { Unlock(); }
};

#if !defined(TARGET_IPHONE)
#define DATA_PATH "/data/"
#else
#define DATA_PATH "/"
#endif

//Set the path !
void set_user_config_dir(const string& dir);
void set_user_data_dir(const string& dir);
void add_system_config_dir(const string& dir);
void add_system_data_dir(const string& dir);

//subpath format: /data/fsca-table.bit
string get_writable_config_path(const string& filename);
string get_writable_data_path(const string& filename);
string get_readonly_config_path(const string& filename);
string get_readonly_data_path(const string& filename);
bool file_exists(const string& filename);
bool make_directory(const string& path);

string get_game_save_prefix();
string get_game_basename();
string get_game_dir();

bool mem_region_lock(void *start, size_t len);
bool mem_region_unlock(void *start, size_t len);
bool mem_region_set_exec(void *start, size_t len);
void *mem_region_reserve(void *start, size_t len);
bool mem_region_release(void *start, size_t len);
void *mem_region_map_file(void *file_handle, void *dest, size_t len, size_t offset, bool readwrite);
bool mem_region_unmap_file(void *start, size_t len);

class VArray2 {
public:
	u8* data;
	unsigned size;

	void Zero() {
		memset(data, 0, size);
	}

	INLINE u8& operator [](unsigned i) {
#ifdef MEM_BOUND_CHECK
        if (i >= size)
		{
        	ERROR_LOG(COMMON, "Error: VArray2 , index out of range (%d > %d)\n", i, size - 1);
			MEM_DO_BREAK;
		}
#endif
		return data[i];
    }
};

int msgboxf(const char* text,unsigned int type,...);

#define MBX_OK                       0
#define MBX_ICONEXCLAMATION          0
#define MBX_ICONERROR                0
