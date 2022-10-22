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

	void term() override
	{
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		if (wait)
		{
			if (last_time.time_since_epoch() != the_clock::duration::zero())
			{
				auto fduration = std::chrono::nanoseconds(1000000000L * samples / 44100);
				auto duration = fduration - (the_clock::now() - last_time);
				std::this_thread::sleep_for(duration);
				last_time += fduration;
			}
			else
				last_time = the_clock::now();
		}
		return 1;
	}

	bool initRecord(u32 sampling_freq) override
	{
		return true;
	}

	u32 record(void *buffer, u32 samples) override
	{
		memset(buffer, 0, samples * 2);
		return samples;
	}

private:
	the_clock::time_point last_time;
};

static NullAudioBackend nullBackend;

/*
static audiobackend_t audiobackend_null = {
    "null", // Slug
    "No Audio", // Name
    &null_init,
    &null_push,
    &null_term,
	nullptr,
	&null_init_record,
	&null_record,
	&null_term
};

static bool null = RegisterAudioBackend(&audiobackend_null);
*/
