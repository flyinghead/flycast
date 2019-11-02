#include <climits>
#include "cfg/cfg.h"
#include "oslib.h"
#include "audiostream.h"

struct SoundFrame { s16 l;s16 r; };
#define SAMPLE_COUNT 512

SoundFrame RingBuffer[SAMPLE_COUNT];
const u32 RingBufferByteSize = sizeof(RingBuffer);
const u32 RingBufferSampleCount = SAMPLE_COUNT;

u32 WritePtr;  //last WRITEN sample
u32 ReadPtr;   //next sample to read

u32 gen_samples=0;

double time_diff = 128/44100.0;
double time_last;

#ifdef LOG_SOUND
// TODO Only works on Windows!
WaveWriter rawout("d:\\aica_out.wav");
#endif

static unsigned int audiobackends_num_max = 1;
static unsigned int audiobackends_num_registered = 0;
static audiobackend_t **audiobackends = NULL;
static audiobackend_t *audiobackend_current = NULL;

u32 GetAudioBackendCount()
{
	return audiobackends_num_registered;
}

audiobackend_t* GetAudioBackend(int num)
{
	return audiobackends[num];
}

bool RegisterAudioBackend(audiobackend_t *backend)
{
	/* This function announces the availability of an audio backend to reicast. */
	// Check if backend is valid
	if (backend == NULL)
	{
		WARN_LOG(AUDIO, "ERROR: Tried to register invalid audio backend (NULL pointer).");
		return false;
	}

	if (backend->slug == "auto" || backend->slug == "none") {
		WARN_LOG(AUDIO, "ERROR: Tried to register invalid audio backend (slug \"%s\" is a reserved keyword).", backend->slug.c_str());
		return false;
	}

	// First call to RegisterAudioBackend(), create the backend structure;
	if (audiobackends == NULL)
		audiobackends = static_cast<audiobackend_t**>(calloc(audiobackends_num_max, sizeof(audiobackend_t*)));

	// Check if we need to allocate addition memory for storing the pointers and allocate if neccessary
	if (audiobackends_num_registered == audiobackends_num_max)
	{
		// Check for integer overflows
		if (audiobackends_num_max == UINT_MAX)
		{
			WARN_LOG(AUDIO, "ERROR: Registering audio backend \"%s\" (%s) failed. Cannot register more than %u backends", backend->slug.c_str(), backend->name.c_str(), audiobackends_num_max);
			return false;
		}
		audiobackends_num_max++;
		audiobackend_t **new_ptr = static_cast<audiobackend_t**>(realloc(audiobackends, audiobackends_num_max*sizeof(audiobackend_t*)));
		// Make sure that allocation worked
		if (new_ptr == NULL)
		{
			WARN_LOG(AUDIO, "ERROR: Registering audio backend \"%s\" (%s) failed. Cannot allocate additional memory.", backend->slug.c_str(), backend->name.c_str());
			return false;
		}
		audiobackends = new_ptr;
	}

	audiobackends[audiobackends_num_registered] = backend;
	audiobackends_num_registered++;
	return true;
}

audiobackend_t* GetAudioBackend(const std::string& slug)
{
	if (slug == "none")
	{
		INFO_LOG(AUDIO, "WARNING: Audio backend set to \"none\"!");
	}
	else if (audiobackends_num_registered > 0)
	{
		if (slug == "auto")
		{
			/* FIXME: At some point, one might want to insert some intelligent
				 algorithm for autoselecting the approriate audio backend here.
				 I'm too lazy right now. */
			INFO_LOG(AUDIO, "Auto-selected audio backend \"%s\" (%s).", audiobackends[0]->slug.c_str(), audiobackends[0]->name.c_str());
			return audiobackends[0];
		}
		else
		{
			for(unsigned int i = 0; i < audiobackends_num_registered; i++)
			{
				if(audiobackends[i]->slug == slug)
				{
						return audiobackends[i];
				}
			}
			WARN_LOG(AUDIO, "WARNING: Audio backend \"%s\" not found!", slug.c_str());
		}
	}
	else
	{
		WARN_LOG(AUDIO, "WARNING: No audio backends available!");
	}
	return NULL;
}

u32 PushAudio(void* frame, u32 amt, bool wait)
{
	if (audiobackend_current != NULL) {
		return audiobackend_current->push(frame, amt, wait);
	}
	return 0;
}

u32 asRingUsedCount()
{
	if (WritePtr>ReadPtr)
		return WritePtr-ReadPtr;
	else
		return RingBufferSampleCount-(ReadPtr-WritePtr);
	//s32 sz=(WritePtr+1)%RingBufferSampleCount-ReadPtr;
	//return sz<0?sz+RingBufferSampleCount:sz;
}

u32 asRingFreeCount()
{
	return RingBufferSampleCount-asRingUsedCount();
}

extern double mspdf;
void WriteSample(s16 r, s16 l)
{
	const u32 ptr=(WritePtr+1)%RingBufferSampleCount;
	RingBuffer[ptr].r=r;
	RingBuffer[ptr].l=l;
	WritePtr=ptr;

	if (WritePtr==(SAMPLE_COUNT-1))
	{
		bool do_wait = settings.aica.LimitFPS == LimitFPSEnabled
				|| (settings.aica.LimitFPS == LimitFPSAuto && mspdf <= 11);

		PushAudio(RingBuffer,SAMPLE_COUNT, do_wait);
	}
}

static bool backends_sorted = false;
void SortAudioBackends()
{
	if (backends_sorted)
		return;

	// Sort backends by slug
	for (int n = audiobackends_num_registered; n > 0; n--)
	{
		for (int i = 0; i < n-1; i++)
		{
			if (audiobackends[i]->slug > audiobackends[i+1]->slug)
			{
				audiobackend_t* swap = audiobackends[i];
				audiobackends[i] = audiobackends[i+1];
				audiobackends[i+1] = swap;
			}
		}
	}
}

void InitAudio()
{
	if (cfgLoadInt("audio", "disable", 0)) {
		INFO_LOG(AUDIO, "WARNING: Audio disabled in config!");
		return;
	}

	cfgSaveInt("audio", "disable", 0);

	if (audiobackend_current != NULL) {
		WARN_LOG(AUDIO, "ERROR: The audio backend \"%s\" (%s) has already been initialized, you need to terminate it before you can call audio_init() again!", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
		return;
	}

	SortAudioBackends();

	string audiobackend_slug = settings.audio.backend;
	audiobackend_current = GetAudioBackend(audiobackend_slug);
	if (audiobackend_current == NULL) {
		INFO_LOG(AUDIO, "WARNING: Running without audio!");
		return;
	}

	INFO_LOG(AUDIO, "Initializing audio backend \"%s\" (%s)...", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
	audiobackend_current->init();
}

void TermAudio()
{
	if (audiobackend_current != NULL) {
		audiobackend_current->term();
		INFO_LOG(AUDIO, "Terminating audio backend \"%s\" (%s)...", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
		audiobackend_current = NULL;
	}
}
