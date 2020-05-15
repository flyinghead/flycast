/*
    Created on: Sep 23, 2019

	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "types.h"

struct Cheat
{
	const char *game_id;
	const char *area_or_version;
	u32 addresses[16];
	u32 values[16];
};

class CheatManager
{
public:
	CheatManager() : _widescreen_cheat(nullptr) {}
	bool Reset();	// Returns true if using 16:9 anamorphic screen ratio
	void Apply();
private:
	static const Cheat _widescreen_cheats[];
	static const Cheat _naomi_widescreen_cheats[];
	const Cheat *_widescreen_cheat;
};

extern CheatManager cheatManager;
