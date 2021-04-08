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
#include "rend/gui.h"
#include "hw/maple/maple_devs.h"
#include "overlay.h"
#include "cfg/option.h"

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

vk::CommandBuffer VulkanOverlay::Prepare(vk::CommandPool commandPool, bool vmu, bool crosshair)
{
	VulkanContext *context = VulkanContext::Instance();
	commandBuffers.resize(context->GetSwapChainSize());
	commandBuffers[context->GetCurrentImageIndex()] = std::move(
			VulkanContext::Instance()->GetDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1))
			.front());
	vk::CommandBuffer cmdBuffer = *commandBuffers[context->GetCurrentImageIndex()];
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
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
	cmdBuffer.end();

	return cmdBuffer;
}

void VulkanOverlay::Draw(vk::Extent2D viewport, float scaling, bool vmu, bool crosshair)
{
	VulkanContext *context = VulkanContext::Instance();
	vk::CommandBuffer commandBuffer = context->GetCurrentCommandBuffer();
	QuadVertex vtx[] = {
		{ { -1.f, -1.f, 0.f }, { 0.f, 1.f } },
		{ {  1.f, -1.f, 0.f }, { 1.f, 1.f } },
		{ { -1.f,  1.f, 0.f }, { 0.f, 0.f } },
		{ {  1.f,  1.f, 0.f }, { 1.f, 0.f } },
	};

	if (vmu)
	{
		f32 vmu_padding = 8.f * scaling;
		f32 vmu_height = 70.f * scaling;
		f32 vmu_width = 48.f / 32.f * vmu_height;

		pipeline->BindPipeline(commandBuffer);
		float blendConstants[4] = { 0.75f, 0.75f, 0.75f, 0.75f };
		commandBuffer.setBlendConstants(blendConstants);

		for (size_t i = 0; i < vmuTextures.size(); i++)
		{
			if (!vmuTextures[i])
				continue;
			f32 x;
			if (i & 2)
				x = viewport.width - vmu_padding - vmu_width;
			else
				x = vmu_padding;
			f32 y;
			if (i & 4)
			{
				y = viewport.height - vmu_padding - vmu_height;
				if (i & 1)
					y -= vmu_padding + vmu_height;
			}
			else
			{
				y = vmu_padding;
				if (i & 1)
					y += vmu_padding + vmu_height;
			}
			vk::Viewport viewport(x, y, vmu_width, vmu_height);
			commandBuffer.setViewport(0, 1, &viewport);
			commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(x, y), vk::Extent2D(vmu_width, vmu_height)));

			drawers[i]->Draw(commandBuffer, vmuTextures[i]->GetImageView(), vtx, true);
		}
	}
	if (crosshair && crosshairsNeeded())
	{
		alphaPipeline->BindPipeline(commandBuffer);
		for (size_t i = 0; i < config::CrosshairColor.size(); i++)
		{
			if (config::CrosshairColor[i] == 0)
				continue;
			if (settings.platform.system == DC_PLATFORM_DREAMCAST && config::MapleMainDevices[i] != MDT_LightGun)
				continue;

			float x, y;
			std::tie(x, y) = getCrosshairPosition(i);
			x -= XHAIR_WIDTH / 2;
			y -= XHAIR_HEIGHT / 2;
			vk::Viewport viewport(x, y, XHAIR_WIDTH, XHAIR_HEIGHT);
			commandBuffer.setViewport(0, 1, &viewport);
			commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(std::max(0.f, x), std::max(0.f, y)),
					vk::Extent2D(XHAIR_WIDTH, XHAIR_HEIGHT)));
			u32 color = config::CrosshairColor[i];
			float xhairColor[4] {
				(color & 0xff) / 255.f,
				((color >> 8) & 0xff) / 255.f,
				((color >> 16) & 0xff) / 255.f,
				((color >> 24) & 0xff) / 255.f
			};
			xhairDrawer->Draw(commandBuffer, xhairTexture->GetImageView(), vtx, true, xhairColor);
		}
	}
}
