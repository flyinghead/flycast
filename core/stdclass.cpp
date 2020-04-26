#include "types.h"
#include "stdclass.h"

#include <chrono>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32
	#include <algorithm>
	#include <io.h>
	#include <direct.h>
	#define access _access
	#ifndef R_OK
		#define R_OK   4
	#endif
	#define mkdir(dir, mode) _mkdir(dir)
#else
	#include <unistd.h>
#endif

std::string user_config_dir;
std::string user_data_dir;
std::vector<std::string> system_config_dirs;
std::vector<std::string> system_data_dirs;

bool file_exists(const std::string& filename)
{
	return (access(filename.c_str(), R_OK) == 0);
}

void set_user_config_dir(const std::string& dir)
{
	user_config_dir = dir;
}

void set_user_data_dir(const std::string& dir)
{
	user_data_dir = dir;
}

void add_system_config_dir(const std::string& dir)
{
	system_config_dirs.push_back(dir);
}

void add_system_data_dir(const std::string& dir)
{
	system_data_dirs.push_back(dir);
}

std::string get_writable_config_path(const std::string& filename)
{
	/* Only stuff in the user_config_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_config_dir + filename);
}

std::string get_readonly_config_path(const std::string& filename)
{
	std::string user_filepath = get_writable_config_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	std::string filepath;
	for (size_t i = 0; i < system_config_dirs.size(); i++) {
		filepath = system_config_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

std::string get_writable_data_path(const std::string& filename)
{
	/* Only stuff in the user_data_dir is supposed to be writable,
	 * so we always return that.
	 */
	return (user_data_dir + filename);
}

std::string get_readonly_data_path(const std::string& filename)
{
	std::string user_filepath = get_writable_data_path(filename);
	if(file_exists(user_filepath))
	{
		return user_filepath;
	}

	std::string filepath;
	for (size_t i = 0; i < system_data_dirs.size(); i++) {
		filepath = system_data_dirs[i] + filename;
		if (file_exists(filepath))
		{
			return filepath;
		}
	}

	// Not found, so we return the user variant
	return user_filepath;
}

std::string get_game_save_prefix()
{
	std::string save_file = settings.imgread.ImagePath;
	size_t lastindex = save_file.find_last_of('/');
#ifdef _WIN32
	size_t lastindex2 = save_file.find_last_of('\\');
	lastindex = std::max(lastindex, lastindex2);
#endif
	if (lastindex != std::string::npos)
		save_file = save_file.substr(lastindex + 1);
	return get_writable_data_path(DATA_PATH) + save_file;
}

std::string get_game_basename()
{
	std::string game_dir = settings.imgread.ImagePath;
	size_t lastindex = game_dir.find_last_of('.');
	if (lastindex != std::string::npos)
		game_dir = game_dir.substr(0, lastindex);
	return game_dir;
}

std::string get_game_dir()
{
	std::string game_dir = settings.imgread.ImagePath;
	size_t lastindex = game_dir.find_last_of('/');
#ifdef _WIN32
	size_t lastindex2 = game_dir.find_last_of('\\');
	lastindex = std::max(lastindex, lastindex2);
#endif
	if (lastindex != std::string::npos)
		game_dir = game_dir.substr(0, lastindex + 1);
	return game_dir;
}

bool make_directory(const std::string& path)
{
	return mkdir(path.c_str(), 0755) == 0;
}

void cThread::Start()
{
	verify(!thread.joinable());
	thread = std::thread(entry, param);
}

void cThread::WaitToEnd()
{
	if (thread.joinable()) {
		thread.join();
	}
}

cResetEvent::cResetEvent() : state(false)
{

}

cResetEvent::~cResetEvent()
{

}

void cResetEvent::Set()//Signal
{
    std::lock_guard<std::mutex> lock(mutx);

    state = true;
    cond.notify_one();
}

void cResetEvent::Reset()
{
    std::lock_guard<std::mutex> lock(mutx);

    state = false;
}

bool cResetEvent::Wait(u32 msec)
{
    bool rc = true;

    std::unique_lock<std::mutex> lock(mutx);

    if (!state) {
        rc = (cond.wait_for(lock, std::chrono::milliseconds(msec)) == std::cv_status::no_timeout);
    }

    state = false;

    return rc;
}

void cResetEvent::Wait()
{
    std::unique_lock<std::mutex> lock(mutx);

    if (!state) {
        cond.wait(lock);
    }

    state = false;
}
