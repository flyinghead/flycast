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

void gui_init();
void gui_open_settings();
void gui_display_ui();
void gui_display_notification(const char *msg, int duration);
void gui_display_osd();
void gui_open_onboarding();
void gui_term();
void gui_refresh_files();

extern int screen_dpi;
extern u32 vmu_lcd_data[8][48 * 32];
extern bool vmu_lcd_status[8];
extern bool vmu_lcd_changed[8];

typedef enum {
	Closed,
	Commands,
	Settings,
	Main,
	Onboarding,
	VJoyEdit,
	VJoyEditCommands,
	SelectDisk,
	Loading,
	NetworkStart
} GuiState;
extern GuiState gui_state;
void ImGui_Impl_NewFrame();

static inline bool gui_is_open()
{
	return gui_state != Closed && gui_state != VJoyEdit;
}
static inline bool gui_is_content_browser()
{
	return gui_state == Main;
}
float gui_get_scaling();
