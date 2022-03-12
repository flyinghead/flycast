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
			pixelBuffer = std::unique_ptr<BufferData>(new BufferData(std::min<vk::DeviceSize>(config::PixelBufferSize, context->GetMaxMemoryAllocationSize()),
					vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal));
		}
		if (!pixelCounter)
		{
			pixelCounter = std::unique_ptr<BufferData>(new BufferData(4,
					vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal));
			pixelCounterReset = std::unique_ptr<BufferData>(new BufferData(4, vk::BufferUsageFlagBits::eTransferSrc));
			const int zero = 0;
			pixelCounterReset->upload(sizeof(zero), &zero);
		}
		// We need to wait until this buffer is not used before deleting it
		context->WaitIdle();
		abufferPointer.reset();
		abufferPointer = std::unique_ptr<BufferData>(new BufferData(maxWidth * maxHeight * sizeof(int),
				vk::BufferUsageFlagBits::eStorageBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal));
		firstFrameAfterInit = true;
	}

	void updateDescriptorSet(vk::DescriptorSet descSet, std::vector<vk::WriteDescriptorSet>& writeDescSets)
	{
		static vk::DescriptorBufferInfo pixelBufferInfo({}, 0, VK_WHOLE_SIZE);
		pixelBufferInfo.buffer = *pixelBuffer->buffer;
		writeDescSets.emplace_back(descSet, 7, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &pixelBufferInfo, nullptr);
		static vk::DescriptorBufferInfo pixelCounterBufferInfo({}, 0, 4);
		pixelCounterBufferInfo.buffer = *pixelCounter->buffer;
		writeDescSets.emplace_back(descSet, 8, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &pixelCounterBufferInfo, nullptr);
		static vk::DescriptorBufferInfo abufferPointerInfo({}, 0, VK_WHOLE_SIZE);
		abufferPointerInfo.buffer = *abufferPointer->buffer;
		writeDescSets.emplace_back(descSet, 9, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &abufferPointerInfo, nullptr);
	}

	void OnNewFrame(vk::CommandBuffer commandBuffer)
	{
		firstFrameAfterInit = false;
	}

	void ResetPixelCounter(vk::CommandBuffer commandBuffer)
	{
    	vk::BufferCopy copy(0, 0, sizeof(int));
    	commandBuffer.copyBuffer(*pixelCounterReset->buffer, *pixelCounter->buffer, 1, &copy);
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
};
