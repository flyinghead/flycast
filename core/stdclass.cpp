#include <string.h>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include "types.h"
#include "cfg/cfg.h"
#include "stdclass.h"


#if COMPILER_VC_OR_CLANG_WIN32
	#include <io.h>
	#include <direct.h>
	#define access _access
	#define R_OK   4
#else
	#include <unistd.h>
#endif

#include "hw/mem/_vmem.h"

string user_config_dir;
string user_data_dir;
std::vector<string> system_config_dirs;
std::vector<string> system_data_dirs;

bool file_exists(const string& filename)
{
	return (access(filename.c_str(), R_OK) == 0);
}

void set_user_config_dir(const string& dir)
{
	user_config_dir = dir;
}

void set_user_data_dir(const string& dir)
{
	user_data_dir = dir;
}

void add_system_config_dir(const string& dir)
{
	system_config_dirs.push_back(dir);
}

void add_system_data_dir(const string& dir)
{
	system_data_dirs.push_back(dir);
}

string get_writable_config_path(const string& filename)
{
	/* Only stuff in the user_config_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_config_dir + filename);
}

string get_readonly_config_path(const string& filename)
{
	string user_filepath = get_writable_config_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	string filepath;
	for (unsigned int i = 0; i < system_config_dirs.size(); i++) {
		filepath = system_config_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

string get_writable_data_path(const string& filename)
{
	/* Only stuff in the user_data_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_data_dir + filename);
}

string get_readonly_data_path(const string& filename)
{
	string user_filepath = get_writable_data_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	string filepath;
	for (unsigned int i = 0; i < system_data_dirs.size(); i++) {
		filepath = system_data_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

string get_game_save_prefix()
{
	string save_file = cfgLoadStr("config", "image", "");
	size_t lastindex = save_file.find_last_of("/");
#ifdef _WIN32
	size_t lastindex2 = save_file.find_last_of("\\");
	lastindex = max(lastindex, lastindex2);
#endif
	if (lastindex != -1)
		save_file = save_file.substr(lastindex + 1);
	return get_writable_data_path("/data/") + save_file;
}

string get_game_basename()
{
	string game_dir = cfgLoadStr("config", "image", "");
	size_t lastindex = game_dir.find_last_of(".");
	if (lastindex != -1)
		game_dir = game_dir.substr(0, lastindex);
	return game_dir;
}

string get_game_dir()
{
	string game_dir = cfgLoadStr("config", "image", "");
	size_t lastindex = game_dir.find_last_of("/");
#ifdef _WIN32
	size_t lastindex2 = game_dir.find_last_of("\\");
	lastindex = max(lastindex, lastindex2);
#endif
	if (lastindex != -1)
		game_dir = game_dir.substr(0, lastindex + 1);
	return game_dir;
}

bool make_directory(const string& path)
{
#if COMPILER_VC_OR_CLANG_WIN32
#define mkdir _mkdir
#endif

#ifdef _WIN32
	return mkdir(path.c_str()) == 0;
#else
	return mkdir(path.c_str(), 0755) == 0;
#endif
}

// Thread & related platform dependant code
#if !defined(HOST_NO_THREADS)

#if HOST_OS==OS_WINDOWS
void cThread::Start() {
	verify(hThread == NULL);
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)entry, param, 0, NULL);
	ResumeThread(hThread);
}
void cThread::WaitToEnd() {
	WaitForSingleObject(hThread,INFINITE);
	CloseHandle(hThread);
	hThread = NULL;
}
#else
void cThread::Start() {
	verify(hThread == NULL);
	hThread = new pthread_t;
	if (pthread_create( hThread, NULL, entry, param))
		die("Thread creation failed");
}
void cThread::WaitToEnd() {
	if (hThread) {
		pthread_join(*hThread,0);
		delete hThread;
		hThread = NULL;
	}
}
#endif

#endif


#if HOST_OS==OS_WINDOWS
cResetEvent::cResetEvent() {
		hEvent = CreateEvent(
		NULL,             // default security attributes
		FALSE,            // auto-reset event?
		FALSE,            // initial state is State
		NULL			  // unnamed object
		);
}
cResetEvent::~cResetEvent()
{
	//Destroy the event object ?
	 CloseHandle(hEvent);
}
void cResetEvent::Set()//Signal
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	SetEvent(hEvent);
}
void cResetEvent::Reset()//reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	ResetEvent(hEvent);
}
bool cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	return WaitForSingleObject(hEvent,msec) == WAIT_OBJECT_0;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	#if defined(DEBUG_THREADS)
		Sleep(rand() % 10);
	#endif
	WaitForSingleObject(hEvent,(u32)-1);
}
#else
cResetEvent::cResetEvent() {
	pthread_mutex_init(&mutx, NULL);
	pthread_cond_init(&cond, NULL);
}
cResetEvent::~cResetEvent() {
}
void cResetEvent::Set()//Signal
{
	pthread_mutex_lock( &mutx );
	state=true;
    pthread_cond_signal( &cond);
	pthread_mutex_unlock( &mutx );
}
void cResetEvent::Reset()//reset
{
	pthread_mutex_lock( &mutx );
	state=false;
	pthread_mutex_unlock( &mutx );
}
bool cResetEvent::Wait(u32 msec)//Wait for signal , then reset
{
	pthread_mutex_lock( &mutx );
	if (!state)
	{
		struct timespec ts;
#if HOST_OS == OS_DARWIN
		// OSX doesn't have clock_gettime.
		clock_serv_t cclock;
		mach_timespec_t mts;

		host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
		clock_get_time(cclock, &mts);
		mach_port_deallocate(mach_task_self(), cclock);
		ts.tv_sec = mts.tv_sec;
		ts.tv_nsec = mts.tv_nsec;
#else
		clock_gettime(CLOCK_REALTIME, &ts);
#endif
		ts.tv_sec += msec / 1000;
		ts.tv_nsec += (msec % 1000) * 1000000;
		while (ts.tv_nsec > 1000000000)
		{
			ts.tv_nsec -= 1000000000;
			ts.tv_sec++;
		}
		pthread_cond_timedwait( &cond, &mutx, &ts );
	}
	bool rc = state;
	state=false;
	pthread_mutex_unlock( &mutx );

	return rc;
}
void cResetEvent::Wait()//Wait for signal , then reset
{
	pthread_mutex_lock( &mutx );
	if (!state)
	{
		pthread_cond_wait( &cond, &mutx );
	}
	state=false;
	pthread_mutex_unlock( &mutx );
}
#endif


