/*
	Copyright 2019 flyinghead

	This file is part of reicast.

    reicast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    reicast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with reicast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <string>

#include "types.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "vulkan/imgui_impl_vulkan.h"
#include "gles/imgui_impl_opengl3.h"
#include "vulkan/vulkan_context.h"
#include "gui.h"

typedef void (*StringCallback)(bool cancelled, std::string selection);

void select_directory_popup(const char *prompt, float scaling, StringCallback callback);

static inline void ImGui_impl_RenderDrawData(ImDrawData *draw_data, bool save_background = false)
{
#ifdef USE_VULKAN
	if (!config::RendererType.isOpenGL())
	{
		VulkanContext *context = VulkanContext::Instance();
		if (!context->IsValid())
			return;
		try {
			bool rendering = context->IsRendering();
			const std::vector<vk::UniqueCommandBuffer> *vmuCmdBuffers = nullptr;
			if (!rendering)
			{
				context->NewFrame();
				vmuCmdBuffers = context->PrepareOverlay(true, false);
				context->BeginRenderPass();
				context->PresentLastFrame();
				context->DrawOverlay(gui_get_scaling(), true, false);
			}
			// Record Imgui Draw Data and draw funcs into command buffer
			ImGui_ImplVulkan_RenderDrawData(draw_data, (VkCommandBuffer)context->GetCurrentCommandBuffer());
			if (!rendering)
				context->EndFrame(vmuCmdBuffers);
		} catch (const InvalidVulkanContext& err) {
		}
	}
	else
#endif
	{
		ImGui_ImplOpenGL3_RenderDrawData(draw_data, save_background);
	}
}

void ScrollWhenDraggingOnVoid(const ImVec2& delta, ImGuiMouseButton mouse_button);

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
