#include "oslib/audiostream.h"
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
	if (device == "")
	{
		printf("ALSA: trying to determine audio device\n");
		/* Open PCM device for playback. */
		device = "default";
		rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);

		if (rc < 0)
		{
			device = "plughw:0,0,0";
			rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
		}

		if (rc < 0)
		{
			device = "plughw:0,0";
			rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
		}

		if (rc >= 0)
		{
			// init successfull, write value back to config
			cfgSaveStr("alsa", "device", device.c_str());
		}
	}
	else {
		rc = snd_pcm_open(&handle, device.c_str(), SND_PCM_STREAM_PLAYBACK, 0);
	}

	if (rc < 0)
	{
		fprintf(stderr, "unable to open PCM device %s: %s\n", device.c_str(), snd_strerror(rc));
		return;
	}

	printf("ALSA: Successfully initialized \"%s\"\n", device.c_str());

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	rc=snd_pcm_hw_params_any(handle, params);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_any %s\n", snd_strerror(rc));
		return;
	}

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	rc=snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_access %s\n", snd_strerror(rc));
		return;
	}

	/* Signed 16-bit little-endian format */
	rc=snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_format %s\n", snd_strerror(rc));
		return;
	}

	/* Two channels (stereo) */
	rc=snd_pcm_hw_params_set_channels(handle, params, 2);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_channels %s\n", snd_strerror(rc));
		return;
	}

	/* 44100 bits/second sampling rate (CD quality) */
	val = 44100;
	rc=snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_rate_near %s\n", snd_strerror(rc));
		return;
	}

	/* Set period size to settings.aica.BufferSize frames. */
	period_size = settings.aica.BufferSize;
	rc=snd_pcm_hw_params_set_period_size_near(handle, params, &period_size, &dir);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_buffer_size_near %s\n", snd_strerror(rc));
		return;
	}
	else
		printf("ALSA: period size set to %ld\n", period_size);
	buffer_size = (44100 * 100 /* settings.omx.Audio_Latency */ / 1000 / period_size + 1) * period_size;
	rc=snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
	if (rc < 0)
	{
		fprintf(stderr, "Error:snd_pcm_hw_params_set_buffer_size_near %s\n", snd_strerror(rc));
		return;
	}
	else
		printf("ALSA: buffer size set to %ld\n", buffer_size);

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0)
	{
		fprintf(stderr, "Unable to set hw parameters: %s\n", snd_strerror(rc));
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
		fprintf(stderr, "ALSA: underrun occurred\n");
		snd_pcm_prepare(handle);
		// Write some silence then our samples
		const size_t silence_size = period_size * 4;
		void *silence = alloca(silence_size * 4);
		memset(silence, 0, silence_size * 4);
		rc = snd_pcm_writei(handle, silence, silence_size);
		if (rc < 0)
			fprintf(stderr, "ALSA: error from writei(silence): %s\n", snd_strerror(rc));
		else if (rc < silence_size)
			fprintf(stderr, "ALSA: short write from writei(silence): %d/%ld frames\n", rc, silence_size);
		rc = snd_pcm_writei(handle, frame, samples);
		if (rc < 0)
			fprintf(stderr, "ALSA: error from writei(again): %s\n", snd_strerror(rc));
		else if (rc < samples)
			fprintf(stderr, "ALSA: short write from writei(again): %d/%d frames\n", rc, samples);
	}
	else if (rc < 0)
	{
		fprintf(stderr, "ALSA: error from writei: %s\n", snd_strerror(rc));
	}
	else if (rc != samples)
	{
		fprintf(stderr, "ALSA: short write, wrote %d frames of %d\n", rc, samples);
	}
	return 1;
}

static void alsa_term()
{
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

static audiobackend_t audiobackend_alsa = {
    "alsa", // Slug
    "Advanced Linux Sound Architecture", // Name
    &alsa_init,
    &alsa_push,
    &alsa_term
};

static bool alsa = RegisterAudioBackend(&audiobackend_alsa);
#endif
