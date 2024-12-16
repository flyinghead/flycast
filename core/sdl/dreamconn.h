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
#include "types.h"
#include "network/net_platform.h"
#include "emulator.h"

// TODO Need a way to detect DreamConn+ controllers
class DreamConn
{
	const int bus;
	sock_t sock = INVALID_SOCKET;
	u8 expansionDevs = 0;
	static constexpr u16 BASE_PORT = 37393;

public:
	DreamConn(int bus) : bus(bus) {
		connect();
	}
	~DreamConn() {
		disconnect();
	}

	bool send(const u8* data, int size);

	int getBus() const {
		return bus;
	}
	bool hasVmu() {
		return expansionDevs & 1;
	}
	bool hasRumble() {
		return expansionDevs & 2;
	}

private:
	void connect();
	void disconnect();
	static void handleEvent(Event event, void *arg);
};
