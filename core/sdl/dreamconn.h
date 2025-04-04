/*
	Copyright 2024 flyinghead

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
#include "dreamlink.h"

#ifdef USE_DREAMCASTCONTROLLER

#include <asio.hpp>
#include <mutex>

class DreamConn : public DreamLink
{
	//! Base port of communication to DreamConn
	static constexpr u16 BASE_PORT = 37393;

	int bus = -1;
	bool maple_io_connected = false;
	u8 expansionDevs = 0;
	asio::ip::tcp::iostream iostream;
	std::mutex send_mutex;

public:
	//! DreamConn VID:4457 PID:4443
	static constexpr const char* VID_PID_GUID = "5744000043440000";

public:
	DreamConn(int bus);

	~DreamConn();

	bool send(const MapleMsg& msg) override;

    bool send(const MapleMsg& txMsg, MapleMsg& rxMsg) override;

	int getBus() const override {
		return bus;
	}

    u32 getFunctionCode(int forPort) const override {
		if (forPort == 1 && hasVmu()) {
			return 0x0E000000;
		}
		else if (forPort == 2 && hasRumble()) {
			return 0x00010000;
		}
		return 0;
	}

	bool hasVmu() const {
		return expansionDevs & 1;
	}

	bool hasRumble() const {
		return expansionDevs & 2;
	}

	void changeBus(int newBus) override;

	std::string getName() const override {
		return "DreamConn+ / DreamConn S Controller";
	}

	void connect() override;

	void disconnect() override;
};

#endif // USE_DREAMCASTCONTROLLER
