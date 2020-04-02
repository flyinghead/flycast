#pragma once
#include "types.h"

#include <condition_variable>
#include <mutex>

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

class cResetEvent
{
private:
	std::mutex mutx;
	std::condition_variable cond;
	bool state;

public :
	cResetEvent();
	~cResetEvent();
	void Set();		//Set state to signaled
	void Reset();	//Set state to non signaled
	bool Wait(u32 msec);//Wait for signal , then reset[if auto]. Returns false if timed out
	void Wait();	//Wait for signal , then reset[if auto]
};

#if !defined(TARGET_IPHONE)
#define DATA_PATH "/data/"
#else
#define DATA_PATH "/"
#endif

//Set the path !
void set_user_config_dir(const std::string& dir);
void set_user_data_dir(const std::string& dir);
void add_system_config_dir(const std::string& dir);
void add_system_data_dir(const std::string& dir);

//subpath format: /data/fsca-table.bit
std::string get_writable_config_path(const std::string& filename);
std::string get_writable_data_path(const std::string& filename);
std::string get_readonly_config_path(const std::string& filename);
std::string get_readonly_data_path(const std::string& filename);
bool file_exists(const std::string& filename);
bool make_directory(const std::string& path);

std::string get_game_save_prefix();
std::string get_game_basename();
std::string get_game_dir();

bool mem_region_lock(void *start, std::size_t len);
bool mem_region_unlock(void *start, std::size_t len);
bool mem_region_set_exec(void *start, std::size_t len);
void *mem_region_reserve(void *start, std::size_t len);
bool mem_region_release(void *start, std::size_t len);
void *mem_region_map_file(void *file_handle, void *dest, std::size_t len, std::size_t offset, bool readwrite);
bool mem_region_unmap_file(void *start, std::size_t len);

class VArray2 {
public:
	u8* data;
	unsigned size;

	void Zero() {
		std::memset(data, 0, size);
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
