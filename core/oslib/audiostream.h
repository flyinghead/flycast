#pragma once
#include "types.h"
#include "cfg/option.h"
#include <vector>
#include <algorithm>
#include <atomic>

typedef std::vector<std::string> (*audio_option_callback_t)();
enum audio_option_type
{
	integer = 0
,	checkbox = 1
,	list = 2
};

typedef struct {
	std::string cfg_name;
	std::string caption;
	audio_option_type type;

	// type int_value (spin edit)
	int min_value;
	int max_value;

	// type list edit (string/char*)
	audio_option_callback_t list_callback;
} audio_option_t;

typedef audio_option_t* (*audio_options_func_t)(int* option_count);

typedef void (*audio_backend_init_func_t)();
typedef u32 (*audio_backend_push_func_t)(const void *data, u32 frames, bool wait);
typedef void (*audio_backend_term_func_t)();
typedef struct {
    std::string slug;
    std::string name;
    audio_backend_init_func_t init;
    audio_backend_push_func_t push;
    audio_backend_term_func_t term;
	audio_options_func_t get_options;
    bool (*init_record)(u32 sampling_freq);
    u32 (*record)(void *, u32);
    audio_backend_term_func_t term_record;
} audiobackend_t;
bool RegisterAudioBackend(audiobackend_t* backend);
void InitAudio();
void TermAudio();
void WriteSample(s16 right, s16 left);

void StartAudioRecording(bool eight_khz);
u32 RecordAudio(void *buffer, u32 samples);
void StopAudioRecording();

u32 GetAudioBackendCount();
audiobackend_t* GetAudioBackend(int num);
audiobackend_t* GetAudioBackend(const std::string& slug);

constexpr u32 SAMPLE_COUNT = 512;	// push() is always called with that many frames

class RingBuffer
{
	std::vector<u8> buffer;
	std::atomic_int readCursor { 0 };
	std::atomic_int writeCursor { 0 };

	u32 readSize() {
		return (writeCursor - readCursor + buffer.size()) % buffer.size();
	}
	u32 writeSize() {
		return (readCursor - writeCursor + buffer.size() - 1) % buffer.size();
	}

public:
	bool write(const u8 *data, u32 size)
	{
		if (size > writeSize())
			return false;
		u32 wc = writeCursor;
		u32 chunkSize = std::min<u32>(size, buffer.size() - wc);
		memcpy(&buffer[wc], data, chunkSize);
		wc = (wc + chunkSize) % buffer.size();
		size -= chunkSize;
		if (size > 0)
		{
			data += chunkSize;
			memcpy(&buffer[wc], data, size);
			wc = (wc + size) % buffer.size();
		}
		writeCursor = wc;
		return true;
	}

	bool read(u8 *data, u32 size)
	{
		if (size > readSize())
			return false;
		u32 rc = readCursor;
		u32 chunkSize = std::min<u32>(size, buffer.size() - rc);
		memcpy(data, &buffer[rc], chunkSize);
		rc = (rc + chunkSize) % buffer.size();
		size -= chunkSize;
		if (size > 0)
		{
			data += chunkSize;
			memcpy(data, &buffer[rc], size);
			rc = (rc + size) % buffer.size();
		}
		readCursor = rc;
		return true;
	}

	void setCapacity(size_t size)
	{
		std::fill(buffer.begin(), buffer.end(), 0);
		buffer.resize(size);
		readCursor = 0;
		writeCursor = 0;
	}
};
