/*
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
#include "vmu_xhair.h"
#include "cfg/option.h"

const char* VMU_SCREEN_COLOR_NAMES[VMU_NUM_COLORS] = {
		"DEFAULT_ON",
		"DEFAULT_OFF",
		"BLACK",
		"BLUE",
		"LIGHT_BLUE",
		"GREEN",
		"GREEN_BLUE",
		"GREEN_LIGHT_BLUE",
		"LIGHT_GREEN",
		"LIGHT_GREEN_BLUE",
		"LIGHT_GREEN_LIGHT_BLUE",
		"RED",
		"RED_BLUE",
		"RED_LIGHT_BLUE",
		"RED_GREEN",
		"RED_GREEN_BLUE",
		"RED_GREEN_LIGHT_BLUE",
		"RED_LIGHT_GREEN",
		"RED_LIGHT_GREEN_BLUE",
		"RED_LIGHT_GREEN_LIGHT_BLUE",
		"LIGHT_RED",
		"LIGHT_RED_BLUE",
		"LIGHT_RED_LIGHT_BLUE",
		"LIGHT_RED_GREEN",
		"LIGHT_RED_GREEN_BLUE",
		"LIGHT_RED_GREEN_LIGHT_BLUE",
		"LIGHT_RED_LIGHT_GREEN",
		"LIGHT_RED_LIGHT_GREEN_BLUE",
		"WHITE"
};
const rgb_t VMU_SCREEN_COLOR_MAP[VMU_NUM_COLORS] = {
		{ DEFAULT_VMU_PIXEL_ON_R,   DEFAULT_VMU_PIXEL_ON_G ,   DEFAULT_VMU_PIXEL_ON_B  },
		{ DEFAULT_VMU_PIXEL_OFF_R,  DEFAULT_VMU_PIXEL_OFF_G ,  DEFAULT_VMU_PIXEL_OFF_B  },
		{ 0x00, 0x00, 0x00 },
		{ 0x00, 0x00, 0x7F },
		{ 0x00, 0x00, 0xFF },
		{ 0x00, 0x7F, 0x00 },
		{ 0x00, 0x7F, 0x7F },
		{ 0x00, 0x7F, 0xFF },
		{ 0x00, 0xFF, 0x00 },
		{ 0x00, 0xFF, 0x7F },
		{ 0x00, 0xFF, 0xFF },
		{ 0x7F, 0x00, 0x00 },
		{ 0x7F, 0x00, 0x7F },
		{ 0x7F, 0x00, 0xFF },
		{ 0x7F, 0x7F, 0x00 },
		{ 0x7F, 0x7F, 0x7F },
		{ 0x7F, 0x7F, 0xFF },
		{ 0x7F, 0xFF, 0x00 },
		{ 0x7F, 0xFF, 0x7F },
		{ 0x7F, 0xFF, 0xFF },
		{ 0xFF, 0x00, 0x00 },
		{ 0xFF, 0x00, 0x7F },
		{ 0xFF, 0x00, 0xFF },
		{ 0xFF, 0x7F, 0x00 },
		{ 0xFF, 0x7F, 0x7F },
		{ 0xFF, 0x7F, 0xFF },
		{ 0xFF, 0xFF, 0x00 },
		{ 0xFF, 0xFF, 0x7F },
		{ 0xFF, 0xFF, 0xFF }
};

vmu_screen_params_t vmu_screen_params[4];

u8 lightgun_img_crosshair[LIGHTGUN_CROSSHAIR_SIZE*LIGHTGUN_CROSSHAIR_SIZE] =
{
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,
	1,1,1,1,1,1,0,0,0,0,1,1,1,1,1,1,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,
};

u8 lightgun_palette[LIGHTGUN_COLORS_COUNT*3] =
{
	0x00,0x00,0x00, // LIGHTGUN_COLOR_OFF
	0xff,0xff,0xff, // LIGHTGUN_COLOR_WHITE
	0xff,0x10,0x10, // LIGHTGUN_COLOR_RED
	0x10,0xff,0x10, // LIGHTGUN_COLOR_GREEN
	0x10,0x10,0xff, // LIGHTGUN_COLOR_BLUE
};

lightgun_params_t lightgun_params[4];

std::pair<float, float> getCrosshairPosition(int playerNum)
{
	float fx = lightgun_params[playerNum].x * config::RenderResolution * config::ScreenStretching / 480.f / 100.f;
	float fy = lightgun_params[playerNum].y * config::RenderResolution / 480.f;
	if (config::Widescreen && config::ScreenStretching == 100 && !config::EmulateFramebuffer)
		fx += (480.f * 16.f / 9.f - 640.f) / 2.f * config::RenderResolution / 480.f;

	return std::make_pair(fx, fy);
}
