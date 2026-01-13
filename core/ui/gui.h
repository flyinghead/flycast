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

#include <string>
#include <functional>

void gui_init();
void gui_initFonts();
void gui_open_settings();
void gui_display_ui();
void gui_draw_osd();
void gui_display_osd();
void gui_display_profiler();
void gui_open_onboarding();
void gui_term();
void gui_cancel_load();
void gui_refresh_files();
void gui_cheats();
void gui_keyboard_input(u16 wc);
void gui_keyboard_inputUTF8(const std::string& s);
void gui_keyboard_key(u8 keyCode, bool pressed);
bool gui_keyboard_captured();
bool gui_mouse_captured();
void gui_set_mouse_position(int x, int y, bool touchscreen);
// 0: left, 1: right, 2: middle/wheel, 3: button 4
void gui_set_mouse_button(int button, bool pressed, bool touchscreen);
void gui_set_mouse_wheel(float delta);
void gui_set_insets(int left, int right, int top, int bottom);
void gui_stop_game(const std::string& message = "");
void gui_start_game(const std::string& path);
void gui_error(const std::string& what);
void gui_setOnScreenKeyboardCallback(void (*callback)(bool show));
void gui_loadState();
void gui_saveState(bool stopRestart = true);
std::string gui_getCurGameBoxartUrl();
void gui_takeScreenshot();
void gui_runOnUiThread(std::function<void()> function);

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
	Cheats,
	Achievements,
};
extern GuiState gui_state;

void gui_setState(GuiState newState);

static inline bool gui_is_open()
{
	return gui_state != GuiState::Closed;
}
static inline bool gui_is_content_browser()
{
	return gui_state == GuiState::Main;
}
