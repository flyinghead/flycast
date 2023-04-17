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
#include "oit_renderpass.h"

vk::UniqueRenderPass RenderPasses::MakeRenderPass(bool initial, bool last)
{
    std::array<vk::AttachmentDescription, 4> attachmentDescriptions = {
    		// Swap chain image
    		GetAttachment0Description(initial, last),
			// OP+PT color attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
					initial ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
					last ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					initial ? vk::ImageLayout::eUndefined : vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal),
			// OP+PT depth attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
					initial ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
					last ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
					initial ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eDepthStencilAttachmentOptimal),
			// OP+PT depth attachment for subpass 1
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
					initial ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad,
					last ? vk::AttachmentStoreOp::eDontCare : vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
					initial ? vk::ImageLayout::eUndefined : vk::ImageLayout::eDepthStencilAttachmentOptimal, vk::ImageLayout::eDepthStencilAttachmentOptimal),
    };
    vk::AttachmentReference swapChainReference(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference colorReference(1, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference depthReference(2, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference depthReadOnlyRef(2, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
    vk::AttachmentReference depthReference2(3, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    vk::AttachmentReference colorInput(1, vk::ImageLayout::eShaderReadOnlyOptimal);

    std::array<vk::SubpassDescription, 3> subpasses = {
    	// Depth and modvol pass	FIXME subpass 0 shouldn't reference the color attachment
    	vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
    			nullptr,
				colorReference,
				nullptr,
				&depthReference),
    	// Color pass
    	vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
    			depthReadOnlyRef,
				colorReference,
				nullptr,
				&depthReference2),
    	// Final pass
    	vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
    			colorInput,
				swapChainReference,
				nullptr,
				&depthReference),	// depth-only Tr pass when continuation
    };

    std::vector<vk::SubpassDependency> dependencies = GetSubpassDependencies();
    dependencies.emplace_back(VK_SUBPASS_EXTERNAL, 1, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
    		vk::AccessFlagBits::eInputAttachmentRead, vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion);
    dependencies.emplace_back(0, 1, vk::PipelineStageFlagBits::eLateFragmentTests, vk::PipelineStageFlagBits::eFragmentShader,
    		vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead, vk::DependencyFlagBits::eByRegion);
    dependencies.emplace_back(1, 2, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
    		vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eInputAttachmentRead, vk::DependencyFlagBits::eByRegion);
    // This dependency is only needed if the render pass isn't the last: it's needed for the depth-only Tr pass
    // Unfortunately we want all render passes to be compatible, and that means all attachments must be identical
    dependencies.emplace_back(1, 2, vk::PipelineStageFlagBits::eFragmentShader,
    		vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests,
    		vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead,
			vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
			vk::DependencyFlagBits::eByRegion);
    dependencies.emplace_back(2, 2, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
    		vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite,
			vk::DependencyFlagBits::eByRegion);

    return GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
    		attachmentDescriptions,
    		subpasses,
			dependencies));
}

