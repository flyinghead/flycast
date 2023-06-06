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
#include "vulkan.h"

#include <vector>

class CommandPool
{
public:
	void Init(size_t chainSize = 2);

	void Term()
	{
		freeBuffers.clear();
		inFlightBuffers.clear();
		fences.clear();
		commandPools.clear();
	}

	void BeginFrame();
	void EndFrame();
	vk::CommandBuffer Allocate();

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
	size_t chainSize;
};
