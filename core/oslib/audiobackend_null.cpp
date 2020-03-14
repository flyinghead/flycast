#include <chrono>
#include <thread>
#include "audiostream.h"

using the_clock = std::chrono::high_resolution_clock;

static the_clock::time_point last_time;

static void null_init()
{
	last_time = the_clock::time_point();
}

static void null_term()
{
}

static u32 null_push(const void* frame, u32 samples, bool wait)
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

static audiobackend_t audiobackend_null = {
    "null", // Slug
    "No Audio", // Name
    &null_init,
    &null_push,
    &null_term,
	nullptr
};

static bool null = RegisterAudioBackend(&audiobackend_null);
