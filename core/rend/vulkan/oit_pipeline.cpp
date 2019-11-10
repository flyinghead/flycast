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
#include "quad.h"

void OITPipelineManager::MakeRenderPass()
{
    vk::AttachmentDescription attachmentDescriptions[] = {
    		// Swap chain image
    		vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetColorFormat(), vk::SampleCountFlagBits::e1,
    				vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR),
			// OP+PT color attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetColorFormat(), vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal),
			// OP+PT depth attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilReadOnlyOptimal),
    };
    vk::AttachmentReference swapChainReference(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference colorReference(1, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference depthReference(2, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference depthReadOnlyRef(2, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
    vk::AttachmentReference colorInput(1, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::SubpassDescription subpasses[] = {
    	// Depth and modvol pass	FIXME subpass 0 shouldn't reference the color attachment
    	vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorReference, nullptr, &depthReference),
    	// Color pass
    	vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 1, &depthReadOnlyRef, 1, &colorReference, nullptr, &depthReadOnlyRef),
    	// Final pass
    	vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 1, &colorInput, 1, &swapChainReference, nullptr, nullptr),
    };

    vk::SubpassDependency dependencies[] = {
    	vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eEarlyFragmentTests,
    			vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead,
				vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
				vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 1, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::AccessFlagBits::eInputAttachmentRead, vk::AccessFlagBits::eColorAttachmentWrite, vk::DependencyFlagBits::eByRegion),
//    	vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput,
//    			vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead, vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(0, 1, vk::PipelineStageFlagBits::eLateFragmentTests, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
				vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead, vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(1, 2, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eInputAttachmentRead, vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(2, 2, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite, vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(2, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion),
    };

    renderPass = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
    		ARRAY_SIZE(attachmentDescriptions), attachmentDescriptions,
    		ARRAY_SIZE(subpasses), subpasses,
			ARRAY_SIZE(dependencies), dependencies));
}

void OITPipelineManager::CreatePipeline(u32 listType, bool autosort, const PolyParam& pp, int pass)
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetMainVertexInputStateCreateInfo();

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
	if (pass == 1)
		depthOp = vk::CompareOp::eEqual;
	else if (listType == ListType_Punch_Through || autosort)
		depthOp = vk::CompareOp::eGreaterOrEqual;
	else
		depthOp = depthOps[pp.isp.DepthMode];
	bool depthWriteEnable;
	if (pass != 0)
		depthWriteEnable = false;
	// Z Write Disable seems to be ignored for punch-through.
	// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
	else if (listType == ListType_Punch_Through)
		depthWriteEnable = true;
	else
		depthWriteEnable = !pp.isp.ZWriteDis;

	bool shadowed = pass == 0 && (listType == ListType_Opaque || listType == ListType_Punch_Through) && pp.pcw.Shadow != 0;
	vk::StencilOpState stencilOpState;
	if (shadowed)
		stencilOpState = vk::StencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eReplace, vk::StencilOp::eKeep, vk::CompareOp::eAlways, 0, 0x80, 0x80);
	else
		stencilOpState = vk::StencilOpState(vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways);
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
	if ((listType == ListType_Punch_Through || pass > 0) && pass != 3)
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
	  1,                                          // attachmentCount
	  &pipelineColorBlendAttachmentState,         // pAttachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), 2, dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetVertexShader(OITShaderManager::VertexShaderParams{ pp.pcw.Gouraud == 1 });
	OITShaderManager::FragmentShaderParams params = {};
	params.alphaTest = listType == ListType_Punch_Through;
	params.bumpmap = pp.tcw.PixelFmt == PixelBumpMap;
	params.clamping = pp.tsp.ColorClamp && (pvrrc.fog_clamp_min != 0 || pvrrc.fog_clamp_max != 0xffffffff);
	params.insideClipTest = (pp.tileclip >> 28) == 3;
	params.fog = settings.rend.Fog ? pp.tsp.FogCtrl : 2;
	params.gouraud = pp.pcw.Gouraud;
	params.ignoreTexAlpha = pp.tsp.IgnoreTexA;
	params.offset = pp.pcw.Offset;
	params.shaderInstr = pp.tsp.ShadInstr;
	params.texture = pp.pcw.Texture;
	//params.trilinear = pp.pcw.Texture && pp.tsp.FilterMode > 1 && listType != ListType_Punch_Through;
	params.useAlpha = pp.tsp.UseAlpha;
	params.pass = pass;
	params.depthFunc = autosort ? 6 : pp.isp.DepthMode;
	params.twoVolume = pp.tsp1.full != -1 || pp.tcw1.full != -1;
	vk::ShaderModule fragment_module = shaderManager->GetFragmentShader(params);

	vk::PipelineShaderStageCreateInfo stages[] = {
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main" },
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main" },
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  2,                                          // stageCount
	  stages,                                     // pStages
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
	  *renderPass,                                // renderPass
	  pass == 0 ? 0 : 1                           // subpass
	);

	pipelines[hash(listType, autosort, &pp, pass)] = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
			graphicsPipelineCreateInfo);
}

void OITPipelineManager::CreateFinalPipeline(bool autosort)
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
	  1,                                          // attachmentCount
	  &pipelineColorBlendAttachmentState,         // pAttachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), 2, dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetFinalVertexShader();
	vk::ShaderModule fragment_module = shaderManager->GetFinalShader(autosort);

	vk::PipelineShaderStageCreateInfo stages[] = {
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main" },
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main" },
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  2,                                          // stageCount
	  stages,                                     // pStages
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
	  *renderPass,                                // renderPass
	  2                                           // subpass
	);

	if (autosort)
		finalAutosortPipeline = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo);
	else
		finalNosortPipeline = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo);

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
	  1,                                          // attachmentCount
	  &pipelineColorBlendAttachmentState,         // pAttachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), 2, dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetFinalVertexShader();
	vk::ShaderModule fragment_module = shaderManager->GetClearShader();

	vk::PipelineShaderStageCreateInfo stages[] = {
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main" },
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main" },
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  2,                                          // stageCount
	  stages,                                     // pStages
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
	  *renderPass,                                // renderPass
	  2                                           // subpass
	);

	clearPipeline = GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo);
}

void OITPipelineManager::CreateModVolPipeline(ModVolMode mode)
{
	verify(mode != ModVolMode::Final);

	static const vk::VertexInputBindingDescription vertexBindingDescriptions[] =
	{
			{ 0, sizeof(float) * 3 },
	};
	static const vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),	// pos
	};
	// Vertex input state
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
			vk::PipelineVertexInputStateCreateFlags(),
			ARRAY_SIZE(vertexBindingDescriptions),
			vertexBindingDescriptions,
			ARRAY_SIZE(vertexInputAttributeDescriptions),
			vertexInputAttributeDescriptions);
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
	  vk::CullModeFlagBits::eNone,  		        // cullMode
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
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState(
		false,                              // blendEnable
		vk::BlendFactor::eZero,             // srcColorBlendFactor
		vk::BlendFactor::eZero,             // dstColorBlendFactor
		vk::BlendOp::eAdd,                  // colorBlendOp
		vk::BlendFactor::eZero,             // srcAlphaBlendFactor
		vk::BlendFactor::eZero,             // dstAlphaBlendFactor
		vk::BlendOp::eAdd,                  // alphaBlendOp
		vk::ColorComponentFlags()           // colorWriteMask
	);

	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
		vk::PipelineColorBlendStateCreateFlags(),   // flags
		false,                                      // logicOpEnable
		vk::LogicOp::eNoOp,                         // logicOp
		1,                                          // attachmentCount
		&pipelineColorBlendAttachmentState,         // pAttachments
		{ { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), 2, dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetModVolVertexShader();
	vk::ShaderModule fragment_module = shaderManager->GetModVolShader();

	vk::PipelineShaderStageCreateInfo stages[] = {
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main" },
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main" },
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  2,                                          // stageCount
	  stages,                                     // pStages
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
	  *renderPass,                                // renderPass
	  0                                           // subpass
	);

	if (modVolPipelines.empty())
		modVolPipelines.resize((size_t)ModVolMode::Final);
	modVolPipelines[(size_t)mode] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo);
}

void OITPipelineManager::CreateTrModVolPipeline(ModVolMode mode)
{
	verify(mode != ModVolMode::Final);

	static const vk::VertexInputBindingDescription vertexBindingDescriptions[] =
	{
			{ 0, sizeof(float) * 3 },
	};
	static const vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, 0),	// pos
	};
	// Vertex input state
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
			vk::PipelineVertexInputStateCreateFlags(),
			ARRAY_SIZE(vertexBindingDescriptions),
			vertexBindingDescriptions,
			ARRAY_SIZE(vertexInputAttributeDescriptions),
			vertexInputAttributeDescriptions);
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
	  vk::CullModeFlagBits::eNone,  		        // cullMode
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
		1,                                          // attachmentCount
		&pipelineColorBlendAttachmentState,         // pAttachments
		{ { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), 2, dynamicStates);

	vk::ShaderModule vertex_module = shaderManager->GetModVolVertexShader();
	vk::ShaderModule fragment_module = shaderManager->GetTrModVolShader(mode);

	vk::PipelineShaderStageCreateInfo stages[] = {
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, vertex_module, "main" },
			{ vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, fragment_module, "main" },
	};
	vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo
	(
	  vk::PipelineCreateFlags(),                  // flags
	  2,                                          // stageCount
	  stages,                                     // pStages
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
	  *renderPass,                                // renderPass
      2                                           // subpass
	);

	if (trModVolPipelines.empty())
		trModVolPipelines.resize((size_t)ModVolMode::Final);
	trModVolPipelines[(size_t)mode] =
			GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(),
					graphicsPipelineCreateInfo);
}
