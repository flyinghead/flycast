/*
	Copyright 2025 flyinghead

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

namespace net::modbba
{

bool start();
void stop();

void writeModem(u8 b);
int readModem();
int modemAvailable();

void receiveEthFrame(const u8 *frame, u32 size);

class Service
{
public:
	virtual ~Service() = default;
	virtual bool start() = 0;
	virtual void stop() = 0;

	virtual void writeModem(u8 b) = 0;
	virtual int readModem() = 0;
	virtual int modemAvailable() = 0;

	virtual void receiveEthFrame(const u8 *frame, u32 size) = 0;
};

}
