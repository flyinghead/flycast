
#if defined(USE_SDL_AUDIO)

#include <SDL2/SDL.h>
#include "audiostream.h"
#include "stdclass.h"

static SDL_AudioDeviceID audiodev;
static bool needs_resampling;
static cResetEvent read_wait;
static cMutex stream_mutex;
static struct {
	uint32_t prevs;
	uint32_t sample_buffer[2048];
} audiobuf;
static unsigned sample_count = 0;

// To easily access samples.
union Sample { int16_t s[2]; uint32_t l; };

static float InterpolateCatmull4pt3oX(float x0, float x1, float x2, float x3, float t) {
	return 0.45 * ((2 * x1) + t * ((-x0 + x2) + t * ((2 * x0 - 5 * x1 + 4 * x2 - x3) + t * (-x0 + 3 * x1 - 3 * x2 + x3))));
}

static void sdl2_audiocb(void* userdata, Uint8* stream, int len) {
	stream_mutex.Lock();
	// Wait until there's enough samples to feed the kraken
	unsigned oslen = len / sizeof(uint32_t);
	unsigned islen = needs_resampling ? oslen * 16 / 17 : oslen;
	unsigned minlen = needs_resampling ? islen + 2 : islen;  // Resampler looks ahead by 2 samples.

	if (sample_count < minlen) {
		// No data, just output a bit of silence for the underrun
		memset(stream, 0, len);
        stream_mutex.Unlock();
		read_wait.Set();
		return;
	}

	if (!needs_resampling) {
		// Just copy bytes for this case.
		memcpy(stream, &audiobuf.sample_buffer[0], len);
	}
	else {
		// 44.1KHz to 48KHz (actually 46.86KHz) resampling
		uint32_t *outbuf = (uint32_t*)stream;
		const float ra = 1.0f / 17;
		Sample *sbuf = (Sample*)&audiobuf.sample_buffer[0];  // [-1] stores the previous iteration last sample output
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
		audiobuf.prevs = audiobuf.sample_buffer[islen-1];
	}

	// Move samples in the buffer and consume them
	memmove(&audiobuf.sample_buffer[0], &audiobuf.sample_buffer[islen], (sample_count-islen)*sizeof(uint32_t));
	sample_count -= islen;

	stream_mutex.Unlock();
	read_wait.Set();
}

static void sdl2_audio_init() {
	if (!SDL_WasInit(SDL_INIT_AUDIO))
		SDL_InitSubSystem(SDL_INIT_AUDIO);

	// Support 44.1KHz (native) but also upsampling to 48KHz
	SDL_AudioSpec wav_spec, out_spec;
	memset(&wav_spec, 0, sizeof(wav_spec));
	wav_spec.freq = 44100;
	wav_spec.format = AUDIO_S16;
	wav_spec.channels = 2;
	wav_spec.samples = 1024;  // Must be power of two
	wav_spec.callback = sdl2_audiocb;
	
	// Try 44.1KHz which should be faster since it's native.
	audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
	if (!audiodev) {
		needs_resampling = true;
		wav_spec.freq = 48000;
		audiodev = SDL_OpenAudioDevice(NULL, 0, &wav_spec, &out_spec, 0);
		if (!audiodev)
			ERROR_LOG(AUDIO, "SDL2: SDL_OpenAudioDevice failed");
		else
			INFO_LOG(AUDIO, "SDL2: Using resampling to 48 KHz");
	}
}

static u32 sdl2_audio_push(const void* frame, u32 samples, bool wait) {
	// Unpause the device shall it be paused.
	if (SDL_GetAudioDeviceStatus(audiodev) != SDL_AUDIO_PLAYING)
		SDL_PauseAudioDevice(audiodev, 0);

	// If wait, then wait for the buffer to be smaller than a certain size.
	stream_mutex.Lock();
	if (wait) {
		while (sample_count + samples > sizeof(audiobuf.sample_buffer)/sizeof(audiobuf.sample_buffer[0])) {
			stream_mutex.Unlock();
			read_wait.Wait();
			read_wait.Reset();
			stream_mutex.Lock();
		}
	}

	// Copy as many samples as possible, drop any remaining (this should not happen usually)
	unsigned free_samples = sizeof(audiobuf.sample_buffer) / sizeof(audiobuf.sample_buffer[0]) - sample_count;
	unsigned tocopy = samples < free_samples ? samples : free_samples;
	memcpy(&audiobuf.sample_buffer[sample_count], frame, tocopy * sizeof(uint32_t));
	sample_count += tocopy;
	stream_mutex.Unlock();

	return 1;
}

static void sdl2_audio_term() {
	if (audiodev)
	{
		// Stop audio playback.
		SDL_PauseAudioDevice(audiodev, 1);
		read_wait.Set();
		SDL_CloseAudioDevice(audiodev);
		audiodev = SDL_AudioDeviceID();
	}
}

static audiobackend_t audiobackend_sdl2audio = {
		"sdl2", // Slug
		"Simple DirectMedia Layer 2 Audio", // Name
		&sdl2_audio_init,
		&sdl2_audio_push,
		&sdl2_audio_term
};

static bool sdl2audiobe = RegisterAudioBackend(&audiobackend_sdl2audio);

#endif

