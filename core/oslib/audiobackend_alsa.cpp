#if USE_ALSA
#include "audiostream.h"
#include <alsa/asoundlib.h>
#include "cfg/cfg.h"
#include "cfg/option.h"

class AlsaAudioBackend : public AudioBackend
{
	snd_pcm_t *handle = nullptr;
	bool pcm_blocking = true;
	snd_pcm_uframes_t buffer_size = 0;
	snd_pcm_uframes_t period_size = 0;
	snd_pcm_t *handle_record = nullptr;

public:
	AlsaAudioBackend()
		: AudioBackend("alsa", "Advanced Linux Sound Architecture") {}

	bool init() override
	{
		snd_pcm_hw_params_t *params;

		std::string device = cfgLoadStr("alsa", "device", "");

		int rc = -1;
		if (!device.empty() && device != "auto") {
			rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
			if (rc < 0)
				WARN_LOG(AUDIO, "ALSA: Cannot open device %s. Trying auto", device.c_str());
		}
		if (rc < 0)
		{
			INFO_LOG(AUDIO, "ALSA: trying to determine audio device");

			// trying default device
			device = "default";
			rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);

			// "default" didn't work, try first device
			if (rc < 0)
			{
				device = "plughw:0,0,0";
				rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);

				if (rc < 0)
				{
					device = "plughw:0,0";
					rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
				}
			}

			// first didn't work, try second
			if (rc < 0)
			{
				device = "plughw:1,0";
				rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
			}

			// try pulse audio backend
			if (rc < 0)
			{
				device = "pulse";
				rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
			}

			if (rc < 0)
				INFO_LOG(AUDIO, "ALSA: unable to automatically determine audio device.");
		}

		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: unable to open PCM device %s: %s", device.c_str(), snd_strerror(rc));
			return false;
		}

		INFO_LOG(AUDIO, "ALSA: Successfully initialized \"%s\"", device.c_str());

		/* Allocate a hardware parameters object. */
		snd_pcm_hw_params_alloca(&params);

		/* Fill it in with default values. */
		rc=snd_pcm_hw_params_any(handle, params);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_any %s", snd_strerror(rc));
			term();
			return false;
		}

		/* Set the desired hardware parameters. */

		/* Interleaved mode */
		rc=snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_access %s", snd_strerror(rc));
			term();
			return false;
		}

		/* Signed 16-bit little-endian format */
		rc=snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_format %s", snd_strerror(rc));
			term();
			return false;
		}

		/* Two channels (stereo) */
		rc=snd_pcm_hw_params_set_channels(handle, params, 2);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_channels %s", snd_strerror(rc));
			term();
			return false;
		}

		// 44100 samples/second
		rc = snd_pcm_hw_params_set_rate(handle, params, 44100, 0);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_rate %s", snd_strerror(rc));
			term();
			return false;
		}

		// Period size (512)
		period_size = std::min(SAMPLE_COUNT, (u32)config::AudioBufferSize / 4);
		rc = snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, nullptr);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_periods_near %s", snd_strerror(rc));
			term();
			return false;
		}
		INFO_LOG(AUDIO, "ALSA: period size set to %zd", (size_t)period_size);

		// Sample buffer size
		buffer_size = config::AudioBufferSize;
		rc = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_buffer_size_near %s", snd_strerror(rc));
			term();
			return false;
		}
		INFO_LOG(AUDIO, "ALSA: buffer size set to %ld", buffer_size);

		/* Write the parameters to the driver */
		rc = snd_pcm_hw_params(handle, params);
		if (rc < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Unable to set hw parameters: %s", snd_strerror(rc));
			term();
			return false;
		}

		return true;
	}

	bool initRecord(u32 sampling_freq) override
	{
		int err;
		if ((err = snd_pcm_open(&handle_record, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot open default audio capture device: %s", snd_strerror(err));
			return false;
		}
		snd_pcm_hw_params_t *hw_params;
		if ((err = snd_pcm_hw_params_malloc(&hw_params)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot allocate hardware parameter structure: %s", snd_strerror(err));
			snd_pcm_close(handle_record);
			return false;
		}
		if ((err = snd_pcm_hw_params_any(handle_record, hw_params)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot initialize hardware parameter structure: %s", snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			snd_pcm_close(handle_record);
			return false;
		}
		if ((err = snd_pcm_hw_params_set_access(handle_record, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot set access type: %s\n", snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			snd_pcm_close(handle_record);
			return false;
		}
		if ((err = snd_pcm_hw_params_set_format(handle_record, hw_params, SND_PCM_FORMAT_S16_LE)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot set sample format: %s", snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			snd_pcm_close(handle_record);
			return false;
		}
		if ((err = snd_pcm_hw_params_set_rate(handle_record, hw_params, sampling_freq, 0)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot set sample rate to %d Hz: %s", sampling_freq, snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			snd_pcm_close(handle_record);
			return false;
		}
		if ((err = snd_pcm_hw_params_set_channels(handle_record, hw_params, 1)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot set channel count: %s", snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			snd_pcm_close(handle_record);
			return false;
		}
		if ((err = snd_pcm_hw_params(handle_record, hw_params)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot set parameters: %s", snd_strerror(err));
			snd_pcm_hw_params_free(hw_params);
			snd_pcm_close(handle_record);
			return false;
		}
		snd_pcm_hw_params_free(hw_params);
		snd_pcm_nonblock(handle_record, 1);
		if ((err = snd_pcm_prepare(handle_record)) < 0)
		{
			ERROR_LOG(AUDIO, "ALSA: Cannot prepare device: %s", snd_strerror(err));
			snd_pcm_close(handle_record);
			return false;
		}
		INFO_LOG(AUDIO, "ALSA: Successfully initialized capture device");

		return true;
	}

	void termRecord() override
	{
		snd_pcm_close(handle_record);
	}

	u32 record(void* frame, u32 samples) override
	{
		int err = snd_pcm_readi(handle_record, frame, samples);
		if (err < (int)samples)
		{
			if (err < 0)
			{
				DEBUG_LOG(AUDIO, "ALSA: Recording error: %s", snd_strerror(err));
				err = 0;
				err = snd_pcm_prepare(handle_record);
			}
			u8 *buffer = (u8 *)frame + err;
			memset(buffer, 0, (samples - err) * 2);
		}

		return err;
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		if (wait != pcm_blocking) {
			snd_pcm_nonblock(handle, wait ? 0 : 1);
			pcm_blocking = wait;
		}

		int rc = snd_pcm_writei(handle, frame, samples);
		if (rc < 0)
		{
			snd_pcm_recover(handle, rc, 1);
			if (rc == -EPIPE)
			{
				// EPIPE means underrun
				// Write some silence then our samples
				const size_t silence_size = buffer_size - samples;
				void *silence = alloca(silence_size * 4);
				memset(silence, 0, silence_size * 4);
				snd_pcm_writei(handle, silence, silence_size);
				snd_pcm_writei(handle, frame, samples);
			}
		}
		return 1;
	}

	void term() override
	{
		snd_pcm_drop(handle);
		snd_pcm_close(handle);
	}

	std::vector<std::string> getDeviceList()
	{
		std::vector<std::string> result;

		char **hints;
		int err = snd_device_name_hint(-1, "pcm", (void***)&hints);

		// Error initializing ALSA
		if (err != 0)
			return result;

		// special value to automatically detect on initialization
		result.emplace_back("auto");

		char** n = hints;
		while (*n != NULL)
		{
			// Get the type (NULL/Input/Output)
			char *type = snd_device_name_get_hint(*n, "IOID");
			char *name = snd_device_name_get_hint(*n, "NAME");

			if (name != NULL)
			{
				// We only want output or special devices (like "default" or "pulse")
				// TODO Only those with type == NULL?
				if (type == NULL || strcmp(type, "Output") == 0)
				{
					// TODO Check if device works (however we need to hash the resulting list then)
					/*snd_pcm_t *handle;
					int rc = snd_pcm_open(&handle, name, SND_PCM_STREAM_PLAYBACK, 0);

					if (rc == 0)
					{
						result.push_back(name);
						snd_pcm_close(handle);
					}
					*/

					result.emplace_back(name);
				}

			}

			if (type != NULL)
				free(type);

			if (name != NULL)
				free(name);

			n++;
		}

		snd_device_name_free_hint((void**)hints);

		return result;
	}

	Option* getOptions(int *count) override
	{
		*count = 1;
		static Option result;

		result.name = "device";
		result.caption = "Device";
		result.type = Option::list;
		result.values = getDeviceList();

		return &result;
	}
};
static AlsaAudioBackend alsaBackend;

#endif
