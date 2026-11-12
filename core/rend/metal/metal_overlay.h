/*
	Copyright 2025 flyinghead

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
#include "metal_apple.h"
#include "metal_quad.h"

#include <array>
#include <memory>

class MetalTexture;

// Floating VMU screens and lightgun crosshairs, drawn on top of the frame
// (Metal equivalent of VulkanOverlay)
class MetalOverlay
{
public:
	MetalOverlay();
	~MetalOverlay();

	void Init(MetalQuadPipeline *pipeline);
	void Term();
	void Draw(id<MTLRenderCommandEncoder> commandEncoder, u32 width, u32 height, float scaling, bool vmu, bool crosshair);

private:
	std::unique_ptr<MetalTexture> createTexture(int width, int height, const u8 *data);
	void drawQuad(id<MTLRenderCommandEncoder> commandEncoder, u32 width, u32 height,
			float x, float y, float w, float h, id<MTLTexture> texture, const float *color);

	std::array<std::unique_ptr<MetalTexture>, 8> vmuTextures;
	std::array<u64, 8> vmuLastChanged {};
	std::unique_ptr<MetalTexture> xhairTexture;
	std::unique_ptr<MetalQuadDrawer> drawer;
	MetalQuadPipeline *pipeline = nullptr;
};
