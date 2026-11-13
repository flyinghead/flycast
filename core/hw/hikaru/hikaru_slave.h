/*
	Sega Hikaru slave SH-4.

	The slave runs interleaved with the master in small time slices: its context is swapped into
	the global SH-4 context slot and the memory accessors are pointed at the slave memory map for
	the duration of a slice. It is recompiled as a second CPU of the SH-4 dynarec when the master
	is (with its own blocks and memory handlers), interpreted otherwise. Slices that only wait on
	RAM are skipped until it changes.

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
#include <string>

namespace hikaru::slave
{

void init();
void term();
void reset();
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

void setBridgeIrq(bool pending);
// Raise an NMI on the slave
void nmi();
// Debugging: slave registers and code dump
std::string debugState();

}
