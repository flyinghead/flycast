/*
	Sega Hikaru GPU (315-6083A): registers, interrupts, texture upload engine, display timing.

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
#pragma once
#include "types.h"

namespace hikaru::gpu
{

void init();
void term();
void reset(bool hard);
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

// GPU register windows: 0x15000000, 0x18000000 and 0x1A000000
template<typename T> T DYNACALL readReg(u32 addr);
template<typename T> void DYNACALL writeReg(u32 addr, T data);

// Called when a region of texture RAM has been rewritten.
// Encoding: bits 0-6 x/16, bits 8-13 y/16, bits 16-18 log2(w/16), bits 19-21 log2(h/16), bit 31 page B
void textureUploaded(u32 region);

// Texture RAM change tracking, in 64 KB bands (16 rows of a texture page): every write bumps a
// generation counter. Offsets are in texture RAM (0-16 MB, texture pages at 8 and 12 MB).
void texRamWritten(u32 offset, u32 size);
// Latest write generation over a range (0 if never written)
u32 texRamGeneration(u32 offset, u32 size);
// Current generation: a range is unchanged since then if its texRamGeneration() is not greater
u32 texRamCurrentGeneration();

// 2D layer registers at 0x1A000180 (index = (addr - 0x1A000180) / 4)
u32 layerReg(int index);

// 0x1A00001C: bit 23 selects the 3D buffer shown
u32 displayStatus();

}
