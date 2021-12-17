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

#define AUDIO_BUFFER_SIZE_DEFAULT (1 << 11)
#define AUDIO_BUFFER_SIZE_MAX (1 << 20) /* 1 MiB */

extern retro_audio_sample_batch_t audio_batch_cb;

static std::mutex audio_buffer_mutex;
static std::vector<int16_t> audio_buffer;
static size_t audio_buffer_idx;
static size_t audio_batch_frames_max;

static int16_t *audio_out_buffer = nullptr;
static size_t audio_out_buffer_size;

void retro_audio_init(void)
{
	const std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	audio_buffer.resize(AUDIO_BUFFER_SIZE_DEFAULT);
	audio_buffer_idx = 0;
	audio_batch_frames_max = std::numeric_limits<size_t>::max();

	audio_out_buffer_size = AUDIO_BUFFER_SIZE_DEFAULT;
	audio_out_buffer = (int16_t*)malloc(audio_out_buffer_size * sizeof(int16_t));
}

void retro_audio_deinit(void)
{
	const std::lock_guard<std::mutex> lock(audio_buffer_mutex);

	audio_buffer.clear();
	audio_buffer_idx = 0;

	if (audio_out_buffer != nullptr)
		free(audio_out_buffer);

	audio_out_buffer = nullptr;
	audio_out_buffer_size = 0;
}

void retro_audio_flush_buffer(void)
{
	const std::lock_guard<std::mutex> lock(audio_buffer_mutex);
	audio_buffer_idx = 0;
}

void retro_audio_upload(void)
{
	audio_buffer_mutex.lock();

	if (audio_out_buffer_size < audio_buffer_idx)
	{
		int16_t *tmp = (int16_t *)realloc(audio_out_buffer,
				audio_buffer_idx * sizeof(int16_t));

		if (!tmp)
		{
			audio_buffer_idx = 0;
			audio_buffer_mutex.unlock();
			return;
		}

		audio_out_buffer_size = audio_buffer_idx;
		audio_out_buffer = tmp;
	}

	for (size_t i = 0; i < audio_buffer_idx; i++)
		audio_out_buffer[i] = audio_buffer[i];

	size_t num_frames = audio_buffer_idx >> 1;
	audio_buffer_idx = 0;

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

	if (audio_buffer.size() < audio_buffer_idx + 2)
	{
		if (audio_buffer_idx + 2 > AUDIO_BUFFER_SIZE_MAX)
		{
			audio_buffer_idx = 0;
			return;
		}

		try
		{
			audio_buffer.resize(audio_buffer_idx + 2 + AUDIO_BUFFER_SIZE_DEFAULT);
		}
		catch (std::bad_alloc &)
		{
			audio_buffer_idx = 0;
			return;
		}
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
