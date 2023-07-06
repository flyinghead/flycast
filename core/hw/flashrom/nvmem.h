/*
	Copyright 2023 flyinghead

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

namespace nvmem
{

void init();
void term();
void reset();

void serialize(Serializer& ser);
void deserialize(Deserializer& deser);

u32 readFlash(u32 addr, u32 sz);
void writeFlash(u32 addr, u32 data, u32 sz);
u8 *getFlashData();

u32 readBios(u32 addr, u32 sz);
void writeAWBios(u32 addr, u32 data, u32 sz);
u8 *getBiosData();
void reloadAWBios();

bool loadFiles();
void saveFiles();
bool loadHle();

}
