/*
    Created on: Nov 10, 2019

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
#include "../vulkan_context.h"
#include "cfg/option.h"

class RenderPasses
{
public:
	vk::RenderPass GetRenderPass(bool initial, bool last, bool loadClear = false)
	{
		size_t index = (initial ? 1 : 0) | (last ? 2 : 0) | (loadClear ? 4 : 0);
		if (!renderPasses[index])
			renderPasses[index] = MakeRenderPass(initial, last, loadClear);
		return *renderPasses[index];
	}
	void Reset();
	virtual ~RenderPasses() = default;

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	vk::UniqueRenderPass MakeRenderPass(bool initial, bool last, bool loadClear);
	virtual vk::AttachmentDescription GetAttachment0Description(bool initial, bool last, bool loadClear) const;
	virtual std::vector<vk::SubpassDependency> GetSubpassDependencies() const;

private:
	std::array<vk::UniqueRenderPass, 8> renderPasses;
};

class RttRenderPasses : public RenderPasses
{
protected:
	vk::AttachmentDescription GetAttachment0Description(bool initial, bool last, bool loadClear) const override;
	std::vector<vk::SubpassDependency> GetSubpassDependencies() const override;
};
