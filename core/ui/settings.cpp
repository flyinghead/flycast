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
#include "settings.h"
#include "gui.h"
#include "IconsFontAwesome6.h"
#include "mainui.h"
#include "log/LogManager.h"
#include "hw/maple/maple_if.h"
#include "imgui_stdlib.h"

#ifdef GDB_SERVER
#include "hw/mem/addrspace.h"
#endif

#if defined(USE_DREAMLINK_DEVICES)
#include "sdl/dreamlink.h"
#endif

static void gui_settings_advanced()
{
#if FEAT_SHREC != DYNAREC_NONE
    header("CPU Mode");
    {
		ImGui::Columns(2, "cpu_modes", false);
		OptionRadioButton("Dynarec", config::DynarecEnabled, true,
			"Use the dynamic recompiler. Recommended in most cases");
		ImGui::NextColumn();
		OptionRadioButton("Interpreter", config::DynarecEnabled, false,
			"Use the interpreter. Very slow but may help in case of a dynarec problem");
		ImGui::Columns(1, NULL, false);

		OptionSlider("SH4 Clock", config::Sh4Clock, 100, 300,
				"Over/Underclock the main SH4 CPU. Default is 200 MHz. Other values may crash, freeze or trigger unexpected nuclear reactions.",
				"%d MHz");
    }
#ifdef GDB_SERVER
	ImGui::Spacing();
	header("Virtual memory addresses");
	{
		void *ram_base, *ram, *vram, *aram;
		addrspace::getAddress(&ram_base, &ram, &vram, &aram);

		ImGui::Text("Base Address: %p", ram_base);

		if (ram == nullptr) {
			const ImVec4 gray(0.75f, 0.75f, 0.75f, 1.f);
			ImGui::TextColored(gray, "RAM adresses are not available until the emulation is started");
		} else {
			ImGui::Columns(3, "virtualMemoryAddress", false);
			ImGui::Text("RAM: %p", ram);
			ImGui::NextColumn();
			ImGui::Text("VRAM64: %p", vram);
			ImGui::NextColumn();
			ImGui::Text("ARAM: %p", aram);
			ImGui::Columns(1, nullptr, false);
		}

	}
	ImGui::Spacing();
	header("Debugging");
	{
		OptionCheckbox("Enable GDB", config::GDB, "GDB debugging support, disables Dynarec and dramatically reduces performance when a debugger is connected.");
		OptionCheckbox("Wait for connection", config::GDBWaitForConnection, "Start emulation once the debugger is connected.");
#ifndef __ANDROID
		OptionCheckbox("Serial Console", config::SerialConsole, "Dump the Dreamcast serial console to stdout");
		OptionCheckbox("Serial PTY", config::SerialPTY, "Requires the option \"Serial Console\" to work");
#endif

		static int gdbport = config::GDBPort;
		if (ImGui::InputInt("GDB port", &gdbport))
		{
			config::GDBPort = gdbport;
		}
		const ImGuiStyle& style = ImGui::GetStyle();
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ShowHelpMarker("Default port is 3263");
	}
#endif
	ImGui::Spacing();
#endif
    header("Other");
    {
    	OptionCheckbox("HLE BIOS", config::UseReios, "Force high-level BIOS emulation");
        OptionCheckbox("Multi-threaded emulation", config::ThreadedRendering,
        		"Run the emulated CPU and GPU on different threads");
#if !defined(__ANDROID) && !defined(GDB_SERVER)
        OptionCheckbox("Serial Console", config::SerialConsole,
        		"Dump the Dreamcast serial console to stdout");
#endif
		{
			DisabledScope scope(game_started);
			OptionCheckbox("Dreamcast 32MB RAM Mod", config::RamMod32MB,
				"Enables 32MB RAM Mod for Dreamcast. May affect compatibility");
		}
        OptionCheckbox("Dump Textures", config::DumpTextures,
        		"Dump all textures into data/texdump/<game id>");
        bool logToFile = cfgLoadBool("log", "LogToFile", false);
		if (ImGui::Checkbox("Log to File", &logToFile))
			cfgSaveBool("log", "LogToFile", logToFile);
        ImGui::SameLine();
        ShowHelpMarker("Log debug information to flycast.log");
#ifdef SENTRY_UPLOAD
        OptionCheckbox("Automatically Report Crashes", config::UploadCrashLogs,
        		"Automatically upload crash reports to sentry.io to help in troubleshooting. No personal information is included.");
#endif
    }

#ifdef USE_LUA
	header("Lua Scripting");
	{
		ImGui::InputText("Lua Filename", &config::LuaFileName.get(), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
		ImGui::SameLine();
		ShowHelpMarker("Specify lua filename to use. Should be located in Flycast config folder. Defaults to flycast.lua when empty.");
	}
#endif
}

#if !defined(NDEBUG) || defined(DEBUGFAST) || FC_PROFILER

static void gui_debug_tab()
{
	header("Logging");
	{
		LogManager *logManager = LogManager::GetInstance();
		for (LogTypes::LOG_TYPE type = LogTypes::AICA; type < LogTypes::NUMBER_OF_LOGS; type = (LogTypes::LOG_TYPE)(type + 1))
		{
			bool enabled = logManager->IsEnabled(type, logManager->GetLogLevel());
			std::string name = std::string(logManager->GetShortName(type)) + " - " + logManager->GetFullName(type);
			if (ImGui::Checkbox(name.c_str(), &enabled) && logManager->GetLogLevel() > LogTypes::LWARNING) {
				logManager->SetEnable(type, enabled);
				cfgSaveBool("log", logManager->GetShortName(type), enabled);
			}
		}
		ImGui::Spacing();

		static const char *levels[] = { "Notice", "Error", "Warning", "Info", "Debug" };
		if (ImGui::BeginCombo("Log Verbosity", levels[logManager->GetLogLevel() - 1], ImGuiComboFlags_None))
		{
			for (std::size_t i = 0; i < std::size(levels); i++)
			{
				bool is_selected = logManager->GetLogLevel() - 1 == (int)i;
				if (ImGui::Selectable(levels[i], &is_selected)) {
					logManager->SetLogLevel((LogTypes::LOG_LEVELS)(i + 1));
					cfgSaveInt("log", "Verbosity", i + 1);
				}
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
		ImGui::InputText("Log Server", &config::LogServer.get(), ImGuiInputTextFlags_CharsNoBlank, nullptr, nullptr);
        ImGui::SameLine();
        ShowHelpMarker("Log to this hostname[:port] with UDP. Default port is 31667.");
	}
#if FC_PROFILER
	ImGui::Spacing();
	header("Profiling");
	{

		OptionCheckbox("Enable", config::ProfilerEnabled, "Enable the profiler.");
		if (!config::ProfilerEnabled)
		{
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
		OptionCheckbox("Display", config::ProfilerDrawToGUI, "Draw the profiler output in an overlay.");
		OptionCheckbox("Output to terminal", config::ProfilerOutputTTY, "Write the profiler output to the terminal");
		// TODO frame warning time
		if (!config::ProfilerEnabled)
		{
			ImGui::PopItemFlag();
			ImGui::PopStyleVar();
		}
	}
#endif
}
#endif

void gui_display_settings()
{
	static bool maple_devices_changed;

	fullScreenWindow(false);
	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);

    ImGui::Begin("Settings", NULL, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NoResize
    		| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
	ImVec2 normal_padding = ImGui::GetStyle().FramePadding;

    if (ImGui::Button("Done", ScaledVec2(100, 30)))
    {
    	if (uiUserScaleUpdated)
    	{
    		uiUserScaleUpdated = false;
    		mainui_reinit();
    	}
    	if (game_started)
    		gui_setState(GuiState::Commands);
    	else
    		gui_setState(GuiState::Main);
    	if (maple_devices_changed)
    	{
    		maple_devices_changed = false;
    		if (game_started && settings.platform.isConsole())
    		{
#if defined(USE_DREAMLINK_DEVICES)
				reconnectDreamLinks();
#endif
    			maple_ReconnectDevices();
    			reset_vmus();
    		}
    	}
       	SaveSettings();
    }
	if (game_started)
	{
	    ImGui::SameLine();
		ImguiStyleVar _(ImGuiStyleVar_FramePadding, ImVec2(uiScaled(16), normal_padding.y));
		if (config::Settings::instance().hasPerGameConfig())
		{
			if (ImGui::Button("Delete Game Config", ScaledVec2(0, 30)))
			{
				config::Settings::instance().setPerGameConfig(false);
				config::Settings::instance().load(false);
				loadGameSpecificSettings();
			}
		}
		else
		{
			if (ImGui::Button("Make Game Config", ScaledVec2(0, 30)))
				config::Settings::instance().setPerGameConfig(true);
		}
	}

	if (ImGui::GetContentRegionAvail().x >= uiScaled(650.f))
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(16, 6));
	else
		// low width
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ScaledVec2(4, 6));

	// Track selected tab and handle bumper navigation
	static int selectedTab = 0;
	const bool l1Pressed = ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false);
	const bool r1Pressed = ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false);
	
	// Count total tabs (adjust if debug tab is not present)
	int totalTabs = 8;
#if defined(NDEBUG) && !defined(DEBUGFAST) && !FC_PROFILER
	totalTabs = 7;
#endif
	
	// Bumpers: left = previous tab, right = next tab
	if (l1Pressed)
	{
		selectedTab = (selectedTab - 1 + totalTabs) % totalTabs;
	}
	if (r1Pressed)
	{
		selectedTab = (selectedTab + 1) % totalTabs;
	}

	if (ImGui::BeginTabBar("settings", ImGuiTabBarFlags_NoTooltip))
	{
		int tabIndex = 0;

		bool open = ImGui::BeginTabItem(ICON_FA_TOOLBOX " General", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		bool focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_settings_general();
			ImGui::EndTabItem();
		}
		++tabIndex;

		open = ImGui::BeginTabItem(ICON_FA_GAMEPAD " Controls", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_settings_controls(maple_devices_changed);
			ImGui::EndTabItem();
		}
		++tabIndex;

		open = ImGui::BeginTabItem(ICON_FA_DISPLAY " Video", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_settings_video();
			ImGui::EndTabItem();
		}
		++tabIndex;

		open = ImGui::BeginTabItem(ICON_FA_MUSIC " Audio", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_settings_audio();
			ImGui::EndTabItem();
		}
		++tabIndex;

		open = ImGui::BeginTabItem(ICON_FA_WIFI " Network", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_settings_network();
			ImGui::EndTabItem();
		}
		++tabIndex;

		open = ImGui::BeginTabItem(ICON_FA_MICROCHIP " Advanced", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_settings_advanced();
			ImGui::EndTabItem();
		}
		++tabIndex;

#if !defined(NDEBUG) || defined(DEBUGFAST) || FC_PROFILER
		open = ImGui::BeginTabItem(ICON_FA_BUG " Debug", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_debug_tab();
			ImGui::EndTabItem();
		}
		++tabIndex;
#endif

		open = ImGui::BeginTabItem(ICON_FA_CIRCLE_INFO " About", nullptr,
			(selectedTab == tabIndex) ? ImGuiTabItemFlags_SetSelected : 0);
		focused = ImGui::IsItemFocused();
		if (focused)
			selectedTab = tabIndex;
		if (open)
		{
			ImguiStyleVar _(ImGuiStyleVar_FramePadding, normal_padding);
			gui_settings_about();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
    }
    ImGui::PopStyleVar();

    scrollWhenDraggingOnVoid();
    windowDragScroll();
    ImGui::End();
}

