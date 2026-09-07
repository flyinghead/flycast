/*
    Created on: Nov 6, 2019

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
#include "oit_pipeline.h"
#include "../quad.h"

void OITDescriptorSets::init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout,
		vk::DescriptorSetLayout perPolyLayout, vk::DescriptorSetLayout colorInputLayout)
{
	BaseDescriptorSets::init(samplerManager);
	this->pipelineLayout = pipelineLayout;

	perFrameAlloc.setLayout(perFrameLayout);
	perPolyAlloc.setLayout(perPolyLayout);
	colorInputAlloc.setLayout(colorInputLayout);
}

void OITDescriptorSets::updateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset, vk::ImageView fogImageView,
		u32 polyParamsOffset, u32 polyParamsSize, vk::ImageView stencilImageView, vk::ImageView depthImageView,
		vk::ImageView paletteImageView, OITBuffers *oitBuffers)
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
		writeDescriptorSets.emplace_back(perFrameDescSet, 6, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
	}
	else {
		writeDescriptorSets.emplace_back(perFrameDescSet, 6, 0, vk::DescriptorType::eCombinedImageSampler, getEmptyImageInfo());
	}
	if (polyParamsSize > 0)
	{
		static vk::DescriptorBufferInfo polyParamsBufferInfo;
		polyParamsBufferInfo = vk::DescriptorBufferInfo(buffer, polyParamsOffset, polyParamsSize);
		writeDescriptorSets.emplace_back(perFrameDescSet, 3, 0, vk::DescriptorType::eStorageBuffer, nullptr, polyParamsBufferInfo);
	}
	vk::DescriptorImageInfo stencilImageInfo(vk::Sampler(), stencilImageView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
	writeDescriptorSets.emplace_back(perFrameDescSet, 4, 0, vk::DescriptorType::eInputAttachment, stencilImageInfo);
	vk::DescriptorImageInfo depthImageInfo(vk::Sampler(), depthImageView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
	writeDescriptorSets.emplace_back(perFrameDescSet, 5, 0, vk::DescriptorType::eInputAttachment, depthImageInfo);
	oitBuffers->updateDescriptorSet(perFrameDescSet, writeDescriptorSets);

	getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
}

void OITDescriptorSets::updateColorInputDescSet(int index, vk::ImageView colorImageView)
{
	colorInputDescSets[index] = colorInputAlloc.alloc();

	vk::DescriptorImageInfo colorImageInfo(vk::Sampler(), colorImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
	vk::WriteDescriptorSet writeDescriptorSet(colorInputDescSets[index], 0, 0, vk::DescriptorType::eInputAttachment, colorImageInfo);

	getContext()->GetDevice().updateDescriptorSets(writeDescriptorSet, nullptr);
}

void OITDescriptorSets::bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const PolyParam& poly, int polyNumber, vk::Buffer buffer,
		vk::DeviceSize uniformOffset, vk::DeviceSize lightOffset, bool punchThrough)
{
	vk::DescriptorSet perPolyDescSet;
	auto it = perPolyDescSets.find(&poly);
	if (it == perPolyDescSets.end())
	{
		perPolyDescSet = perPolyAlloc.alloc();
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;

		vk::DescriptorImageInfo imageInfo0;
		if (poly.texture != nullptr)
		{
			imageInfo0 = vk::DescriptorImageInfo{ samplerManager->GetSampler(poly, punchThrough, false), ((Texture *)poly.texture)->GetReadOnlyImageView(),
					vk::ImageLayout::eShaderReadOnlyOptimal };
			writeDescriptorSets.emplace_back(perPolyDescSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo0);
		}
		else {
			writeDescriptorSets.emplace_back(perPolyDescSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, getEmptyImageInfo());
		}

		vk::DescriptorImageInfo imageInfo1;
		if (poly.texture1 != nullptr)
		{
			imageInfo1 = vk::DescriptorImageInfo{ samplerManager->GetSampler(poly, punchThrough, true), ((Texture *)poly.texture1)->GetReadOnlyImageView(),
				vk::ImageLayout::eShaderReadOnlyOptimal };
			writeDescriptorSets.emplace_back(perPolyDescSet, 1, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo1);
		}
		else {
			writeDescriptorSets.emplace_back(perPolyDescSet, 1, 0, vk::DescriptorType::eCombinedImageSampler, getEmptyImageInfo());
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

void OITDescriptorSets::bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const ModifierVolumeParam& mvParam, int polyNumber,
		vk::Buffer buffer, vk::DeviceSize uniformOffset)
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

void OITDescriptorSets::nextFrame()
{
	perFrameDescSet = vk::DescriptorSet{};
	colorInputDescSets[0] = vk::DescriptorSet{};
	colorInputDescSets[1] = vk::DescriptorSet{};
	perPolyDescSets.clear();
	perFrameAlloc.nextFrame();
	perPolyAlloc.nextFrame();
	colorInputAlloc.nextFrame();
}

void OITDescriptorSets::term()
{
	perFrameAlloc.term();
	perPolyAlloc.term();
	colorInputAlloc.term();
	BaseDescriptorSets::term();
}

void OITPipelineManager::Init(OITShaderManager *shaderManager, OITBuffers *oitBuffers)
{
	this->shaderManager = shaderManager;
	this->oitBuffers = oitBuffers;

	if (!perFrameLayout)
	{
		vk::Device device = GetContext()->GetDevice();
		// Descriptor set and pipeline layout
		std::array<vk::DescriptorSetLayoutBinding, 10> perFrameBindings = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),			// vertex uniforms
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// fragment uniforms
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// fog texture
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// Tr poly params
				vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),		// stencil input attachment
				vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),		// depth input attachment
				vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// palette texture
				// OIT buffers
				vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// pixel buffer
				vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// pixel counter
				vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// a-buffer pointers
		};
		perFrameLayout = device.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), perFrameBindings));

		vk::DescriptorSetLayoutBinding colorInputBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment);		// color input attachment
		colorInputLayout = device.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), colorInputBinding));

		std::array<vk::DescriptorSetLayoutBinding, 4> perPolyBindings = {
				vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),	// texture 0
				vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),	// texture 1 (for 2-volume mode)
				vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),				// Naomi2 uniforms
				vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),				// Naomi2 lights
		};
		perPolyLayout = device.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), perPolyBindings));

		std::array<vk::PushConstantRange, 2> pushConstants = {
				vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(OITDescriptorSets::PushConstants)),
				vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, sizeof(OITDescriptorSets::PushConstants), sizeof(OITDescriptorSets::VtxPushConstants)),
		};

		std::array<vk::DescriptorSetLayout, 3> layouts = { *perFrameLayout, *perPolyLayout, *colorInputLayout };
		pipelineLayout = device.createPipelineLayoutUnique(
				vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), layouts, pushConstants));
	}

	pipelines.clear();
	modVolPipelines.clear();
	trModVolPipelines.clear();
	finalPipelines.clear();
	clearPipeline.reset();
}

vk::PipelineVertexInputStateCreateInfo OITPipelineManager::GetMainVertexInputStateCreateInfo(bool full, bool naomi2) const
{
	// Vertex input state
	static const vk::VertexInputBindingDescription vertexBindingDescriptions[] =
	{
			{ 0, sizeof(Vertex) },
	};
	static const vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, x)),	// pos
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Vertex, col)),	// base color
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Vertex, spc)),	// offset color
			vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, u)),		// tex coord
			vk::VertexInputAttributeDescription(4, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Vertex, col1)),	// base1 color
			vk::VertexInputAttributeDescription(5, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Vertex, spc1)),	// offset1 color
			vk::VertexInputAttributeDescription(6, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, u1)),		// tex1 coord
			vk::VertexInputAttributeDescription(7, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, nx)),	// naomi2 normal
	};
	static const vk::VertexInputAttributeDescription vertexInputLightAttributeDescriptions[] =
	{
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, x)),	// pos
	};

	const vk::VertexInputAttributeDescription* attributeDescription = vertexInputLightAttributeDescriptions;
	u32 attributeDescriptionSize = std::size(vertexInputLightAttributeDescriptions);

	if (full)
	{
		attributeDescription = vertexInputAttributeDescriptions;

		if (naomi2)
		{
			attributeDescriptionSize = std::size(vertexInputAttributeDescriptions);
		}
		else
		{
			// naomi2 normal not needed
			attributeDescriptionSize = std::size(vertexInputAttributeDescriptions) - 1;
		}
	}
	return vk::PipelineVertexInputStateCreateInfo(
		vk::PipelineVertexInputStateCreateFlags(),
		std::size(vertexBindingDescriptions), vertexBindingDescriptions,
		attributeDescriptionSize, attributeDescription
	);
}

void OITPipelineManager::CreatePipeline(u32 listType, bool autosort, const PolyParam& pp, Pass pass, int gpuPalette, bool useBDA)
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetMainVertexInputStateCreateInfo(true, pp.isNaomi2());

	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleStrip, true);

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
	 if (listType == ListType_Punch_Through || autosort)
		depthOp = vk::CompareOp::eGreaterOrEqual;
	else
		depthOp = depthOps[pp.isp.DepthMode];
	bool depthWriteEnable;
	// Z Write Disable seems to be ignored for punch-through.
	// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
	if (listType == ListType_Punch_Through)
		depthWriteEnable = true;
	else
		depthWriteEnable = !pp.isp.ZWriteDis;

	bool shadowed = pass == Pass::Depth && (listType == ListType_Opaque || listType == ListType_Punch_Through);
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
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState;
	vk::ColorComponentFlags colorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
	// Apparently punch-through polys support blending, or at least some combinations
	if (listType == ListType_Punch_Through || pass == Pass::Color)
	{
		u32 src = pp.tsp.SrcInstr;
		u32 dst = pp.tsp.DstInstr;
		pipelineColorBlendAttachmentState =
		{
		  true,                          // blendEnable
		  getBlendFactor(src, true),     // srcColorBlendFactor
		  getBlendFactor(dst, false),    // dstColorBlendFactor
		  vk::BlendOp::eAdd,             // colorBlendOp
		  getBlendFactor(src, true),     // srcAlphaBlendFactor
		  getBlendFactor(dst, false),    // dstAlphaBlendFactor
		  vk::BlendOp::eAdd,             // alphaBlendOp
		  colorComponentFlags            // colorWriteMask
		};
	}
	else
	{
		if (pass == Pass::Depth || pass == Pass::OIT)
			colorComponentFlags = vk::ColorComponentFlags();
		pipelineColorBlendAttachmentState =
		{
		  false,                      // blendEnable
		  vk::BlendFactor::eZero,     // srcColorBlendFactor
		  vk::BlendFactor::eZero,     // dstColorBlendFactor
		  vk::BlendOp::eAdd,          // colorBlendOp
		  vk::BlendFactor::eZero,     // srcAlphaBlendFactor
		  vk::BlendFactor::eZero,     // dstAlphaBlendFactor
		  vk::BlendOp::eAdd,          // alphaBlendOp
		  colorComponentFlags		  // colorWriteMask
		};
	}

	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
	  vk::PipelineColorBlendStateCreateFlags(),   // flags
	  false,                                      // logicOpEnable
	  vk::LogicOp::eNoOp,                         // logicOp
	  pipelineColorBlendAttachmentState,         // attachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	bool twoVolume = pp.tsp1.full != (u32)-1 || pp.tcw1.full != (u32)-1;
	bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
	vk::ShaderModule vertex_module = shaderManager->GetVertexShader(
			OITShaderManager::VertexShaderParams{ pp.pcw.Gouraud == 1, pp.isNaomi2(), pass != Pass::Depth, twoVolume, pp.isNaomi2() && pp.pcw.Texture == 1, divPosZ });
	OITShaderManager::FragmentShaderParams params = {};
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
	params.useAlpha = pp.tsp.UseAlpha;
	params.pass = pass;
	params.twoVolume = twoVolume;
	params.palette = gpuPalette;
	params.divPosZ = divPosZ;
	params.useBDA = useBDA;
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
	  renderPasses->GetRenderPass(true, true),    // renderPass
	  pass == Pass::Depth ? (listType == ListType_Translucent ? 2 : 0) : 1 // subpass
	);

	pipelines[hash(listType, autosort, &pp, pass, gpuPalette, useBDA)] = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
			graphicsPipelineCreateInfo).value;
}

void OITPipelineManager::CreateFinalPipeline(bool dithering, bool useBDA)
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetQuadInputStateCreateInfo(false);

	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleStrip);

	// Viewport and scissor states
	vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

	// Rasterization and multisample states
	vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo
	(
	  vk::PipelineRasterizationStateCreateFlags(),  // flags
	  false,                                        // depthClampEnable
	  false,                                        // rasterizerDiscardEnable
	  vk::PolygonMode::eFill,                       // polygonMode
	  vk::CullModeFlagBits::eNone,                  // cullMode
	  vk::FrontFace::eCounterClockwise,             // frontFace
	  false,                                        // depthBiasEnable
	  0.0f,                                         // depthBiasConstantFactor
	  0.0f,                                         // depthBiasClamp
	  0.0f,                                         // depthBiasSlopeFactor
	  1.0f                                          // lineWidth
	);
	vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;

	// Depth and stencil
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo;

	// Color flags and blending
	vk::ColorComponentFlags colorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState =
	{
		false,                         // blendEnable
		vk::BlendFactor::eZero,        // srcColorBlendFactor
		vk::BlendFactor::eZero,        // dstColorBlendFactor
		vk::BlendOp::eAdd,             // colorBlendOp
		vk::BlendFactor::eZero,        // srcAlphaBlendFactor
		vk::BlendFactor::eZero,        // dstAlphaBlendFactor
		vk::BlendOp::eAdd,             // alphaBlendOp
		colorComponentFlags            // colorWriteMask
	};
	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
	  vk::PipelineColorBlendStateCreateFlags(),   // flags
	  false,                                      // logicOpEnable
	  vk::LogicOp::eNoOp,                         // logicOp
	  pipelineColorBlendAttachmentState,         // attachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetFinalVertexShader();
	vk::ShaderModule fragment_module = shaderManager->GetFinalShader(OITShaderManager::FinalShaderParams{dithering, useBDA});

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
	  renderPasses->GetRenderPass(true, true),    // renderPass
	  2                                           // subpass
	);

	finalPipelines[hash(dithering, useBDA)] = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo).value;
}

void OITPipelineManager::CreateClearPipeline()
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetQuadInputStateCreateInfo(false);

	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleStrip);

	// Viewport and scissor states
	vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

	// Rasterization and multisample states
	vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo
	(
	  vk::PipelineRasterizationStateCreateFlags(),  // flags
	  false,                                        // depthClampEnable
	  false,                                        // rasterizerDiscardEnable
	  vk::PolygonMode::eFill,                       // polygonMode
	  vk::CullModeFlagBits::eNone,                  // cullMode
	  vk::FrontFace::eCounterClockwise,             // frontFace
	  false,                                        // depthBiasEnable
	  0.0f,                                         // depthBiasConstantFactor
	  0.0f,                                         // depthBiasClamp
	  0.0f,                                         // depthBiasSlopeFactor
	  1.0f                                          // lineWidth
	);
	vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;

	// Depth and stencil
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo;

	// Color flags and blending
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState;
	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
	  vk::PipelineColorBlendStateCreateFlags(),   // flags
	  false,                                      // logicOpEnable
	  vk::LogicOp::eNoOp,                         // logicOp
	  pipelineColorBlendAttachmentState,         // attachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetFinalVertexShader();
	vk::ShaderModule fragment_module = shaderManager->GetClearShader();

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
	  renderPasses->GetRenderPass(true, true),    // renderPass
	  1                                           // subpass
	);

	clearPipeline = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo).value;
}

void OITPipelineManager::CreateModVolPipeline(ModVolMode mode, int cullMode, bool naomi2)
{
	verify(mode != ModVolMode::Final);

	static const vk::VertexInputBindingDescription vertexBindingDescription(0, sizeof(float) * 3);
	static const vk::VertexInputAttributeDescription vertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0);	// pos
	// Vertex input state
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
			vk::PipelineVertexInputStateCreateFlags(),
			vertexBindingDescription,
			vertexInputAttributeDescription);
	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList);

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
	default:
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
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState;

	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
		vk::PipelineColorBlendStateCreateFlags(),   // flags
		false,                                      // logicOpEnable
		vk::LogicOp::eNoOp,                         // logicOp
		pipelineColorBlendAttachmentState,         // attachments
		{ { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetModVolVertexShader(OITShaderManager::ModVolShaderParams{ naomi2, !settings.platform.isNaomi2() && config::NativeDepthInterpolation });
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
	  renderPasses->GetRenderPass(true, true)     // renderPass
	);

	modVolPipelines[hash(mode, cullMode, naomi2, false)] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo).value;
}

void OITPipelineManager::CreateTrModVolPipeline(ModVolMode mode, int cullMode, bool naomi2, bool useBDA)
{
	verify(mode != ModVolMode::Final);

	static const vk::VertexInputBindingDescription vertexBindingDescription(0, sizeof(float) * 3);
	static const vk::VertexInputAttributeDescription vertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0);	// pos
	// Vertex input state
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
			vk::PipelineVertexInputStateCreateFlags(),
			vertexBindingDescription,
			vertexInputAttributeDescription);
	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList);

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
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo;

	// Color flags and blending
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState;

	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
		vk::PipelineColorBlendStateCreateFlags(),   // flags
		false,                                      // logicOpEnable
		vk::LogicOp::eNoOp,                         // logicOp
		pipelineColorBlendAttachmentState,         // attachments
		{ { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
	vk::ShaderModule vertex_module = shaderManager->GetModVolVertexShader(OITShaderManager::ModVolShaderParams{ naomi2, divPosZ });
	vk::ShaderModule fragment_module = shaderManager->GetTrModVolShader(OITShaderManager::TrModVolShaderParams{ mode, divPosZ, useBDA });

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
	  renderPasses->GetRenderPass(true, true),    // renderPass
      2                                           // subpass
	);

	trModVolPipelines[hash(mode, cullMode, naomi2, useBDA)] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo).value;
}

void OITPipelineManager::checkMaxLayers()
{
	int layers = std::clamp<int>(config::PerPixelLayers, 1, 256);
	if (maxLayers != layers)
	{
		maxLayers = layers;
		trModVolPipelines.clear();
		finalPipelines.clear();
	}
}
