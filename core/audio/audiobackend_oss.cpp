#ifdef USE_OSS
#include "audiostream.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include <unistd.h>

class OSSAudioBackend : public AudioBackend
{
	int audioFD = -1;
	int recordFD = -1;

public:
	OSSAudioBackend()
		: AudioBackend("oss", "Open Sound System") {}

	bool init() override
	{
		audioFD = open("/dev/dsp", O_WRONLY);
		if (audioFD < 0)
		{
			WARN_LOG(AUDIO, "Couldn't open /dev/dsp.");
			return false;
		}
		INFO_LOG(AUDIO, "sound enabled, dsp opened for write");
		int tmp=44100;
		int err_ret;
		err_ret=ioctl(audioFD,SNDCTL_DSP_SPEED,&tmp);
		INFO_LOG(AUDIO, "set Frequency to %i, return %i (rate=%i)", 44100, err_ret, tmp);
		int channels=2;
		err_ret=ioctl(audioFD, SNDCTL_DSP_CHANNELS, &channels);
		INFO_LOG(AUDIO, "set dsp to stereo (%i => %i)", channels, err_ret);
		int format=AFMT_S16_LE;
		err_ret=ioctl(audioFD, SNDCTL_DSP_SETFMT, &format);
		INFO_LOG(AUDIO, "set dsp to %s audio (%i/%i => %i)", "16bits signed", AFMT_S16_LE, format, err_ret);

		return true;
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		write(audioFD, frame, samples * 4);
		return 1;
	}

	void term() override
	{
		if (audioFD >= 0)
			close(audioFD);
		audioFD = -1;
	}

	// recording untested

	bool initRecord(u32 sampling_freq) override
	{
		recordFD = open("/dev/dsp", O_RDONLY);
		if (recordFD < 0)
		{
			INFO_LOG(AUDIO, "OSS: can't open default audio capture device");
			return false;
		}
		int tmp = AFMT_S16_NE;	// Native 16 bits
		if (ioctl(recordFD, SNDCTL_DSP_SETFMT, &tmp) == -1 || tmp != AFMT_S16_NE)
		{
			INFO_LOG(AUDIO, "OSS: can't set sample format");
			close(recordFD);
			recordFD = -1;
			return false;
		}
		tmp = 1;
		if (ioctl(recordFD, SNDCTL_DSP_CHANNELS, &tmp) == -1)
		{
			INFO_LOG(AUDIO, "OSS: can't set channel count");
			close(recordFD);
			recordFD = -1;
			return false;
		}
		tmp = sampling_freq;
		if (ioctl(recordFD, SNDCTL_DSP_SPEED, &tmp) == -1)
		{
			INFO_LOG(AUDIO, "OSS: can't set sample rate");
			close(recordFD);
			recordFD = -1;
			return false;
		}

		return true;
	}

	void termRecord() override
	{
		if (recordFD >= 0)
			close(recordFD);
		recordFD = -1;
	}

	u32 record(void *buffer, u32 samples) override
	{
		samples *= 2;
		int l = read(recordFD, buffer, samples);
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
};
static OSSAudioBackend ossAudioBackend;

#endif
