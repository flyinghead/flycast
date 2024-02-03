#if defined(USE_SDL_AUDIO)

#include <SDL.h>
#include "audiostream.h"
#include "cfg/option.h"
#include "stdclass.h"

#include <algorithm>
#include <atomic>
#include <mutex>

class SDLAudioBackend : AudioBackend
{
	SDL_AudioDeviceID audiodev {};
	bool needs_resampling = false;
	cResetEvent read_wait;
	std::mutex stream_mutex;
	uint32_t *sample_buffer;
	unsigned sample_buffer_size = 0;
	unsigned sample_count = 0;
	SDL_AudioCVT audioCvt;

	SDL_AudioDeviceID recorddev {};
	u8 recordbuf[480 * 4];
	std::atomic<size_t> rec_read;
	std::atomic<size_t> rec_write;

	static void audioCallback(void* userdata, Uint8* stream, int len)
	{
		SDLAudioBackend *backend = (SDLAudioBackend *)userdata;

		backend->stream_mutex.lock();
		// Wait until there's enough samples to feed the kraken
		unsigned oslen = len / sizeof(uint32_t);
		unsigned islen = backend->needs_resampling ? std::ceil(oslen / backend->audioCvt.len_ratio) : oslen;

		if (backend->sample_count < islen)
		{
			// No data, just output a bit of silence for the underrun
			memset(stream, 0, len);
			backend->stream_mutex.unlock();
			backend->read_wait.Set();
			return;
		}

		if (!backend->needs_resampling) {
			// Just copy bytes for this case.
			memcpy(stream, &backend->sample_buffer[0], len);
		}
		else
		{
			SDL_AudioCVT& cvt = backend->audioCvt;
			cvt.len = islen * sizeof(uint32_t);
			memcpy(cvt.buf, &backend->sample_buffer[0], cvt.len);
			SDL_ConvertAudio(&cvt);
			memcpy(stream, cvt.buf, cvt.len_cvt);
		}

		// Move samples in the buffer and consume them
		memmove(&backend->sample_buffer[0], &backend->sample_buffer[islen], (backend->sample_count - islen) * sizeof(uint32_t));
		backend->sample_count -= islen;

		backend->stream_mutex.unlock();
		backend->read_wait.Set();
	}

public:
	SDLAudioBackend()
		: AudioBackend("sdl2", "Simple DirectMedia Layer 2 Audio") {}

	bool init() override
	{
		if (!SDL_WasInit(SDL_INIT_AUDIO))
		{
			if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
				ERROR_LOG(AUDIO, "SDL2 error initializing audio subsystem: %s", SDL_GetError());
				return false;
			}
		}
	
		sample_buffer_size = std::max<u32>(SAMPLE_COUNT * 2, config::AudioBufferSize);
		sample_buffer = new uint32_t[sample_buffer_size]();
		sample_count = 0;

		// Support 44.1KHz (native) but also upsampling to 48KHz
		SDL_AudioSpec wav_spec, out_spec;
		memset(&wav_spec, 0, sizeof(wav_spec));
		wav_spec.freq = 44100;
		wav_spec.format = AUDIO_S16;
		wav_spec.channels = 2;
		wav_spec.samples = SAMPLE_COUNT * 2;  // Must be power of two
		wav_spec.callback = audioCallback;
		wav_spec.userdata = this;
		needs_resampling = false;

		// Try 44.1KHz which should be faster since it's native.
		audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
		if (audiodev == 0)
		{
			INFO_LOG(AUDIO, "SDL2: SDL_OpenAudioDevice failed: %s", SDL_GetError());
			needs_resampling = true;
			wav_spec.freq = 48000;
			audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
			if (audiodev == 0)
				ERROR_LOG(AUDIO, "SDL2: SDL_OpenAudioDevice failed: %s", SDL_GetError());
			else
			{
				INFO_LOG(AUDIO, "SDL2: Using resampling to 48 KHz");
				int ret = SDL_BuildAudioCVT(&audioCvt, AUDIO_S16, 2, 44100, AUDIO_S16, 2, 48000);
				if (ret != 1 || audioCvt.needed == 0)
				{
					ERROR_LOG(AUDIO, "SDL2: can't build audio converter: %s", SDL_GetError());
					SDL_CloseAudioDevice(audiodev);
					audiodev = 0;
				}
				else
				{
					audioCvt.buf = new u8[SAMPLE_COUNT * 2 * sizeof(uint32_t) * audioCvt.len_mult];
				}
			}
		}

		return audiodev != 0;
	}

	u32 push(const void* frame, u32 samples, bool wait) override
	{
		// Unpause the device shall it be paused.
		if (SDL_GetAudioDeviceStatus(audiodev) != SDL_AUDIO_PLAYING)
			SDL_PauseAudioDevice(audiodev, 0);

		// If wait, then wait for the buffer to be smaller than a certain size.
		stream_mutex.lock();
		if (wait) {
			while (sample_count + samples > sample_buffer_size) {
				stream_mutex.unlock();
				read_wait.Wait();
				stream_mutex.lock();
			}
		}

		// Copy as many samples as possible, drop any remaining (this should not happen usually)
		unsigned free_samples = sample_buffer_size - sample_count;
		unsigned tocopy = samples < free_samples ? samples : free_samples;
		memcpy(&sample_buffer[sample_count], frame, tocopy * sizeof(uint32_t));
		sample_count += tocopy;
		stream_mutex.unlock();

		return 1;
	}

	void term() override
	{
		if (audiodev)
		{
			// Stop audio playback.
			SDL_PauseAudioDevice(audiodev, 1);
			read_wait.Set();
			SDL_CloseAudioDevice(audiodev);
			audiodev = SDL_AudioDeviceID();
		}
		delete [] sample_buffer;
		sample_buffer = nullptr;
		if (needs_resampling)
		{
			delete [] audioCvt.buf;
			audioCvt.buf = nullptr;
		}
	}

	static void recordCallback(void *userdata, u8 *stream, int len)
	{
		SDLAudioBackend *backend = (SDLAudioBackend *)userdata;
		DEBUG_LOG(AUDIO, "SDL2: sdl2_record_cb len %d write %zd read %zd", len, (size_t)backend->rec_write, (size_t)backend->rec_read);
		while (len > 0)
		{
			size_t plen = std::min((size_t)len, sizeof(backend->recordbuf) - backend->rec_write);
			memcpy(&backend->recordbuf[backend->rec_write], stream, plen);
			len -= plen;
			backend->rec_write = (backend->rec_write + plen) % sizeof(backend->recordbuf);
			stream += plen;
		}
	}

	bool initRecord(u32 sampling_freq) override
	{
		rec_write = 0;
		rec_read = 0;

		SDL_AudioSpec wav_spec, out_spec;
		memset(&wav_spec, 0, sizeof(wav_spec));
		wav_spec.freq = sampling_freq;
		wav_spec.format = AUDIO_S16;
		wav_spec.channels = 1;
		wav_spec.samples = 256;  // Must be power of two
		wav_spec.callback = recordCallback;
		wav_spec.userdata = this;
		recorddev = SDL_OpenAudioDevice(NULL, 1, &wav_spec, &out_spec, 0);
		if (recorddev == 0)
		{
			ERROR_LOG(AUDIO, "SDL2: Cannot open audio capture device: %s", SDL_GetError());
			return false;
		}
		SDL_PauseAudioDevice(recorddev, 0);
		INFO_LOG(AUDIO, "SDL2: opened audio capture device");

		return true;
	}

	void termRecord() override
	{
		if (recorddev != 0)
		{
			SDL_PauseAudioDevice(recorddev, 1);
			SDL_CloseAudioDevice(recorddev);
			recorddev = 0;
		}
	}

	u32 record(void* frame, u32 samples) override
	{
		u32 count = 0;
		samples *= 2;
		while (samples > 0)
		{
			u32 avail = std::min(rec_write - rec_read, sizeof(recordbuf) - rec_read);
			if (avail == 0)
				break;
			avail = std::min(avail, samples);
			memcpy((u8 *)frame + count, &recordbuf[rec_read], avail);
			rec_read = (rec_read + avail) % sizeof(recordbuf);
			samples -= avail;
			count += avail;
		}
		DEBUG_LOG(AUDIO, "SDL2: sdl2_record len %d ret %d write %zd read %zd", samples * 2, count, (size_t)rec_write, (size_t)rec_read);

		return count / 2;
	}
};
static SDLAudioBackend sdlAudioBackend;

#endif

