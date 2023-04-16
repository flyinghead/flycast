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
#include "alienfnt_modem.h"
#include "hw/sh4/sh4_sched.h"
#include "hw/sh4/modules/modules.h"
#include "picoppp.h"

#include <vector>
#include <deque>
#include <memory>

struct ModemEmu : public SerialPipe
{
	ModemEmu() {
		serial_setPipe(this);
		schedId = sh4_sched_register(0, schedCallback);
	}

	~ModemEmu() {
		sh4_sched_unregister(schedId);
		stop_pico();
		serial_setPipe(nullptr);
	}

	u8 read() override
	{
		if (!toSend.empty())
		{
			char c = toSend.front();
			toSend.pop_front();
			return c;
		}
		else if (connected)
			return read_pico();
		else
			return 0;
	}

	int available() override
	{
		if (!toSend.empty())
			return toSend.size();
		else if (connected)
			return pico_available();
		else
			return 0;
	}

	void write(u8 data) override
	{
		if (connected)
			write_pico(data);
		else if (data == '\r' || data == '\n')
			handleCmd();
		else
			recvBuf.push_back(data);
	}

private:
	void handleCmd()
	{
		if (recvBuf.empty())
			return;
		std::string line(recvBuf.begin(), recvBuf.end());
		recvBuf.clear();
		if (line.substr(0, 4) == "ATDT") {
			send("CONNECT 14400");
			start_pico();
			connected = true;
			sh4_sched_request(schedId, SH4_MAIN_CLOCK / 60);
		}
		else if (line.substr(0, 2) == "AT")
			send("OK");
	}

	void send(const std::string& l)
	{
		toSend.insert(toSend.end(), l.begin(), l.end());
		toSend.push_back('\n');
		serial_updateStatusRegister();
	}

	static int schedCallback(int tag, int cycles, int lag)
	{
		serial_updateStatusRegister();
		return SH4_MAIN_CLOCK / 60;
	}

	std::deque<char> toSend;
	std::vector<char> recvBuf;
	bool connected = false;
	int schedId = -1;
};

static std::unique_ptr<ModemEmu> modemEmu;

void serialModemInit() {
	modemEmu = std::make_unique<ModemEmu>();
}

void serialModemTerm() {
	modemEmu.reset();
}
