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

class RenderPasses
{
public:
	vk::RenderPass GetRenderPass(bool initial, bool last)
	{
		size_t index = (initial ? 1 : 0) | (last ? 2 : 0);
		if (!renderPasses[index])
			renderPasses[index] = MakeRenderPass(initial, last);
		return *renderPasses[index];
	}
	void Reset()
	{
		for (auto& renderPass : renderPasses)
			renderPass.reset();
	}
	virtual ~RenderPasses() = default;

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	vk::UniqueRenderPass MakeRenderPass(bool initial, bool last);
	virtual vk::AttachmentDescription GetAttachment0Description(bool initial, bool last) const
	{
		return vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetColorFormat(), vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	virtual vk::Format GetColorFormat() const { return GetContext()->GetColorFormat(); }
	virtual std::vector<vk::SubpassDependency> GetSubpassDependencies() const
	{
		std::vector<vk::SubpassDependency> deps;
		deps.emplace_back(2, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::DependencyFlagBits::eByRegion);
		return deps;
	}

private:
	std::array<vk::UniqueRenderPass, 4> renderPasses;
};

class RttRenderPasses : public RenderPasses
{
protected:
	virtual vk::AttachmentDescription GetAttachment0Description(bool initial, bool last) const override
	{
		return vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined,
				settings.rend.RenderToTextureBuffer && last ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	virtual vk::Format GetColorFormat() const override { return vk::Format::eR8G8B8A8Unorm; }
	virtual std::vector<vk::SubpassDependency> GetSubpassDependencies() const override
	{
		std::vector<vk::SubpassDependency> deps;
		if (settings.rend.RenderToTextureBuffer)
			deps.emplace_back(2, VK_SUBPASS_EXTERNAL,
					vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eHost,
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eHostRead);
		else
			deps.emplace_back(2, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead);
		return deps;
	}
};
