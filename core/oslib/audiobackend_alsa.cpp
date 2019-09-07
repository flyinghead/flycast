#include "audiostream.h"
#if USE_ALSA
#include <alsa/asoundlib.h>
#include "cfg/cfg.h"

static snd_pcm_t *handle;
static bool pcm_blocking = true;
static snd_pcm_uframes_t buffer_size;
static snd_pcm_uframes_t period_size;

// We're making these functions static - there's no need to pollute the global namespace
static void alsa_init()
{
	snd_pcm_hw_params_t *params;
	unsigned int val;
	int dir=-1;

	string device = cfgLoadStr("alsa", "device", "");

	int rc = -1;
	if (device.empty() || device == "auto")
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
	else {
		rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
	}

	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: unable to open PCM device %s: %s", device.c_str(), snd_strerror(rc));
		return;
	}

	INFO_LOG(AUDIO, "ALSA: Successfully initialized \"%s\"", device.c_str());

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	rc=snd_pcm_hw_params_any(handle, params);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_any %s", snd_strerror(rc));
		return;
	}

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	rc=snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_access %s", snd_strerror(rc));
		return;
	}

	/* Signed 16-bit little-endian format */
	rc=snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_format %s", snd_strerror(rc));
		return;
	}

	/* Two channels (stereo) */
	rc=snd_pcm_hw_params_set_channels(handle, params, 2);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_channels %s", snd_strerror(rc));
		return;
	}

	/* 44100 bits/second sampling rate (CD quality) */
	val = 44100;
	rc=snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_rate_near %s", snd_strerror(rc));
		return;
	}

	/* Set period size to settings.aica.BufferSize frames. */
	period_size = settings.aica.BufferSize;
	rc=snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, &dir);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_buffer_size_near %s", snd_strerror(rc));
		return;
	}
	else
	{
		INFO_LOG(AUDIO, "ALSA: period size set to %ld", period_size);
	}

	buffer_size = (44100 * 100 /* settings.omx.Audio_Latency */ / 1000 / period_size + 1) * period_size;
	rc=snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Error:snd_pcm_hw_params_set_buffer_size_near %s", snd_strerror(rc));
		return;
	}
	else
	{
		INFO_LOG(AUDIO, "ALSA: buffer size set to %ld", buffer_size);
	}

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0)
	{
		WARN_LOG(AUDIO, "ALSA: Unable to set hw parameters: %s", snd_strerror(rc));
		return;
	}
}

static u32 alsa_push(void* frame, u32 samples, bool wait)
{
	if (wait != pcm_blocking) {
		snd_pcm_nonblock(handle, wait ? 0 : 1);
		pcm_blocking = wait;
	}

	int rc = snd_pcm_writei(handle, frame, samples);
	if (rc == -EPIPE)
	{
		/* EPIPE means underrun */
		snd_pcm_prepare(handle);
		// Write some silence then our samples
		const size_t silence_size = period_size * 4;
		void *silence = alloca(silence_size * 4);
		memset(silence, 0, silence_size * 4);
		snd_pcm_writei(handle, silence, silence_size);
		snd_pcm_writei(handle, frame, samples);
	}
	return 1;
}

static void alsa_term()
{
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

std::vector<std::string> alsa_get_devicelist()
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

static audio_option_t* alsa_audio_options(int* option_count)
{
	*option_count = 1;
	static audio_option_t result[1];

	result[0].cfg_name = "device";
	result[0].caption = "Device";
	result[0].type = list;
	result[0].list_callback = alsa_get_devicelist;

	return result;
}

static audiobackend_t audiobackend_alsa = {
    "alsa", // Slug
    "Advanced Linux Sound Architecture", // Name
    &alsa_init,
    &alsa_push,
    &alsa_term,
	&alsa_audio_options
};

static bool alsa = RegisterAudioBackend(&audiobackend_alsa);
#endif
