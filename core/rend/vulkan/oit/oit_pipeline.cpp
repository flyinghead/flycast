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

void OITPipelineManager::CreatePipeline(u32 listType, bool autosort, const PolyParam& pp, Pass pass, bool gpuPalette)
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetMainVertexInputStateCreateInfo();

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
	// Apparently punch-through polys support blending, or at least some combinations
	if (listType == ListType_Punch_Through || pass == Pass::Color)
	{
		vk::ColorComponentFlags colorComponentFlags(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
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
		pipelineColorBlendAttachmentState =
		{
		  false,                      // blendEnable
		  vk::BlendFactor::eZero,     // srcColorBlendFactor
		  vk::BlendFactor::eZero,     // dstColorBlendFactor
		  vk::BlendOp::eAdd,          // colorBlendOp
		  vk::BlendFactor::eZero,     // srcAlphaBlendFactor
		  vk::BlendFactor::eZero,     // dstAlphaBlendFactor
		  vk::BlendOp::eAdd,          // alphaBlendOp
		  vk::ColorComponentFlags()   // colorWriteMask
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
			OITShaderManager::VertexShaderParams{ pp.pcw.Gouraud == 1, pp.isNaomi2(), pass != Pass::Depth, twoVolume, pp.pcw.Texture == 1, divPosZ });
	OITShaderManager::FragmentShaderParams params = {};
	params.alphaTest = listType == ListType_Punch_Through;
	params.bumpmap = pp.tcw.PixelFmt == PixelBumpMap;
	params.clamping = pp.tsp.ColorClamp && (pvrrc.fog_clamp_min.full != 0 || pvrrc.fog_clamp_max.full != 0xffffffff);
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

	pipelines[hash(listType, autosort, &pp, pass, gpuPalette)] = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
			graphicsPipelineCreateInfo).value;
}

void OITPipelineManager::CreateFinalPipeline()
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
	vk::ShaderModule fragment_module = shaderManager->GetFinalShader();

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

	finalPipeline = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo).value;
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
	  2                                           // subpass
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

	modVolPipelines[hash(mode, cullMode, naomi2)] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo).value;
}

void OITPipelineManager::CreateTrModVolPipeline(ModVolMode mode, int cullMode, bool naomi2)
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
	vk::ShaderModule fragment_module = shaderManager->GetTrModVolShader(OITShaderManager::TrModVolShaderParams{ mode, divPosZ });

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

	trModVolPipelines[hash(mode, cullMode, naomi2)] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo).value;
}
