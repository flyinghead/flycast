/*
	Created on: Oct 8, 2019

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
#include "vulkan_context.h"

class CommandPool
{
public:
	void Init()
	{
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
		if (freeBuffers.size() != chainSize)
			freeBuffers.resize(chainSize);
		if (inFlightBuffers.size() != chainSize)
			inFlightBuffers.resize(chainSize);
	}

	void Term()
	{
		freeBuffers.clear();
		inFlightBuffers.clear();
		fences.clear();
		commandPools.clear();
	}

	void EndFrame()
	{
		std::vector<vk::CommandBuffer> commandBuffers = vk::uniqueToRaw(inFlightBuffers[index]);
		VulkanContext::Instance()->SubmitCommandBuffers(commandBuffers.size(), commandBuffers.data(), *fences[index]);
	}

	void BeginFrame()
	{
		index = (index + 1) % chainSize;
		VulkanContext::Instance()->GetDevice().waitForFences(1, &fences[index].get(), true, UINT64_MAX);
		VulkanContext::Instance()->GetDevice().resetFences(1, &fences[index].get());
		std::vector<vk::UniqueCommandBuffer>& inFlight = inFlightBuffers[index];
		std::vector<vk::UniqueCommandBuffer>& freeBuf = freeBuffers[index];
		std::move(inFlight.begin(), inFlight.end(), std::back_inserter(freeBuf));
		inFlight.clear();
		VulkanContext::Instance()->GetDevice().resetCommandPool(*commandPools[index], vk::CommandPoolResetFlagBits::eReleaseResources);
	}

	vk::CommandBuffer Allocate()
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

	vk::Fence GetCurrentFence()
	{
		return *fences[index];
	}

	int GetIndex() const
	{
		return index;
	}

private:
	int index = 0;
	std::vector<std::vector<vk::UniqueCommandBuffer>> freeBuffers;
	std::vector<std::vector<vk::UniqueCommandBuffer>> inFlightBuffers;
	std::vector<vk::UniqueCommandPool> commandPools;
	std::vector<vk::UniqueFence> fences;
	// size should be the same as used by client: 2 for renderer (texCommandPool)
	static constexpr size_t chainSize = 2;
};
