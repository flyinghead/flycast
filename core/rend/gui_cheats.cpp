/*
	Copyright 2021 flyinghead

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
#include "gui.h"
#include "imgui/imgui.h"
#include "gui_util.h"
#include "cheats.h"

static bool addingCheat;

static void addCheat()
{
	static char cheatName[64];
	static char cheatCode[128];
    centerNextWindow();
    ImGui::SetNextWindowSize(ImVec2(std::min(ImGui::GetIO().DisplaySize.x, 600 * scaling), std::min(ImGui::GetIO().DisplaySize.y, 400 * scaling)));

    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 8 * scaling));		// from 8, 4
    ImGui::AlignTextToFramePadding();
    ImGui::Indent(10 * scaling);
    ImGui::Text("ADD CHEAT");

	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Cancel").x - ImGui::GetStyle().FramePadding.x * 4.f
    	- ImGui::CalcTextSize("OK").x - ImGui::GetStyle().ItemSpacing.x);
	if (ImGui::Button("Cancel"))
		addingCheat = false;
	ImGui::SameLine();
	if (ImGui::Button("OK"))
	{
		try {
			cheatManager.addGameSharkCheat(cheatName, cheatCode);
			addingCheat = false;
			cheatName[0] = 0;
			cheatCode[0] = 0;
		} catch (const FlycastException& e) {
			gui_error(e.what());
		}
	}

    ImGui::Unindent(10 * scaling);
    ImGui::PopStyleVar();

	ImGui::BeginChild(ImGui::GetID("input"), ImVec2(0, 0), true);
    {
		ImGui::InputText("Name", cheatName, sizeof(cheatName), 0, nullptr, nullptr);
		ImGui::InputTextMultiline("Code", cheatCode, sizeof(cheatCode), ImVec2(0, ImGui::GetTextLineHeight() * 8), 0, nullptr, nullptr);
    }
	ImGui::EndChild();
	ImGui::End();
}

void gui_cheats()
{
	if (addingCheat)
	{
		addCheat();
		return;
	}
    centerNextWindow();
    ImGui::SetNextWindowSize(ImVec2(std::min(ImGui::GetIO().DisplaySize.x, 600 * scaling), std::min(ImGui::GetIO().DisplaySize.y, 400 * scaling)));

    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20 * scaling, 8 * scaling));		// from 8, 4
    ImGui::AlignTextToFramePadding();
    ImGui::Indent(10 * scaling);
    ImGui::Text("CHEATS");

	ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Add").x  - ImGui::CalcTextSize("Close").x - ImGui::GetStyle().FramePadding.x * 6.f
    	- ImGui::CalcTextSize("Load").x - ImGui::GetStyle().ItemSpacing.x * 2);
	if (ImGui::Button("Add"))
		addingCheat = true;
	ImGui::SameLine();
	if (ImGui::Button("Load"))
    	ImGui::OpenPopup("Select cheat file");
	select_file_popup("Select cheat file", [](bool cancelled, std::string selection)
		{
			if (!cancelled)
				cheatManager.loadCheatFile(selection);
			return true;
		}, true, "cht");

	ImGui::SameLine();
	if (ImGui::Button("Close"))
		gui_state = GuiState::Commands;

    ImGui::Unindent(10 * scaling);
    ImGui::PopStyleVar();

	ImGui::BeginChild(ImGui::GetID("cheats"), ImVec2(0, 0), true);
    {
		if (cheatManager.cheatCount() == 0)
			ImGui::Text("(No cheat loaded)");
		else
			for (size_t i = 0; i < cheatManager.cheatCount(); i++)
			{
				ImGui::PushID(("cheat" + std::to_string(i)).c_str());
				bool v = cheatManager.cheatEnabled(i);
				if (ImGui::Checkbox(cheatManager.cheatDescription(i).c_str(), &v))
					cheatManager.enableCheat(i, v);
				ImGui::PopID();
			}
    }
	ImGui::EndChild();
	ImGui::End();
}
