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
#ifdef _ANDROID

#include "gui_android.h"
#include "gui.h"

#include "types.h"
#include "stdclass.h"
#include "imgui/imgui.h"
#include "gles/imgui_impl_opengl3.h"

extern bool settings_opening;

void vjoy_reset_editing();
void vjoy_stop_editing(bool canceled);

void gui_display_vjoy_commands(int screen_width, int screen_height, float scaling)
{
	ImGui_Impl_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(screen_width / 2.f, screen_height / 2.f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    ImGui::Begin("Virtual Joystick", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	if (ImGui::Button("Save", ImVec2(150 * scaling, 50 * scaling)))
	{
		vjoy_stop_editing(false);
		gui_state = Settings;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset", ImVec2(150 * scaling, 50 * scaling)))
	{
		vjoy_reset_editing();
		gui_state = VJoyEdit;
	}

	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(150 * scaling, 50 * scaling)))
	{
		vjoy_stop_editing(true);
		gui_state = Settings;
	}
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData(), false);
}

#endif // _ANDROID
