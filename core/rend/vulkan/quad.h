/*
    Created on: Nov 7, 2019

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
#include "vulkan.h"
#include "buffer.h"

struct QuadVertex
{
	f32 pos[3];
	f32 uv[2];
};

vk::PipelineVertexInputStateCreateInfo GetQuadInputStateCreateInfo(bool uv);

class QuadBuffer
{
public:
	QuadBuffer(Allocator *allocator)
	{
		buffer = std::unique_ptr<BufferData>(new BufferData(VulkanContext::Instance()->GetPhysicalDevice(),
				VulkanContext::Instance()->GetDevice(), sizeof(QuadVertex) * 4, vk::BufferUsageFlagBits::eVertexBuffer, allocator));
	}

	void Bind(vk::CommandBuffer commandBuffer)
	{
		const vk::DeviceSize zero = 0;
		commandBuffer.bindVertexBuffers(0, 1, &buffer->buffer.get(), &zero);
	}
	void Draw(vk::CommandBuffer commandBuffer)
	{
		commandBuffer.draw(4, 1, 0, 0);
	}

	void Update(QuadVertex vertices[] = nullptr)
	{
		if (vertices == nullptr)
		{
			static QuadVertex defaultVtx[] = {
				{ { -1, -1, 0 }, { 0, 0 } },
				{ {  1, -1, 0 }, { 1, 0 } },
				{ { -1,  1, 0 }, { 0, 1 } },
				{ {  1,  1, 0 }, { 1, 1 } },
			};
			vertices = defaultVtx;
		};
		buffer->upload(sizeof(QuadVertex) * 4, vertices);
	}
private:
	std::unique_ptr<BufferData> buffer;
};
