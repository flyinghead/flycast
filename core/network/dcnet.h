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
#include "netservice.h"

namespace net::modbba
{

class DCNetService : public Service
{
public:
	bool start() override;
	void stop() override;

	void writeModem(u8 b) override;
	int readModem() override;
	int modemAvailable() override;

	void receiveEthFrame(const u8 *frame, u32 size) override;
};

}
