#pragma once
#include "types.h"
#include "cfg/option.h"

typedef std::vector<std::string> (*audio_option_callback_t)();
enum audio_option_type
{
	integer = 0
,	checkbox = 1
,	list = 2
};

typedef struct {
	std::string cfg_name;
	std::string caption;
	audio_option_type type;

	// type int_value (spin edit)
	int min_value;
	int max_value;

	// type list edit (string/char*)
	audio_option_callback_t list_callback;
} audio_option_t;

typedef audio_option_t* (*audio_options_func_t)(int* option_count);

typedef void (*audio_backend_init_func_t)();
typedef u32 (*audio_backend_push_func_t)(const void *data, u32 frames, bool wait);
typedef void (*audio_backend_term_func_t)();
typedef struct {
    std::string slug;
    std::string name;
    audio_backend_init_func_t init;
    audio_backend_push_func_t push;
    audio_backend_term_func_t term;
	audio_options_func_t get_options;
    bool (*init_record)(u32 sampling_freq);
    u32 (*record)(void *, u32);
    audio_backend_term_func_t term_record;
} audiobackend_t;
bool RegisterAudioBackend(audiobackend_t* backend);
void InitAudio();
void TermAudio();
void WriteSample(s16 right, s16 left);

void StartAudioRecording(bool eight_khz);
u32 RecordAudio(void *buffer, u32 samples);
void StopAudioRecording();

u32 GetAudioBackendCount();
audiobackend_t* GetAudioBackend(int num);
audiobackend_t* GetAudioBackend(const std::string& slug);

constexpr u32 SAMPLE_COUNT = 512;	// push() is always called with that many frames
