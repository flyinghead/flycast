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
#include "gui_achievements.h"
#include "gui.h"
#include "gui_util.h"
#include "imgui_driver.h"
#include "stdclass.h"
#include <cmath>

extern ImFont *largeFont;

namespace achievements
{

Notification notifier;

static constexpr u64 DISPLAY_TIME = 5000;
static constexpr u64 START_ANIM_TIME = 500;
static constexpr u64 END_ANIM_TIME = 1000;

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
				endTime = 0x1000000000000;	// never
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
	this->imagePath = image;
	this->imageId = {};
	text[0] = text1;
	text[1] = text2;
	text[2] = text3;
}

bool Notification::draw()
{
	std::lock_guard<std::mutex> _(mutex);
	if (type == None)
		return false;
	u64 now = getTimeMs();
	if (now > endTime + END_ANIM_TIME) {
		// Hide notification
		type = None;
		return false;
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
	if (imageId == ImTextureID{})
		getImage();
	float y = ImGui::GetIO().DisplaySize.y;
	if (now - startTime < START_ANIM_TIME)
		// Slide up
		y += 80.f * settings.display.uiScale * (std::cos((now - startTime) / (float)START_ANIM_TIME * (float)M_PI) + 1.f) / 2.f;

	ImGui::SetNextWindowPos(ImVec2(0, y), ImGuiCond_Always, ImVec2(0.f, 1.f));	// Lower left corner
	ImGui::SetNextWindowSizeConstraints(ScaledVec2(80.f, 80.f) + ImVec2(ImGui::GetStyle().WindowPadding.x * 2, 0.f), ImVec2(FLT_MAX, FLT_MAX));
	const float winPaddingX = ImGui::GetStyle().WindowPadding.x;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{});

	ImGui::Begin("##achievements", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav
			| ImGuiWindowFlags_NoInputs);
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
			ImGui::Image(imageId, ScaledVec2(80.f, 80.f), { 0.f, 0.f }, { 1.f, 1.f });
			ImGui::TableSetColumnIndex(1);
		}

		float w = largeFont->CalcTextSizeA(largeFont->FontSize, FLT_MAX, -1.f, text[0].c_str()).x;
		w = std::max(w, ImGui::CalcTextSize(text[1].c_str()).x);
		w = std::max(w, ImGui::CalcTextSize(text[2].c_str()).x) + winPaddingX * 2;
		int lines = (int)!text[0].empty() + (int)!text[1].empty() + (int)!text[2].empty();
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ hasPic ? 0.f : winPaddingX, (3 - lines) * ImGui::GetTextLineHeight() / 2 });
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
		ImGui::PopStyleVar();

		ImGui::EndTable();
	}
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::GetStyle().Alpha = 1.f;

	return true;
}

void Notification::getImage()
{
	if (imagePath.empty())
		return;

	// Get the texture. Load it if needed.
	imageId = imguiDriver->getTexture(imagePath);
	if (imageId == ImTextureID())
	{
		int width, height;
		u8 *imgData = loadImage(imagePath, width, height);
		if (imgData != nullptr)
		{
			try {
				imageId = imguiDriver->updateTextureAndAspectRatio(imagePath, imgData, width, height);
			} catch (...) {
				// vulkan can throw during resizing
			}
			free(imgData);
		}
	}
}

}	// namespace achievements
