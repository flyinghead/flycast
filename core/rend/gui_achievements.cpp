/*
	Copyright 2024 flyinghead

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
#ifdef USE_RACHIEVEMENTS
#include "gui_achievements.h"
#include "gui.h"
#include "gui_util.h"
#include "imgui_driver.h"
#include "stdclass.h"
#include "achievements/achievements.h"
#include "IconsFontAwesome6.h"
#include <cmath>
#include <sstream>

extern ImFont *largeFont;

namespace achievements
{

Notification notifier;

static constexpr u64 DISPLAY_TIME = 5000;
static constexpr u64 START_ANIM_TIME = 500;
static constexpr u64 END_ANIM_TIME = 1000;
static constexpr u64 NEVER_ENDS = 1000000000000;

void Notification::notify(Type type, const std::string& image, const std::string& text1,
		const std::string& text2, const std::string& text3)
{
	std::lock_guard<std::mutex> _(mutex);
	u64 now = getTimeMs();
	if (type == Progress)
	{
		if (!text1.empty())
		{
			if (this->type == None)
			{
				// New progress
				startTime = now;
				endTime = NEVER_ENDS;
			}
		}
		else
		{
			// Hide progress
			endTime = now;
		}
	}
	else {
		startTime = now;
		endTime = startTime + DISPLAY_TIME;
	}
	this->type = type;
	this->image = { image };
	text[0] = text1;
	text[1] = text2;
	text[2] = text3;
}

void Notification::showChallenge(const std::string& image)
{
	std::lock_guard<std::mutex> _(mutex);
	ImguiTexture texture{ image };
	if (std::find(challenges.begin(), challenges.end(), texture) != challenges.end())
		return;
	challenges.push_back(texture);
	if (this->type == None)
	{
		this->type = Challenge;
		startTime = getTimeMs();
		endTime = NEVER_ENDS;
	}
}

void Notification::hideChallenge(const std::string& image)
{
	std::lock_guard<std::mutex> _(mutex);
	auto it = std::find(challenges.begin(), challenges.end(), image);
	if (it == challenges.end())
		return;
	challenges.erase(it);
	if (this->type == Challenge && challenges.empty())
		endTime = getTimeMs();
}

bool Notification::draw()
{
	std::lock_guard<std::mutex> _(mutex);
	if (type == None)
		return false;
	u64 now = getTimeMs();
	if (now > endTime + END_ANIM_TIME)
	{
		if (!challenges.empty())
		{
			// Show current challenge indicators
			type = Challenge;
			startTime = getTimeMs();
			endTime = NEVER_ENDS;
		}
		else
		{
			// Hide notification
			type = None;
			return false;
		}
	}
	if (now > endTime)
	{
		// Fade out
		float alpha = (std::cos((now - endTime) / (float)END_ANIM_TIME * (float)M_PI) + 1.f) / 2.f;
		ImGui::GetStyle().Alpha = alpha;
		ImGui::SetNextWindowBgAlpha(alpha / 2.f);
	}
	else {
		ImGui::SetNextWindowBgAlpha(0.5f);
	}
	float y = ImGui::GetIO().DisplaySize.y;
	if (now - startTime < START_ANIM_TIME)
		// Slide up
		y += 80.f * settings.display.uiScale * (std::cos((now - startTime) / (float)START_ANIM_TIME * (float)M_PI) + 1.f) / 2.f;

	ImGui::SetNextWindowPos(ImVec2(0, y), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner
	if (type == Challenge)
	{
		ImGui::Begin("##achievement", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
				| ImGuiWindowFlags_NoInputs);
		for (const auto& img : challenges)
		{
			img.draw(ScaledVec2(60.f, 60.f));
			ImGui::SameLine();
		}
		ImGui::End();
	}
	else
	{
		ImGui::SetNextWindowSizeConstraints(ScaledVec2(80.f, 80.f) + ImVec2(ImGui::GetStyle().WindowPadding.x * 2, 0.f), ImVec2(FLT_MAX, FLT_MAX));
		const float winPaddingX = ImGui::GetStyle().WindowPadding.x;
		ImguiStyleVar _(ImGuiStyleVar_WindowPadding, ImVec2{});

		ImGui::Begin("##achievement", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
				| ImGuiWindowFlags_NoInputs);
		ImTextureID imageId = image.getId();
		const bool hasPic = imageId != ImTextureID{};
		if (ImGui::BeginTable("achievementNotif", hasPic ? 2 : 1, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings))
		{
			if (hasPic)
				ImGui::TableSetupColumn("icon", ImGuiTableColumnFlags_WidthFixed);
			ImGui::TableSetupColumn("text", ImGuiTableColumnFlags_WidthStretch);

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			if (hasPic)
			{
				image.draw(ScaledVec2(80.f, 80.f));
				ImGui::TableSetColumnIndex(1);
			}

			float w = largeFont->CalcTextSizeA(largeFont->FontSize, FLT_MAX, -1.f, text[0].c_str()).x;
			w = std::max(w, ImGui::CalcTextSize(text[1].c_str()).x);
			w = std::max(w, ImGui::CalcTextSize(text[2].c_str()).x) + winPaddingX * 2;
			int lines = (int)!text[0].empty() + (int)!text[1].empty() + (int)!text[2].empty();
			ImguiStyleVar _(ImGuiStyleVar_WindowPadding, ImVec2{ hasPic ? 0.f : winPaddingX, (3 - lines) * ImGui::GetTextLineHeight() / 2 });
			if (ImGui::BeginChild("##text", ImVec2(w, 0), ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_None))
			{
				ImGui::PushFont(largeFont);
				ImGui::Text("%s", text[0].c_str());
				ImGui::PopFont();
				if (!text[1].empty())
					ImGui::TextColored(ImVec4(1, 1, 0, 0.7f), "%s", text[1].c_str());
				if (!text[2].empty())
					ImGui::TextColored(ImVec4(1, 1, 0, 0.7f), "%s", text[2].c_str());
			}
			ImGui::EndChild();
			ImGui::EndTable();
		}
		ImGui::End();
	}
	ImGui::GetStyle().Alpha = 1.f;

	return true;
}

void achievementList()
{
	fullScreenWindow(false);
	ImguiStyleVar _(ImGuiStyleVar_WindowBorderSize, 0);

	ImGui::Begin("##achievements", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);

	{
		float w = ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize("Close").x - ImGui::GetStyle().ItemSpacing.x * 2 - ImGui::GetStyle().WindowPadding.x
				- (80.f + 20.f * 2) * settings.display.uiScale;	// image width and button frame padding
		Game game = getCurrentGame();
		ImguiTexture tex(game.image);
		tex.draw(ScaledVec2(80.f, 80.f));
		ImGui::SameLine();
		ImGui::BeginChild("game_info", ImVec2(w, 80.f * settings.display.uiScale), ImGuiChildFlags_None, ImGuiWindowFlags_None);
		ImGui::PushFont(largeFont);
		ImGui::Text("%s", game.title.c_str());
		ImGui::PopFont();
		std::stringstream ss;
		ss << "You have unlocked " << game.unlockedAchievements << " of " << game.totalAchievements
				<< " achievements and " << game.points << " of " << game.totalPoints << " points.";
		{
			ImguiStyleColor _(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
			ImGui::TextWrapped("%s", ss.str().c_str());
		}
		if (settings.raHardcoreMode)
			ImGui::Text("Hardcore Mode");
		ImGui::EndChild();

		ImGui::SameLine();
		ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
		if (ImGui::Button("Close"))
			gui_setState(GuiState::Commands);
    }

	if (ImGui::BeginChild(ImGui::GetID("ach_list"), ImVec2(0, 0), ImGuiChildFlags_Border, ImGuiWindowFlags_DragScrolling | ImGuiWindowFlags_NavFlattened))
	{
		std::vector<Achievement> achList = getAchievementList();
		int id = 0;
		std::string category;
		for (const auto& ach : achList)
		{
			if (ach.category != category)
			{
				category = ach.category;
				ImGui::Indent(10 * settings.display.uiScale);
				if (category == "Locked" || category == "Active Challenges" || category == "Almost There")
					ImGui::Text(ICON_FA_LOCK);
				else if (category == "Unlocked" || category == "Recently Unlocked")
					ImGui::Text(ICON_FA_LOCK_OPEN);
				ImGui::SameLine();
				ImGui::PushFont(largeFont);
				ImGui::Text("%s", category.c_str());
				ImGui::PopFont();
				ImGui::Unindent(10 * settings.display.uiScale);
			}
			ImguiID _("achiev" + std::to_string(id++));
			ImguiTexture tex(ach.image);
			tex.draw(ScaledVec2(80.f, 80.f));
			ImGui::SameLine();
			ImGui::BeginChild(ImGui::GetID("ach_item"), ImVec2(0, 0), ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_None);
			ImGui::PushFont(largeFont);
			ImGui::Text("%s", ach.title.c_str());
			ImGui::PopFont();

			{
				ImguiStyleColor _(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
				ImGui::TextWrapped("%s", ach.description.c_str());
				ImGui::TextWrapped("%s", ach.status.c_str());
			}

			scrollWhenDraggingOnVoid();
			ImGui::EndChild();
		}
	}
	scrollWhenDraggingOnVoid();
	windowDragScroll();

	ImGui::EndChild();
	ImGui::End();
}

}	// namespace achievements
#endif // USE_RACHIEVEMENTS
