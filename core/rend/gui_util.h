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
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "gui.h"
#include "emulator.h"

#include <algorithm>
#include <chrono>
#include <future>
#include <string>

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
bool OptionSlider(const char *name, config::Option<int>& option, int min, int max, const char *help = nullptr, const char *format = nullptr);
template<typename T>
bool OptionRadioButton(const char *name, config::Option<T>& option, T value, const char *help = nullptr);
void OptionComboBox(const char *name, config::Option<int>& option, const char *values[], int count,
			const char *help = nullptr);
bool OptionArrowButtons(const char *name, config::Option<int>& option, int min, int max, const char *help = nullptr);

static inline void centerNextWindow()
{
	ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x / 2.f, ImGui::GetIO().DisplaySize.y / 2.f),
			ImGuiCond_Always, ImVec2(0.5f, 0.5f));
}

static inline bool operator==(const ImVec2& l, const ImVec2& r)
{
	return l.x == r.x && l.y == r.y;
}

static inline bool operator!=(const ImVec2& l, const ImVec2& r)
{
	return !(l == r);
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

struct ScaledVec2 : public ImVec2
{
	ScaledVec2()
		: ImVec2() {}
	ScaledVec2(float x, float y)
		: ImVec2(x * settings.display.uiScale, y * settings.display.uiScale) {}
};

inline static ImVec2 min(const ImVec2& l, const ImVec2& r) {
	return ImVec2(std::min(l.x, r.x), std::min(l.y, r.y));
}
inline static ImVec2 operator+(const ImVec2& l, const ImVec2& r) {
	return ImVec2(l.x + r.x, l.y + r.y);
}
inline static ImVec2 operator-(const ImVec2& l, const ImVec2& r) {
	return ImVec2(l.x - r.x, l.y - r.y);
}
inline static ImVec2 operator*(const ImVec2& v, float f) {
	return ImVec2(v.x * f, v.y * f);
}
inline static ImVec2 operator/(const ImVec2& v, float f) {
	return ImVec2(v.x / f, v.y / f);
}

u8 *loadImage(const std::string& path, int& width, int& height);

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
