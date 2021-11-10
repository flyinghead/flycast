/*
    Created on: Dec 13, 2019

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
#include "texture.h"
#include "hw/maple/maple_devs.h"
#include "overlay.h"
#include "cfg/option.h"
#include "rend/osd.h"
#ifdef LIBRETRO
#include "vmu_xhair.h"
#endif

VulkanOverlay::~VulkanOverlay() = default;

std::unique_ptr<Texture> VulkanOverlay::createTexture(vk::CommandBuffer commandBuffer, int width, int height, u8 *data)
{
	VulkanContext *context = VulkanContext::Instance();
	auto texture = std::unique_ptr<Texture>(new Texture());
	texture->tex_type = TextureType::_8888;
	texture->SetDevice(context->GetDevice());
	texture->SetPhysicalDevice(context->GetPhysicalDevice());
	texture->SetCommandBuffer(commandBuffer);
	texture->UploadToGPU(width, height, data, false);
	texture->SetCommandBuffer(nullptr);

	return texture;
}

void VulkanOverlay::Prepare(vk::CommandBuffer cmdBuffer, bool vmu, bool crosshair)
{
	if (vmu)
	{
		for (size_t i = 0; i < vmuTextures.size(); i++)
		{
			std::unique_ptr<Texture>& texture = vmuTextures[i];
			if (!vmu_lcd_status[i])
			{
				texture.reset();
				continue;
			}
			if (texture != nullptr && !vmu_lcd_changed[i])
				continue;

			texture = createTexture(cmdBuffer, 48, 32, (u8*)vmu_lcd_data[i]);
			vmu_lcd_changed[i] = false;
		}
	}
	if (crosshair && !xhairTexture)
	{
		const u32* texData = getCrosshairTextureData();
		xhairTexture = createTexture(cmdBuffer, 16, 16, (u8*)texData);
	}
}

vk::CommandBuffer VulkanOverlay::Prepare(vk::CommandPool commandPool, bool vmu, bool crosshair)
{
	VulkanContext *context = VulkanContext::Instance();
	commandBuffers.resize(context->GetSwapChainSize());
	commandBuffers[context->GetCurrentImageIndex()] = std::move(
			VulkanContext::Instance()->GetDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1))
			.front());
	vk::CommandBuffer cmdBuffer = *commandBuffers[context->GetCurrentImageIndex()];
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	Prepare(cmdBuffer, vmu, crosshair);
	cmdBuffer.end();

	return cmdBuffer;
}

void VulkanOverlay::Draw(vk::CommandBuffer commandBuffer, vk::Extent2D viewport, float scaling, bool vmu, bool crosshair)
{
	QuadVertex vtx[] = {
		{ { -1.f, -1.f, 0.f }, { 0.f, 1.f } },
		{ {  1.f, -1.f, 0.f }, { 1.f, 1.f } },
		{ { -1.f,  1.f, 0.f }, { 0.f, 0.f } },
		{ {  1.f,  1.f, 0.f }, { 1.f, 0.f } },
	};

	if (vmu)
	{
		f32 vmu_padding = 8.f * scaling;
		f32 vmu_height = 32.f * scaling;
		f32 vmu_width = 48.f * scaling;

		pipeline->BindPipeline(commandBuffer);
		const float *color = nullptr;
#ifndef LIBRETRO
		vmu_height *= 2.f;
		vmu_width *= 2.f;
		float blendConstants[4] = { 0.75f, 0.75f, 0.75f, 0.75f };
		color = blendConstants;
#endif

		for (size_t i = 0; i < vmuTextures.size(); i++)
		{
			if (!vmuTextures[i])
				continue;
			float x;
			float y;
			float w = vmu_width;
			float h = vmu_height;
#ifdef LIBRETRO
			if (i & 1)
				continue;
			w *= vmu_screen_params[i / 2].vmu_screen_size_mult;
			h *= vmu_screen_params[i / 2].vmu_screen_size_mult;
			switch (vmu_screen_params[i / 2].vmu_screen_position)
			{
			case UPPER_LEFT:
			default:
				x = vmu_padding;
				y = vmu_padding;
				break;
			case UPPER_RIGHT:
				x = viewport.width - vmu_padding - w;
				y = vmu_padding;
				break;
			case LOWER_LEFT:
				x = vmu_padding;
				y = viewport.height - vmu_padding - h;
				break;
			case LOWER_RIGHT:
				x = viewport.width - vmu_padding - w;
				y = viewport.height - vmu_padding - h;
				break;
			}
#else
			if (i & 2)
				x = viewport.width - vmu_padding - w;
			else
				x = vmu_padding;
			if (i & 4)
			{
				y = viewport.height - vmu_padding - h;
				if (i & 1)
					y -= vmu_padding + h;
			}
			else
			{
				y = vmu_padding;
				if (i & 1)
					y += vmu_padding + h;
			}
#endif
			vk::Viewport viewport(x, y, w, h);
			commandBuffer.setViewport(0, 1, &viewport);
			commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(x, y), vk::Extent2D(w, h)));

			drawers[i]->Draw(commandBuffer, vmuTextures[i]->GetImageView(), vtx, true, color);
		}
	}
	if (crosshair && crosshairsNeeded())
	{
		pipeline->BindPipeline(commandBuffer);
		for (size_t i = 0; i < config::CrosshairColor.size(); i++)
		{
			if (config::CrosshairColor[i] == 0)
				continue;
			if (settings.platform.system == DC_PLATFORM_DREAMCAST && config::MapleMainDevices[i] != MDT_LightGun)
				continue;

			float x, y;
			std::tie(x, y) = getCrosshairPosition(i);

#ifdef LIBRETRO
			float w = LIGHTGUN_CROSSHAIR_SIZE * scaling;
			float h = LIGHTGUN_CROSSHAIR_SIZE * scaling;
#else
			float w = XHAIR_WIDTH * scaling;
			float h = XHAIR_HEIGHT * scaling;
#endif
			x -= w / 2;
			y -= h / 2;
			vk::Viewport viewport(x, y, w, h);
			commandBuffer.setViewport(0, 1, &viewport);
			commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(std::max(0.f, x), std::max(0.f, y)),
					vk::Extent2D(w, h)));
			u32 color = config::CrosshairColor[i];
			float xhairColor[4] {
				(color & 0xff) / 255.f,
				((color >> 8) & 0xff) / 255.f,
				((color >> 16) & 0xff) / 255.f,
				((color >> 24) & 0xff) / 255.f
			};
			xhairDrawer->Draw(commandBuffer, i == 0 ? xhairTexture->GetImageView() : vk::ImageView(), vtx, true, xhairColor);
		}
	}
}
