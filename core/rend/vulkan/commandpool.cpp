/*
	Copyright 2023 flyinghead

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
#include "commandpool.h"
#include "vulkan_context.h"

void CommandPool::Init(size_t chainSize)
{
	this->chainSize = chainSize;
	if (commandPools.size() > chainSize)
	{
		commandPools.resize(chainSize);
		fences.resize(chainSize);
	}
	else
	{
		while (commandPools.size() < chainSize)
		{
			commandPools.push_back(VulkanContext::Instance()->GetDevice().createCommandPoolUnique(
					vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient, VulkanContext::Instance()->GetGraphicsQueueFamilyIndex())));
			fences.push_back(VulkanContext::Instance()->GetDevice().createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
		}
	}
	freeBuffers.resize(chainSize);
	inFlightBuffers.resize(chainSize);
	inFlightObjects.resize(chainSize);
}

void CommandPool::Term()
{
	if (!fences.empty())
	{
		std::vector<vk::Fence> allFences = vk::uniqueToRaw(fences);
		vk::Result res = VulkanContext::Instance()->GetDevice().waitForFences(allFences, true, UINT64_MAX);
		if (res != vk::Result::eSuccess)
			WARN_LOG(RENDERER, "CommandPool::Term: waitForFences failed %d", (int)res);
	}
	inFlightObjects.clear();
	freeBuffers.clear();
	inFlightBuffers.clear();
	fences.clear();
	commandPools.clear();
}

void CommandPool::BeginFrame()
{
	if (frameStarted)
		return;
	frameStarted = true;
	index = (index + 1) % chainSize;
	vk::Result res = VulkanContext::Instance()->GetDevice().waitForFences(fences[index].get(), true, UINT64_MAX);
	if (res != vk::Result::eSuccess)
		WARN_LOG(RENDERER, "CommandPool::BeginFrame: waitForFences failed %d", (int)res);
	std::vector<vk::UniqueCommandBuffer>& inFlightBuf = inFlightBuffers[index];
	std::vector<vk::UniqueCommandBuffer>& freeBuf = freeBuffers[index];
	std::move(inFlightBuf.begin(), inFlightBuf.end(), std::back_inserter(freeBuf));
	inFlightBuf.clear();
	VulkanContext::Instance()->GetDevice().resetCommandPool(*commandPools[index], vk::CommandPoolResetFlagBits::eReleaseResources);
	inFlightObjects[index].clear();
}

void CommandPool::EndFrame()
{
	if (!frameStarted)
		return;
	frameStarted = false;
	std::vector<vk::CommandBuffer> commandBuffers = vk::uniqueToRaw(inFlightBuffers[index]);
	VulkanContext::Instance()->GetDevice().resetFences(fences[index].get());
	VulkanContext::Instance()->SubmitCommandBuffers(commandBuffers, *fences[index]);
}

vk::CommandBuffer CommandPool::Allocate()
{
	if (freeBuffers[index].empty())
	{
		inFlightBuffers[index].emplace_back(std::move(
				VulkanContext::Instance()->GetDevice().allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*commandPools[index], vk::CommandBufferLevel::ePrimary, 1))
				.front()));
	}
	else
	{
		inFlightBuffers[index].emplace_back(std::move(freeBuffers[index].back()));
		freeBuffers[index].pop_back();
	}
	return *inFlightBuffers[index].back();
}

void CommandPool::EndFrameAndWait()
{
	EndFrame();
	vk::Result res = VulkanContext::Instance()->GetDevice().waitForFences(fences[index].get(), true, UINT64_MAX);
	if (res != vk::Result::eSuccess)
		WARN_LOG(RENDERER, "CommandPool::waitForCommandCompletion: waitForFences failed %d", (int)res);
	inFlightObjects[index].clear();
}
