/*
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
#include "cfg/option.h"

void gui_init();
void gui_open_settings();
void gui_display_ui();
void gui_display_notification(const char *msg, int duration);
void gui_display_osd();
void gui_open_onboarding();
void gui_term();
void gui_refresh_files();
void gui_cheats();
void gui_set_insets(int left, int right, int top, int bottom);

extern int screen_dpi;
extern float scaling;
extern u32 vmu_lcd_data[8][48 * 32];
extern bool vmu_lcd_status[8];
extern bool vmu_lcd_changed[8];

enum class GuiState {
	Closed,
	Commands,
	Settings,
	Main,
	Onboarding,
	VJoyEdit,
	VJoyEditCommands,
	SelectDisk,
	Loading,
	NetworkStart,
	Cheats
};
extern GuiState gui_state;

static inline bool gui_is_open()
{
	return gui_state != GuiState::Closed && gui_state != GuiState::VJoyEdit;
}
static inline bool gui_is_content_browser()
{
	return gui_state == GuiState::Main;
}
static inline float gui_get_scaling() {
	return scaling;
}

#define XHAIR_WIDTH (40 * scaling)
#define XHAIR_HEIGHT (40 * scaling)
static inline bool crosshairsNeeded()
{
	if (config::CrosshairColor[0] == 0 && config::CrosshairColor[1] == 0
			&& config::CrosshairColor[2] == 0 && config::CrosshairColor[3] == 0)
		return false;
	if (settings.platform.system != DC_PLATFORM_DREAMCAST
			&& settings.input.JammaSetup != JVS::LightGun
			&& settings.input.JammaSetup != JVS::LightGunAsAnalog
			&& settings.input.JammaSetup != JVS::Mazan)
		// not a lightgun game
		return false;
	return true;
}
const u32 *getCrosshairTextureData();
std::pair<float, float> getCrosshairPosition(int playerNum);
