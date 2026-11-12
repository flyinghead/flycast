/*
	Sega 315-6146 "MIE" (Maple/JVS controller) low-level emulation for the Hikaru.

	The MIE is a Z80-based MCU. On the Hikaru the SH-4 sees both its I/O ports and its
	memory space directly, one byte per 16-bit word.

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

namespace hikaru::mie
{

void init();
void term();
void reset();
// Load the MIE firmware (315-6146.bin from mie.zip). Returns false if not found.
bool loadFirmware();
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

// SH-4 access: I/O port space (0x00800000-0x0081FFFF) and memory space (0x00820000-0x0083FFFF).
// addr is the SH-4 address, each Z80 byte occupies a 16-bit word.
u8 hostReadPort(u32 addr);
void hostWritePort(u32 addr, u8 data);
u8 hostReadMem(u32 addr);
void hostWriteMem(u32 addr, u8 data);

}
