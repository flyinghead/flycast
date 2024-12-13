/*
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
#include "cfg/option.h"

// VMUs
extern u32 vmu_lcd_data[8][48 * 32];
extern bool vmu_lcd_status[8];
extern u64 vmuLastChanged[8];

void push_vmu_screen(int bus_id, int bus_port, u8* buffer);

// Crosshair
const u32 *getCrosshairTextureData();
std::pair<float, float> getCrosshairPosition(int playerNum);

static inline bool crosshairNeeded(int port)
{
	if (port < 0 || port >= 4)
		return false;
	if (config::CrosshairColor[port] == 0)
		return false;
	if (settings.platform.isArcade())
	{
		// Arcade game: only for lightgun games and P1 or P2 (no known 4-player lightgun or touchscreen game for now)
		if (!settings.input.lightgunGame || (port >= 2 && !settings.input.fourPlayerGames))
			return false;
	}
	else {
		// Console game
		if (config::MapleMainDevices[port] != MDT_LightGun)
			return false;
	}
	return true;
}

static inline void blankVmus()
{
	memset(vmu_lcd_data, 0, sizeof(vmu_lcd_data));
	memset(vmuLastChanged, 0, sizeof(vmuLastChanged));
}
