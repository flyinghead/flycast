#pragma once
#include "types.h"
#include "md5/md5.h"

#include <algorithm>
#include <cctype>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <cassert>
#include <time.h>

#if defined(__ANDROID__)
#undef PAGE_MASK
#endif
#if defined(__ANDROID__) && (HOST_CPU == CPU_ARM64 || HOST_CPU == CPU_X64)
#undef PAGE_SIZE
extern const unsigned long PAGE_SIZE;
#define MAX_PAGE_SIZE 16384
#elif defined(__APPLE__) && defined(__aarch64__)
#define PAGE_SIZE 16384
#elif !defined(PAGE_SIZE)
#define PAGE_SIZE 4096
#endif
#ifndef PAGE_MASK
#define PAGE_MASK (PAGE_SIZE-1)
#endif
#ifndef MAX_PAGE_SIZE
#define MAX_PAGE_SIZE PAGE_SIZE
#endif

class cThread
{
private:
	typedef void* ThreadEntryFP(void* param);
	ThreadEntryFP* entry;
	void* param;
	const char *name;

public:
	std::thread thread;

	cThread(ThreadEntryFP* function, void* param, const char *name)
		:entry(function), param(param), name(name) {}
	~cThread() { WaitToEnd(); }
	void Start();
	void WaitToEnd();
};

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

void set_user_config_dir(const std::string& dir);
void set_user_data_dir(const std::string& dir);
void add_system_config_dir(const std::string& dir);
void add_system_data_dir(const std::string& dir);

std::string get_writable_config_path(const std::string& filename);
std::string get_writable_data_path(const std::string& filename);
std::string get_readonly_config_path(const std::string& filename);
std::string get_readonly_data_path(const std::string& filename);
bool file_exists(const std::string& filename);
bool make_directory(const std::string& path);

// returns a prefix for a game save file, for example: ~/.local/share/flycast/mvsc2.zip
std::string get_game_save_prefix();
// returns the position of the last path separator, or string::npos if none
size_t get_last_slash_pos(const std::string& path);

class RamRegion
{
	u8 *data;
	size_t size;
	bool ownsMemory = false;

public:
	void alloc(size_t size);
	void free();

	void setRegion(u8 *data, size_t size)
	{
		this->data = data;
		this->size = size;
		ownsMemory = false;
	}

	void zero() {
		std::memset(data, 0, size);
	}

	u8& operator [](size_t i) {
		return data[i];
    }

	void serialize(Serializer &ser) const;
	void deserialize(Deserializer &deser);
};

static inline void string_tolower(std::string& s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
}

static inline void string_toupper(std::string& s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::toupper(c); });
}

static inline std::string get_file_extension(const std::string& s)
{
	size_t dot = s.find_last_of('.');
	if (dot == std::string::npos)
		return "";
	std::string ext = s.substr(dot + 1, s.length() - dot - 1);
	string_tolower(ext);
	return ext;
}

static inline std::string get_file_basename(const std::string& s)
{
	size_t dot = s.find_last_of('.');
	if (dot == std::string::npos)
		return s;
	return s.substr(0, dot);
}

extern const std::string defaultWs;

static inline std::string trim_trailing_ws(const std::string& str,
                 const std::string& whitespace = defaultWs)
{
    const auto strEnd = str.find_last_not_of(whitespace);
	if (strEnd == std::string::npos)
		return "";

    return str.substr(0, strEnd + 1);
}

static inline std::string trim_ws(const std::string& str,
                 const std::string& whitespace = defaultWs)
{
    const auto strStart = str.find_first_not_of(whitespace);
	if (strStart == std::string::npos)
		return "";

    return str.substr(strStart, str.find_last_not_of(whitespace) + 1 - strStart);
}

static inline bool isAbsolutePath(const std::string& path)
{
#ifdef _WIN32
	if (path.length() >= 3 && std::isalpha(static_cast<u8>(path[0]))
			&& path[1] == ':' && (path[2] == '/' || path[2] == '\\'))
		return true;
	if (!path.empty() && (path[0] == '/' || path[0] == '\\'))
		return true;
	return false;
#else
	return !path.empty() && path[0] == '/';
#endif
}

template<typename ... Args>
std::string strprintf(const char *format, Args ... args)
{
	int size = std::snprintf(nullptr, 0, format, args...);
	std::string out(size + 1, '\0');
	std::snprintf(out.data(), size + 1, format, args...);
	out.resize(size);
	return out;
}

class MD5Sum
{
	MD5_CTX ctx;

public:
	MD5Sum() {
		MD5_Init(&ctx);
	}

	MD5Sum& add(const void *data, unsigned long len) {
		MD5_Update(&ctx, data, len);
		return *this;
	}

	MD5Sum& add(std::FILE *file) {
		std::fseek(file, 0, SEEK_SET);
		char buf[4096];
		unsigned long len = 0;
		while ((len = (unsigned long)std::fread(buf, 1, sizeof(buf), file)) > 0)
			MD5_Update(&ctx, buf, len);
		return *this;
	}

	template<typename T>
	MD5Sum& add(const T& v) {
		MD5_Update(&ctx, &v, (unsigned long)sizeof(T));
		return *this;
	}

	template<typename T>
	MD5Sum& add(const std::vector<T>& v) {
		MD5_Update(&ctx, v.data(), (unsigned long)(v.size() * sizeof(T)));
		return *this;
	}

	void getDigest(u8 digest[16]) {
		MD5_Final(digest, &ctx);
	}

	std::vector<u8> getDigest() {
		std::vector<u8> v(16);
		MD5_Final(v.data(), &ctx);
		return v;
	}
};

u64 getTimeMs();
std::string timeToISO8601(time_t time);
std::string timeToShortDateTimeString(time_t time);

class ThreadRunner
{
public:
	void init() {
		threadId = std::this_thread::get_id();
	}
	void term() {
		threadId = {};
	}
	void runOnThread(std::function<void()> func)
	{
		if (threadId == std::thread::id{}
			|| threadId == std::this_thread::get_id()) {
			func();
		}
		else {
			LockGuard _(mutex);
			tasks.push_back(func);
		}
	}
	void execTasks()
	{
		assert(threadId == std::this_thread::get_id());
		std::vector<std::function<void()>> localTasks;
		{
			LockGuard _(mutex);
			std::swap(localTasks, tasks);
		}
		for (auto& func : localTasks)
			func();
	}

private:
	using LockGuard = std::lock_guard<std::mutex>;

	std::thread::id threadId;
	std::vector<std::function<void()>> tasks;
	std::mutex mutex;
};
