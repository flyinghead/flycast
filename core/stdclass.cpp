#include "types.h"
#include "stdclass.h"
#include "oslib/directory.h"
#include "oslib/oslib.h"
#include "serialize.h"
#include "oslib/storage.h"

#include <chrono>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#ifdef _WIN32
	#include <algorithm>
#endif

static std::string user_config_dir;
static std::string user_data_dir;
static std::vector<std::string> system_config_dirs;
static std::vector<std::string> system_data_dirs;

bool file_exists(const std::string& filename)
{
	return (flycast::access(filename.c_str(), R_OK) == 0);
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
	return user_config_dir + filename;
}

std::string get_readonly_config_path(const std::string& filename)
{
	std::string user_filepath = get_writable_config_path(filename);
	if (file_exists(user_filepath))
		return user_filepath;

	for (const auto& config_dir : system_config_dirs)
	{
		std::string filepath = config_dir + filename;
		if (file_exists(filepath))
			return filepath;
	}

	// Not found, so we return the user variant
	return user_filepath;
}

std::string get_writable_data_path(const std::string& filename)
{
	/* Only stuff in the user_data_dir is supposed to be writable,
	 * so we always return that.
	 */
	return user_data_dir + filename;
}

std::string get_readonly_data_path(const std::string& filename)
{
	std::string user_filepath = get_writable_data_path(filename);
	if (file_exists(user_filepath))
		return user_filepath;

	for (const auto& data_dir : system_data_dirs)
	{
		std::string filepath = data_dir + filename;
		if (file_exists(filepath))
			return filepath;
	}
	// Try the game directory
	std::string parent = hostfs::storage().getParentPath(settings.content.path);
	try {
		std::string filepath = hostfs::storage().getSubPath(parent, filename);
		hostfs::FileInfo info = hostfs::storage().getFileInfo(filepath);
		return info.path;
	} catch (const FlycastException&) { }

	// Not found, so we return the user variant
	return user_filepath;
}

size_t get_last_slash_pos(const std::string& path)
{
	size_t lastindex = path.find_last_of('/');
#ifdef _WIN32
	size_t lastindex2 = path.find_last_of('\\');
	if (lastindex == std::string::npos)
		lastindex = lastindex2;
	else if (lastindex2 != std::string::npos)
		lastindex = std::max(lastindex, lastindex2);
#endif
	return lastindex;
}

std::string get_game_save_prefix()
{
	return get_writable_data_path(settings.content.fileName);
}

bool make_directory(const std::string& path)
{
	return flycast::mkdir(path.c_str(), 0755) == 0;
}

void cThread::Start()
{
	verify(!thread.joinable());
	thread = std::thread(entry, param);
}

void cThread::WaitToEnd()
{
	if (thread.joinable() && thread.get_id() != std::this_thread::get_id())
		thread.join();
}

cResetEvent::cResetEvent() : state(false)
{

}

cResetEvent::~cResetEvent() = default;

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
	std::unique_lock<std::mutex> lock(mutx);

	if (!state)
		cond.wait_for(lock, std::chrono::milliseconds(msec));

	bool rc = state;
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

void RamRegion::serialize(Serializer &ser) const {
	ser.serialize(data, size);
}

void RamRegion::deserialize(Deserializer &deser) {
	deser.deserialize(data, size);
}


void RamRegion::alloc(size_t size)
{
	this->size = size;
	data = (u8 *)allocAligned(PAGE_SIZE, size);
	ownsMemory = true;
}

void RamRegion::free()
{
	this->size = 0;
	if (ownsMemory)
		freeAligned(data);
	data = nullptr;
}
