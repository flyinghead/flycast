#if defined(USE_SDL_AUDIO)

#include <SDL.h>
#include "audiostream.h"
#include "stdclass.h"

#include <mutex>
#include <atomic>

class SDLAudioBackend : AudioBackend
{
	SDL_AudioDeviceID audiodev {};
	bool needs_resampling = false;
	cResetEvent read_wait;
	std::mutex stream_mutex;
	struct {
		uint32_t prevs;
		uint32_t *sample_buffer;
	} audiobuf;
	unsigned sample_count = 0;

	SDL_AudioDeviceID recorddev {};
	u8 recordbuf[480 * 4];
	std::atomic<size_t> rec_read;
	std::atomic<size_t> rec_write;

	// To easily access samples.
	union Sample { int16_t s[2]; uint32_t l; };

	static float InterpolateCatmull4pt3oX(float x0, float x1, float x2, float x3, float t) {
		return 0.45 * ((2 * x1) + t * ((-x0 + x2) + t * ((2 * x0 - 5 * x1 + 4 * x2 - x3) + t * (-x0 + 3 * x1 - 3 * x2 + x3))));
	}

	static void audioCallback(void* userdata, Uint8* stream, int len)
	{
		SDLAudioBackend *backend = (SDLAudioBackend *)userdata;

		backend->stream_mutex.lock();
		// Wait until there's enough samples to feed the kraken
		unsigned oslen = len / sizeof(uint32_t);
		unsigned islen = backend->needs_resampling ? oslen * 16 / 17 : oslen;
		unsigned minlen = backend->needs_resampling ? islen + 2 : islen;  // Resampler looks ahead by 2 samples.

		if (backend->sample_count < minlen) {
			// No data, just output a bit of silence for the underrun
			memset(stream, 0, len);
			backend->stream_mutex.unlock();
			backend->read_wait.Set();
			return;
		}

		if (!backend->needs_resampling) {
			// Just copy bytes for this case.
			memcpy(stream, &backend->audiobuf.sample_buffer[0], len);
		}
		else {
			// 44.1KHz to 48KHz (actually 46.86KHz) resampling
			uint32_t *outbuf = (uint32_t*)stream;
			const float ra = 1.0f / 17;
			Sample *sbuf = (Sample*)&backend->audiobuf.sample_buffer[0];  // [-1] stores the previous iteration last sample output
			for (u32 i = 0; i < islen/16; i++) {
				*outbuf++ = sbuf[i*16+ 0].l;   // First sample stays at the same location.
				for (int k = 1; k < 17; k++) {
					Sample r;
					// Note we access offset -1 on first iteration, as to access prevs
					r.s[0] = InterpolateCatmull4pt3oX(sbuf[i*16+k-2].s[0], sbuf[i*16+k-1].s[0], sbuf[i*16+k].s[0], sbuf[i*16+k+1].s[0], 1 - ra*k);
					r.s[1] = InterpolateCatmull4pt3oX(sbuf[i*16+k-2].s[1], sbuf[i*16+k-1].s[1], sbuf[i*16+k].s[1], sbuf[i*16+k+1].s[1], 1 - ra*k);
					*outbuf++ = r.l;
				}
			}
			backend->audiobuf.prevs = backend->audiobuf.sample_buffer[islen-1];
		}

		// Move samples in the buffer and consume them
		memmove(&backend->audiobuf.sample_buffer[0], &backend->audiobuf.sample_buffer[islen], (backend->sample_count-islen)*sizeof(uint32_t));
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
	
		audiobuf.sample_buffer = new uint32_t[config::AudioBufferSize]();

		// Support 44.1KHz (native) but also upsampling to 48KHz
		SDL_AudioSpec wav_spec, out_spec;
		memset(&wav_spec, 0, sizeof(wav_spec));
		wav_spec.freq = 44100;
		wav_spec.format = AUDIO_S16;
		wav_spec.channels = 2;
		wav_spec.samples = SAMPLE_COUNT * 2;  // Must be power of two
		wav_spec.callback = audioCallback;
		wav_spec.userdata = this;

		// Try 44.1KHz which should be faster since it's native.
		audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
		if (audiodev == 0)
		{
			WARN_LOG(AUDIO, "SDL2: SDL_OpenAudioDevice failed: %s", SDL_GetError());
			needs_resampling = true;
			wav_spec.freq = 48000;
			audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
			if (audiodev == 0)
				ERROR_LOG(AUDIO, "SDL2: SDL_OpenAudioDevice failed: %s", SDL_GetError());
			else
				INFO_LOG(AUDIO, "SDL2: Using resampling to 48 KHz");
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
			while (sample_count + samples > (u32)config::AudioBufferSize) {
				stream_mutex.unlock();
				read_wait.Wait();
				stream_mutex.lock();
			}
		}

		// Copy as many samples as possible, drop any remaining (this should not happen usually)
		unsigned free_samples = config::AudioBufferSize - sample_count;
		unsigned tocopy = samples < free_samples ? samples : free_samples;
		memcpy(&audiobuf.sample_buffer[sample_count], frame, tocopy * sizeof(uint32_t));
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
		delete [] audiobuf.sample_buffer;
		audiobuf.sample_buffer = nullptr;
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

