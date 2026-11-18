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
#include "metal_overlay.h"
#include "metal_texture.h"
#include "cfg/option.h"
#include "rend/osd.h"

#include <algorithm>

MetalOverlay::MetalOverlay() = default;
MetalOverlay::~MetalOverlay() = default;

void MetalOverlay::Init(MetalQuadPipeline *pipeline)
{
	this->pipeline = pipeline;
	drawer = std::make_unique<MetalQuadDrawer>();
	drawer->Init(pipeline);
	vmuLastChanged.fill({});
}

void MetalOverlay::Term()
{
	// Command buffers retain the textures they sample, so the references can
	// be dropped even if a frame using them is still in flight.
	for (auto& tex : vmuTextures)
		tex.reset();
	xhairTexture.reset();
	drawer.reset();
}

std::unique_ptr<MetalTexture> MetalOverlay::createTexture(int width, int height, const u8 *data)
{
	auto texture = std::make_unique<MetalTexture>();
	texture->tex_type = TextureType::_8888;
	texture->UploadToGPU(width, height, data, false);

	return texture;
}

void MetalOverlay::drawQuad(id<MTLRenderCommandEncoder> commandEncoder, u32 width, u32 height,
		float x, float y, float w, float h, id<MTLTexture> texture, const float *color)
{
	// The scissor rect must lie within the render target
	const float x0 = std::max(0.f, x);
	const float y0 = std::max(0.f, y);
	const float x1 = std::min((float)width, x + w);
	const float y1 = std::min((float)height, y + h);
	if (x1 <= x0 || y1 <= y0)
		return;
	MTLScissorRect scissor = { (NSUInteger)x0, (NSUInteger)y0, (NSUInteger)(x1 - x0), (NSUInteger)(y1 - y0) };
	if (scissor.width == 0 || scissor.height == 0)
		return;

	// Flipped compared to DrawFrame: the VMU image is stored bottom row first
	MetalQuadVertex vtx[4] {
		{ -1.f, -1.f, 0.f, 0.f, 0.f },
		{  1.f, -1.f, 0.f, 1.f, 0.f },
		{ -1.f,  1.f, 0.f, 0.f, 1.f },
		{  1.f,  1.f, 0.f, 1.f, 1.f },
	};
	[commandEncoder setViewport:(MTLViewport){ x, y, w, h, 0, 1 }];
	[commandEncoder setScissorRect:scissor];
	drawer->Draw(commandEncoder, texture, vtx, true, color);
}

void MetalOverlay::Draw(id<MTLRenderCommandEncoder> commandEncoder, u32 width, u32 height, float scaling, bool vmu, bool crosshair)
{
	if (drawer == nullptr || commandEncoder == nil)
		return;
	[commandEncoder pushDebugGroup:@"Overlay"];
	if (vmu)
	{
		const float vmu_padding = 8.f * scaling;
		const float vmu_height = 32.f * scaling * 2.f;
		const float vmu_width = 48.f * scaling * 2.f;
		static const float color[4] = { 0.75f, 0.75f, 0.75f, 0.75f };

		for (size_t i = 0; i < vmuTextures.size(); i++)
		{
			std::unique_ptr<MetalTexture>& texture = vmuTextures[i];
			if (!vmu_lcd_status[i])
			{
				texture.reset();
				continue;
			}
			// A new texture each time the screen changes: a frame still in
			// flight may be sampling the current one.
			if (texture == nullptr || ::vmuLastChanged[i] != vmuLastChanged[i])
			{
				texture = createTexture(48, 32, (const u8 *)vmu_lcd_data[i]);
				vmuLastChanged[i] = ::vmuLastChanged[i];
			}

			const float w = vmu_width;
			const float h = vmu_height;
			float x;
			float y;
			if (i & 2)
				x = width - vmu_padding - w;
			else
				x = vmu_padding;
			if (i & 4)
			{
				y = height - vmu_padding - h;
				if (i & 1)
					y -= vmu_padding + h;
			}
			else
			{
				y = vmu_padding;
				if (i & 1)
					y += vmu_padding + h;
			}
			drawQuad(commandEncoder, width, height, x, y, w, h, texture->GetTexture(), color);
		}
	}
	if (crosshair)
	{
		for (size_t i = 0; i < config::CrosshairColor.size(); i++)
		{
			if (!crosshairNeeded(i))
				continue;
			if (xhairTexture == nullptr)
				xhairTexture = createTexture(16, 16, (const u8 *)getCrosshairTextureData());

			auto [x, y] = getCrosshairPosition(i);
			const float w = config::CrosshairSize * scaling;
			const float h = config::CrosshairSize * scaling;
			x -= w / 2;
			y -= h / 2;
			const u32 color = config::CrosshairColor[i];
			const float xhairColor[4] {
				(color & 0xff) / 255.f,
				((color >> 8) & 0xff) / 255.f,
				((color >> 16) & 0xff) / 255.f,
				((color >> 24) & 0xff) / 255.f
			};
			drawQuad(commandEncoder, width, height, x, y, w, h, xhairTexture->GetTexture(), xhairColor);
		}
	}
	[commandEncoder popDebugGroup];
}
