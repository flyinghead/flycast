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
#pragma once
#include "quad.h"

#include <array>
#include <memory>
#include <vector>

class Texture;

class VulkanVMUs
{
public:
	~VulkanVMUs();
	void Init(QuadPipeline *pipeline) {
		this->pipeline = pipeline;
		for (auto& drawer : drawers)
		{
			drawer = std::unique_ptr<QuadDrawer>(new QuadDrawer());
			drawer->Init(pipeline);
		}
	}
	const std::vector<vk::UniqueCommandBuffer>* PrepareVMUs(vk::CommandPool commandPool);
	void DrawVMUs(vk::Extent2D viewport, float scaling);

private:
	std::array<std::unique_ptr<Texture>, 8> vmuTextures;
	std::vector<std::vector<vk::UniqueCommandBuffer>> commandBuffers;
	std::array<std::unique_ptr<QuadDrawer>, 8> drawers;
	QuadPipeline *pipeline = nullptr;
};
