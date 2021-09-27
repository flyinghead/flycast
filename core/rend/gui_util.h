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

#include <string>

#include "types.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "gles/imgui_impl_opengl3.h"
#include "vulkan/vulkan_context.h"
#include "dx9/dxcontext.h"
#include "gui.h"

typedef void (*StringCallback)(bool cancelled, std::string selection);

void select_file_popup(const char *prompt, StringCallback callback,
		bool selectFile = false, const std::string& extension = "");

static inline void ImGui_impl_RenderDrawData(ImDrawData *draw_data)
{
#ifdef USE_VULKAN
	if (config::RendererType.isVulkan())
		ImGui_ImplVulkan_RenderDrawData(draw_data);
	else
#endif
#ifdef _WIN32
		if (config::RendererType.isDirectX())
			theDXContext.EndImGuiFrame();
	else
#endif
		ImGui_ImplOpenGL3_RenderDrawData(draw_data);
}

void scrollWhenDraggingOnVoid(ImGuiMouseButton mouse_button = ImGuiMouseButton_Left);

IMGUI_API const ImWchar*    GetGlyphRangesChineseSimplifiedOfficial();// Default + Half-Width + Japanese Hiragana/Katakana + set of 7800 CJK Unified Ideographs from General Standard Chinese Characters
IMGUI_API const ImWchar*    GetGlyphRangesChineseTraditionalOfficial();// Default + Half-Width + Japanese Hiragana/Katakana + set of 4700 CJK Unified Ideographs from Hong Kong's List of Graphemes of Commonly-Used Chinese Characters

// Helper to display a little (?) mark which shows a tooltip when hovered.
void ShowHelpMarker(const char* desc);
template<bool PerGameOption>
bool OptionCheckbox(const char *name, config::Option<bool, PerGameOption>& option, const char *help = nullptr);
bool OptionSlider(const char *name, config::Option<int>& option, int min, int max, const char *help = nullptr);
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
