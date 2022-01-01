/*
	Originally based on nullDC: nullExtDev/modem.h

	Created on: Sep 9, 2018

	Copyright 2018 skmp, flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include "types.h"

void ModemInit();
void ModemReset();
void ModemTerm();
u32 ModemReadMem_A0_006(u32 addr,u32 size);
void ModemWriteMem_A0_006(u32 addr,u32 data,u32 size);
void ModemSerialize(Serializer& ser);
void ModemDeserialize(Deserializer& deser);
