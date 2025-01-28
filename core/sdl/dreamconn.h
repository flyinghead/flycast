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
#include "emulator.h"
#include "sdl_gamepad.h"
#if (defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))) && !defined(TARGET_UWP)
#define USE_DREAMCASTCONTROLLER 1
#define TYPE_DREAMCONN 1
#define TYPE_DREAMCASTCONTROLLERUSB 2
#include <asio.hpp>
#endif
#include <memory>

struct MapleMsg
{
	u8 command;
	u8 destAP;
	u8 originAP;
	u8 size;
	u8 data[1024];

	u32 getDataSize() const {
		return size * 4;
	}

	template<typename T>
	void setData(const T& p) {
		memcpy(data, &p, sizeof(T));
		this->size = (sizeof(T) + 3) / 4;
	}
};
static_assert(sizeof(MapleMsg) == 1028);

class DreamConn
{
	const int bus;
	const int dreamcastControllerType;
#ifdef USE_DREAMCASTCONTROLLER
	std::unique_ptr<class DreamcastControllerConnection> dcConnection;
#endif
	bool maple_io_connected;
	u8 expansionDevs = 0;

public:
	DreamConn(int bus, int dreamcastControllerType);

	~DreamConn();

	bool send(const MapleMsg& msg);

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
};

class DreamConnGamepad : public SDLGamepad
{
public:
	DreamConnGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick);
	~DreamConnGamepad();

	void set_maple_port(int port) override;
	bool gamepad_btn_input(u32 code, bool pressed) override;
	bool gamepad_axis_input(u32 code, int value) override;
	static bool isDreamcastController(int deviceIndex);

private:
	static void handleEvent(Event event, void *arg);
	void checkKeyCombo();

	std::shared_ptr<DreamConn> dreamconn;
	bool ltrigPressed = false;
	bool rtrigPressed = false;
	bool startPressed = false;
	int dreamcastControllerType;
};
