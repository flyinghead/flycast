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

struct ModemEmu : public SerialPort::Pipe
{
	ModemEmu() {
		SCIFSerialPort::Instance().setPipe(this);
		schedId = sh4_sched_register(0, schedCallback);
	}

	~ModemEmu() {
		sh4_sched_unregister(schedId);
		stop_pico();
		SCIFSerialPort::Instance().setPipe(nullptr);
	}

	u8 read() override
	{
		if (!toSend.empty())
		{
			char c = toSend.front();
			toSend.pop_front();
			return c;
		}
		else if (dataMode)
			return read_pico();
		else
			return 0;
	}

	int available() override
	{
		if (!toSend.empty())
			return toSend.size();
		else if (dataMode)
			return pico_available();
		else
			return 0;
	}

	void write(u8 data) override
	{
		if (dataMode)
		{
			if (pluses == 3)
			{
				if (sh4_sched_now64() - plusTime >= SH4_MAIN_CLOCK)
				{
					dataMode = false;
					send("OK");
					recvBuf.push_back(data);
				}
				else
				{
					write_pico('+');
					write_pico('+');
					write_pico('+');
					write_pico(data);
				}
				pluses = 0;
				plusTime = 0;
			}
			else if (data == '+')
			{
				if (++pluses == 3)
					plusTime = sh4_sched_now64();
			}
			else
			{
				while (pluses > 0) {
					write_pico('+');
					pluses--;
				}
				write_pico(data);
			}
		}
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
			dataMode = true;
			sh4_sched_request(schedId, SH4_MAIN_CLOCK / 60);
		}
		if (line.substr(0, 3) == "ATH")
		{
			stop_pico();
			send("OK");
		}
		else if (line.substr(0, 2) == "AT")
			send("OK");
		else if (!line.empty())
			send("ERROR");
	}

	void send(const std::string& l)
	{
		toSend.insert(toSend.end(), l.begin(), l.end());
		toSend.push_back('\n');
		SCIFSerialPort::Instance().updateStatus();
	}

	static int schedCallback(int tag, int cycles, int lag, void *arg)
	{
		SCIFSerialPort::Instance().updateStatus();
		return SH4_MAIN_CLOCK / 60;
	}

	std::deque<char> toSend;
	std::vector<char> recvBuf;
	int schedId = -1;
	int pluses = 0;
	u64 plusTime = 0;
	bool dataMode = false;
};

static std::unique_ptr<ModemEmu> modemEmu;

void serialModemInit() {
	modemEmu = std::make_unique<ModemEmu>();
}

void serialModemTerm() {
	modemEmu.reset();
}
