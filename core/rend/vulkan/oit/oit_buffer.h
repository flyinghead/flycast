/*
    Created on: Nov 12, 2019

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
#include "../buffer.h"
#include "../texture.h"

#include <memory>

class OITBuffers
{
public:
	void Init(int width, int height)
	{
		const VulkanContext *context = VulkanContext::Instance();

		if (width <= maxWidth && height <= maxHeight)
			return;
		maxWidth = std::max(maxWidth, width);
		maxHeight = std::max(maxHeight, height);

		if (!pixelBuffer)
		{
			pixelBufferSize = config::PixelBufferSize;
			pixelBuffer = std::make_unique<BufferData>(std::min<vk::DeviceSize>(pixelBufferSize, context->GetMaxMemoryAllocationSize()),
					vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
		}
		if (!pixelCounter)
		{
			pixelCounter = std::make_unique<BufferData>(4,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);
			pixelCounterReset = std::make_unique<BufferData>(4, vk::BufferUsageFlagBits::eTransferSrc);
			const int zero = 0;
			pixelCounterReset->upload(sizeof(zero), &zero);
		}
		// We need to wait until this buffer is not used before deleting it
		context->WaitIdle();
		abufferPointer.reset();
		abufferPointer = std::make_unique<BufferData>(maxWidth * maxHeight * sizeof(int),
				vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
		firstFrameAfterInit = true;
	}

	void updateDescriptorSet(vk::DescriptorSet descSet, std::vector<vk::WriteDescriptorSet>& writeDescSets)
	{
		static vk::DescriptorBufferInfo pixelBufferInfo({}, 0, VK_WHOLE_SIZE);
		pixelBufferInfo.buffer = *pixelBuffer->buffer;
		writeDescSets.emplace_back(descSet, 7, 0, vk::DescriptorType::eStorageBuffer, nullptr, pixelBufferInfo);
		static vk::DescriptorBufferInfo pixelCounterBufferInfo({}, 0, 4);
		pixelCounterBufferInfo.buffer = *pixelCounter->buffer;
		writeDescSets.emplace_back(descSet, 8, 0, vk::DescriptorType::eStorageBuffer, nullptr, pixelCounterBufferInfo);
		static vk::DescriptorBufferInfo abufferPointerInfo({}, 0, VK_WHOLE_SIZE);
		abufferPointerInfo.buffer = *abufferPointer->buffer;
		writeDescSets.emplace_back(descSet, 9, 0, vk::DescriptorType::eStorageBuffer, nullptr, abufferPointerInfo);
	}

	void OnNewFrame(vk::CommandBuffer commandBuffer)
	{
		firstFrameAfterInit = false;
		if (pixelBufferSize != config::PixelBufferSize)
		{
			pixelBufferSize = config::PixelBufferSize;
			pixelBuffer = std::make_unique<BufferData>(std::min<vk::DeviceSize>(pixelBufferSize, VulkanContext::Instance()->GetMaxMemoryAllocationSize()),
					vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
		}
	}

	void ResetPixelCounter(vk::CommandBuffer commandBuffer)
	{
    	vk::BufferCopy copy(0, 0, sizeof(int));
    	commandBuffer.copyBuffer(*pixelCounterReset->buffer, *pixelCounter->buffer, copy);
	}

	void Term()
	{
		pixelBuffer.reset();
		pixelCounter.reset();
		pixelCounterReset.reset();
		abufferPointer.reset();
	}

	bool isFirstFrameAfterInit() const { return firstFrameAfterInit; }

private:
	std::unique_ptr<BufferData> pixelBuffer;
	std::unique_ptr<BufferData> pixelCounter;
	std::unique_ptr<BufferData> pixelCounterReset;
	std::unique_ptr<BufferData> abufferPointer;
	bool firstFrameAfterInit = false;
	int maxWidth = 0;
	int maxHeight = 0;
	int64_t pixelBufferSize = 0;
};
