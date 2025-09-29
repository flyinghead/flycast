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
#include "imgui.h"
#include "gui_util.h"
#include "cheats.h"
#include "IconsFontAwesome6.h"
#include "oslib/http_client.h"
#include "emulator.h"
#include "stdclass.h"
#ifdef __ANDROID__
#include "oslib/storage.h"
#endif
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static void addCheat()
{
	static char cheatName[64];
	static char cheatCode[128];
    centerNextWindow();
    ImGui::SetNextWindowSize(min(ImGui::GetIO().DisplaySize, ScaledVec2(600.f, 400.f)));
    ImguiStyleVar _(ImGuiStyleVar_WindowBorderSize, 1);

    if (ImGui::BeginPopupModal("addCheat", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize))
    {
    	{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
			ImGui::AlignTextToFramePadding();
			ImGui::Indent(uiScaled(10));
			ImGui::Text("ADD CHEAT");

			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Cancel").x - ImGui::GetStyle().FramePadding.x * 4.f
				- ImGui::CalcTextSize("OK").x - ImGui::GetStyle().ItemSpacing.x);
			if (ImGui::Button("Cancel"))
				ImGui::CloseCurrentPopup();
			ImGui::SameLine();
			if (ImGui::Button("OK"))
			{
				try {
					cheatManager.addGameSharkCheat(cheatName, cheatCode);
					ImGui::CloseCurrentPopup();
					cheatName[0] = 0;
					cheatCode[0] = 0;
				} catch (const FlycastException& e) {
					gui_error(e.what());
				}
			}

			ImGui::Unindent(uiScaled(10));
		}

		ImGui::BeginChild(ImGui::GetID("input"), ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_NavFlattened);
		{
			ImGui::InputText("Name", cheatName, sizeof(cheatName), 0, nullptr, nullptr);
			ImGui::InputTextMultiline("Code", cheatCode, sizeof(cheatCode), ImVec2(0, ImGui::GetTextLineHeight() * 8), 0, nullptr, nullptr);
		}
		ImGui::EndChild();
		ImGui::EndPopup();
    }
}

static void cheatFileSelected(bool cancelled, std::string path)
{
	if (!cancelled)
		gui_runOnUiThread([path]() {
			cheatManager.loadCheatFile(path);
		});
}

static void downloadCheats()
{
	// Check if a game is running and we have a game ID
	if (settings.content.gameId.empty())
	{
		gui_error("ERROR: No game is currently running.");
		return;
	}

	// Initialize HTTP client if needed
	static bool httpInitialized = false;
	if (!httpInitialized)
	{
		http::init();
		httpInitialized = true;
	}

	// Construct the filename to look for based on ROM ID
	std::string cheatFileName = settings.content.gameId + ".cht";
	
	// Direct URL to download the cheat file from S3-compatible storage
	std::string downloadUrl = "https://flycast-cheats.s3.fr-par.scw.cloud/" + cheatFileName;
	
	// Show a status message
	INFO_LOG(COMMON, "Checking for cheats for %s...", settings.content.gameId.c_str());
	gui_error("Cheat DB: Checking for cheats...");
	
	try
	{
		// Download the file with a direct GET request
		std::vector<u8> fileContent;
		std::string contentType;
		int status = http::get(downloadUrl, fileContent, contentType);
		
		if (http::success(status) && !fileContent.empty())
		{
			INFO_LOG(COMMON, "Successfully downloaded cheat file, size: %d bytes", (int)fileContent.size());
			
			// Save the downloaded cheat file directly to data folder
			std::string cheatFilePath = get_writable_data_path(cheatFileName);
			
			FILE *fp = nowide::fopen(cheatFilePath.c_str(), "wb");
			if (fp == nullptr)
			{
				gui_error("ERROR: Failed to save cheat file: " + cheatFilePath);
				return;
			}
			
			fwrite(fileContent.data(), 1, fileContent.size(), fp);
			fclose(fp);
			
			// Load the cheat file
			cheatManager.loadCheatFile(cheatFilePath);
			
			gui_error("SUCCESS: Cheats downloaded and imported successfully!");
		}
		else
		{
			INFO_LOG(COMMON, "Failed to download cheat file, status: %d", status);
			if (status == 404)
			{
				gui_error("ERROR: No cheats available for " + settings.content.gameId);
			}
			else
			{
				gui_error("ERROR: Failed to download. Please try again later.");
			}
		}
	}
	catch (const std::exception& e)
	{
		INFO_LOG(COMMON, "Error downloading cheats: %s", e.what());
		gui_error("ERROR: " + std::string(e.what()));
	}
	catch (...)
	{
		INFO_LOG(COMMON, "Unknown error downloading cheats");
		gui_error("ERROR: Unknown error occurred.");
	}
}

// Check for ROM ID based cheat files when a game starts
static void checkRomIdCheatsCallback(Event event, void *)
{
	if (settings.content.gameId.empty())
		return;

	// Check if a ROM ID based cheat file exists in the data folder
	std::string romIdCheatPath = get_writable_data_path(settings.content.gameId + ".cht");
	if (file_exists(romIdCheatPath))
	{
		// Load the ROM ID based cheat file
		cheatManager.loadCheatFile(romIdCheatPath);
		INFO_LOG(COMMON, "Loaded ROM ID based cheat file: %s", romIdCheatPath.c_str());
	}
}

// Register the event listener for when games start
static struct RomIdCheatInitializer {
	RomIdCheatInitializer() {
		EventManager::listen(Event::Start, checkRomIdCheatsCallback);
	}
} romIdCheatInitializer;

void gui_cheats()
{
	fullScreenWindow(false);
	ImguiStyleVar _(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::Begin("##main", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);

	const char *title = "Select a cheat file";
    {
		ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
		ImGui::AlignTextToFramePadding();
		ImGui::Indent(uiScaled(10));
		ImGui::Text(ICON_FA_MASK "  CHEATS");

		ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Add").x - ImGui::CalcTextSize("Download").x - ImGui::CalcTextSize("Close").x - ImGui::GetStyle().FramePadding.x * 8.f
			- ImGui::CalcTextSize("Load").x - ImGui::GetStyle().ItemSpacing.x * 3);
		if (ImGui::Button("Add"))
			ImGui::OpenPopup("addCheat");
		addCheat();
		ImGui::SameLine();
#ifdef __ANDROID__
		if (ImGui::Button("Load"))
		{
			if (!hostfs::addStorage(false, true, title, cheatFileSelected))
				ImGui::OpenPopup(title);
		}
#else
		if (ImGui::Button("Load"))
			ImGui::OpenPopup(title);
#endif

		ImGui::SameLine();
		if (ImGui::Button("Download"))
			downloadCheats();

		ImGui::SameLine();
		if (ImGui::Button("Close"))
			gui_setState(GuiState::Commands);

		ImGui::Unindent(uiScaled(10));
    }
	select_file_popup(title, [](bool cancelled, std::string selection)
		{
			cheatFileSelected(cancelled, selection);
			return true;
		}, true, "cht");

	ImGui::BeginChild(ImGui::GetID("cheats"), ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NavFlattened);
    {
		if (cheatManager.cheatCount() == 0)
			ImGui::Text("(No cheat loaded)");
		else
			for (size_t i = 0; i < cheatManager.cheatCount(); i++)
			{
				ImguiID _(("cheat" + std::to_string(i)).c_str());
				bool v = cheatManager.cheatEnabled(i);
				if (ImGui::Checkbox(cheatManager.cheatDescription(i).c_str(), &v))
					cheatManager.enableCheat(i, v);
			}
    }
    scrollWhenDraggingOnVoid();
    windowDragScroll();

	ImGui::EndChild();
	ImGui::End();
}
