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
#include "types.h"
#include "input/gamepad.h"
#include "input/gamepad_device.h"
#include "TexCache.h"
#include "hw/maple/maple_devs.h"
#ifdef LIBRETRO
#include "vmu_xhair.h"
#endif

#include <stb_image.h>

#if defined(__ANDROID__) && !defined(LIBRETRO)
extern float vjoy_pos[15][8];
#else

static float vjoy_pos[15][8]=
{
	{24+0,24+64,64,64},     //LEFT
	{24+64,24+0,64,64},     //UP
	{24+128,24+64,64,64},   //RIGHT
	{24+64,24+128,64,64},   //DOWN

	{440+0,280+64,64,64},   //X
	{440+64,280+0,64,64},   //Y
	{440+128,280+64,64,64}, //B
	{440+64,280+128,64,64}, //A

	{320-32,360+32,64,64},  //Start

	{440,200,90,64},        //LT
	{542,200,90,64},        //RT

	{-24,128+224,128,128},  //ANALOG_RING
	{96,320,64,64},         //ANALOG_POINT
	{320-32,24,64,64},		// FFORWARD
	{1}						// VJOY_VISIBLE
};
#endif // !__ANDROID__

static const float vjoy_sz[2][15] = {
	{ 64,64,64,64, 64,64,64,64, 64, 90,90, 128, 64, 64 },
	{ 64,64,64,64, 64,64,64,64, 64, 64,64, 128, 64, 64 },
};

static std::vector<OSDVertex> osdVertices;
std::vector<u8> DefaultOSDButtons;

void HideOSD()
{
	vjoy_pos[VJOY_VISIBLE][0] = 0;
}

static void DrawButton(const float xy[8], u32 state)
{
	OSDVertex vtx;

	vtx.r = vtx.g = vtx.b = (0x7F - 0x40 * state / 255) * vjoy_pos[VJOY_VISIBLE][0];
	vtx.a = 0xA0 * vjoy_pos[VJOY_VISIBLE][4];
	vjoy_pos[VJOY_VISIBLE][4] += (vjoy_pos[VJOY_VISIBLE][0] - vjoy_pos[VJOY_VISIBLE][4]) / 2;

	vtx.x = xy[0]; vtx.y = xy[1];
	vtx.u = xy[4]; vtx.v = xy[5];
	osdVertices.push_back(vtx);

	vtx.x = xy[0] + xy[2]; vtx.y = xy[1];
	vtx.u = xy[6]; vtx.v = xy[5];
	osdVertices.push_back(vtx);

	vtx.x = xy[0]; vtx.y = xy[1] + xy[3];
	vtx.u = xy[4]; vtx.v = xy[7];
	osdVertices.push_back(vtx);

	vtx.x = xy[0] + xy[2]; vtx.y = xy[1] + xy[3];
	vtx.u = xy[6]; vtx.v = xy[7];
	osdVertices.push_back(vtx);
}

static void DrawButton2(const float xy[8], bool state)
{
	DrawButton(xy, state ? 0 : 255);
}

const std::vector<OSDVertex>& GetOSDVertices()
{
	osdVertices.reserve(ARRAY_SIZE(vjoy_pos) * 4);
	osdVertices.clear();
	DrawButton2(vjoy_pos[0], kcode[0] & DC_DPAD_LEFT);
	DrawButton2(vjoy_pos[1], kcode[0] & DC_DPAD_UP);
	DrawButton2(vjoy_pos[2], kcode[0] & DC_DPAD_RIGHT);
	DrawButton2(vjoy_pos[3], kcode[0] & DC_DPAD_DOWN);

	DrawButton2(vjoy_pos[4], kcode[0] & (settings.platform.system == DC_PLATFORM_DREAMCAST ? DC_BTN_X : DC_BTN_C));
	DrawButton2(vjoy_pos[5], kcode[0] & (settings.platform.system == DC_PLATFORM_DREAMCAST ? DC_BTN_Y : DC_BTN_X));
	DrawButton2(vjoy_pos[6], kcode[0] & DC_BTN_B);
	DrawButton2(vjoy_pos[7], kcode[0] & DC_BTN_A);

	DrawButton2(vjoy_pos[8], kcode[0] & DC_BTN_START);

	DrawButton(vjoy_pos[9], lt[0]);

	DrawButton(vjoy_pos[10], rt[0]);

	DrawButton2(vjoy_pos[11], true);
	DrawButton2(vjoy_pos[12], false);

	DrawButton2(vjoy_pos[13], false);

	return osdVertices;
}

static void setVjoyUV()
{
	float u = 0;
	float v = 0;

	for (int i = 0; i < VJOY_VISIBLE; i++)
	{
		//umin, vmin, umax, vmax
		vjoy_pos[i][4] = (u + 1) / OSD_TEX_W;
		vjoy_pos[i][5] = 1.f - (v + 1) / OSD_TEX_H;

		vjoy_pos[i][6] = (u + vjoy_sz[0][i] - 1) / OSD_TEX_W;
		vjoy_pos[i][7] = 1.f - (v + vjoy_sz[1][i] - 1) / OSD_TEX_H;

		u += vjoy_sz[0][i];
		if (u >= OSD_TEX_W)
		{
			u -= OSD_TEX_W;
			v += vjoy_sz[1][i];
		}
	}
}

static OnLoad setVjoyUVOnLoad(&setVjoyUV);

u8 *loadOSDButtons(int &width, int &height)
{
	int n;
	stbi_set_flip_vertically_on_load(1);

	FILE *file = nowide::fopen(get_readonly_data_path("buttons.png").c_str(), "rb");
	u8 *image_data = nullptr;
	if (file != nullptr)
	{
		image_data = stbi_load_from_file(file, &width, &height, &n, STBI_rgb_alpha);
		std::fclose(file);
	}
	if (image_data == nullptr)
	{
		if (DefaultOSDButtons.empty())
			die("No default OSD buttons");
		image_data = stbi_load_from_memory(DefaultOSDButtons.data(), DefaultOSDButtons.size(), &width, &height, &n, STBI_rgb_alpha);
	}
	return image_data;
}

u32 vmu_lcd_data[8][48 * 32];
bool vmu_lcd_status[8];
bool vmu_lcd_changed[8];

void push_vmu_screen(int bus_id, int bus_port, u8* buffer)
{
	int vmu_id = bus_id * 2 + bus_port;
	if (vmu_id < 0 || vmu_id >= (int)ARRAY_SIZE(vmu_lcd_data))
		return;
	u32 *p = &vmu_lcd_data[vmu_id][0];
	for (int i = 0; i < (int)ARRAY_SIZE(vmu_lcd_data[vmu_id]); i++, buffer++)
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
	vmu_lcd_changed[vmu_id] = true;
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
