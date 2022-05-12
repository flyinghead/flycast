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
#ifdef __ANDROID__

#include "gui_android.h"
#include "gui.h"

#include "types.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "gui_util.h"

void vjoy_reset_editing();
void vjoy_stop_editing(bool canceled);

void gui_display_vjoy_commands()
{
    centerNextWindow();

    ImGui::Begin("Virtual Joystick", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);

	if (ImGui::Button("Save", ScaledVec2(150, 50)))
	{
		vjoy_stop_editing(false);
		gui_state = GuiState::Settings;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset", ScaledVec2(150, 50)))
	{
		vjoy_reset_editing();
		gui_state = GuiState::VJoyEdit;
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel", ScaledVec2(150, 50)))
	{
		vjoy_stop_editing(true);
		gui_state = GuiState::Settings;
	}
    ImGui::End();
}

#endif // __ANDROID__
