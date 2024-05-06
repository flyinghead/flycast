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
#include "imgui_driver.h"
#include "gui_util.h"
#include "osd.h"
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>

constexpr float VMU_WIDTH = 96.f;
constexpr float VMU_HEIGHT = 64.f;
constexpr float VMU_PADDING = 8.f;

void ImGuiDriver::reset()
{
	aspectRatios.clear();
	for (auto& tex : vmu_lcd_tex_ids)
		tex = ImTextureID{};
	textureLoadCount = 0;
	vmuLastChanged.fill({});
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

ImTextureID ImGuiDriver::getOrLoadTexture(const std::string& path, bool nearestSampling)
{
	ImTextureID id = getTexture(path);
	if (id == ImTextureID() && textureLoadCount < 10)
	{
		textureLoadCount++;
		int width, height;
		u8 *imgData = loadImage(path, width, height);
		if (imgData != nullptr)
		{
			try {
				id = updateTextureAndAspectRatio(path, imgData, width, height, nearestSampling);
			} catch (...) {
				// vulkan can throw during resizing
			}
			free(imgData);
		}
	}
	return id;
}

void ImGuiDriver::updateVmuTextures()
{
	for (int i = 0; i < 8; i++)
	{
		if (!vmu_lcd_status[i])
			continue;

		if (this->vmuLastChanged[i] != ::vmuLastChanged[i] || vmu_lcd_tex_ids[i] == ImTextureID())
		{
			try {
				vmu_lcd_tex_ids[i] = updateTexture("__vmu" + std::to_string(i), (const u8 *)vmu_lcd_data[i], 48, 32, true);
			} catch (...) {
				 continue;
			}
			if (vmu_lcd_tex_ids[i] != ImTextureID())
				this->vmuLastChanged[i] = ::vmuLastChanged[i];
		}
	}
}

void ImGuiDriver::displayVmus(const ImVec2& pos)
{
	updateVmuTextures();
	const ScaledVec2 size(VMU_WIDTH, VMU_HEIGHT);
	const float padding = uiScaled(VMU_PADDING);
	ImDrawList *dl = ImGui::GetForegroundDrawList();
	ImVec2 cpos(pos + ScaledVec2(2.f, 0));	// 96 pixels wide + 2 * 2 -> 100
	for (int i = 0; i < 8; i++)
	{
		if (!vmu_lcd_status[i])
			continue;

		ImVec2 pos_b = cpos + size;
		dl->AddImage(vmu_lcd_tex_ids[i], cpos, pos_b, ImVec2(0, 1), ImVec2(1, 0), 0x80ffffff);
		cpos.y += size.y + padding;
	}
}

