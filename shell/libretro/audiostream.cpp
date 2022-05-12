/*
    This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "types.h"
#include "cfg/option.h"
#include "oslib/audiostream.h"
#include "emulator.h"

#include <libretro.h>

#include <vector>
#include <mutex>

extern retro_audio_sample_batch_t audio_batch_cb;

static std::mutex audio_buffer_mutex;
static std::vector<int16_t> audio_buffer;
static size_t audio_buffer_idx;
static size_t audio_batch_frames_max;
static bool drop_samples = true;

static int16_t *audio_out_buffer = nullptr;

void retro_audio_init(void)
{
	const std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	/* Worst case is 25 fps content with an audio sample rate
	 * of 44.1 kHz -> 1764 stereo samples
	 * But flycast can stop rendering for arbitrary lengths of
	 * time, leading to multiple 'frames' worth of audio being
	 * uploaded in retro_run(). We therefore require some leniency,
	 * but must limit the total number of samples that can be
	 * uploaded since the libretro frontend can 'hang' if too
	 * many samples are sent during a single call of retro_run().
	 * We therefore (arbitrarily) choose to allow up to 10 frames
	 * worth of 'worst case' stereo samples... */
	size_t audio_buffer_size = (44100 / 25) * 2 * 10;

	audio_buffer.resize(audio_buffer_size);
	audio_buffer_idx = 0;
	audio_batch_frames_max = std::numeric_limits<size_t>::max();

	audio_out_buffer = (int16_t*)malloc(audio_buffer_size * sizeof(int16_t));

	drop_samples = false;
}

void retro_audio_deinit(void)
{
	const std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	audio_buffer.clear();
	audio_buffer_idx = 0;

	if (audio_out_buffer != nullptr)
		free(audio_out_buffer);

	audio_out_buffer = nullptr;

	drop_samples = true;
}

void retro_audio_flush_buffer(void)
{
	const std::lock_guard<std::mutex> lock(audio_buffer_mutex);
	audio_buffer_idx = 0;

	/* We are manually 'resetting' the audio buffer
	 * -> any 'drop samples' lock can be released */
	drop_samples = false;
}

void retro_audio_upload(void)
{
	audio_buffer_mutex.lock();

	for (size_t i = 0; i < audio_buffer_idx; i++)
		audio_out_buffer[i] = audio_buffer[i];

	size_t num_frames = audio_buffer_idx >> 1;
	audio_buffer_idx = 0;

	/* Uploading audio 'resets' the audio buffer
	 * -> any 'drop samples' lock can be released */
	drop_samples = false;

	audio_buffer_mutex.unlock();

	int16_t *audio_out_buffer_ptr = audio_out_buffer;
	while (num_frames > 0)
	{
		size_t frames_to_write = (num_frames > audio_batch_frames_max) ?
				audio_batch_frames_max : num_frames;
		size_t frames_written = audio_batch_cb(audio_out_buffer_ptr,
				frames_to_write);

		if ((frames_written < frames_to_write) &&
			 (frames_written > 0))
			audio_batch_frames_max = frames_written;

		num_frames -= frames_to_write;
		audio_out_buffer_ptr += frames_to_write << 1;
	}
}

void WriteSample(s16 r, s16 l)
{
	const std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	if (drop_samples)
		return;

	if (audio_buffer.size() < audio_buffer_idx + 2)
	{
		/* Audio buffer overflow...
		 * > Drop any existing samples
		 * > Drop any future samples until the next
		 *   call of retro_audio_upload() */
		audio_buffer_idx = 0;
		drop_samples = true;
		return;
	}

	audio_buffer[audio_buffer_idx++] = l;
	audio_buffer[audio_buffer_idx++] = r;
}

void InitAudio()
{
}

void TermAudio()
{
}

void StartAudioRecording(bool eight_khz)
{
}

u32 RecordAudio(void *buffer, u32 samples)
{
	return 0;
}

void StopAudioRecording()
{
}
