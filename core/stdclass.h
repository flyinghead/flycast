#pragma once
#include "types.h"
#include "md5/md5.h"

#include <condition_variable>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <thread>

#ifdef __ANDROID__
#include <sys/mman.h>
#undef PAGE_MASK
#elif defined(__APPLE__) && defined(__aarch64__)
#define PAGE_SIZE 16384
#else
#define PAGE_SIZE 4096
#endif
#define PAGE_MASK (PAGE_SIZE-1)

class cThread
{
private:
	typedef void* ThreadEntryFP(void* param);
	ThreadEntryFP* entry;
	void* param;

public:
	std::thread thread;

	cThread(ThreadEntryFP* function, void* param)
		:entry(function), param(param) {}
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
// returns the full path of the game, without the file extension
std::string get_game_basename();
// returns the game directory
std::string get_game_dir();
// returns the position of the last path separator, or string::npos if none
size_t get_last_slash_pos(const std::string& path);

bool mem_region_lock(void *start, std::size_t len);
bool mem_region_unlock(void *start, std::size_t len);
bool mem_region_set_exec(void *start, std::size_t len);

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

static inline void string_tolower(std::string& s)
{
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
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

static inline std::string trim_trailing_ws(const std::string& str,
                 const std::string& whitespace = " ")
{
    const auto strEnd = str.find_last_not_of(whitespace);
	if (strEnd == std::string::npos)
		return "";

    return str.substr(0, strEnd + 1);
}

static inline std::string trim_ws(const std::string& str,
                 const std::string& whitespace = " ")
{
    const auto strStart = str.find_first_not_of(whitespace);
	if (strStart == std::string::npos)
		return "";

    return str.substr(strStart, str.find_last_not_of(whitespace) + 1 - strStart);
}

class MD5Sum
{
	MD5_CTX ctx;

public:
	MD5Sum() {
		MD5_Init(&ctx);
	}

	MD5Sum& add(const void *data, size_t len) {
		MD5_Update(&ctx, data, len);
		return *this;
	}

	MD5Sum& add(std::FILE *file) {
		std::fseek(file, 0, SEEK_SET);
		char buf[4096];
		size_t len = 0;
		while ((len = std::fread(buf, 1, sizeof(buf), file)) > 0)
			MD5_Update(&ctx, buf, len);
		return *this;
	}

	template<typename T>
	MD5Sum& add(const T& v) {
		MD5_Update(&ctx, &v, sizeof(T));
		return *this;
	}

	template<typename T>
	MD5Sum& add(const std::vector<T>& v) {
		MD5_Update(&ctx, &v[0], v.size() * sizeof(T));
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
