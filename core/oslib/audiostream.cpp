#include "audiostream.h"
#include "cfg/option.h"

struct SoundFrame { s16 l; s16 r; };

static SoundFrame Buffer[SAMPLE_COUNT];
static u32 writePtr;  // next sample index

static AudioBackend *currentBackend;
std::vector<AudioBackend *> *AudioBackend::backends;

static bool audio_recording_started;
static bool eight_khz;

AudioBackend *AudioBackend::getBackend(const std::string& slug)
{
	if (backends == nullptr)
		return nullptr;
	if (slug == "auto")
	{
		// Prefer sdl2 if available and avoid the null driver
		AudioBackend *sdlBackend = nullptr;
		AudioBackend *autoBackend = nullptr;
		for (auto backend : *backends)
		{
			if (backend->slug == "sdl2")
				sdlBackend = backend;
			if (backend->slug != "null" && autoBackend == nullptr)
				autoBackend = backend;
		}
		if (sdlBackend != nullptr)
			autoBackend = sdlBackend;
		if (autoBackend == nullptr)
			autoBackend = backends->front();
		INFO_LOG(AUDIO, "Auto-selected audio backend \"%s\" (%s).", autoBackend->slug.c_str(), autoBackend->name.c_str());

		return autoBackend;
	}
	for (auto backend : *backends)
	{
		if (backend->slug == slug)
			return backend;
	}
	WARN_LOG(AUDIO, "WARNING: Audio backend \"%s\" not found!", slug.c_str());
	return nullptr;
}

void WriteSample(s16 r, s16 l)
{
	Buffer[writePtr].r = r * config::AudioVolume.dbPower();
	Buffer[writePtr].l = l * config::AudioVolume.dbPower();

	if (++writePtr == SAMPLE_COUNT)
	{
		if (currentBackend != nullptr)
			currentBackend->push(Buffer, SAMPLE_COUNT, config::LimitFPS);
		writePtr = 0;
	}
}

void InitAudio()
{
	TermAudio();

	std::string slug = config::AudioBackend;
	currentBackend = AudioBackend::getBackend(slug);
	if (currentBackend == nullptr && slug != "auto")
	{
		slug = "auto";
		currentBackend = AudioBackend::getBackend(slug);
	}
	if (currentBackend != nullptr)
	{
		INFO_LOG(AUDIO, "Initializing audio backend \"%s\" (%s)...", currentBackend->slug.c_str(), currentBackend->name.c_str());
		if (!currentBackend->init())
		{
			currentBackend = nullptr;
			if (slug != "auto")
			{
				WARN_LOG(AUDIO, "Audio driver %s failed to initialize. Defaulting to 'auto'", slug.c_str());
				slug = "auto";
				currentBackend = AudioBackend::getBackend(slug);
				if (!currentBackend->init())
					currentBackend = nullptr;
			}
		}
	}

	if (currentBackend == nullptr)
	{
		WARN_LOG(AUDIO, "Running without audio!");
		return;
	}

	if (audio_recording_started)
	{
		// Restart recording
		audio_recording_started = false;
		StartAudioRecording(eight_khz);
	}
}

void TermAudio()
{
	if (currentBackend == nullptr)
		return;

	// Save recording state before stopping
	bool rec_started = audio_recording_started;
	StopAudioRecording();
	audio_recording_started = rec_started;
	currentBackend->term();
	INFO_LOG(AUDIO, "Terminating audio backend \"%s\" (%s)...", currentBackend->slug.c_str(), currentBackend->name.c_str());
	currentBackend = nullptr;
}

void StartAudioRecording(bool eight_khz)
{
	::eight_khz = eight_khz;
	if (currentBackend != nullptr)
		audio_recording_started = currentBackend->initRecord(eight_khz ? 8000 : 11025);
	else
		// might be called between TermAudio/InitAudio
		audio_recording_started = true;
}

u32 RecordAudio(void *buffer, u32 samples)
{
	if (!audio_recording_started || currentBackend == nullptr)
		return 0;
	return currentBackend->record(buffer, samples);
}

void StopAudioRecording()
{
	// might be called between TermAudio/InitAudio
	if (audio_recording_started && currentBackend != nullptr)
		currentBackend->termRecord();
	audio_recording_started = false;
}
