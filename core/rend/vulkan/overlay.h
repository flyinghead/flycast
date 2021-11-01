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

class VulkanOverlay
{
public:
	~VulkanOverlay();

	void Init(QuadPipeline *pipeline)
	{
		this->pipeline = pipeline;
		for (auto& drawer : drawers)
		{
			drawer = std::unique_ptr<QuadDrawer>(new QuadDrawer());
			drawer->Init(pipeline);
		}
		xhairDrawer = std::unique_ptr<QuadDrawer>(new QuadDrawer());
		xhairDrawer->Init(pipeline);
	}

	void Term()
	{
		commandBuffers.clear();
		for (auto& drawer : drawers)
			drawer.reset();
		xhairDrawer.reset();
	}

	vk::CommandBuffer Prepare(vk::CommandPool commandPool, bool vmu, bool crosshair);
	void Prepare(vk::CommandBuffer commandBuffer, bool vmu, bool crosshair);
	void Draw(vk::CommandBuffer commandBuffer, vk::Extent2D viewport, float scaling, bool vmu, bool crosshair);

private:
	std::unique_ptr<Texture> createTexture(vk::CommandBuffer commandBuffer, int width, int height, u8 *data);

	std::array<std::unique_ptr<Texture>, 8> vmuTextures;
	std::vector<vk::UniqueCommandBuffer> commandBuffers;
	std::array<std::unique_ptr<QuadDrawer>, 8> drawers;
	QuadPipeline *pipeline = nullptr;

	std::unique_ptr<Texture> xhairTexture;
	std::unique_ptr<QuadDrawer> xhairDrawer;
};
