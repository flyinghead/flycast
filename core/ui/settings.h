/*
	Copyright 2025 flyinghead

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
#pragma once
#include "game_scanner.h"
#include "gui_util.h"
#include "imgui.h"
#include "oslib/i18n.h"
#include <array>
using namespace i18n;

void gui_display_settings();
void gui_settings_general();
void applyCurrentTheme();
void addContentPath(bool start);
void gui_settings_controls(std::array<bool, 4>& mapleDevicesChanges, std::array<std::array<bool, 2>, 4>& expDevicesChanges);
void gui_settings_video();
void gui_settings_audio();
void gui_settings_network();
void gui_settings_about();
void reset_vmus();
void error_popup();

extern GameScanner scanner;
extern bool uiUserScaleUpdated;
extern bool game_started;
extern int insetLeft, insetRight, insetTop, insetBottom;

inline static void header(const char *title)
{
	ImguiStyleVar _(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
	ImguiStyleVar _1(ImGuiStyleVar_DisabledAlpha, 1.0f);
	ImGui::BeginDisabled();
	ImGui::ButtonEx(title, ImVec2(-1, 0));
	ImGui::EndDisabled();
}

// ImGui InputText char filter for valid DNS names and IPv4/IPv6 addresses
inline static int dnsCharFilter(ImGuiInputTextCallbackData *data)
{
	ImWchar c = data->EventChar;
	bool good = c == '.' || c == '-' || c == ':' || (c >= '0' && c <= '9')
			|| (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
	return static_cast<int>(!good);
}
