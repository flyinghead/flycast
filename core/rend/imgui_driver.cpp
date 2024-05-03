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
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include <stb_image.h>

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

ImTextureID ImGuiDriver::getOrLoadTexture(const std::string& path)
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
				id = updateTextureAndAspectRatio(path, imgData, width, height);
			} catch (...) {
				// vulkan can throw during resizing
			}
			free(imgData);
		}
	}
	return id;
}
