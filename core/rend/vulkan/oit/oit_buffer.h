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
		if (!descSetLayout)
		{
			// Descriptor set and pipeline layout
			vk::DescriptorSetLayoutBinding descSetLayoutBindings[] = {
					{ 0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// pixel buffer
					{ 1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// pixel counter
					{ 2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// a-buffer pointers
			};

			descSetLayout = context->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(descSetLayoutBindings), descSetLayoutBindings));
		}
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

		if (!descSet)
			descSet = std::move(context->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(context->GetDescriptorPool(), 1, &descSetLayout.get())).front());
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		vk::DescriptorBufferInfo pixelBufferInfo(*pixelBuffer->buffer, 0, VK_WHOLE_SIZE);
		writeDescriptorSets.emplace_back(*descSet, 0, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &pixelBufferInfo, nullptr);
		vk::DescriptorBufferInfo pixelCounterBufferInfo(*pixelCounter->buffer, 0, 4);
		writeDescriptorSets.emplace_back(*descSet, 1, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &pixelCounterBufferInfo, nullptr);
		vk::DescriptorBufferInfo abufferPointerInfo(*abufferPointer->buffer, 0, VK_WHOLE_SIZE);
		writeDescriptorSets.emplace_back(*descSet, 2, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &abufferPointerInfo, nullptr);
		context->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void BindDescriptorSet(vk::CommandBuffer cmdBuffer, vk::PipelineLayout pipelineLayout, u32 firstSet)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, firstSet, 1, &descSet.get(), 0, nullptr);
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

	vk::DescriptorSetLayout GetDescriptorSetLayout() const { return *descSetLayout; }
	bool isFirstFrameAfterInit() const { return firstFrameAfterInit; }

private:
	vk::UniqueDescriptorSet descSet;
	vk::UniqueDescriptorSetLayout descSetLayout;

	std::unique_ptr<BufferData> pixelBuffer;
	std::unique_ptr<BufferData> pixelCounter;
	std::unique_ptr<BufferData> pixelCounterReset;
	std::unique_ptr<BufferData> abufferPointer;
	bool firstFrameAfterInit = false;
	int maxWidth = 0;
	int maxHeight = 0;
};
