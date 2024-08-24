/*
	Copyright 2019 flyinghead

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
#include "cfg/option.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "gui.h"
#include "emulator.h"
#include "oslib/oslib.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <string>
#include <mutex>

typedef bool (*StringCallback)(bool cancelled, std::string selection);

void select_file_popup(const char *prompt, StringCallback callback,
		bool selectFile = false, const std::string& extension = "");

void scrollWhenDraggingOnVoid(ImGuiMouseButton mouse_button = ImGuiMouseButton_Left);

IMGUI_API const ImWchar*    GetGlyphRangesChineseSimplifiedOfficial();// Default + Half-Width + Japanese Hiragana/Katakana + set of 7800 CJK Unified Ideographs from General Standard Chinese Characters
IMGUI_API const ImWchar*    GetGlyphRangesChineseTraditionalOfficial();// Default + Half-Width + Japanese Hiragana/Katakana + set of 4700 CJK Unified Ideographs from Hong Kong's List of Graphemes of Commonly-Used Chinese Characters

// Helper to display a little (?) mark which shows a tooltip when hovered.
void ShowHelpMarker(const char* desc);
template<bool PerGameOption>
bool OptionCheckbox(const char *name, config::Option<bool, PerGameOption>& option, const char *help = nullptr);
template<bool PerGameOption>
bool OptionSlider(const char *name, config::Option<int, PerGameOption>& option, int min, int max, const char *help = nullptr, const char *format = nullptr);
template<typename T>
bool OptionRadioButton(const char *name, config::Option<T>& option, T value, const char *help = nullptr);
void OptionComboBox(const char *name, config::Option<int>& option, const char *values[], int count,
			const char *help = nullptr);
bool OptionArrowButtons(const char *name, config::Option<int>& option, int min, int max, const char *help = nullptr, const char *format = "%d");

static inline void centerNextWindow()
{
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f),
			ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

void fullScreenWindow(bool modal);
void windowDragScroll();

class BackgroundGameLoader
{
public:
	void load(const std::string& path)
	{
		progress.reset();
		future = std::async(std::launch::async, [this, path] {
			ThreadName _("GameLoader");
			emu.loadGame(path.c_str(), &progress);
		});
	}

	void cancel()
	{
		if (progress.cancelled)
			return;
		progress.cancelled = true;
		if (future.valid())
			try {
				future.get();
			} catch (const FlycastException&) {
			}
		emu.unloadGame();
		gui_setState(GuiState::Main);
	}

	bool ready()
	{
		if (!future.valid())
			return true;
		if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			future.get();
			return true;
		}
		return false;
	}

	const LoadProgress& getProgress() const {
		return progress;
	}

private:
	LoadProgress progress;
	std::future<void> future;
};

static inline float uiScaled(float f) {
	return f * settings.display.uiScale;
}

struct ScaledVec2 : public ImVec2
{
	ScaledVec2()
		: ImVec2() {}
	ScaledVec2(float x, float y)
		: ImVec2(uiScaled(x), uiScaled(y)) {}
};

inline static ImVec2 min(const ImVec2& l, const ImVec2& r) {
	return ImVec2(std::min(l.x, r.x), std::min(l.y, r.y));
}

class DisabledScope
{
public:
	DisabledScope(bool disabled) : disabled(disabled)
	{
		if (disabled)
		{
	        ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
	        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
		}
	}
	~DisabledScope()
	{
		if (disabled)
		{
	        ImGui::PopItemFlag();
	        ImGui::PopStyleVar();
		}
	}
	bool isDisabled() const {
		return disabled;
	}

private:
	bool disabled;
};

bool BeginListBox(const char* label, const ImVec2& size_arg = ImVec2(0, 0), ImGuiWindowFlags windowFlags = 0);

class ImguiID
{
public:
	ImguiID(const std::string& id)
		: ImguiID(id.c_str()) {}
	ImguiID(const char *id) {
		ImGui::PushID(id);
	}
	~ImguiID() {
		ImGui::PopID();
	}
};

class ImguiStyleVar
{
public:
	ImguiStyleVar(ImGuiStyleVar idx, const ImVec2& val) {
		ImGui::PushStyleVar(idx, val);
	}
	ImguiStyleVar(ImGuiStyleVar idx, float val) {
		ImGui::PushStyleVar(idx, val);
	}
	~ImguiStyleVar() {
		ImGui::PopStyleVar();
	}
};

class ImguiStyleColor
{
public:
	ImguiStyleColor(ImGuiCol idx, const ImVec4& col) {
		ImGui::PushStyleColor(idx, col);
	}
	ImguiStyleColor(ImGuiCol idx, ImU32 col) {
		ImGui::PushStyleColor(idx, col);
	}
	~ImguiStyleColor() {
		ImGui::PopStyleColor();
	}
};

class ImguiTexture
{
public:
	void draw(const ImVec2& size, const ImVec4& tint_col = ImVec4(1, 1, 1, 1),
			const ImVec4& border_col = ImVec4(0, 0, 0, 0));
	void draw(ImDrawList *drawList, const ImVec2& pos, const ImVec2& size, float alpha = 1.f);
	bool button(const char* str_id, const ImVec2& image_size, const std::string& title = {}, const ImVec4& bg_col = ImVec4(0, 0, 0, 0),
			const ImVec4& tint_col = ImVec4(1, 1, 1, 1));

	operator ImTextureID() {
		return getId();
	}
	void setNearestSampling(bool nearestSampling) {
		this->nearestSampling = nearestSampling;
	}

	virtual ImTextureID getId() = 0;
	virtual ~ImguiTexture() = default;

protected:
	bool nearestSampling = false;
};

class ImguiFileTexture : public ImguiTexture
{
public:
	ImguiFileTexture() = default;
	ImguiFileTexture(const std::string& path) : ImguiTexture(), path(path) {}

	bool operator==(const ImguiFileTexture& other) const {
		return other.path == path;
	}
	ImTextureID getId() override;

	static void resetLoadCount() {
		textureLoadCount = 0;
	}

private:
	std::string path;
	static int textureLoadCount;
};

class ImguiStateTexture : public ImguiTexture
{
public:
	ImTextureID getId() override;

	bool exists();
	void invalidate();

private:
	struct LoadedPic
	{
		u8 *data;
		int width;
		int height;
	};
	static std::future<LoadedPic> asyncLoad;
};

class ImguiVmuTexture : public ImguiTexture
{
public:
	ImguiVmuTexture(int index = 0) : index(index) {}

	// draw all active vmus in a single column at the given position
	static void displayVmus(const ImVec2& pos);
	ImTextureID getId() override;

private:
	int index = 0;
	std::string idPath;
	u64 vmuLastChanged = 0;

	static std::array<ImguiVmuTexture, 8> Vmus;
};

static inline bool iconButton(const char *icon, const std::string& label, const ImVec2& size = {})
{
	ImguiStyleVar _{ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)};	// left aligned
	std::string s(5 + label.size(), '\0');
	s.resize(sprintf(s.data(), "%s  %s", icon, label.c_str()));
	return ImGui::Button(s.c_str(), size);
}

static inline float iconButtonWidth(const char *icon, const std::string& label)
{
	// TODO avoid doing stuff twice
	std::string s(5 + label.size(), '\0');
	s.resize(sprintf(s.data(), "%s  %s", icon, label.c_str()));
	return ImGui::CalcTextSize(s.c_str()).x + ImGui::GetStyle().FramePadding.x * 2;
}

static inline ImU32 alphaOverride(ImU32 color, float alpha) {
	return (color & ~IM_COL32_A_MASK) | (IM_F32_TO_INT8_SAT(alpha) << IM_COL32_A_SHIFT);
}

class Toast
{
public:
	void show(const std::string& title, const std::string& message, u32 durationMs);
	bool draw();

private:
	static constexpr u64 START_ANIM_TIME = 500;
	static constexpr u64 END_ANIM_TIME = 1000;

	std::string title;
	std::string message;
	u64 startTime = 0;
	u64 endTime = 0;
	std::mutex mutex;
};

std::string middleEllipsis(const std::string& s, float width);
