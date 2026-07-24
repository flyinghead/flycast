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
	device = VulkanContext::Instance()->GetDevice();
	fencePending.resize(chainSize, false);
	if (commandPools.size() > chainSize)
	{
		commandPools.resize(chainSize);
		fences.resize(chainSize);
	}
	else
	{
		while (commandPools.size() < chainSize)
		{
			commandPools.push_back(device.createCommandPoolUnique(
					vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient, VulkanContext::Instance()->GetGraphicsQueueFamilyIndex())));
			fences.push_back(device.createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
		}
	}
	freeBuffers.resize(chainSize);
	inFlightBuffers.resize(chainSize);
	inFlightObjects.resize(chainSize);
}

void CommandPool::Term()
{
	std::vector<vk::Fence> pendingFences;
	for (size_t i = 0; i < fences.size(); ++i)
	{
		if (fencePending[i])
			pendingFences.push_back(fences[i].get());
	}
	if (!pendingFences.empty())
	{
		vk::Result res = device.waitForFences(pendingFences, true, UINT64_MAX);
		if (res != vk::Result::eSuccess)
			WARN_LOG(RENDERER, "CommandPool::Term: waitForFences failed %d", (int)res);
	}
	inFlightObjects.clear();
	freeBuffers.clear();
	inFlightBuffers.clear();
	fences.clear();
	fencePending.clear();
	commandPools.clear();
	frameStarted = false;
}

void CommandPool::BeginFrame()
{
	if (frameStarted)
		return;
	frameStarted = true;
	index = (index + 1) % chainSize;
	if (fencePending[index])
	{
		vk::Result res = device.waitForFences(fences[index].get(), true, UINT64_MAX);
		if (res != vk::Result::eSuccess)
		{
			WARN_LOG(RENDERER, "CommandPool::BeginFrame: waitForFences failed %d", (int)res);
			vk::detail::resultCheck(res, "CommandPool::BeginFrame");
		}
		fencePending[index] = false;
	}
	std::vector<vk::UniqueCommandBuffer>& inFlightBuf = inFlightBuffers[index];
	std::vector<vk::UniqueCommandBuffer>& freeBuf = freeBuffers[index];
	std::move(inFlightBuf.begin(), inFlightBuf.end(), std::back_inserter(freeBuf));
	inFlightBuf.clear();
	device.resetCommandPool(*commandPools[index], vk::CommandPoolResetFlagBits::eReleaseResources);
	inFlightObjects[index].clear();
	lastBuffers.clear();
}

void CommandPool::EndFrame()
{
	if (!frameStarted)
		return;
	frameStarted = false;
	std::vector<vk::CommandBuffer> commandBuffers = vk::uniqueToRaw(inFlightBuffers[index]);
	if (!commandBuffers.empty())
	{
		// sort buffers: !last, last
		size_t len = commandBuffers.size() - 1;
		while (len != 0)
		{
			for (size_t i = 0; i < len; i++)
				if (lastBuffers[i] && !lastBuffers[i + 1]) {
					std::vector<bool>::swap(lastBuffers[i], lastBuffers[i + 1]);
					std::swap(commandBuffers[i], commandBuffers[i + 1]);
				}
			len--;
		}
	}
	// A failed submission leaves the reset fence unsignaled. Mark it pending
	// only after the queue accepts the command buffers.
	fencePending[index] = false;
	device.resetFences(fences[index].get());
	VulkanContext::Instance()->SubmitCommandBuffers(commandBuffers, *fences[index]);
	fencePending[index] = true;
}

vk::CommandBuffer CommandPool::Allocate(bool submitLast)
{
	if (freeBuffers[index].empty())
	{
		inFlightBuffers[index].emplace_back(std::move(
				device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*commandPools[index], vk::CommandBufferLevel::ePrimary, 1))
				.front()));
	}
	else
	{
		inFlightBuffers[index].emplace_back(std::move(freeBuffers[index].back()));
		freeBuffers[index].pop_back();
	}
	lastBuffers.push_back(submitLast);
	return *inFlightBuffers[index].back();
}

void CommandPool::EndFrameAndWait()
{
	EndFrame();
	vk::Result res = device.waitForFences(fences[index].get(), true, UINT64_MAX);
	if (res != vk::Result::eSuccess)
	{
		WARN_LOG(RENDERER, "CommandPool::waitForCommandCompletion: waitForFences failed %d", (int)res);
		vk::detail::resultCheck(res, "CommandPool::EndFrameAndWait");
	}
	fencePending[index] = false;
	inFlightObjects[index].clear();
}
