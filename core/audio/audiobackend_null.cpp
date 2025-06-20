#include "audiostream.h"

#include <chrono>
#include <thread>

class NullAudioBackend : public AudioBackend
{
	using the_clock = std::chrono::high_resolution_clock;

public:
	NullAudioBackend()
		: AudioBackend("null", "No Audio") {}

	bool init() override
	{
		last_time = the_clock::time_point();
		return true;
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		if (wait && last_time.time_since_epoch() != the_clock::duration::zero())
		{
			auto fduration = std::chrono::nanoseconds(1'000'000'000LL * samples / 44100);
			auto duration = fduration - (the_clock::now() - last_time);
			if (duration > std::chrono::nanoseconds::zero())
				std::this_thread::sleep_for(duration);
			if (duration < -std::chrono::milliseconds(67))
				// if ~4 frames ahead, reset time (fast forward detection)
				last_time = the_clock::now();
			else
				last_time += fduration;
		}
		else
		{
			last_time = the_clock::now();
		}
		return 1;
	}

	bool initRecord(u32 sampling_freq) override
	{
		return true;
	}

	u32 record(void* buffer, u32 samples) override
	{
		memset(buffer, 0, samples * 2);
		return samples;
	}

private:
	the_clock::time_point last_time;
};
static NullAudioBackend nullBackend;
