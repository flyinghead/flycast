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
#include "oslib/i18n.h"
#include <cmath>
#include <sstream>
using namespace i18n;

extern ImFont *largeFont;
extern int insetLeft;

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
	verify(type != Challenge && type != Leaderboard);
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
	ImguiFileTexture texture{ image };
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

void Notification::showLeaderboard(u32 id, const std::string& text)
{
	std::lock_guard<std::mutex> _(mutex);
	auto it = leaderboards.find(id);
	if (it == leaderboards.end())
	{
		if (leaderboards.empty())
		{
			this->type = Leaderboard;
			startTime = getTimeMs();
			endTime = NEVER_ENDS;
		}
		leaderboards[id] = text;
	}
	else {
		it->second = text;
	}
}

void Notification::hideLeaderboard(u32 id)
{
	std::lock_guard<std::mutex> _(mutex);
	auto it = leaderboards.find(id);
	if (it == leaderboards.end())
		return;
	leaderboards.erase(it);
	if (this->type == Leaderboard && leaderboards.empty())
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
		if (!leaderboards.empty())
		{
			// Show current leaderboards
			type = Leaderboard;
			startTime = getTimeMs();
			endTime = NEVER_ENDS;
		}
		else if (!challenges.empty())
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
	float alpha = 1.f;
	if (now > endTime)
		// Fade out
		alpha = (std::cos((now - endTime) / (float)END_ANIM_TIME * (float)M_PI) + 1.f) / 2.f;
	float animY = 0.f;
	if (now - startTime < START_ANIM_TIME)
		// Slide up
		animY = (std::cos((now - startTime) / (float)START_ANIM_TIME * (float)M_PI) + 1.f) / 2.f;

	const ImVec2 padding = ImGui::GetStyle().WindowPadding;
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	const ImU32 bg_col = alphaOverride(ImGui::GetColorU32(ImGuiCol_WindowBg), alpha / 2.f);
	const ImU32 borderCol = alphaOverride(ImGui::GetColorU32(ImGuiCol_Border), alpha);
	if (type == Challenge)
	{
		const ScaledVec2 size(60.f, 60.f);
		const float hspacing = ImGui::GetStyle().ItemSpacing.x;
		ImVec2 totalSize = padding * 2 + size;
		totalSize.x += (size.x + hspacing) * (challenges.size() - 1);
		ImVec2 pos(insetLeft, ImGui::GetIO().DisplaySize.y - totalSize.y * (1.f - animY));
		dl->AddRectFilled(pos, pos + totalSize, bg_col, 0.f);
		dl->AddRect(pos, pos + totalSize, borderCol, 0.f);

		pos += padding;
		for (auto& img : challenges) {
			img.draw(dl, pos, size, alpha);
			pos.x += hspacing + size.x;
		}
	}
	else if (type == Leaderboard)
	{
		ImFont *font = ImGui::GetFont();
		const ImVec2 padding = ImGui::GetStyle().FramePadding;
		// iterate from the end
		ImVec2 pos(insetLeft + padding.x, ImGui::GetIO().DisplaySize.y - padding.y);
		for (auto it = leaderboards.rbegin(); it != leaderboards.rend(); ++it)
		{
			const std::string& text = it->second;
			ImVec2 size = font->CalcTextSizeA(font->LegacySize, FLT_MAX, -1.f, text.c_str());
			ImVec2 psize = size + padding * 2;
			pos.y -= psize.y;
			dl->AddRectFilled(pos, pos + psize, bg_col, 0.f);
			ImVec2 tpos = pos + padding;
			const ImU32 col = alphaOverride(0xffffff, alpha);
			dl->AddText(font, font->LegacySize, tpos, col, &text.front(), &text.back() + 1, FLT_MAX);
			pos.y -= padding.y;
		}
	}
	else
	{
		const float hspacing = ImGui::GetStyle().ItemSpacing.x;
		const float vspacing = ImGui::GetStyle().ItemSpacing.y;
		const ScaledVec2 imgSize = image.getId() != ImTextureID{} ? ScaledVec2(80.f, 80.f) : ScaledVec2();
		// text size
		const float maxW = std::min(ImGui::GetIO().DisplaySize.x, uiScaled(640.f)) - padding.x
				- (imgSize.x != 0.f ? imgSize.x + hspacing : padding.x);
		ImFont *regularFont = ImGui::GetFont();
		ImVec2 textSize[3] {};
		ImVec2 totalSize(0.f, padding.y * 2);
		for (size_t i = 0; i < std::size(text); i++)
		{
			if (text[i].empty())
				continue;
			ImFont *font = i == 0 ? largeFont : regularFont;
			textSize[i] = font->CalcTextSizeA(font->LegacySize, FLT_MAX, maxW, text[i].c_str());
			totalSize.x = std::max(totalSize.x, textSize[i].x);
			totalSize.y += textSize[i].y;
		}
		float topMargin = 0.f;
		// image / left padding
		if (imgSize.x != 0.f)
		{
			if (totalSize.y < imgSize.y)
				topMargin = (imgSize.y - totalSize.y) / 2.f;
			totalSize.x += imgSize.x + hspacing;
			totalSize.y = std::max(totalSize.y, imgSize.y);
		}
		else {
			totalSize.x += padding.x;
		}
		// right padding
		totalSize.x += padding.x;
		// border
		totalSize += ImVec2(2.f, 2.f);
		// draw background, border
		ImVec2 pos(insetLeft, ImGui::GetIO().DisplaySize.y - totalSize.y * (1.f - animY));
		dl->AddRectFilled(pos, pos + totalSize, bg_col, 0.f);
		dl->AddRect(pos, pos + totalSize, borderCol, 0.f);

		// draw image and text
		pos += ImVec2(1.f, 1.f); // border
		if (imgSize.x != 0.f) {
			image.draw(dl, pos, imgSize, alpha);
			pos.x += imgSize.x + hspacing;
		}
		else {
			pos.x += padding.x;
		}
		pos.y += topMargin;
		for (size_t i = 0; i < std::size(text); i++)
		{
			if (text[i].empty())
				continue;
			ImFont *font = i == 0 ? largeFont : regularFont;
			const ImU32 col = alphaOverride(i == 0 ? 0xffffff : 0x00ffff, alpha);
			dl->AddText(font, font->LegacySize, pos, col, &text[i].front(), &text[i].back() + 1, maxW);
			pos.y += textSize[i].y + vspacing;
		}
	}

	return true;
}

void achievementList()
{
	fullScreenWindow(false);
	ImguiStyleVar _(ImGuiStyleVar_WindowBorderSize, 0);

	ImGui::Begin("##achievements", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);

	{
		const char *closeLabel = T("Close");
		float w = ImGui::GetWindowContentRegionMax().x - ImGui::CalcTextSize(closeLabel).x - ImGui::GetStyle().ItemSpacing.x * 2 - ImGui::GetStyle().WindowPadding.x
				- uiScaled(80.f + 20.f * 2);	// image width and button frame padding
		Game game = getCurrentGame();
		ImguiFileTexture tex(game.image);
		tex.draw(ScaledVec2(80.f, 80.f));
		ImGui::SameLine();
		ImGui::BeginChild("game_info", ImVec2(w, uiScaled(80.f)), ImGuiChildFlags_None, ImGuiWindowFlags_None);
		ImGui::PushFont(largeFont);
		ImGui::Text("%s", game.title.c_str());
		ImGui::PopFont();
		std::string str = strprintf(T("You have unlocked %d of %d achievements and %d of %d points."),
				game.unlockedAchievements, game.totalAchievements, game.points, game.totalPoints);
		{
			ImguiStyleColor _(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.f));
			ImGui::TextWrapped("%s", str.c_str());
		}
		if (settings.raHardcoreMode)
			ImGui::Text("%s", T("Hardcore Mode"));
		ImGui::EndChild();

		ImGui::SameLine();
		ImguiStyleVar _(ImGuiStyleVar_FramePadding, ScaledVec2(20, 8));
		if (ImGui::Button(closeLabel))
			gui_setState(GuiState::Commands);
    }

	// ImGuiWindowFlags_NavFlattened prevents the child window from getting the focus and thus the list can't be scrolled with a keyboard or gamepad.
	if (ImGui::BeginChild(ImGui::GetID("ach_list"), ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_DragScrolling))
	{
		std::vector<Achievement> achList = getAchievementList();
		int id = 0;
		std::string category;
		for (const auto& ach : achList)
		{
			if (ach.category != category)
			{
				category = ach.category;
				ImGui::Indent(uiScaled(10));
				if (category == Tnop("Locked") || category == Tnop("Active Challenges") || category == Tnop("Almost There"))
					ImGui::Text(ICON_FA_LOCK);
				else if (category == Tnop("Unlocked") || category == Tnop("Recently Unlocked"))
					ImGui::Text(ICON_FA_LOCK_OPEN);
				ImGui::SameLine();
				ImGui::PushFont(largeFont);
				ImGui::Text("%s", T(category.c_str()));
				ImGui::PopFont();
				ImGui::Unindent(uiScaled(10));
			}
			ImguiID _("achiev" + std::to_string(id++));
			ImguiFileTexture tex(ach.image);
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
