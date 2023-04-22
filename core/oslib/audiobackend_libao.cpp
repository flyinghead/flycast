#ifdef USE_LIBAO
#include "audiostream.h"
#include <ao/ao.h>

class LibAOBackend : public AudioBackend
{
	ao_device *aodevice = nullptr;

public:
	LibAOBackend()
		: AudioBackend("libao", "libao") {}

	bool init() override
	{
		ao_initialize();

		ao_sample_format aoformat {};
		aoformat.bits = 16;
		aoformat.channels = 2;
		aoformat.rate = 44100;
		aoformat.byte_format = AO_FMT_LITTLE;

		aodevice = ao_open_live(ao_default_driver_id(), &aoformat, NULL); // Live output
		if (aodevice == nullptr)
			aodevice = ao_open_live(ao_driver_id("null"), &aoformat, NULL);
		if (aodevice == nullptr) {
			ERROR_LOG(AUDIO, "Cannot open libao driver");
			ao_shutdown();
		}

		return aodevice != nullptr;
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		if (aodevice != nullptr)
			ao_play(aodevice, (char*)frame, samples * 4);

		return 1;
	}

	void term() override
	{
		if (aodevice != nullptr)
		{
			ao_close(aodevice);
			aodevice = nullptr;
			ao_shutdown();
		}
	}
};
static LibAOBackend libAOBackend;

#endif
