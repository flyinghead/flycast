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
#include "gui_util.h"
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>

#include "types.h"
#include "stdclass.h"
#include "oslib/oslib.h"
#include "oslib/directory.h"
#include "oslib/storage.h"
#include "oslib/http_client.h"
#include "oslib/i18n.h"
#include "imgui_driver.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "stdclass.h"
#include "rend/osd.h"
#include <stb_image.h>

using namespace i18n;

static std::string select_current_directory = "**home**";
static std::vector<hostfs::FileInfo> subfolders;
static std::vector<hostfs::FileInfo> folderFiles;
bool subfolders_read;

extern int insetLeft, insetRight, insetTop, insetBottom;
extern ImFont *largeFont;
void error_popup();

namespace hostfs
{
	bool operator<(const FileInfo& a, const FileInfo& b) {
		return locale()(a.name, b.name);
	}
}

void select_file_popup(const char *prompt, StringCallback callback,
		bool selectFile, const std::string& selectExtension)
{
	fullScreenWindow(true);
	ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
	ImguiStyleVar _1(ImGuiStyleVar_FramePadding, ImVec2(4, 3)); // default

	if (ImGui::BeginPopup(prompt, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize ))
	{
		static std::string error_message;

		if (select_current_directory == "**home**")
			select_current_directory = hostfs::storage().getDefaultDirectory();

		if (!subfolders_read)
		{
			subfolders.clear();
            folderFiles.clear();
			error_message.clear();

			try {
				for (const hostfs::FileInfo& entry : hostfs::storage().listContent(select_current_directory))
				{
					if (entry.isDirectory)
					{
						subfolders.push_back(entry);
					}
					else
					{
						std::string extension = get_file_extension(entry.name);
						if (selectFile)
						{
							if (extension == selectExtension)
								folderFiles.push_back(entry);
						}
						else if (extension == "zip" || extension == "7z" || extension == "chd"
								|| extension == "gdi" || extension == "cdi" || extension == "cue"
								|| (!config::HideLegacyNaomiRoms
										&& (extension == "bin" || extension == "lst" || extension == "dat")))
							folderFiles.push_back(entry);
					}
				}
			} catch (const hostfs::StorageException& e) {
				error_message = e.what();
			}

			std::sort(subfolders.begin(), subfolders.end());
			std::sort(folderFiles.begin(), folderFiles.end());
			subfolders_read = true;
		}
		if (prompt != nullptr) {
			ImguiStyleVar _(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
			ImguiStyleVar _1(ImGuiStyleVar_DisabledAlpha, 1.0f);
			ImGui::BeginDisabled();
			ImGui::PushFont(largeFont);
			ImGui::ButtonEx(prompt, ImVec2(-1, 0));
			ImGui::PopFont();
			ImGui::EndDisabled();
		}
		std::string title;
		if (!error_message.empty())
			title = error_message;
		else if (select_current_directory.empty())
			title = T("Storage");
		else
			title = select_current_directory;

		ImGui::Text("%s", title.c_str());
		ImGui::BeginChild(ImGui::GetID("dir_list"), ImVec2(0, - uiScaled(30) - ImGui::GetStyle().ItemSpacing.y),
				ImGuiChildFlags_Borders, ImGuiWindowFlags_DragScrolling | ImGuiChildFlags_NavFlattened);
		{
			ImguiStyleVar _(ImGuiStyleVar_ItemSpacing, ScaledVec2(8, 20));

			if (!select_current_directory.empty() && select_current_directory != "/")
			{
				if (ImGui::Selectable(T(".. Up to Parent Folder")))
				{
					subfolders_read = false;
					select_current_directory = hostfs::storage().getParentPath(select_current_directory);
				}
			}

			for (const auto& entry : subfolders)
			{
				if (ImGui::Selectable(entry.name.c_str()))
				{
					subfolders_read = false;
					select_current_directory = entry.path;
				}
			}
			ImguiStyleColor _1(ImGuiCol_Text, { 1, 1, 1, selectFile ? 1.f : 0.3f });
			for (const auto& entry : folderFiles)
			{
				if (selectFile)
				{
					if (ImGui::Selectable(entry.name.c_str()))
					{
						subfolders_read = false;
						if (callback(false, entry.path))
							ImGui::CloseCurrentPopup();
					}
				}
				else
				{
					ImGui::Text("%s", entry.name.c_str());
				}
			}
			scrollWhenDraggingOnVoid();
			windowDragScroll();
		}
		ImGui::EndChild();
		if (!selectFile)
		{
			if (ImGui::Button(T("Select Current Folder"), ScaledVec2(0, 30)))
			{
				if (callback(false, select_current_directory))
				{
					subfolders_read = false;
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
		}
		if (ImGui::Button(T("Cancel"), ScaledVec2(0, 30)))
		{
			subfolders_read = false;
			callback(true, "");
			ImGui::CloseCurrentPopup();
		}
		error_popup();
		ImGui::EndPopup();
	}
}

// See https://github.com/ocornut/imgui/issues/3379
void scrollWhenDraggingOnVoid(ImGuiMouseButton mouse_button)
{
	ImGuiContext& g = *ImGui::GetCurrentContext();
	ImGuiWindow* window = g.CurrentWindow;
	while (window != nullptr
			&& (window->Flags & ImGuiWindowFlags_ChildWindow)
			&& !(window->Flags & ImGuiWindowFlags_DragScrolling)
			&& window->ScrollMax.x == 0.0f
			&& window->ScrollMax.y == 0.0f)
		window = window->ParentWindow;
	if (window == nullptr || !(window->Flags & ImGuiWindowFlags_DragScrolling))
		return;
    bool hovered = false;
    bool held = false;
    ImGuiButtonFlags button_flags = (mouse_button == ImGuiMouseButton_Left) ? ImGuiButtonFlags_MouseButtonLeft
    		: (mouse_button == ImGuiMouseButton_Right) ? ImGuiButtonFlags_MouseButtonRight : ImGuiButtonFlags_MouseButtonMiddle;
    // If nothing hovered so far in the frame (not same as IsAnyItemHovered()!) or item is disabled
	if (g.HoveredId == 0 || g.HoveredIdIsDisabled)
    {
    	bool hoveredAllowOverlap = g.HoveredIdAllowOverlap;
    	g.HoveredIdAllowOverlap = true;
    	ImGuiID overlayId = window->GetID("##scrolldraggingoverlay");
    	ImGui::ButtonBehavior(window->Rect(), overlayId, &hovered, &held, button_flags);
    	ImGui::KeepAliveID(overlayId);
    	g.HoveredIdAllowOverlap = hoveredAllowOverlap;
    }
    const ImVec2& delta = ImGui::GetIO().MouseDelta;
    if (held && delta != ImVec2())
    {
    	window->DragScrolling = true;
    	window->ScrollSpeed = delta;
    }
}

// Helper to display a little (?) mark which shows a tooltip when hovered.
void ShowHelpMarker(const char* desc)
{
    ImGui::TextDisabled("%s", T("(?)"));
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 25.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

template<bool PerGameOption>
bool OptionCheckbox(const char *name, config::Option<bool, PerGameOption>& option, const char *help)
{
	bool pressed;
	{
		DisabledScope scope(option.isReadOnly());

		bool b = option;
		pressed = ImGui::Checkbox(name, &b);
		if (pressed)
			option.set(b);
	}
	if (help != nullptr)
	{
		ImGui::SameLine();
		ShowHelpMarker(help);
	}
	return pressed;
}
template bool OptionCheckbox(const char *name, config::Option<bool, true>& option, const char *help);
template bool OptionCheckbox(const char *name, config::Option<bool, false>& option, const char *help);

template<bool PerGameOption>
bool OptionSlider(const char *name, config::Option<int, PerGameOption>& option, int min, int max, const char *help, const char *format)
{
	bool valueChanged;
	{
		DisabledScope scope(option.isReadOnly());

		int v = option;
		valueChanged = ImGui::SliderInt(name, &v, min, max, format);
		if (valueChanged)
			option.set(v);
	}
	if (help != nullptr)
	{
		ImGui::SameLine();
		ShowHelpMarker(help);
	}
	return valueChanged;
}
template bool OptionSlider(const char *name, config::Option<int, true>& option, int min, int max, const char *help, const char *format);
template bool OptionSlider(const char *name, config::Option<int, false>& option, int min, int max, const char *help, const char *format);

bool OptionArrowButtons(const char *name, config::Option<int>& option, int min, int max, const char *help, const char *format)
{
	const float innerSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const std::string id = "##" + std::string(name);
	{
		ImguiStyleVar _(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.f, 0.5f)); // Left
		ImguiStyleColor _1(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
		const float width = ImGui::CalcItemWidth() - innerSpacing * 2.0f - ImGui::GetFrameHeight() * 2.0f;
		ImguiStyleVar _2(ImGuiStyleVar_DisabledAlpha, 1.0f);
		ImGui::BeginDisabled();
		std::string value = strprintf(format, (int)option);
		ImGui::ButtonEx((value + id).c_str(), ImVec2(width, 0));
		ImGui::EndDisabled();
	}

	ImGui::SameLine(0.0f, innerSpacing);
	ImGui::PushButtonRepeat(true);
	bool valueChanged = false;
	{
		DisabledScope scope(option.isReadOnly());

		if (ImGui::ArrowButton((id + "left").c_str(), ImGuiDir_Left)) { option.set(std::max(min, option - 1)); valueChanged = true; }
		ImGui::SameLine(0.0f, innerSpacing);
		if (ImGui::ArrowButton((id + "right").c_str(), ImGuiDir_Right)) { option.set(std::min(max, option + 1)); valueChanged = true; }
	}
	ImGui::PopButtonRepeat();
	ImGui::SameLine(0.0f, innerSpacing);
	ImGui::Text("%s", name);
	if (help != nullptr)
	{
		ImGui::SameLine();
		ShowHelpMarker(help);
	}
	return valueChanged;
}

template<typename T>
bool OptionRadioButton(const char *name, config::Option<T>& option, T value, const char *help)
{
	bool pressed;
	{
		DisabledScope scope(option.isReadOnly());

		int v = (int)option;
		pressed = ImGui::RadioButton(name, &v, (int)value);
		if (pressed)
			option.set((T)v);
	}
	if (help != nullptr)
	{
		ImGui::SameLine();
		ShowHelpMarker(help);
	}
	return pressed;
}
template bool OptionRadioButton<bool>(const char *name, config::Option<bool>& option, bool value, const char *help);
template bool OptionRadioButton<int>(const char *name, config::Option<int>& option, int value, const char *help);

template<bool PerGameOption>
void OptionComboBox(const char *name, config::Option<int, PerGameOption>& option, const char *values[], int count,
			const char *help)
{
	{
		DisabledScope scope(option.isReadOnly());

		const char *value = option >= 0 && option < count ? values[option] : "?";
		if (ImGui::BeginCombo(name, value, ImGuiComboFlags_None))
		{
			for (int i = 0; i < count; i++)
			{
				bool is_selected = option == i;
				if (ImGui::Selectable(values[i], &is_selected))
					option = i;
				if (is_selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}
	}
	if (help != nullptr)
	{
		ImGui::SameLine();
		ShowHelpMarker(help);
	}
}

// Explicit template instantiations
template void OptionComboBox<true>(const char *name, config::Option<int, true>& option, const char *values[], int count, const char *help);
template void OptionComboBox<false>(const char *name, config::Option<int, false>& option, const char *values[], int count, const char *help);

void fullScreenWindow(bool modal)
{
	if (!modal)
	{
		ImguiStyleVar _(ImGuiStyleVar_WindowRounding, 0);
		ImguiStyleVar _1(ImGuiStyleVar_WindowBorderSize, 0);

		if (insetLeft > 0)
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(insetLeft, ImGui::GetIO().DisplaySize.y));
			ImGui::Begin("##insetLeft", NULL, ImGuiWindowFlags_NoDecoration);
			ImGui::End();
		}
		if (insetRight > 0)
		{
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - insetRight, 0));
			ImGui::SetNextWindowSize(ImVec2(insetRight, ImGui::GetIO().DisplaySize.y));
			ImGui::Begin("##insetRight", NULL, ImGuiWindowFlags_NoDecoration);
			ImGui::End();
		}
		if (insetTop > 0)
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, insetTop));
			ImGui::Begin("##insetTop", NULL, ImGuiWindowFlags_NoDecoration);
			ImGui::End();
		}
		if (insetBottom > 0)
		{
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - insetBottom));
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, insetBottom));
			ImGui::Begin("##insetBottom", NULL, ImGuiWindowFlags_NoDecoration);
			ImGui::End();
		}
	}
	ImGui::SetNextWindowPos(ImVec2(insetLeft, insetTop));
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - insetLeft - insetRight, ImGui::GetIO().DisplaySize.y - insetTop - insetBottom));
}

static void computeScrollSpeed(float &v)
{
	constexpr float friction = 3.f;
	if (std::abs(v) > friction)
	{
		float sign = (v > 0.f) - (v < 0.f);
		v -= friction * sign;
	}
	else
	{
		v = 0.f;
	}
}

void windowDragScroll()
{
	ImGuiWindow *window = ImGui::GetCurrentWindow();
	if (window->DragScrolling)
	{
		if (!ImGui::GetIO().MouseDown[ImGuiMouseButton_Left])
		{
			computeScrollSpeed(window->ScrollSpeed.x);
			computeScrollSpeed(window->ScrollSpeed.y);
			if (window->ScrollSpeed == ImVec2())
			{
				window->DragScrolling = false;
				// FIXME we should really move the mouse off-screen after a touch up and this wouldn't be necessary
				// the only problem is tool tips
				gui_set_mouse_position(-1, -1, true);
			}
		}
		else
		{
			ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
			if (delta != ImVec2())
				ImGui::ResetMouseDragDelta();
			window->ScrollSpeed = delta;
		}
		if (window->DragScrolling)
		{
			ImGui::SetScrollX(window, window->Scroll.x - window->ScrollSpeed.x);
			ImGui::SetScrollY(window, window->Scroll.y - window->ScrollSpeed.y);
		}
	}
}

static void setUV(float ar, ImVec2& uv0, ImVec2& uv1)
{
	uv0 = { 0.f, 0.f };
	uv1 = { 1.f, 1.f };
	if (ar > 1)
	{
		uv0.y = -(ar - 1) / 2;
		uv1.y = 1 + (ar - 1) / 2;
	}
	else if (ar != 0)
	{
		ar = 1 / ar;
		uv0.x = -(ar - 1) / 2;
		uv1.x = 1 + (ar - 1) / 2;
	}
}

void ImguiTexture::draw(const ImVec2& size, const ImVec4& tint_col, const ImVec4& border_col)
{
	ImTextureID id = getId();
	if (id == ImTextureID{})
		ImGui::Dummy(size);
	else
	{
		const float ar = imguiDriver->getAspectRatio(id);
		ImVec2 drawSize(size);
		if (size.x == 0.f)
			drawSize.x = size.y * ar;
		else if (size.y == 0.f)
			drawSize.y = size.x / ar;
		ImVec2 uv0, uv1;
		setUV(ar / drawSize.x * drawSize.y, uv0, uv1);
		ImGui::Image(id, drawSize, uv0, uv1, tint_col, border_col);
	}
}

void ImguiTexture::draw(ImDrawList *drawList, const ImVec2& pos, const ImVec2& size, float alpha)
{
	ImTextureID id = getId();
	if (id == ImTextureID{})
		return;
	const float ar = imguiDriver->getAspectRatio(id);
	ImVec2 uv0, uv1;
	setUV(ar / size.x * size.y, uv0, uv1);
	u32 col = alphaOverride(0xffffff, alpha);
	drawList->AddImage(id, pos, pos + size, uv0, uv1, col);
}

void ImguiTexture::draw(ImDrawList *drawList, const ImVec2& pos, const ImVec2& size,
		const ImVec2& uv0, const ImVec2& uv1, const ImVec4& color)
{
	ImTextureID id = getId();
	if (id == ImTextureID{})
		return;
	u32 col = ImGui::ColorConvertFloat4ToU32(color);
	drawList->AddImage(id, pos, pos + size, uv0, uv1, col);
}

bool ImguiTexture::button(const char* str_id, const ImVec2& image_size, const std::string& title,
		const ImVec4& bg_col, const ImVec4& tint_col)
{
	ImTextureID id = getId();
	if (id == ImTextureID{})
		return ImGui::Button(title.c_str(), image_size);
	else
	{
		const float ar = imguiDriver->getAspectRatio(id);
		const ImVec2 size = image_size - ImGui::GetStyle().FramePadding * 2;
		ImVec2 uv0, uv1;
		setUV(ar / size.x * size.y, uv0, uv1);
		return ImGui::ImageButton(str_id, id, size, uv0, uv1, bg_col, tint_col);
	}
}

static u8 *loadImage(const std::string& path, int& width, int& height)
{
	FILE *file = nowide::fopen(path.c_str(), "rb");
	if (file == nullptr)
		return nullptr;

	int channels;
	stbi_set_flip_vertically_on_load(0);
	u8 *imgData = stbi_load_from_file(file, &width, &height, &channels, STBI_rgb_alpha);
	std::fclose(file);
	return imgData;
}

int ImguiFileTexture::textureLoadCount;

ImTextureID ImguiFileTexture::getId()
{
	if (path.empty())
		return {};
	ImTextureID id = imguiDriver->getTexture(path);
	if (id == ImTextureID() && textureLoadCount < 10)
	{
		textureLoadCount++;
		int width, height;
		u8 *imgData = loadImage(path, width, height);
		if (imgData != nullptr)
		{
			try {
				id = imguiDriver->updateTextureAndAspectRatio(path, imgData, width, height, nearestSampling);
			} catch (...) {
				// vulkan can throw during resizing
			}
			free(imgData);
		}
	}
	return id;
}

std::future<ImguiStateTexture::LoadedPic> ImguiStateTexture::asyncLoad;

bool ImguiStateTexture::exists()
{
	std::string path = hostfs::getSavestatePath(config::SavestateSlot, false);
	return hostfs::storage().exists(path);
}

ImTextureID ImguiStateTexture::getId()
{
	std::string path = hostfs::getSavestatePath(config::SavestateSlot, false);
	ImTextureID texid = imguiDriver->getTexture(path);
	if (texid != ImTextureID())
		return texid;
	if (asyncLoad.valid())
	{
		if (asyncLoad.wait_for(std::chrono::seconds::zero()) == std::future_status::timeout)
			return {};
		LoadedPic loadedPic = asyncLoad.get();
		if (loadedPic.data != nullptr)
		{
			try {
				texid = imguiDriver->updateTextureAndAspectRatio(path, loadedPic.data, loadedPic.width, loadedPic.height, nearestSampling);
			} catch (...) {
				// vulkan can throw during resizing
			}
			free(loadedPic.data);
		}
		return texid;
	}
	asyncLoad = std::async(std::launch::async, []() {
		LoadedPic loadedPic{};
		// load savestate info
		std::vector<u8> pngData;
		dc_getStateScreenshot(config::SavestateSlot, pngData);
		if (pngData.empty())
			return loadedPic;

		int channels;
		stbi_set_flip_vertically_on_load(0);
		loadedPic.data = stbi_load_from_memory(&pngData[0], pngData.size(), &loadedPic.width, &loadedPic.height, &channels, STBI_rgb_alpha);

		return loadedPic;
	});
	return {};
}

void ImguiStateTexture::invalidate()
{
	if (imguiDriver)
	{
		std::string path = hostfs::getSavestatePath(config::SavestateSlot, false);
		imguiDriver->deleteTexture(path);
	}
}

std::array<ImguiVmuTexture, 8> ImguiVmuTexture::Vmus { 0, 1, 2, 3, 4, 5, 6, 7 };
constexpr float VMU_WIDTH = 96.f;
constexpr float VMU_HEIGHT = 64.f;
constexpr float VMU_PADDING = 8.f;

ImTextureID ImguiVmuTexture::getId()
{
	if (!vmu_lcd_status[index])
		return {};
	if (idPath.empty())
		idPath = ":vmu:" + std::to_string(index);
	ImTextureID texid = imguiDriver->getTexture(idPath);
	if (texid == ImTextureID() || vmuLastChanged != ::vmuLastChanged[index])
	{
		try {
			texid = imguiDriver->updateTexture(idPath, (const u8 *)vmu_lcd_data[index], 48, 32, true);
			vmuLastChanged = ::vmuLastChanged[index];
		} catch (...) {
		}
	}
	return texid;
}

void ImguiVmuTexture::displayVmus(const ImVec2& pos)
{
	const ScaledVec2 size(VMU_WIDTH, VMU_HEIGHT);
	const float padding = uiScaled(VMU_PADDING);
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	ImVec2 cpos(pos + ScaledVec2(2.f, 0));	// 96 pixels wide + 2 * 2 -> 100
	for (int i = 0; i < 8; i++)
	{
		if (!vmu_lcd_status[i])
			continue;

		ImTextureID texid = Vmus[i].getId();
		if (texid == ImTextureID())
			continue;
		ImVec2 pos_b = cpos + size;
		dl->AddImage(texid, cpos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0x80ffffff);
		cpos.y += size.y + padding;
	}
}

void Toast::show(const std::string& title, const std::string& message, u32 durationMs)
{
	const u64 now = getTimeMs();
	std::lock_guard<std::mutex> _{mutex};
	// no start anim if still visible
	if (now > endTime + END_ANIM_TIME)
		startTime = getTimeMs();
	endTime = now + durationMs;
	this->title = title;
	this->message = message;
}

bool Toast::draw()
{
	const u64 now = getTimeMs();
	std::lock_guard<std::mutex> _{mutex};
	if (now > endTime + END_ANIM_TIME) {
		title.clear();
		message.clear();
	}
	if (title.empty() && message.empty())
		return false;
	float alpha = 1.f;
	if (now > endTime)
		// Fade out
		alpha = (std::cos((now - endTime) / (float)END_ANIM_TIME * (float)M_PI) + 1.f) / 2.f;

	const ImVec2 displaySize(ImGui::GetIO().DisplaySize);
	const float maxW = std::min(uiScaled(640.f), displaySize.x);
	ImFont *regularFont = ImGui::GetFont();
	const ImVec2 titleSize = title.empty() ? ImVec2()
			: largeFont->CalcTextSizeA(largeFont->LegacySize, FLT_MAX, maxW, &title.front(), &title.back() + 1);
	const ImVec2 msgSize = message.empty() ? ImVec2()
			: regularFont->CalcTextSizeA(regularFont->LegacySize, FLT_MAX, maxW, &message.front(), &message.back() + 1);
	const ScaledVec2 padding(5.f, 4.f);
	const ScaledVec2 spacing(0.f, 2.f);
	ImVec2 totalSize(std::max(titleSize.x, msgSize.x), titleSize.y + msgSize.y);
	totalSize += padding * 2.f + spacing * (float)(!title.empty() && !message.empty());

	ImVec2 pos(insetLeft, displaySize.y - totalSize.y);
	if (now - startTime < START_ANIM_TIME)
		// Slide up
		pos.y += totalSize.y * (std::cos((now - startTime) / (float)START_ANIM_TIME * (float)M_PI) + 1.f) / 2.f;
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	const ImU32 bg_col = alphaOverride(ImGui::GetColorU32(ImGuiCol_WindowBg), alpha / 2.f);
	dl->AddRectFilled(pos, pos + totalSize, bg_col, 0.f);
	const ImU32 col = alphaOverride(ImGui::GetColorU32(ImGuiCol_Border), alpha);
	dl->AddRect(pos, pos + totalSize, col, 0.f);

	pos += padding;
	if (!title.empty())
	{
		const ImU32 col = alphaOverride(ImGui::GetColorU32(ImGuiCol_Text), alpha);
		dl->AddText(largeFont, largeFont->LegacySize, pos, col, &title.front(), &title.back() + 1, maxW);
		pos.y += spacing.y + titleSize.y;
	}
	if (!message.empty())
	{
		const ImU32 col = alphaOverride(0xFF00FFFF, alpha);	// yellow
		dl->AddText(regularFont, regularFont->LegacySize, pos, col, &message.front(), &message.back() + 1, maxW);
	}

	return true;
}

std::string middleEllipsis(const std::string& s, float width)
{
	float tw = ImGui::CalcTextSize(s.c_str()).x;
	if (tw <= width)
		return s;
	char buf[5];
	ImTextCharToUtf8(buf, ImGui::GetFont()->EllipsisChar);
	std::string ellipsis = buf;

	int l = s.length() / 2;
	int d = l;

	while (true)
	{
		std::string ss = s.substr(0, l / 2) + ellipsis + s.substr(s.length() - l / 2 - (l & 1));
		tw = ImGui::CalcTextSize(ss.c_str()).x;
		if (tw == width)
			return ss;
		d /= 2;
		if (d == 0)
			return ss;
		if (tw > width)
			l -= d;
		else
			l += d;
	}
}

bool beginFrame(const char *label, const ImVec2& size_arg, ImVec2 *out_size)
{
	using namespace ImGui;
    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return false;
    const ImGuiStyle& style = g.Style;
	const ImVec2 label_size = CalcTextSize(label, NULL, true);
    ImVec2 size = ImTrunc(CalcItemSize(size_arg, CalcItemWidth(), GetTextLineHeightWithSpacing() * 7.25f + style.FramePadding.y * 2.0f));
    ImVec2 frame_size = ImVec2(size.x, ImMax(size.y, label_size.y));
    ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);
    ImRect bb(frame_bb.Min, frame_bb.Max + ImVec2(label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f, 0.0f));
    window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, bb.Max);

    BeginGroup();
    if (label_size.x > 0.0f)
    {
        ImVec2 label_pos = ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y);
        RenderText(label_pos, label);
        window->DC.CursorMaxPos = ImMax(window->DC.CursorMaxPos, label_pos + label_size);
    }

    const ImU32 bg_col = GetColorU32(ImGuiCol_FrameBg);
    window->DrawList->AddRectFilled(frame_bb.Min, frame_bb.Max, bg_col, g.Style.FrameRounding, 0);
    window->DC.CursorPos += style.FramePadding;
    PushClipRect(frame_bb.Min + style.FramePadding, frame_bb.Max - style.FramePadding, false);
    if (out_size != nullptr)
    	*out_size = frame_size - style.FramePadding * 2.f;
    BeginGroup();

    return true;
}

void endFrame()
{
	using namespace ImGui;
	EndGroup();
	PopClipRect();
	EndGroup();
}

#ifdef __SWITCH__

static constexpr unsigned Flags_Multiline = 1 << 31;

bool switchEditText(char *value, size_t capacity, ImGuiInputTextFlags flags, bool multiline);

static int switchInputTextCallback(ImGuiInputTextCallbackData *data)
{
	if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && (data->Flags & ImGuiInputTextFlags_ReadOnly) == 0)
	{
		data->Buf[data->BufTextLen] = '\0';
		if (switchEditText(data->Buf, data->BufSize, data->Flags, (data->Flags & Flags_Multiline) != 0))
		{
			data->BufDirty = true;
			data->BufTextLen = strlen(data->Buf);
			ImGui::ClearActiveID();
			return 1;
		}
		ImGui::ClearActiveID();
	}
	return 0;
}
#endif

bool InputText(const char *label, std::string *str, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
#ifdef __SWITCH__
	if ((flags & ImGuiInputTextFlags_ReadOnly) == 0)
	{
		// TODO This doesn't handle growing the string capacity dynamically
		str->reserve(512);
		return ImGui::InputText(label, str, flags | ImGuiInputTextFlags_CallbackAlways, switchInputTextCallback);
	}
#endif
	return ImGui::InputText(label, str, flags, callback, user_data);
}

bool InputText(const char *label, char *str, size_t size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
#ifdef __SWITCH__
	if ((flags & ImGuiInputTextFlags_ReadOnly) == 0)
		return ImGui::InputText(label, str, size, flags | ImGuiInputTextFlags_CallbackAlways, switchInputTextCallback);
#endif
	return ImGui::InputText(label, str, size, flags, callback, user_data);
}

bool InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
#ifdef __SWITCH__
	if ((flags & ImGuiInputTextFlags_ReadOnly) == 0)
		return ImGui::InputTextMultiline(label, buf, buf_size, size, flags | ImGuiInputTextFlags_CallbackAlways | Flags_Multiline, switchInputTextCallback);
#endif
	return ImGui::InputTextMultiline(label, buf, buf_size, size, flags, callback, user_data);
}
