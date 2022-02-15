/*
	Copyright 2022 flyinghead

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

namespace elan {

void init();
void reset(bool hard);
void term();

void vmem_init();
void vmem_map(u32 base);

void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

extern u8 *RAM;
constexpr u32 ELAN_RAM_SIZE = 32 * 1024 * 1024;
constexpr u32 ELAN_RAM_MASK = ELAN_RAM_SIZE - 1;
}
