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

	static int openDevice(int flags)
	{
		const char* path = "/dev/dsp";
		int fd = open(path, flags);

		if (fd < 0)
			ERROR_LOG(AUDIO, "OSS: open(%s) failed: %s", path, strerror(errno));

		return fd;
	}

	static bool setRate(int fd, int rate)
	{
		int tmp = rate;

		if (ioctl(fd, SNDCTL_DSP_SPEED, &tmp) < 0)
		{
			ERROR_LOG(AUDIO, "OSS: ioctl(SNDCTL_DSP_SPEED) failed: %s", strerror(errno));
			return false;
		}

		if (tmp != rate)
		{
			ERROR_LOG(AUDIO, "OSS: sample rate unsupported: %d => %d", rate, tmp);
			return false;
		}

		return true;
	}

	static bool setChannels(int fd, int channels)
	{
		int tmp = channels;

		if (ioctl(fd, SNDCTL_DSP_CHANNELS, &tmp) < 0)
		{
			ERROR_LOG(AUDIO, "OSS: ioctl(SNDCTL_DSP_CHANNELS) failed: %s", strerror(errno));
			return false;
		}

		if (tmp != channels)
		{
			ERROR_LOG(AUDIO, "OSS: channels unsupported: %d => %d", channels, tmp);
			return false;
		}

		return true;
	}

	static bool setFormat(int fd, int format)
	{
		int tmp = format;

		if (ioctl(fd, SNDCTL_DSP_SETFMT, &tmp) < 0)
		{
			ERROR_LOG(AUDIO, "OSS: ioctl(SNDCTL_DSP_SETFMT) failed: %s", strerror(errno));
			return false;
		}

		if (tmp != format)
		{
			ERROR_LOG(AUDIO, "OSS: sample format unsupported: %#.8x => %#.8x", format, tmp);
			return false;
		}

		return true;
	}

public:
	OSSAudioBackend()
		: AudioBackend("oss", "Open Sound System") {}

	bool init() override
	{
		audioFD = openDevice(O_WRONLY);

		if (audioFD < 0 || !setRate(audioFD, 44100) || !setChannels(audioFD, 2) || !setFormat(audioFD, AFMT_S16_LE))
		{
			term();
			return false;
		}

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
		recordFD = openDevice(O_RDONLY);

		if (recordFD < 0 || !setRate(recordFD, sampling_freq) || !setChannels(recordFD, 1) || !setFormat(recordFD, AFMT_S16_NE /* Native 16 bits */ ))
		{
			termRecord();
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
