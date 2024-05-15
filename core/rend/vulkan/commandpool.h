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
#include <memory>

class CommandPool : public FlightManager
{
public:
	void Init(size_t chainSize = 2);
	void Term();
	void BeginFrame();
	void EndFrame();
	void EndFrameAndWait();
	vk::CommandBuffer Allocate(bool submitLast = false);

	int GetIndex() const {
		return index;
	}

	void addToFlight(Deletable *object) override {
		inFlightObjects[index].emplace_back(object);
	}

private:
	int index = 0;
	std::vector<std::vector<vk::UniqueCommandBuffer>> freeBuffers;
	std::vector<std::vector<vk::UniqueCommandBuffer>> inFlightBuffers;
	std::vector<bool> lastBuffers;
	std::vector<vk::UniqueCommandPool> commandPools;
	std::vector<vk::UniqueFence> fences;
	// size should be the same as used by client: 2 for renderer (texCommandPool)
	size_t chainSize;
	std::vector<std::vector<std::unique_ptr<Deletable>>> inFlightObjects;
	bool frameStarted = false;
	vk::Device device{};
};
