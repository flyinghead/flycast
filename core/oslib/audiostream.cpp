#include "audiostream.h"
#include "cfg/cfg.h"

#include <algorithm>
#include <memory>

struct SoundFrame { s16 l;s16 r; };
constexpr u32 SAMPLE_COUNT =  512;

static SoundFrame RingBuffer[SAMPLE_COUNT];

static u32 WritePtr;  //last WRITTEN sample

static audiobackend_t *audiobackend_current = nullptr;
static std::unique_ptr<std::vector<audiobackend_t *>> audiobackends;	// Using a pointer to avoid out of order init

static bool audio_recording_started;
static bool eight_khz;

u32 GetAudioBackendCount()
{
	return audiobackends != nullptr ? audiobackends->size() : 0;
}

audiobackend_t* GetAudioBackend(int num)
{
	return audiobackends->at(num);
}

static void SortAudioBackends()
{
	if (audiobackends != nullptr)
		std::sort(audiobackends->begin(), audiobackends->end(), [](audiobackend_t *b1, audiobackend_t *b2) { return b1->slug < b2->slug; });
}

bool RegisterAudioBackend(audiobackend_t *backend)
{
	verify(backend != nullptr);
	verify(!backend->slug.empty() && backend->slug != "auto");

	if (audiobackends == nullptr)
		audiobackends = std::unique_ptr<std::vector<audiobackend_t *>>(new std::vector<audiobackend_t *>());
	audiobackends->push_back(backend);
	SortAudioBackends();

	return true;
}

audiobackend_t* GetAudioBackend(const std::string& slug)
{
	if (audiobackends != nullptr && !audiobackends->empty())
	{
		if (slug == "auto")
		{
			// Don't select the null driver
			audiobackend_t *autoselection = nullptr;
			for (auto backend : *audiobackends)
				if (backend->slug != "null")
				{
					autoselection = backend;
					break;
				}
			if (autoselection == nullptr)
				autoselection = audiobackends->front();
			INFO_LOG(AUDIO, "Auto-selected audio backend \"%s\" (%s).", autoselection->slug.c_str(), autoselection->name.c_str());
			return autoselection;
		}
		else
		{
			for (auto backend : *audiobackends)
			{
				if (backend->slug == slug)
					return backend;
			}
			WARN_LOG(AUDIO, "WARNING: Audio backend \"%s\" not found!", slug.c_str());
		}
	}
	else
	{
		WARN_LOG(AUDIO, "WARNING: No audio backends available!");
	}
	return nullptr;
}

static u32 PushAudio(void* frame, u32 amt, bool wait)
{
	if (audiobackend_current != nullptr)
		return audiobackend_current->push(frame, amt, wait);

	return 0;
}

void WriteSample(s16 r, s16 l)
{
	const u32 ptr=(WritePtr+1)%SAMPLE_COUNT;
	RingBuffer[ptr].r=r;
	RingBuffer[ptr].l=l;
	WritePtr=ptr;

	if (WritePtr == SAMPLE_COUNT - 1)
		PushAudio(RingBuffer,SAMPLE_COUNT, settings.aica.LimitFPS);
}

void InitAudio()
{
	if (cfgLoadInt("audio", "disable", 0)) {
		INFO_LOG(AUDIO, "WARNING: Audio disabled in config!");
		return;
	}

	cfgSaveInt("audio", "disable", 0);

	verify(audiobackend_current == nullptr);

	SortAudioBackends();

	std::string audiobackend_slug = settings.audio.backend;
	audiobackend_current = GetAudioBackend(audiobackend_slug);
	if (audiobackend_current == nullptr) {
		INFO_LOG(AUDIO, "WARNING: Running without audio!");
		return;
	}

	INFO_LOG(AUDIO, "Initializing audio backend \"%s\" (%s)...", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
	audiobackend_current->init();
	if (audio_recording_started)
	{
		// Restart recording
		audio_recording_started = false;
		StartAudioRecording(eight_khz);
	}
}

void TermAudio()
{
	if (audiobackend_current != nullptr) {
		// Save recording state before stopping
		bool rec_started = audio_recording_started;
		StopAudioRecording();
		audio_recording_started = rec_started;
		audiobackend_current->term();
		INFO_LOG(AUDIO, "Terminating audio backend \"%s\" (%s)...", audiobackend_current->slug.c_str(), audiobackend_current->name.c_str());
		audiobackend_current = nullptr;
	}
}

void StartAudioRecording(bool eight_khz)
{
	::eight_khz = eight_khz;
	if (audiobackend_current != nullptr)
	{
		audio_recording_started = false;
		if (audiobackend_current->init_record != nullptr)
			audio_recording_started = audiobackend_current->init_record(eight_khz ? 8000 : 11025);
	}
	else
		// might be called between TermAudio/InitAudio
		audio_recording_started = true;
}

u32 RecordAudio(void *buffer, u32 samples)
{
	if (!audio_recording_started || audiobackend_current == nullptr)
		return 0;
	return audiobackend_current->record(buffer, samples);
}

void StopAudioRecording()
{
	// might be called between TermAudio/InitAudio
	if (audio_recording_started && audiobackend_current != nullptr && audiobackend_current->term_record != nullptr)
		audiobackend_current->term_record();
	audio_recording_started = false;
}
