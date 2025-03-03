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

// This file contains abstraction layer for access to different kinds of physical controllers

#include "types.h"
#include "emulator.h"
#include "sdl_gamepad.h"

#if (defined(_WIN32) || defined(__linux__) || (defined(__APPLE__) && defined(TARGET_OS_MAC))) && !defined(TARGET_UWP)
#define USE_DREAMCASTCONTROLLER 1
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

// Abstract base class for physical controller implementations
class DreamLink
{
public:
    DreamLink() = default;

	virtual ~DreamLink() = default;

	virtual bool send(const MapleMsg& msg) = 0;

    virtual bool receive(MapleMsg& msg) = 0;

	// When called, do teardown stuff like reset screen
	virtual inline void gameTermination() {}

	virtual int getBus() const = 0;

	virtual bool hasVmu() const = 0;

	virtual bool hasRumble() const = 0;

	virtual int getDefaultBus() const {
		// No default bus by default
		return -1;
	}

	virtual void changeBus(int newBus) = 0;

	virtual std::string getName() const = 0;

	virtual void connect() = 0;

	virtual void disconnect() = 0;
};

class DreamLinkGamepad : public SDLGamepad
{
public:
    DreamLinkGamepad(int maple_port, int joystick_idx, SDL_Joystick* sdl_joystick);
	~DreamLinkGamepad();

	void set_maple_port(int port) override;
	void registered() override;
	bool gamepad_btn_input(u32 code, bool pressed) override;
	bool gamepad_axis_input(u32 code, int value) override;
	static bool isDreamcastController(int deviceIndex);

private:
	static void handleEvent(Event event, void *arg);
	void checkKeyCombo();

	std::shared_ptr<DreamLink> dreamlink;
	bool ltrigPressed = false;
	bool rtrigPressed = false;
	bool startPressed = false;
};
