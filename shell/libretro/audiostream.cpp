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

#define SAMPLE_COUNT 512
struct SoundFrame { s16 l; s16 r; };

extern retro_audio_sample_batch_t audio_batch_cb;

void WriteSample(s16 r, s16 l)
{
   static SoundFrame Buffer[SAMPLE_COUNT];
   static u32 writePtr; // next sample index
   Buffer[writePtr].r = r;
   Buffer[writePtr].l = l;

   if (++writePtr == SAMPLE_COUNT)
   {
      if (emu.running() && (!config::ThreadedRendering || config::LimitFPS))
         audio_batch_cb((const int16_t*)Buffer, SAMPLE_COUNT);
      writePtr = 0;
   }
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
