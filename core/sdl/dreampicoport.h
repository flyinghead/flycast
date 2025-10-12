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

#ifdef USE_DREAMLINK_DEVICES

#include "dreamlink.h"

#include <memory>

//! See: https://github.com/OrangeFox86/DreamPicoPort
class DreamPicoPort : public DreamLink
{
public:
    //! Dreamcast Controller USB VID:1209 PID:2f07
    static constexpr const std::uint16_t VID = 0x1209;
    static constexpr const std::uint16_t PID = 0x2f07;
    static constexpr const char* VID_PID_GUID = "09120000072f0000";

protected:
    DreamPicoPort() = default;
	virtual ~DreamPicoPort() = default;

public:
	static std::shared_ptr<DreamPicoPort> create_shared(int bus, int joystick_idx, SDL_Joystick* sdl_joystick);
};

#endif // USE_DREAMCASTCONTROLLER
