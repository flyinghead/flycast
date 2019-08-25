#pragma once
#include "types.h"
#include <stdlib.h>
#include <vector>
#include <string.h>

#if HOST_OS!=OS_WINDOWS
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

//Commonly used classes across the project
//Simple Array class for helping me out ;P
template<class T>
class Array
{
public:
	T* data;
	u32 Size;

	Array(T* Source,u32 ellements)
	{
		//initialise array
		data=Source;
		Size=ellements;
	}

	Array(u32 ellements)
	{
		//initialise array
		data=0;
		Resize(ellements,false);
		Size=ellements;
	}

	Array(u32 ellements,bool zero)
	{
		//initialise array
		data=0;
		Resize(ellements,zero);
		Size=ellements;
	}

	Array()
	{
		//initialise array
		data=0;
		Size=0;
	}

	~Array()
	{
		if  (data)
		{
			#ifdef MEM_ALLOC_TRACE
			DEBUG_LOG(COMMON, "WARNING : DESTRUCTOR WITH NON FREED ARRAY [arrayid:%d]", id);
			#endif
			Free();
		}
	}

	void SetPtr(T* Source,u32 ellements)
	{
		//initialise array
		Free();
		data=Source;
		Size=ellements;
	}

	T* Resize(u32 size,bool bZero)
	{
		if (size==0)
		{
			if (data)
			{
				#ifdef MEM_ALLOC_TRACE
				DEBUG_LOG(COMMON, "Freeing data -> resize to zero[Array:%d]", id);
				#endif
				Free();
			}

		}
		
		if (!data)
			data=(T*)malloc(size*sizeof(T));
		else
			data=(T*)realloc(data,size*sizeof(T));

		//TODO : Optimise this
		//if we allocated more , Zero it out
		if (bZero)
		{
			if (size>Size)
			{
				for (u32 i=Size;i<size;i++)
				{
					u8*p =(u8*)&data[i];
					for (size_t j=0;j<sizeof(T);j++)
					{
						p[j]=0;
					}
				}
			}
		}
		Size=size;

		return data;
	}

	void Zero()
	{
		memset(data,0,sizeof(T)*Size);
	}

	void Free()
	{
		if (Size != 0)
		{
			if (data)
				free(data);

			data = NULL;
		}
	}


	INLINE T& operator [](const u32 i)
	{
#ifdef MEM_BOUND_CHECK
		if (i>=Size)
		{
			ERROR_LOG(COMMON, "Error: Array %d , index out of range (%d > %d)", id, i, Size - 1);
			MEM_DO_BREAK;
		}
#endif
		return data[i];
	}

	INLINE T& operator [](const s32 i)
	{
#ifdef MEM_BOUND_CHECK
		if (!(i>=0 && i<(s32)Size))
		{
			ERROR_LOG(COMMON, "Error: Array %d , index out of range (%d > %d)", id, i, Size - 1);
			MEM_DO_BREAK;
		}
#endif
		return data[i];
	}
};

//Windoze code
//Threads

#if !defined(HOST_NO_THREADS)
typedef  void* ThreadEntryFP(void* param);

class cThread {
private:
	ThreadEntryFP* entry;
	void* param;
public :
	#if HOST_OS==OS_WINDOWS
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
#if HOST_OS==OS_WINDOWS
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
#if HOST_OS==OS_WINDOWS
	CRITICAL_SECTION cs;
#else
	pthread_mutex_t mutx;
#endif

public :
	bool state;
	cMutex()
	{
#if HOST_OS==OS_WINDOWS
		InitializeCriticalSection(&cs);
#else
		pthread_mutex_init ( &mutx, NULL);
#endif
	}
	~cMutex()
	{
#if HOST_OS==OS_WINDOWS
		DeleteCriticalSection(&cs);
#else
		pthread_mutex_destroy(&mutx);
#endif
	}
	void Lock()
	{
#if HOST_OS==OS_WINDOWS
		EnterCriticalSection(&cs);
#else
		pthread_mutex_lock(&mutx);
#endif
	}
	bool TryLock()
	{
#if HOST_OS==OS_WINDOWS
		return TryEnterCriticalSection(&cs);
#else
		return pthread_mutex_trylock(&mutx)==0;
#endif
	}
	void Unlock()
	{
#if HOST_OS==OS_WINDOWS
		LeaveCriticalSection(&cs);
#else
		pthread_mutex_unlock(&mutx);
#endif
	}
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

// Locked memory class, was used for texture invalidation purposes.
class VLockedMemory {
public:
	u8* data;
	unsigned size;

	void SetRegion(void* ptr, unsigned size) {
		this->data = (u8*)ptr;
		this->size = size;
	}
	void *getPtr() const { return data; }
	unsigned getSize() const { return size; }

	void Zero() {
		memset(data, 0, size);
	}

	INLINE u8& operator [](unsigned i) {
#ifdef MEM_BOUND_CHECK
        if (i >= size)
		{
        	ERROR_LOG(COMMON, "Error: VLockedMemory , index out of range (%d > %d)\n", i, size - 1);
			MEM_DO_BREAK;
		}
#endif
		return data[i];
    }
};

int msgboxf(const wchar* text,unsigned int type,...);

#define MBX_OK                       0
#define MBX_ICONEXCLAMATION          0
#define MBX_ICONERROR                0
