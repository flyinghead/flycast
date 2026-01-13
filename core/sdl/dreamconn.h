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

#if defined(_WIN32) && !defined(TARGET_UWP)
#define USE_DREAMCONN 1

#include "dreamlink.h"

#include <memory>

class DreamConn : public DreamLink
{
public:
	//! Base port of communication to DreamConn
	static constexpr u16 BASE_PORT = 37393;
	//! DreamConn VID:4457 PID:4443
	static constexpr const char* VID_PID_GUID = "5744000043440000";

protected:
	DreamConn() = default;
	virtual ~DreamConn() = default;

public:
	static std::shared_ptr<DreamConn> create_shared(int bus);
};

#endif // WIN32 && !UWP
#endif // USE_DREAMLINK_DEVICES
