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
#include "vmu.h"
#include "texture.h"
#include "rend/gui.h"

VulkanVMUs::~VulkanVMUs()
{
}

const std::vector<vk::UniqueCommandBuffer>* VulkanVMUs::PrepareVMUs(vk::CommandPool commandPool)
{
	VulkanContext *context = VulkanContext::Instance();
	commandBuffers.resize(context->GetSwapChainSize());
	commandBuffers[context->GetCurrentImageIndex()].clear();
	for (int i = 0; i < 8; i++)
	{
		std::unique_ptr<Texture>& texture = vmuTextures[i];
		if (!vmu_lcd_status[i])
		{
			texture.reset();
			continue;
		}
		if (!texture)
			texture = std::unique_ptr<Texture>(new Texture());
		else if (!vmu_lcd_changed[i])
			continue;

		texture->tex_type = TextureType::_8888;
		texture->SetDevice(context->GetDevice());
		texture->SetPhysicalDevice(context->GetPhysicalDevice());
		commandBuffers[context->GetCurrentImageIndex()].emplace_back(std::move(
				VulkanContext::Instance()->GetDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1))
				.front()));
		texture->SetCommandBuffer(*commandBuffers[context->GetCurrentImageIndex()].back());
		texture->UploadToGPU(48, 32, (u8*)vmu_lcd_data[i], false);
		texture->SetCommandBuffer(nullptr);
		vmu_lcd_changed[i] = false;
	}
	return &commandBuffers[context->GetCurrentImageIndex()];
}

void VulkanVMUs::DrawVMUs(vk::Extent2D viewport, float scaling)
{
	f32 vmu_padding = 8.f * scaling;
	f32 vmu_height = 70.f * scaling;
	f32 vmu_width = 48.f / 32.f * vmu_height;

	VulkanContext *context = VulkanContext::Instance();
	vk::CommandBuffer commandBuffer = context->GetCurrentCommandBuffer();
	pipeline->BindPipeline(commandBuffer);
	float blendConstants[4] = { 0.75f, 0.75f, 0.75f, 0.75f };
	commandBuffer.setBlendConstants(blendConstants);
	QuadVertex vtx[] = {
		{ { -1.f, -1.f, 0.f }, { 0.f, 1.f } },
		{ {  1.f, -1.f, 0.f }, { 1.f, 1.f } },
		{ { -1.f,  1.f, 0.f }, { 0.f, 0.f } },
		{ {  1.f,  1.f, 0.f }, { 1.f, 0.f } },
	};

	for (int i = 0; i < 8; i++)
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
