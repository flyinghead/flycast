#pragma once
#include "types.h"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <vector>

class AudioBackend
{
public:
	virtual ~AudioBackend() = default;

	virtual bool init() = 0;
	virtual u32 push(const void *data, u32 frames, bool wait) = 0;
	virtual void term() {}

	struct Option {
		std::string name;
		std::string caption;
		enum { integer, checkbox, list } type;

		int minValue;
		int maxValue;
		std::vector<std::string> values;
	};
	virtual const Option *getOptions(int *count) {
		*count = 0;
		return nullptr;
	}

	virtual bool initRecord(u32 sampling_freq) { return false; }
	virtual u32 record(void *, u32) { return 0; }
	virtual void termRecord() {}

	std::string slug;
	std::string name;

	static size_t getCount() { return backends == nullptr ? 0 : backends->size(); }
	static AudioBackend *getBackend(size_t index) { return backends == nullptr ? nullptr : (*backends)[index]; }
	static AudioBackend *getBackend(const std::string& slug);

protected:
	AudioBackend(const std::string& slug, const std::string& name)
		: slug(slug), name(name) {
		registerAudioBackend(this);
	}

private:
	static void registerAudioBackend(AudioBackend *backend)
	{
		if (backends == nullptr)
			backends = new std::vector<AudioBackend *>();
		backends->push_back(backend);
		std::sort(backends->begin(), backends->end(), [](AudioBackend *b1, AudioBackend *b2) { return b1->slug < b2->slug; });
	}

	static std::vector<AudioBackend *> *backends;
};

void InitAudio();
void TermAudio();
void WriteSample(s16 right, s16 left);

void StartAudioRecording(bool eight_khz);
u32 RecordAudio(void *buffer, u32 samples);
void StopAudioRecording();

constexpr u32 SAMPLE_COUNT = 512;	// AudioBackend::push() is always called with that many frames

class RingBuffer
{
	std::vector<u8> buffer;
	std::atomic_int readCursor { 0 };
	std::atomic_int writeCursor { 0 };

	u32 readSize() {
		return (u32)((writeCursor - readCursor + buffer.size()) % buffer.size());
	}
	u32 writeSize() {
		return (u32)((readCursor - writeCursor + buffer.size() - 1) % buffer.size());
	}

public:
	bool write(const u8 *data, u32 size)
	{
		if (size > writeSize())
			return false;
		u32 wc = writeCursor;
		u32 chunkSize = std::min<u32>(size, (u32)buffer.size() - wc);
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
		u32 chunkSize = std::min<u32>(size, (u32)buffer.size() - rc);
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
