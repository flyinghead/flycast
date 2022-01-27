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
#include <vector>

#define VJOY_VISIBLE 14
#define OSD_TEX_W 512
#define OSD_TEX_H 256

struct OSDVertex
{
	float x, y;
	float u, v;
	u8 r, g, b, a;
};

const std::vector<OSDVertex>& GetOSDVertices();

extern std::vector<u8> DefaultOSDButtons;
u8 *loadOSDButtons(int &width, int &height);
void HideOSD();

// VMUs
extern u32 vmu_lcd_data[8][48 * 32];
extern bool vmu_lcd_status[8];
extern bool vmu_lcd_changed[8];

void push_vmu_screen(int bus_id, int bus_port, u8* buffer);

// Crosshair
const u32 *getCrosshairTextureData();
std::pair<float, float> getCrosshairPosition(int playerNum);

constexpr int XHAIR_WIDTH = 40;
constexpr int XHAIR_HEIGHT = 40;

static inline bool crosshairsNeeded()
{
	if (config::CrosshairColor[0] == 0 && config::CrosshairColor[1] == 0
			&& config::CrosshairColor[2] == 0 && config::CrosshairColor[3] == 0)
		return false;
	if (settings.platform.isArcade()
			&& settings.input.JammaSetup != JVS::LightGun
			&& settings.input.JammaSetup != JVS::LightGunAsAnalog
			&& settings.input.JammaSetup != JVS::Mazan)
		// not a lightgun game
		return false;
	return true;
}

static inline void blankVmus()
{
	memset(vmu_lcd_data, 0, sizeof(vmu_lcd_data));
	memset(vmu_lcd_changed, true, sizeof(vmu_lcd_changed));
}
