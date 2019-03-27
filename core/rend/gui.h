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

void gui_init();
void gui_open_settings();
void gui_display_ui();
void gui_display_notification(const char *msg, int duration);
void gui_display_osd();
void gui_open_onboarding();
void gui_term();
void gui_refresh_files();

extern int screen_dpi;

typedef enum { Closed, Commands, Settings, ClosedNoResume, Main, Onboarding, VJoyEdit, VJoyEditCommands } GuiState;
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
