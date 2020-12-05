#include "audiostream.h"
#ifdef USE_OSS
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>

static int oss_audio_fd = -1;
static int oss_rec_fd = -1;

static void oss_init()
{
	oss_audio_fd = open("/dev/dsp", O_WRONLY);
	if (oss_audio_fd < 0)
	{
		WARN_LOG(AUDIO, "Couldn't open /dev/dsp.");
	}
	else
	{
		INFO_LOG(AUDIO, "sound enabled, dsp opened for write");
		int tmp=44100;
		int err_ret;
		err_ret=ioctl(oss_audio_fd,SNDCTL_DSP_SPEED,&tmp);
		INFO_LOG(AUDIO, "set Frequency to %i, return %i (rate=%i)", 44100, err_ret, tmp);
		int channels=2;
		err_ret=ioctl(oss_audio_fd, SNDCTL_DSP_CHANNELS, &channels);
		INFO_LOG(AUDIO, "set dsp to stereo (%i => %i)", channels, err_ret);
		int format=AFMT_S16_LE;
		err_ret=ioctl(oss_audio_fd, SNDCTL_DSP_SETFMT, &format);
		INFO_LOG(AUDIO, "set dsp to %s audio (%i/%i => %i)", "16bits signed", AFMT_S16_LE, format, err_ret);
	}
}

static u32 oss_push(const void* frame, u32 samples, bool wait)
{
	write(oss_audio_fd, frame, samples*4);
	return 1;
}

static void oss_term()
{
	if (oss_audio_fd >= 0)
		close(oss_audio_fd);
	oss_audio_fd = -1;
}

// recording untested

static bool oss_init_record(u32 sampling_freq)
{
	int oss_rec_fd = open("/dev/dsp", O_RDONLY);
	if (oss_rec_fd < 0)
	{
		INFO_LOG(AUDIO, "OSS: can't open default audio capture device");
		return false;
	}
	int tmp = AFMT_S16_NE;	// Native 16 bits
	if (ioctl(oss_rec_fd, SNDCTL_DSP_SETFMT, &tmp) == -1 || tmp != AFMT_S16_NE)
	{
		INFO_LOG(AUDIO, "OSS: can't set sample format");
		close(oss_rec_fd);
		oss_rec_fd = -1;
		return false;
	}
	tmp = 1;
	if (ioctl(oss_rec_fd, SNDCTL_DSP_CHANNELS, &tmp) == -1)
	{
		INFO_LOG(AUDIO, "OSS: can't set channel count");
		close(oss_rec_fd);
		oss_rec_fd = -1;
		return false;
	}
	tmp = sampling_freq;
	if (ioctl(oss_rec_fd, SNDCTL_DSP_SPEED, &tmp) == -1)
	{
		INFO_LOG(AUDIO, "OSS: can't set sample rate");
		close(oss_rec_fd);
		oss_rec_fd = -1;
		return false;
	}

	return true;
}

static void oss_term_record()
{
	if (oss_rec_fd >= 0)
		close(oss_rec_fd);
	oss_rec_fd = -1;
}

static u32 oss_record(void *buffer, u32 samples)
{
	samples *= 2;
	int l = read(oss_rec_fd, buffer, samples);
	if (l < (int)samples)
	{
		if (l < 0)
		{
			INFO_LOG(AUDIO, "OSS: Recording error");
			l = 0;
		}
		memset((u8 *)buffer + l, 0, samples - l);
	}
	return l / 2;
}

static audiobackend_t audiobackend_oss = {
		"oss", // Slug
		"Open Sound System", // Name
		&oss_init,
		&oss_push,
		&oss_term,
		NULL,
		&oss_init_record,
		&oss_record,
		&oss_term_record
};

static bool oss = RegisterAudioBackend(&audiobackend_oss);
#endif
