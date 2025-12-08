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
#include "network/ggpo.h"
#include "network/ice.h"
#include "imgui_stdlib.h"

void gui_settings_network()
{
	ImGuiStyle& style = ImGui::GetStyle();
	header(T("Network Type"));
	{
		DisabledScope scope(game_started);

		int netType = 0;
		if (config::GGPOEnable)
			netType = 1;
		else if (config::NetworkEnable)
			netType = 2;
		else if (config::BattleCableEnable)
			netType = 3;
		ImGui::Columns(4, "networkType", false);
		ImGui::RadioButton((Ts("Disabled") + "##network").c_str(), &netType, 0);
		ImGui::NextColumn();
		ImGui::RadioButton("GGPO", &netType, 1);
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ShowHelpMarker(T("Enable networking using GGPO"));
		ImGui::NextColumn();
		ImGui::RadioButton("Naomi", &netType, 2);
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ShowHelpMarker(T("Enable networking for supported Naomi and Atomiswave games"));
		ImGui::NextColumn();
		ImGui::RadioButton(T("Battle Cable"), &netType, 3);
		ImGui::SameLine(0, style.ItemInnerSpacing.x);
		ShowHelpMarker(T("Emulate the Taisen (Battle) null modem cable for games that support it"));
		ImGui::Columns(1, nullptr, false);

		config::GGPOEnable = false;
		config::NetworkEnable = false;
		config::BattleCableEnable = false;
		switch (netType) {
		case 1:
			config::GGPOEnable = true;
			break;
		case 2:
			config::NetworkEnable = true;
			break;
		case 3:
			config::BattleCableEnable = true;
			break;
		}
	}
	if (config::GGPOEnable || config::NetworkEnable || config::BattleCableEnable) {
		ImGui::Spacing();
		header(T("Configuration"));
	}
	{
		if (config::GGPOEnable)
		{
			config::NetworkEnable = false;
			OptionCheckbox(T("Play as Player 1"), config::ActAsServer,
					T("Deselect to play as player 2"));
			InputText(T("Peer"), &config::NetworkServer.get(), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, dnsCharFilter);
			ImGui::SameLine();
			ShowHelpMarker(T("Your peer IP address and optional port"));
			OptionSlider(T("Frame Delay"), config::GGPODelay, 0, 20,
					T("Sets Frame Delay, advisable for sessions with ping >100 ms"));

			ImGui::Text("%s", T("Left Thumbstick:"));
			OptionRadioButton<int>((Ts("Disabled") + "##analogaxis").c_str(), config::GGPOAnalogAxes, 0, T("Left thumbstick not used"));
			ImGui::SameLine();
			OptionRadioButton<int>(T("Horizontal"), config::GGPOAnalogAxes, 1, T("Use the left thumbstick horizontal axis only"));
			ImGui::SameLine();
			OptionRadioButton<int>(T("Full"), config::GGPOAnalogAxes, 2, T("Use the left thumbstick horizontal and vertical axes"));

			OptionCheckbox(T("Enable Chat"), config::GGPOChat, T("Open the chat window when a chat message is received"));
			if (config::GGPOChat)
			{
				OptionCheckbox(T("Enable Chat Window Timeout"), config::GGPOChatTimeoutToggle, T("Automatically close chat window after 20 seconds"));
				if (config::GGPOChatTimeoutToggle)
				{
					char chatTimeout[256];
					snprintf(chatTimeout, sizeof(chatTimeout), "%d", (int)config::GGPOChatTimeout);
					InputText(T("Chat Window Timeout (seconds)"), chatTimeout, sizeof(chatTimeout), ImGuiInputTextFlags_CharsDecimal);
					ImGui::SameLine();
					ShowHelpMarker(T("Sets duration that chat window stays open after new message is received."));
					config::GGPOChatTimeout.set(atoi(chatTimeout));
				}
			}
			OptionCheckbox(T("Network Statistics"), config::NetworkStats,
					T("Display network statistics on screen"));
		}
		else if (config::NetworkEnable)
		{
			OptionCheckbox(T("Act as Server"), config::ActAsServer,
					T("Create a local server for Naomi network games"));
			if (!config::ActAsServer)
			{
				InputText(T("Server"), &config::NetworkServer.get(), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, dnsCharFilter);
				ImGui::SameLine();
				ShowHelpMarker(T("The server to connect to. Leave blank to find a server automatically on the default port"));
			}
			char localPort[256];
			snprintf(localPort, sizeof(localPort), "%d", (int)config::LocalPort);
			InputText(T("Local Port"), localPort, sizeof(localPort), ImGuiInputTextFlags_CharsDecimal);
			ImGui::SameLine();
			ShowHelpMarker(T("The local UDP port to use"));
			config::LocalPort.set(atoi(localPort));
		}
		else if (config::BattleCableEnable)
		{
#ifdef USE_ICE
		    if (ImGui::BeginTabBar("battleMode", ImGuiTabBarFlags_NoTooltip))
		    {
				if (ImGui::BeginTabItem(T("Match Code")))
				{
					ice::State state = ice::getState();
					ImGuiInputTextFlags textFlags = state == ice::Offline ? ImGuiInputTextFlags_CharsNoBlank : ImGuiInputTextFlags_ReadOnly;
					static std::string matchCode;
					InputText(T("Code"), &matchCode, textFlags);
					ImGui::SameLine();
					ShowHelpMarker(T("Choose a unique word or number and share it with your opponent."));
					if (state == ice::Offline) {
						if (ImGui::Button(T("Connect")) && !matchCode.empty())
							ice::init(matchCode, true);
					}
					else {
						if (ImGui::Button(T("Disconnect")))
							try { ice::term(); } catch (...) {}
					}
					std::string status;
					switch (state)
					{
					case ice::Offline:
						status = ice::getStatusText();
						break;
					case ice::Online:
						status = T("Waiting at meeting point...");
						break;
					case ice::ChalAccepted:
						status = T("Preparing game...");
						break;
					case ice::Playing:
						status = strprintf(T("Playing %s (%s)"), matchCode.c_str(), ice::getStatusText().c_str());
						break;
					default:
						break;
					}
					ImGui::TextDisabled("%s", status.c_str());
					OptionCheckbox(T("Network Statistics"), config::NetworkStats,
							T("Display network statistics on screen"));
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem("Manual"))
				{
#endif
					InputText(T("Peer"), &config::NetworkServer.get(), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, dnsCharFilter);
					ImGui::SameLine();
					ShowHelpMarker(T("The peer to connect to. Leave blank to find a player automatically on the default port"));
					char localPort[256];
					snprintf(localPort, sizeof(localPort), "%d", (int)config::LocalPort);
					InputText(T("Local Port"), localPort, sizeof(localPort), ImGuiInputTextFlags_CharsDecimal);
					ImGui::SameLine();
					ShowHelpMarker(T("The local UDP port to use"));
					config::LocalPort.set(atoi(localPort));
#ifdef USE_ICE
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
		    }
#endif
			OptionCheckbox(T("Act as Master"), config::ActAsServer,
					T("Only used for Maximum Speed. One of the peer must be master."));
		}
	}
	ImGui::Spacing();
	header(T("Network Options"));
	{
		OptionCheckbox(T("Enable UPnP"), config::EnableUPnP, T("Automatically configure your network router for netplay"));
		OptionCheckbox(T("Broadcast Digital Outputs"), config::NetworkOutput, T("Broadcast digital outputs and force-feedback state on TCP port 8000. "
				"Compatible with the \"-output network\" MAME option. Arcade games only."));
		{
			DisabledScope scope(game_started);

			OptionCheckbox(T("Broadband Adapter Emulation"), config::EmulateBBA,
					T("Emulate the Ethernet Broadband Adapter (BBA) instead of the Modem"));
		}
		OptionCheckbox(T("Use DCNet"), config::UseDCNet, T("Use the DCNet cloud service for Dreamcast Internet access."));

		std::string& ispUsername = config::ISPUsername;
		InputText(T("ISP User Name"), &ispUsername, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter,
				[](ImGuiInputTextCallbackData *data) { return static_cast<int>(data->EventChar <= ' ' || data->EventChar > '~'); });
		// CallbackCharFilter isn't called on switch
		auto it = std::remove_if(ispUsername.begin(), ispUsername.end(), [](char c) { return c <= ' ' || c > '~'; });
		ispUsername.erase(it, ispUsername.end());
		ImGui::SameLine();
		ShowHelpMarker(T("The ISP user name stored in the console Flash RAM. Used by some online games as the player name. Leave blank to keep the current Flash RAM value."));
#if !defined(NDEBUG) || defined(DEBUGFAST)
		{
			DisabledScope scope(config::UseDCNet);
			InputText("DNS", &config::DNS.get(), ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CallbackCharFilter, dnsCharFilter);
			ImGui::SameLine();
			ShowHelpMarker("DNS server name or IP address");
		}
#endif
	}
#ifdef NAOMI_MULTIBOARD
	ImGui::Spacing();
	header(T("Multiboard Screens"));
	{
		//OptionRadioButton<int>(T("Disabled##multiboard"), config::MultiboardSlaves, 0, T("Multiboard disabled (when optional)"));
		OptionRadioButton<int>(T("1 (Twin)"), config::MultiboardSlaves, 1, T("One screen configuration (F355 Twin)"));
		ImGui::SameLine();
		OptionRadioButton<int>(T("3 (Deluxe)"), config::MultiboardSlaves, 2, T("Three screens configuration"));
	}
#endif
}
