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
#include "osd.h"
#include "stdclass.h"
#ifdef LIBRETRO
#include "vmu_xhair.h"
#endif

u32 vmu_lcd_data[8][48 * 32];
bool vmu_lcd_status[8];
u64 vmuLastChanged[8];

void push_vmu_screen(int bus_id, int bus_port, u8* buffer)
{
	int vmu_id = bus_id * 2 + bus_port;
	if (vmu_id < 0 || vmu_id >= (int)std::size(vmu_lcd_data))
		return;
	u32 *p = &vmu_lcd_data[vmu_id][0];
	for (int i = 0; i < (int)std::size(vmu_lcd_data[vmu_id]); i++, buffer++)
#ifdef LIBRETRO
		*p++ = (*buffer != 0
				? vmu_screen_params[bus_id].vmu_pixel_on_R | (vmu_screen_params[bus_id].vmu_pixel_on_G << 8) | (vmu_screen_params[bus_id].vmu_pixel_on_B << 16)
				: vmu_screen_params[bus_id].vmu_pixel_off_R | (vmu_screen_params[bus_id].vmu_pixel_off_G << 8) | (vmu_screen_params[bus_id].vmu_pixel_off_B << 16))
				  | (vmu_screen_params[bus_id].vmu_screen_opacity << 24);
#else
		*p++ = *buffer != 0 ? 0xFFFFFFFFu : 0xFF000000u;
#endif
#ifndef LIBRETRO
	vmu_lcd_status[vmu_id] = true;
#endif
	vmuLastChanged[vmu_id] = getTimeMs();
}

static const int lightgunCrosshairData[16 * 16] =
{
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	-1,-1,-1,-1,-1,-1, 0, 0, 0, 0,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1, 0, 0, 0, 0,-1,-1,-1,-1,-1,-1,
	 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
	 0, 0, 0, 0, 0, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0,
};

const u32 *getCrosshairTextureData()
{
	return (u32 *)lightgunCrosshairData;
}

#ifndef LIBRETRO
#include "input/mouse.h"

std::pair<float, float> getCrosshairPosition(int playerNum)
{
	float fx = mo_x_abs[playerNum];
	float fy = mo_y_abs[playerNum];
	float width = 640.f;
	float height = 480.f;

	if (config::Rotate90)
	{
		float t = fy;
		fy = 639.f - fx;
		fx = t;
		std::swap(width, height);
	}
	float scale = height / settings.display.height;
	fy /= scale;
	scale /= config::ScreenStretching / 100.f;
	fx = fx / scale + (settings.display.width - width / scale) / 2.f;

	return std::make_pair(fx, fy);
}

#endif
