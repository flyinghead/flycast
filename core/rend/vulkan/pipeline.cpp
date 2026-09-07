/*
 *  Created on: Oct 3, 2019

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
#include "pipeline.h"
#include "hw/pvr/Renderer_if.h"

void BaseDescriptorSets::term()
{
	dummyImageView.reset();
	dummyImage.reset();
	dummyImageAllocation = {};
	dummyBuffer.reset();
	dummyBufferAllocation = {};
}

const vk::DescriptorImageInfo& BaseDescriptorSets::getEmptyImageInfo()
{
	if (!dummyImageView)
	{
		vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm, vk::Extent3D(1, 1, 1), 1, 1,
											vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eSampled,
											vk::SharingMode::eExclusive, nullptr, vk::ImageLayout::eUndefined);
		dummyImage = getContext()->GetDevice().createImageUnique(imageCreateInfo);

		VmaAllocationCreateInfo allocCreateInfo { VmaAllocationCreateFlags(), VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY };
		dummyImageAllocation = getContext()->GetAllocator().AllocateForImage(*dummyImage, allocCreateInfo);

		vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), *dummyImage, vk::ImageViewType::e2D, vk::Format::eR8G8B8A8Unorm, vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		dummyImageView = getContext()->GetDevice().createImageViewUnique(imageViewCreateInfo);
		emptyImageInfo = { samplerManager->GetSampler(TSP{}), *dummyImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
	}
	return emptyImageInfo;
}

const vk::DescriptorBufferInfo& BaseDescriptorSets::getEmptyBufferInfo()
{
	if (!dummyBuffer)
	{
		dummyBuffer = getContext()->GetDevice().createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), 16, vk::BufferUsageFlagBits::eUniformBuffer));
		VmaAllocationCreateInfo allocInfo {};
		allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		dummyBufferAllocation = getContext()->GetAllocator().AllocateForBuffer(*dummyBuffer, allocInfo);
		emptyBufferInfo = vk::DescriptorBufferInfo(*dummyBuffer, 0, 16);
	}
	return emptyBufferInfo;
}

void DescriptorSets::init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout, vk::DescriptorSetLayout perPolyLayout)
{
	BaseDescriptorSets::init(samplerManager);
	this->pipelineLayout = pipelineLayout;
	perFrameAlloc.setLayout(perFrameLayout);
	perPolyAlloc.setLayout(perPolyLayout);

}

void DescriptorSets::updateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset,
		vk::ImageView fogImageView, vk::ImageView paletteImageView, vk::ImageView secAccum)
{
	perFrameDescSet = perFrameAlloc.alloc();
	perPolyDescSets.clear();

	std::vector<vk::DescriptorBufferInfo> bufferInfos;
	bufferInfos.emplace_back(buffer, vertexUniformOffset, sizeof(VertexShaderUniforms));
	bufferInfos.emplace_back(buffer, fragmentUniformOffset, sizeof(FragmentShaderUniforms));

	std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
	writeDescriptorSets.emplace_back(perFrameDescSet, 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfos[0]);
	writeDescriptorSets.emplace_back(perFrameDescSet, 1, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfos[1]);
	if (fogImageView)
	{
		TSP fogTsp = {};
		fogTsp.FilterMode = 1;
		fogTsp.ClampU = 1;
		fogTsp.ClampV = 1;
		vk::Sampler fogSampler = samplerManager->GetSampler(fogTsp);
		static vk::DescriptorImageInfo imageInfo;
		imageInfo = { fogSampler, fogImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
		writeDescriptorSets.emplace_back(perFrameDescSet, 2, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
	}
	else {
		writeDescriptorSets.emplace_back(perFrameDescSet, 2, 0, vk::DescriptorType::eCombinedImageSampler, getEmptyImageInfo());
	}
	if (paletteImageView)
	{
		TSP palTsp = {};
		palTsp.FilterMode = 0;
		palTsp.ClampU = 1;
		palTsp.ClampV = 1;
		vk::Sampler palSampler = samplerManager->GetSampler(palTsp);
		static vk::DescriptorImageInfo imageInfo;
		imageInfo = { palSampler, paletteImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
		writeDescriptorSets.emplace_back(perFrameDescSet, 3, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
	}
	else {
		writeDescriptorSets.emplace_back(perFrameDescSet, 3, 0, vk::DescriptorType::eCombinedImageSampler, getEmptyImageInfo());
	}
	if (secAccum)
	{
		static vk::DescriptorImageInfo imageInfo;
		imageInfo.imageView = secAccum;
		imageInfo.imageLayout = vk::ImageLayout::eRenderingLocalReadKHR;
		writeDescriptorSets.emplace_back(perFrameDescSet, 4, 0, vk::DescriptorType::eInputAttachment, imageInfo);
	}
	getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
}

void DescriptorSets::bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const PolyParam& poly, int polyNumber, vk::Buffer buffer,
		vk::DeviceSize uniformOffset, vk::DeviceSize lightOffset, bool punchThrough)
{
	vk::DescriptorSet perPolyDescSet;
	auto it = perPolyDescSets.find(&poly);
	if (it == perPolyDescSets.end())
	{
		perPolyDescSet = perPolyAlloc.alloc();
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;

		vk::DescriptorImageInfo imageInfo;
		if (poly.texture != nullptr)
		{
			imageInfo = vk::DescriptorImageInfo(samplerManager->GetSampler(poly, punchThrough),
					((Texture *)poly.texture)->GetReadOnlyImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
			writeDescriptorSets.emplace_back(perPolyDescSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
		}
		else {
			writeDescriptorSets.emplace_back(perPolyDescSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, getEmptyImageInfo());
		}

		vk::DescriptorBufferInfo uniBufferInfo;
		vk::DescriptorBufferInfo lightBufferInfo;
		if (poly.isNaomi2())
		{
			const vk::DeviceSize uniformAlignment = VulkanContext::Instance()->GetUniformBufferAlignment();
			size_t size = sizeof(N2VertexShaderUniforms) + align(sizeof(N2VertexShaderUniforms), uniformAlignment);
			uniBufferInfo = vk::DescriptorBufferInfo{ buffer, uniformOffset + polyNumber * size, sizeof(N2VertexShaderUniforms) };
			writeDescriptorSets.emplace_back(perPolyDescSet, 2, 0, vk::DescriptorType::eUniformBuffer, nullptr, uniBufferInfo);

			size = sizeof(N2LightModel) + align(sizeof(N2LightModel), uniformAlignment);
			lightBufferInfo = vk::DescriptorBufferInfo{ buffer, lightOffset + poly.lightModel * size, sizeof(N2LightModel) };
			writeDescriptorSets.emplace_back(perPolyDescSet, 3, 0, vk::DescriptorType::eUniformBuffer, nullptr, lightBufferInfo);
		}
		else {
			writeDescriptorSets.emplace_back(perPolyDescSet, 2, 0, vk::DescriptorType::eUniformBuffer, nullptr, getEmptyBufferInfo());
			writeDescriptorSets.emplace_back(perPolyDescSet, 3, 0, vk::DescriptorType::eUniformBuffer, nullptr, getEmptyBufferInfo());
		}

		getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
		perPolyDescSets[&poly] = perPolyDescSet;
	}
	else {
		perPolyDescSet = it->second;
	}
	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, perPolyDescSet, nullptr);
}

void DescriptorSets::bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const ModifierVolumeParam& mvParam, int polyNumber, vk::Buffer buffer,
		vk::DeviceSize uniformOffset)
{
	if (!mvParam.isNaomi2())
		return;
	vk::DescriptorSet perPolyDescSet;
	auto it = perPolyDescSets.find(&mvParam);
	if (it == perPolyDescSets.end())
	{
		perPolyDescSet = perPolyAlloc.alloc();

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		const vk::DeviceSize uniformAlignment = VulkanContext::Instance()->GetUniformBufferAlignment();
		size_t size = sizeof(N2VertexShaderUniforms) + align(sizeof(N2VertexShaderUniforms), uniformAlignment);
		vk::DescriptorBufferInfo uniBufferInfo{ buffer, uniformOffset + polyNumber * size, sizeof(N2VertexShaderUniforms) };
		writeDescriptorSets.emplace_back(perPolyDescSet, 2, 0, vk::DescriptorType::eUniformBuffer, nullptr, uniBufferInfo);

		writeDescriptorSets.emplace_back(perPolyDescSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, getEmptyImageInfo());
		writeDescriptorSets.emplace_back(perPolyDescSet, 3, 0, vk::DescriptorType::eUniformBuffer, nullptr, getEmptyBufferInfo());

		getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
		perPolyDescSets[&mvParam] = perPolyDescSet;
	}
	else {
		perPolyDescSet = it->second;
	}
	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, perPolyDescSet, nullptr);
}

void DescriptorSets::nextFrame()
{
	perFrameAlloc.nextFrame();
	perPolyAlloc.nextFrame();
	perFrameDescSet = vk::DescriptorSet{};
	perPolyDescSets.clear();
}

void DescriptorSets::term()
{
	perFrameAlloc.term();
	perPolyAlloc.term();
	BaseDescriptorSets::term();
}

void PipelineManager::CreateModVolPipeline(ModVolMode mode, int cullMode, bool naomi2)
{
	// Vertex input state
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo;

	if (mode == ModVolMode::Final)
	{
		pipelineVertexInputStateCreateInfo = GetMainVertexInputStateCreateInfo(false, naomi2);
		pipelineInputAssemblyStateCreateInfo = vk::PipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
				vk::PrimitiveTopology::eTriangleStrip);
	}
	else
	{
		static const vk::VertexInputBindingDescription vertexBindingDescription(0, sizeof(float) * 3);
		static const vk::VertexInputAttributeDescription vertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0);	// pos
		pipelineVertexInputStateCreateInfo = vk::PipelineVertexInputStateCreateInfo(
				vk::PipelineVertexInputStateCreateFlags(),
				vertexBindingDescription,
				vertexInputAttributeDescription);
		pipelineInputAssemblyStateCreateInfo = vk::PipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
				vk::PrimitiveTopology::eTriangleList);
	}

	// Viewport and scissor states
	vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

	// Rasterization and multisample states
	vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo
	(
	  vk::PipelineRasterizationStateCreateFlags(),  // flags
	  false,                                        // depthClampEnable
	  false,                                        // rasterizerDiscardEnable
	  vk::PolygonMode::eFill,                       // polygonMode
	  cullMode == 3 ? vk::CullModeFlagBits::eBack
			  : cullMode == 2 ? vk::CullModeFlagBits::eFront
			  : vk::CullModeFlagBits::eNone,        // cullMode
	  vk::FrontFace::eCounterClockwise,             // frontFace
	  false,                                        // depthBiasEnable
	  0.0f,                                         // depthBiasConstantFactor
	  0.0f,                                         // depthBiasClamp
	  0.0f,                                         // depthBiasSlopeFactor
	  1.0f                                          // lineWidth
	);
	vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;

	// Depth and stencil
	vk::StencilOpState stencilOpState;
	switch (mode)
	{
	case ModVolMode::Xor:
		stencilOpState = vk::StencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eInvert, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 0, 2, 2);
		break;
	case ModVolMode::Or:
		stencilOpState = vk::StencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eReplace, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 2, 2, 2);
		break;
	case ModVolMode::Inclusion:
		stencilOpState = vk::StencilOpState(vk::StencilOp::eZero, vk::StencilOp::eReplace, vk::StencilOp::eZero, vk::CompareOp::eLessOrEqual, 3, 3, 1);
		break;
	case ModVolMode::Exclusion:
		stencilOpState = vk::StencilOpState(vk::StencilOp::eZero, vk::StencilOp::eKeep, vk::StencilOp::eZero, vk::CompareOp::eEqual, 3, 3, 1);
		break;
	case ModVolMode::Final:
		stencilOpState = vk::StencilOpState(vk::StencilOp::eZero, vk::StencilOp::eZero, vk::StencilOp::eZero, vk::CompareOp::eEqual, 0x81, 3, 0x81);
		break;
	}
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo
	(
	  vk::PipelineDepthStencilStateCreateFlags(), // flags
	  mode == ModVolMode::Xor || mode == ModVolMode::Or, // depthTestEnable
	  false,                                      // depthWriteEnable
	  vk::CompareOp::eGreater,                    // depthCompareOp
	  false,                                      // depthBoundTestEnable
	  true,                                       // stencilTestEnable
	  stencilOpState,                             // front
	  stencilOpState                              // back
	);

	// Color flags and blending
	vk::ColorComponentFlags colorComponentFlags(
			mode != ModVolMode::Final ? (vk::ColorComponentFlagBits)0
					: vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentStates[] {
		{
		  mode == ModVolMode::Final,			// blendEnable
		  vk::BlendFactor::eSrcAlpha,     		// srcColorBlendFactor
		  vk::BlendFactor::eOneMinusSrcAlpha,   // dstColorBlendFactor
		  vk::BlendOp::eAdd,             		// colorBlendOp
		  vk::BlendFactor::eSrcAlpha,     		// srcAlphaBlendFactor
		  vk::BlendFactor::eOneMinusSrcAlpha,   // dstAlphaBlendFactor
		  vk::BlendOp::eAdd,             		// alphaBlendOp
		  colorComponentFlags            		// colorWriteMask
		},
		{
		  mode == ModVolMode::Final,			// blendEnable
		  vk::BlendFactor::eSrcAlpha,     		// srcColorBlendFactor
		  vk::BlendFactor::eOneMinusSrcAlpha,   // dstColorBlendFactor
		  vk::BlendOp::eAdd,             		// colorBlendOp
		  vk::BlendFactor::eSrcAlpha,     		// srcAlphaBlendFactor
		  vk::BlendFactor::eOneMinusSrcAlpha,   // dstAlphaBlendFactor
		  vk::BlendOp::eAdd,             		// alphaBlendOp
		  colorComponentFlags            		// colorWriteMask
		},
	};

	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
		vk::PipelineColorBlendStateCreateFlags(),   // flags
		false,                                      // logicOpEnable
		vk::LogicOp::eNoOp,                         // logicOp
		pipelineColorBlendAttachmentStates,         // attachments
		{ { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);
	const bool dynamicLocalReadSupported = renderPass == nullptr;
	if (!dynamicLocalReadSupported)
		pipelineColorBlendStateCreateInfo.attachmentCount = 1;

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	ModVolShaderParams shaderParams { naomi2, !settings.platform.isNaomi2() && config::NativeDepthInterpolation };
	vk::ShaderModule vertex_module = shaderManager->GetModVolVertexShader(shaderParams);
	vk::ShaderModule fragment_module = shaderManager->GetModVolShader(!settings.platform.isNaomi2() && config::NativeDepthInterpolation);

	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main"),
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  stages,                                     // stages
	  &pipelineVertexInputStateCreateInfo,        // pVertexInputState
	  &pipelineInputAssemblyStateCreateInfo,      // pInputAssemblyState
	  nullptr,                                    // pTessellationState
	  &pipelineViewportStateCreateInfo,           // pViewportState
	  &pipelineRasterizationStateCreateInfo,      // pRasterizationState
	  &pipelineMultisampleStateCreateInfo,        // pMultisampleState
	  &pipelineDepthStencilStateCreateInfo,       // pDepthStencilState
	  &pipelineColorBlendStateCreateInfo,         // pColorBlendState
	  &pipelineDynamicStateCreateInfo,            // pDynamicState
	  *pipelineLayout,                            // layout
	  renderPass                                  // renderPass
	);
	const vk::Format colorAttachmentFormats[] { vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm };
	vk::PipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo { 0, colorAttachmentFormats,
		GetContext()->GetDepthFormat(), GetContext()->GetDepthFormat() };
	u32 inputIndexes[] { vk::AttachmentUnused, 1 };
	vk::RenderingInputAttachmentIndexInfoKHR riai { inputIndexes };
	pipelineRenderingCreateInfo.pNext = &riai;
	if (dynamicLocalReadSupported)
		graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;

	modVolPipelines[hash(mode, cullMode, naomi2)] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo).value;
}

void PipelineManager::CreateDepthPassPipeline(int cullMode, bool naomi2)
{
	// Vertex input state
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetMainVertexInputStateCreateInfo(false, false);
	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo
	(
			vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList
	);

	// Viewport and scissor states
	vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

	// Rasterization and multisample states
	vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo
	(
	  vk::PipelineRasterizationStateCreateFlags(),  // flags
	  false,                                        // depthClampEnable
	  false,                                        // rasterizerDiscardEnable
	  vk::PolygonMode::eFill,                       // polygonMode
	  cullMode == 3 ? vk::CullModeFlagBits::eBack
			  : cullMode == 2 ? vk::CullModeFlagBits::eFront
			  : vk::CullModeFlagBits::eNone,        // cullMode
	  vk::FrontFace::eCounterClockwise,             // frontFace
	  false,                                        // depthBiasEnable
	  0.0f,                                         // depthBiasConstantFactor
	  0.0f,                                         // depthBiasClamp
	  0.0f,                                         // depthBiasSlopeFactor
	  1.0f                                          // lineWidth
	);

	// Dreamcast uses the last vertex as the provoking vertex, but Vulkan uses the first.
	// Utilize VK_EXT_provoking_vertex when available to set the provoking vertex to be the
	// last vertex
	vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT provokingVertexInfo{};
	if (GetContext()->hasProvokingVertex())
	{
		provokingVertexInfo.provokingVertexMode = vk::ProvokingVertexModeEXT::eLastVertex;
		pipelineRasterizationStateCreateInfo.pNext = &provokingVertexInfo;
	}

	vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;

	// Depth and stencil
	vk::StencilOpState stencilOpState;
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo
	(
	  vk::PipelineDepthStencilStateCreateFlags(), // flags
	  true,                                       // depthTestEnable
	  true,                                       // depthWriteEnable
	  vk::CompareOp::eGreaterOrEqual,             // depthCompareOp
	  false,                                      // depthBoundTestEnable
	  false,                                      // stencilTestEnable
	  stencilOpState,                             // front
	  stencilOpState                              // back
	);

	// Color flags and blending
	vk::ColorComponentFlags colorComponentFlags((vk::ColorComponentFlagBits)0);
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentStates[] {
		{
			false,                              // blendEnable
			vk::BlendFactor::eZero,             // srcColorBlendFactor
			vk::BlendFactor::eZero,             // dstColorBlendFactor
			vk::BlendOp::eAdd,                  // colorBlendOp
			vk::BlendFactor::eZero,             // srcAlphaBlendFactor
			vk::BlendFactor::eZero,             // dstAlphaBlendFactor
			vk::BlendOp::eAdd,                  // alphaBlendOp
			colorComponentFlags                 // colorWriteMask
		},
		{
			false,                              // blendEnable
			vk::BlendFactor::eZero,             // srcColorBlendFactor
			vk::BlendFactor::eZero,             // dstColorBlendFactor
			vk::BlendOp::eAdd,                  // colorBlendOp
			vk::BlendFactor::eZero,             // srcAlphaBlendFactor
			vk::BlendFactor::eZero,             // dstAlphaBlendFactor
			vk::BlendOp::eAdd,                  // alphaBlendOp
			colorComponentFlags                 // colorWriteMask
		},
	};

	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
		vk::PipelineColorBlendStateCreateFlags(),   // flags
		false,                                      // logicOpEnable
		vk::LogicOp::eNoOp,                         // logicOp
		pipelineColorBlendAttachmentStates,         // attachments
		{ { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);
	const bool dynamicLocalReadSupported = renderPass == nullptr;
	if (!dynamicLocalReadSupported)
		pipelineColorBlendStateCreateInfo.attachmentCount = 1;

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	ModVolShaderParams shaderParams { naomi2, !settings.platform.isNaomi2() && config::NativeDepthInterpolation };
	vk::ShaderModule vertex_module = shaderManager->GetModVolVertexShader(shaderParams);
	vk::ShaderModule fragment_module = shaderManager->GetModVolShader(!settings.platform.isNaomi2() && config::NativeDepthInterpolation);

	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main"),
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  stages,                                     // stages
	  &pipelineVertexInputStateCreateInfo,        // pVertexInputState
	  &pipelineInputAssemblyStateCreateInfo,      // pInputAssemblyState
	  nullptr,                                    // pTessellationState
	  &pipelineViewportStateCreateInfo,           // pViewportState
	  &pipelineRasterizationStateCreateInfo,      // pRasterizationState
	  &pipelineMultisampleStateCreateInfo,        // pMultisampleState
	  &pipelineDepthStencilStateCreateInfo,       // pDepthStencilState
	  &pipelineColorBlendStateCreateInfo,         // pColorBlendState
	  &pipelineDynamicStateCreateInfo,            // pDynamicState
	  *pipelineLayout,                            // layout
	  renderPass                                  // renderPass
	);
	const vk::Format colorAttachmentFormats[] { vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm };
	vk::PipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo { 0, colorAttachmentFormats,
		GetContext()->GetDepthFormat(), GetContext()->GetDepthFormat() };
	u32 inputIndexes[] { vk::AttachmentUnused, 1 };
	vk::RenderingInputAttachmentIndexInfoKHR riai { inputIndexes };
	pipelineRenderingCreateInfo.pNext = &riai;
	if (dynamicLocalReadSupported)
		graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;

	depthPassPipelines[hash(cullMode, naomi2)] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo).value;
}

void PipelineManager::CreatePipeline(u32 listType, bool sortTriangles, const PolyParam& pp, int gpuPalette, bool dithering)
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetMainVertexInputStateCreateInfo(true, pp.isNaomi2());

	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo;
	if (sortTriangles && !config::PerStripSorting) {
		pipelineInputAssemblyStateCreateInfo.topology = vk::PrimitiveTopology::eTriangleList;
	}
	else
	{
		pipelineInputAssemblyStateCreateInfo.topology = vk::PrimitiveTopology::eTriangleStrip;
		pipelineInputAssemblyStateCreateInfo.primitiveRestartEnable = true;
	}

	// Viewport and scissor states
	vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

	// Rasterization and multisample states
	vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo
	(
	  vk::PipelineRasterizationStateCreateFlags(),  // flags
	  false,                                        // depthClampEnable
	  false,                                        // rasterizerDiscardEnable
	  vk::PolygonMode::eFill,                       // polygonMode
	  pp.isp.CullMode == 3 ? vk::CullModeFlagBits::eBack
			  : pp.isp.CullMode == 2 ? vk::CullModeFlagBits::eFront
			  : vk::CullModeFlagBits::eNone,        // cullMode
	  vk::FrontFace::eCounterClockwise,             // frontFace
	  false,                                        // depthBiasEnable
	  0.0f,                                         // depthBiasConstantFactor
	  0.0f,                                         // depthBiasClamp
	  0.0f,                                         // depthBiasSlopeFactor
	  1.0f                                          // lineWidth
	);

	// Dreamcast uses the last vertex as the provoking vertex, but Vulkan uses the first.
	// Utilize VK_EXT_provoking_vertex when available to set the provoking vertex to be the
	// last vertex
	vk::PipelineRasterizationProvokingVertexStateCreateInfoEXT provokingVertexInfo{};
	if (GetContext()->hasProvokingVertex())
	{
		provokingVertexInfo.provokingVertexMode = vk::ProvokingVertexModeEXT::eLastVertex;
		pipelineRasterizationStateCreateInfo.pNext = &provokingVertexInfo;
	}

	vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;

	// Depth and stencil
	vk::CompareOp depthOp;
	if (listType == ListType_Punch_Through || sortTriangles)
		depthOp = vk::CompareOp::eGreaterOrEqual;
	else
		depthOp = depthOps[pp.isp.DepthMode];
	bool depthWriteEnable;
	if (sortTriangles /* && !config::PerStripSorting */)
		// FIXME temporary work-around for intel driver bug
		depthWriteEnable = GetContext()->GetVendorID() == VulkanContext::VENDOR_INTEL;
	else
	{
		// Z Write Disable seems to be ignored for punch-through.
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (listType == ListType_Punch_Through)
			depthWriteEnable = true;
		else
			depthWriteEnable = !pp.isp.ZWriteDis;
	}

	bool shadowed = listType == ListType_Opaque || listType == ListType_Punch_Through;
	vk::StencilOpState stencilOpState;
	if (shadowed)
	{
		if (pp.pcw.Shadow != 0)
			stencilOpState = vk::StencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eReplace, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 0, 0x80, 0x80);
		else
			stencilOpState = vk::StencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eReplace, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 0, 0x80, 0);
	}
	else
		stencilOpState = vk::StencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eNever);
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo
	(
	  vk::PipelineDepthStencilStateCreateFlags(), // flags
	  true,                                       // depthTestEnable
	  depthWriteEnable,                           // depthWriteEnable
	  depthOp,                                    // depthCompareOp
	  false,                                      // depthBoundTestEnable
	  shadowed,                                   // stencilTestEnable
	  stencilOpState,                             // front
	  stencilOpState                              // back
	);

	// Color flags and blending
	vk::ColorComponentFlags colorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
	// Apparently punch-through polys support blending, or at least some combinations
	// Opaque polygons support blending in list continuations (wild guess)
	u32 src = pp.tsp.SrcInstr;
	u32 dst = pp.tsp.DstInstr;
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentStates[] {
		{
		  true,                          // blendEnable
		  getBlendFactor(src, true),     // srcColorBlendFactor
		  getBlendFactor(dst, false),    // dstColorBlendFactor
		  vk::BlendOp::eAdd,             // colorBlendOp
		  getBlendFactor(src, true),     // srcAlphaBlendFactor
		  getBlendFactor(dst, false),    // dstAlphaBlendFactor
		  vk::BlendOp::eAdd,             // alphaBlendOp
		  colorComponentFlags            // colorWriteMask
		},
		{
		  true,                          // blendEnable
		  getBlendFactor(src, true),     // srcColorBlendFactor
		  getBlendFactor(dst, false),    // dstColorBlendFactor
		  vk::BlendOp::eAdd,             // colorBlendOp
		  getBlendFactor(src, true),     // srcAlphaBlendFactor
		  getBlendFactor(dst, false),    // dstAlphaBlendFactor
		  vk::BlendOp::eAdd,             // alphaBlendOp
		  colorComponentFlags            // colorWriteMask
		},
	};

	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
	  vk::PipelineColorBlendStateCreateFlags(),   // flags
	  false,                                      // logicOpEnable
	  vk::LogicOp::eNoOp,                         // logicOp
	  pipelineColorBlendAttachmentStates,         // attachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);
	const bool dynamicLocalReadSupported = renderPass == nullptr;
	if (!dynamicLocalReadSupported)
		pipelineColorBlendStateCreateInfo.attachmentCount = 1;

	vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), 2, dynamicStates);

	bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
	vk::ShaderModule vertex_module = shaderManager->GetVertexShader(VertexShaderParams { pp.pcw.Gouraud == 1, pp.isNaomi2(), divPosZ, pp.isNaomi2() && pp.pcw.Texture == 1 });
	FragmentShaderParams params = {};
	params.alphaTest = listType == ListType_Punch_Through;
	params.bumpmap = pp.tcw.PixelFmt == PixelBumpMap;
	params.clamping = pp.tsp.ColorClamp;
	params.insideClipTest = (pp.tileclip >> 28) == 3;
	params.fog = config::Fog ? pp.tsp.FogCtrl : 2;
	params.gouraud = pp.pcw.Gouraud;
	params.ignoreTexAlpha = pp.tsp.IgnoreTexA || pp.tcw.PixelFmt == Pixel565;
	params.offset = pp.pcw.Offset;
	params.shaderInstr = pp.tsp.ShadInstr;
	params.texture = pp.pcw.Texture;
	params.trilinear = pp.pcw.Texture && pp.tsp.FilterMode > 1 && listType != ListType_Punch_Through && pp.tcw.MipMapped == 1;
	params.useAlpha = pp.tsp.UseAlpha;
	params.palette = gpuPalette;
	params.divPosZ = divPosZ;
	params.dithering = dithering;
	params.secAccum = pp.tsp.SrcSelect == 1 && dynamicLocalReadSupported;
	vk::ShaderModule fragment_module = shaderManager->GetFragmentShader(params);

	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main"),
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  stages,                                     // stages
	  &pipelineVertexInputStateCreateInfo,        // pVertexInputState
	  &pipelineInputAssemblyStateCreateInfo,      // pInputAssemblyState
	  nullptr,                                    // pTessellationState
	  &pipelineViewportStateCreateInfo,           // pViewportState
	  &pipelineRasterizationStateCreateInfo,      // pRasterizationState
	  &pipelineMultisampleStateCreateInfo,        // pMultisampleState
	  &pipelineDepthStencilStateCreateInfo,       // pDepthStencilState
	  &pipelineColorBlendStateCreateInfo,         // pColorBlendState
	  &pipelineDynamicStateCreateInfo,            // pDynamicState
	  *pipelineLayout,                            // layout
	  renderPass                                  // renderPass
	);
	const vk::Format colorAttachmentFormats[] { vk::Format::eR8G8B8A8Unorm, vk::Format::eR8G8B8A8Unorm };
	vk::PipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo { 0, colorAttachmentFormats,
		GetContext()->GetDepthFormat(), GetContext()->GetDepthFormat() };
	u32 inputIndexes[] { vk::AttachmentUnused, 1 };
	vk::RenderingInputAttachmentIndexInfoKHR riai { inputIndexes };
	pipelineRenderingCreateInfo.pNext = &riai;
	if (dynamicLocalReadSupported)
	{
		graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
		if (pp.tsp.DstSelect == 1)
		{
			// Write to attachment 1
			static const u32 indexes[] { vk::AttachmentUnused, 0 };
			static const vk::RenderingAttachmentLocationInfoKHR rali { indexes };
			riai.pNext = &rali;
			inputIndexes[1] = vk::AttachmentUnused;
		}
	}

	pipelines[hash(listType, sortTriangles, &pp, gpuPalette, dithering)] = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
			graphicsPipelineCreateInfo).value;
}
