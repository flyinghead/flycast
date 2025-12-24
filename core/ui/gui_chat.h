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
#pragma once
#include "types.h"
#include "gui.h"
#include "imgui.h"
#include "network/ggpo.h"
#include "oslib/i18n.h"
#include <chrono>

class Chat
{
	std::vector<std::pair<ImVec4, std::string>> lines;
	bool visible = false;
	bool newMessage = false;
	bool focus = false;
	std::string localPlayerName;
	std::string remotePlayerName;

	const ImVec4 WHITE { 1, 1, 1, 1 };
	const ImVec4 YELLOW { 1, 1, 0, 1 };

	bool manual_open = false;
	bool enable_timeout = false;
	std::chrono::steady_clock::time_point launch_time;

	std::string playerName(bool remote)
	{
		if (remote)
			return !remotePlayerName.empty() ? remotePlayerName : config::ActAsServer ? i18n::T("P2") : i18n::T("P1");
		else
			return !localPlayerName.empty() ? localPlayerName : config::ActAsServer ? i18n::T("P1") : i18n::T("P2");
	}

public:
	void toggle_timeout()
	{
		if (config::GGPOChatTimeoutToggle && !manual_open)
		{
			enable_timeout = true;
			launch_time = std::chrono::steady_clock::now();
		}
	}

	void reset()
	{
		visible = false;
		lines.clear();
	}

	void display()
	{
		auto timeout = std::chrono::seconds(config::GGPOChatTimeout.get());

		if (enable_timeout &&
			std::chrono::steady_clock::now() - launch_time > timeout)
		{
			visible = false;
			enable_timeout = false;
			manual_open = false;
		}

		if (!visible)
			return;

		ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
		ImguiStyleVar _1(ImGuiStyleVar_WindowBorderSize, 0);
		ImGui::SetNextWindowPos(ImVec2(settings.display.width / 2, settings.display.height) - ScaledVec2(200.f, 220.f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ScaledVec2(400, 220), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowBgAlpha(0.7f);
		ImGui::SetNextWindowFocus();
		if (ImGui::Begin("Chat", &visible, ImGuiWindowFlags_NoScrollbar))
		{
			ImGui::BeginChild(ImGui::GetID("log"), ImVec2(0, -ImGui::GetStyle().ItemSpacing.x - ImGui::GetFontSize() - ImGui::GetStyle().FramePadding.x * 2),
					ImGuiChildFlags_Borders, ImGuiWindowFlags_DragScrolling);
			ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
			for (const auto& p : lines)
				ImGui::TextColored(p.first, "%s", p.second.c_str());
			ImGui::PopTextWrapPos();
			if (newMessage)
			{
				newMessage = false;
				ImGui::SetScrollHereY(1.f);
			}
			ImGui::EndChild();
			static char buf[512];
			ImGui::SetNextItemWidth(-0.001f);
			if (InputText("##input", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if (buf[0] != '\0')
				{
					ggpo::sendChatMessage(config::ActAsServer ? 0 : 1, buf);
					std::string line = "<" + playerName(false) + "> " + std::string(buf);
					lines.push_back(std::make_pair(WHITE, line));
					buf[0] = '\0';
					newMessage = true;
					enable_timeout = false;
				}
				ImGui::SetKeyboardFocusHere(-1);
			}
			else if (focus)
			{
				ImGui::SetKeyboardFocusHere(-1);
				focus = false;
			}
			ImGui::SetItemDefaultFocus();
		}
		ImGui::End();
	}

	void receive(int playerNum, const std::string& msg)
	{
		if (config::GGPOChat && !visible)
		{
			visible = true;
			toggle_timeout();
		}
		std::string line = "<" + playerName(true) + "> " + msg;
		lines.push_back(std::make_pair(YELLOW, line));
		newMessage = true;
	}

	void toggle(bool manual = false)
	{
		visible = !visible;
		focus = visible;
		if (manual)
			manual_open = manual;
	}

	void setLocalPlayerName(const std::string& name) {
		localPlayerName = name;
	}

	void setRemotePlayerName(const std::string& name) {
		remotePlayerName = name;
	}
};
