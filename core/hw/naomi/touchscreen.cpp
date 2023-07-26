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
#include "touchscreen.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/modules/modules.h"
#include "hw/maple/maple_cfg.h"
#include "input/gamepad.h"
#include "serialize.h"

#include <algorithm>
#include <deque>
#include <memory>

namespace touchscreen
{

//
// 837-14672 touchscreen sensor board
// used by Manic Panic Ghosts and Touch De Zunou
//
class TouchscreenPipe final : public SerialPort::Pipe
{
public:
	TouchscreenPipe()
	{
		schedId = sh4_sched_register(0, schedCallback, this);
		SCIFSerialPort::Instance().setPipe(this);
	}

	~TouchscreenPipe()
	{
		sh4_sched_unregister(schedId);
	}

	void write(u8 data) override
	{
		if (data == 0x39)
		{
			// status request
			constexpr u8 status[] = { 0xaa, 0x39, 0 };
			send(status, sizeof(status));
		}
		if (!schedulerStarted)
		{
			sh4_sched_request(schedId, FRAME_CYCLES);
			schedulerStarted = true;
		}
	}

	int available() override {
		return toSend.size();
	}

	u8 read() override
	{
		u8 data = toSend.front();
		toSend.pop_front();
		return data;
	}

	void serialize(Serializer& ser)
	{
		ser << schedulerStarted;
		sh4_sched_serialize(ser, schedId);
		ser << (int)toSend.size();
		for (u8 b : toSend)
			ser << b;
	}

	void deserialize(Deserializer& deser)
	{
		deser >> schedulerStarted;
		sh4_sched_deserialize(deser, schedId);
		int size;
		deser >> size;
		toSend.resize(size);
		for (int i = 0; i < size; i++)
			deser >> toSend[i];
	}

private:
	void send(const u8 *msg, int size)
	{
		if (toSend.size() >= 32)
			return;
		toSend.insert(toSend.end(), &msg[0], &msg[size]);
		toSend.push_back(calcChecksum(msg, size));
		SCIFSerialPort::Instance().updateStatus();
	}

	u8 calcChecksum(const u8 *data, int size)
	{
		u8 c = 0;
		for (int i = 0; i < size; i++)
			c += data[i];
		return 256 - c;
	}

	static int schedCallback(int tag, int cycles, int jitter, void *arg)
	{
		TouchscreenPipe *instance = (TouchscreenPipe *)arg;
		u32 pack[2];
		for (size_t i = 0; i < std::size(pack); i++)
		{
			int x = std::clamp(mapleInputState[i].absPos.x, 0, 1023);
			int y = std::clamp(mapleInputState[i].absPos.y, 0, 1023);
			int hit = (mapleInputState[i].kcode & DC_BTN_A) == 0;
			int charge = (mapleInputState[i].kcode & DC_BTN_B) == 0;
			// touches require bits 20, 21 and 22
			// drag needs bit 22 off
			// bit 23 is charge
			pack[i] = (charge << 23) | (hit << 21) | (hit << 20) | (y << 10) | x;
			if (!instance->touch[i])
				pack[i] |= hit << 22;
			instance->touch[i] = hit;
		}
		u8 msg[] = { 0xaa, 0x10,
				u8(pack[0] >> 16), u8(pack[0] >> 8), u8(pack[0]),
				u8(pack[1] >> 16), u8(pack[1] >> 8), u8(pack[1]) };
		instance->send(msg, sizeof(msg));

		return FRAME_CYCLES;
	}

	std::deque<u8> toSend;
	int schedId = -1;
	bool schedulerStarted = false;
	bool touch[2] {};

	static constexpr int FRAME_CYCLES = SH4_MAIN_CLOCK / 60;
};

std::unique_ptr<TouchscreenPipe> touchscreen;

void init() {
	touchscreen = std::make_unique<TouchscreenPipe>();
}

void term() {
	touchscreen.reset();
}

void serialize(Serializer& ser)
{
	if (touchscreen)
		touchscreen->serialize(ser);
}
void deserialize(Deserializer& deser)
{
	if (touchscreen)
		touchscreen->deserialize(deser);
}

}
