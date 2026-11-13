/*
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
#include "hikaru_drawer.h"

#include <algorithm>

using namespace hikaru;

static constexpr vk::Format TextureFormat = vk::Format::eR8G8B8A8Unorm;

void HikaruPipelineManager::Init(HikaruShaderManager *shaderManager, vk::RenderPass renderPass, vk::RenderPass compositePass)
{
	this->shaderManager = shaderManager;
	vk::Device device = VulkanContext::Instance()->GetDevice();
	if (!pipelineLayout)
	{
		const vk::DescriptorSetLayoutBinding bindings[] = {
				{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },	// draw uniforms
				{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment },	// shading
				{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },	// texture
		};
		descSetLayout = device.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), bindings));
		pipelineLayout = device.createPipelineLayoutUnique(
				vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), descSetLayout.get()));
	}
	if (!screenPipelineLayout)
	{
		// the three slots of the final picture (the frame blender uses the first one)
		const vk::DescriptorSetLayoutBinding bindings[] = {
				{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
				{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
				{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },
		};
		screenDescSetLayout = device.createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), bindings));
		vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(HikaruScreenConstants));
		screenPipelineLayout = device.createPipelineLayoutUnique(
				vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), screenDescSetLayout.get(), pushConstant));
	}
	for (int repeat = 0; repeat < 2; repeat++)
	{
		if (screenSamplers[repeat])
			continue;
		const vk::SamplerAddressMode address = repeat ? vk::SamplerAddressMode::eRepeat : vk::SamplerAddressMode::eClampToEdge;
		screenSamplers[repeat] = device.createSamplerUnique(
				vk::SamplerCreateInfo(vk::SamplerCreateFlags(),
						vk::Filter::eNearest, vk::Filter::eNearest,
						vk::SamplerMipmapMode::eNearest,
						address, address, address, 0.0f, false,
						1.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f,
						vk::BorderColor::eFloatOpaqueBlack));
	}
	if (this->renderPass != renderPass || this->compositePass != compositePass)
	{
		this->renderPass = renderPass;
		this->compositePass = compositePass;
		pipelines.clear();
		blendPipeline.reset();
		compositePipeline.reset();
	}
}

vk::Sampler HikaruPipelineManager::GetSampler(bool linear, bool mipmapped, Address u, Address v)
{
	const u32 key = (((u32)linear * 2 + (u32)mipmapped) * 3 + (u32)u) * 3 + (u32)v;
	auto it = samplers.find(key);
	if (it != samplers.end())
		return it->second.get();

	static const vk::SamplerAddressMode address[3] {
		vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eRepeat, vk::SamplerAddressMode::eMirroredRepeat
	};
	const vk::Filter filter = linear ? vk::Filter::eLinear : vk::Filter::eNearest;
	return samplers.emplace(key, VulkanContext::Instance()->GetDevice().createSamplerUnique(
			vk::SamplerCreateInfo(vk::SamplerCreateFlags(), filter, filter,
					linear ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest,
					address[(int)u], address[(int)v], address[(int)v], 0.0f,
					false, 1.0f, false, vk::CompareOp::eNever,
					// nearest and bilinear filtering sample the base level only
					0.0f, mipmapped ? vk::LodClampNone : 0.0f,
					vk::BorderColor::eFloatOpaqueBlack))).first->second.get();
}

void HikaruPipelineManager::CreatePipeline(bool textured, const Draw& draw)
{
	// Vertex input state
	static const vk::VertexInputBindingDescription vertexBindingDescriptions[] =
	{
			{ 0, sizeof(DrawVertex) },
	};
	static const vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(DrawVertex, pos)),
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(DrawVertex, normal)),
			vk::VertexInputAttributeDescription(2, 0, vk::Format::eR32G32Sfloat, offsetof(DrawVertex, uv)),
			vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32Sfloat, offsetof(DrawVertex, alpha)),
			vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32Uint, offsetof(DrawVertex, mode)),
	};
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo(
			vk::PipelineVertexInputStateCreateFlags(),
			vertexBindingDescriptions,
			vertexInputAttributeDescriptions);

	// Input assembly state
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleList);

	// Viewport and scissor states
	vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);

	// Rasterization and multisample states. Same face convention as flycast's PVR pipelines.
	vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo
	(
	  vk::PipelineRasterizationStateCreateFlags(),  // flags
	  false,                                        // depthClampEnable
	  false,                                        // rasterizerDiscardEnable
	  vk::PolygonMode::eFill,                       // polygonMode
	  draw.cull == Cull::Back ? vk::CullModeFlagBits::eBack
			  : draw.cull == Cull::Front ? vk::CullModeFlagBits::eFront
			  : vk::CullModeFlagBits::eNone,        // cullMode
	  vk::FrontFace::eCounterClockwise,             // frontFace
	  false,                                        // depthBiasEnable
	  0.0f,                                         // depthBiasConstantFactor
	  0.0f,                                         // depthBiasClamp
	  0.0f,                                         // depthBiasSlopeFactor
	  1.0f                                          // lineWidth
	);
	vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;

	// Depth: greater or equal (the depth grows nearer the eye), cleared to 0
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo
	(
	  vk::PipelineDepthStencilStateCreateFlags(), // flags
	  true,                                       // depthTestEnable
	  draw.depthWrite,                            // depthWriteEnable
	  draw.depthTest ? vk::CompareOp::eGreaterOrEqual : vk::CompareOp::eAlways, // depthCompareOp
	  false,                                      // depthBoundTestEnable
	  false,                                      // stencilTestEnable
	  vk::StencilOpState(),                       // front
	  vk::StencilOpState()                        // back
	);

	// Blending: opaque, then the material blend modes (0x081 sub 5)
	static const vk::BlendFactor src[5] { vk::BlendFactor::eOne, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eDstColor,
		vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOne };
	static const vk::BlendFactor dst[5] { vk::BlendFactor::eZero, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendFactor::eZero,
		vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha };
	const int blend = (int)draw.blend;
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState
	(
		draw.blend != Blend::Opaque,		// blendEnable
		src[blend],							// srcColorBlendFactor
		dst[blend],							// dstColorBlendFactor
		vk::BlendOp::eAdd,					// colorBlendOp
		// the 3D buffer keeps the alpha of the last fragment (Demul 2025): 0x1BC bit 0 keys the 3D picture on it
		vk::BlendFactor::eOne,				// srcAlphaBlendFactor
		vk::BlendFactor::eZero,				// dstAlphaBlendFactor
		vk::BlendOp::eAdd,					// alphaBlendOp
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	);
	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
	  vk::PipelineColorBlendStateCreateFlags(),   // flags
	  false,                                      // logicOpEnable
	  vk::LogicOp::eNoOp,                         // logicOp
	  pipelineColorBlendAttachmentState,          // attachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, shaderManager->GetVertexShader(), "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, shaderManager->GetFragmentShader(textured), "main"),
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

	pipelines[hash(textured, draw)] = VulkanContext::Instance()->GetDevice().createGraphicsPipelineUnique(
			VulkanContext::Instance()->GetPipelineCache(), graphicsPipelineCreateInfo).value;
}

void HikaruPipelineManager::CreateScreenPipeline(bool composite)
{
	// Full-screen quad generated by the vertex shader
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo;
	vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo(vk::PipelineInputAssemblyStateCreateFlags(),
			vk::PrimitiveTopology::eTriangleStrip);
	vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo(vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr);
	vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo;
	pipelineRasterizationStateCreateInfo.lineWidth = 1.0;
	vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo;
	// No depth test or write
	vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(vk::PipelineDepthStencilStateCreateFlags(),
			false, false, vk::CompareOp::eAlways);

	// Frame blender: target = factor * source + (1 - factor) * target, target alpha = factor.
	// Final picture: no blending.
	vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState
	(
		!composite,							// blendEnable
		vk::BlendFactor::eSrcAlpha,			// srcColorBlendFactor
		vk::BlendFactor::eOneMinusSrcAlpha, // dstColorBlendFactor
		vk::BlendOp::eAdd,					// colorBlendOp
		vk::BlendFactor::eOne,				// srcAlphaBlendFactor
		vk::BlendFactor::eZero,				// dstAlphaBlendFactor
		vk::BlendOp::eAdd,					// alphaBlendOp
		vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA
	);
	vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo
	(
	  vk::PipelineColorBlendStateCreateFlags(),   // flags
	  false,                                      // logicOpEnable
	  vk::LogicOp::eNoOp,                         // logicOp
	  pipelineColorBlendAttachmentState,          // attachments
	  { { 1.0f, 1.0f, 1.0f, 1.0f } }              // blendConstants
	);

	std::array<vk::DynamicState, 2> dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
	vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo(vk::PipelineDynamicStateCreateFlags(), dynamicStates);

	std::array<vk::PipelineShaderStageCreateInfo, 2> stages = {
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eVertex, shaderManager->GetScreenVertexShader(), "main"),
			vk::PipelineShaderStageCreateInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eFragment, shaderManager->GetScreenFragmentShader(composite), "main"),
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
	  *screenPipelineLayout,                      // layout
	  composite ? compositePass : renderPass      // renderPass
	);

	(composite ? compositePipeline : blendPipeline) = VulkanContext::Instance()->GetDevice().createGraphicsPipelineUnique(
			VulkanContext::Instance()->GetPipelineCache(), graphicsPipelineCreateInfo).value;
}

void HikaruDrawer::Init(CommandPool *commandPool)
{
	SetCommandPool(commandPool);
	if (!renderPass)
	{
		std::array<vk::AttachmentDescription, 2> attachmentDescriptions = {
				// Color attachment, cleared to the draw list's clear color
				vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
						vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
						vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
						vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal),
				// Depth attachment
				vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
						vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
						vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
						vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal),
		};
		vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
						nullptr,
						colorReference,
						nullptr,
						&depthReference);

		// The 3D buffers are read by the final pictures before and after
		const std::array<vk::SubpassDependency, 2> dependencies = {
			vk::SubpassDependency(vk::SubpassExternal, 0, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
					vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite),
			vk::SubpassDependency(0, vk::SubpassExternal, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead),
		};
		renderPass = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
				attachmentDescriptions,
				subpass,
				dependencies));
	}
	if (!compositePass)
	{
		// The final picture covers the whole image
		vk::AttachmentDescription attachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eStore,
				vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
						nullptr,
						colorReference,
						nullptr,
						nullptr);
		const std::array<vk::SubpassDependency, 2> dependencies = {
			vk::SubpassDependency(vk::SubpassExternal, 0, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead),
			vk::SubpassDependency(0, vk::SubpassExternal, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::DependencyFlagBits::eByRegion),
		};
		compositePass = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
				attachmentDescription,
				subpass,
				dependencies));
	}
	pipelineManager.Init(&shaderManager, *renderPass, *compositePass);
	descSetAlloc.setLayout(pipelineManager.GetDescSetLayout());
	screenDescSetAlloc.setLayout(pipelineManager.GetScreenDescSetLayout());
}

void HikaruDrawer::Term()
{
	textures.clear();
	framebuffers.clear();
	colorAttachments.clear();
	bufferFramebuffers.clear();
	buffers.clear();
	depthAttachment.reset();
	buffersCleared = false;
	extent = vk::Extent2D();
	descSetAlloc.term();
	screenDescSetAlloc.term();
	pipelineManager.Term();
	shaderManager.term();
	renderPass.reset();
	compositePass.reset();
	mainBuffers.clear();
	frameRendered = false;
}

void HikaruDrawer::initRenderTarget(u32 width, u32 height)
{
	if (extent == vk::Extent2D(width, height) && !colorAttachments.empty())
		return;
	// the previous frames may still use them
	if (!framebuffers.empty())
		commandPool->addToFlight(new Deleter(std::move(framebuffers)));
	if (!colorAttachments.empty())
		commandPool->addToFlight(new Deleter(std::move(colorAttachments)));
	if (!bufferFramebuffers.empty())
		commandPool->addToFlight(new Deleter(std::move(bufferFramebuffers)));
	if (!buffers.empty())
		commandPool->addToFlight(new Deleter(std::move(buffers)));
	if (depthAttachment)
		commandPool->addToFlight(new Deleter(depthAttachment.release()));
	framebuffers.clear();
	colorAttachments.clear();
	bufferFramebuffers.clear();
	buffers.clear();
	frameRendered = false;
	buffersCleared = false;
	extent = vk::Extent2D(width, height);

	depthAttachment = std::make_unique<FramebufferAttachment>(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice());
	depthAttachment->Init(width, height, GetContext()->GetDepthFormat(),
			vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
			"HIKARU DEPTH ATTACHMENT");
	for (int i = 0; i < HIKARU_3D_BUFFERS; i++)
	{
		buffers.push_back(std::make_unique<FramebufferAttachment>(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice()));
		buffers.back()->Init(width, height, vk::Format::eR8G8B8A8Unorm,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
				"HIKARU 3D BUFFER " + std::to_string(i));
		std::array<vk::ImageView, 2> attachments = {
				buffers.back()->GetImageView(),
				depthAttachment->GetImageView(),
		};
		bufferFramebuffers.push_back(GetContext()->GetDevice().createFramebufferUnique(
				vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass, attachments, width, height, 1)));
	}
	// one being drawn, one being presented
	for (int i = 0; i < 2; i++)
	{
		colorAttachments.push_back(std::make_unique<FramebufferAttachment>(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice()));
		colorAttachments.back()->Init(width, height, vk::Format::eR8G8B8A8Unorm,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
				"HIKARU COLOR ATTACHMENT " + std::to_string(i));
		vk::ImageView attachment = colorAttachments.back()->GetImageView();
		framebuffers.push_back(GetContext()->GetDevice().createFramebufferUnique(
				vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *compositePass, attachment, width, height, 1)));
	}
}

void HikaruDrawer::freeTexture(std::unique_ptr<HikaruTexture>& texture)
{
	if (texture)
		// a previous frame may still use it
		commandPool->addToFlight(new Deleter(texture.release()));
}

// Upload a texture store entry if it changed. Called with the store locked.
bool HikaruDrawer::updateTexture(vk::CommandBuffer cmdBuffer, int id)
{
	TextureStore& store = textureStore();
	if (id < 0 || id >= (int)store.entries.size())
		return false;
	TextureEntry& e = store.entries[id];
	if (e.version == 0 || e.texels.empty())
		return false;
	if (id >= (int)textures.size())
		textures.resize(id + 1);
	std::unique_ptr<HikaruTexture>& t = textures[id];
	if (t && t->version == e.version)
		return true;

	const bool isNew = !t || t->width != e.width || t->height != e.height || t->levels != e.levels;
	if (isNew)
	{
		freeTexture(t);
		t = std::make_unique<HikaruTexture>();
		vk::Device device = GetContext()->GetDevice();
		vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, TextureFormat,
				vk::Extent3D(e.width, e.height, 1), e.levels, 1,
				vk::SampleCountFlagBits::e1, vk::ImageTiling::eOptimal,
				vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
				vk::SharingMode::eExclusive, nullptr, vk::ImageLayout::eUndefined);
		t->image = device.createImageUnique(imageCreateInfo);
		VmaAllocationCreateInfo allocCreateInfo = { VmaAllocationCreateFlags(), VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY };
		t->allocation = GetContext()->GetAllocator().AllocateForImage(*t->image, allocCreateInfo);
		t->imageView = device.createImageViewUnique(vk::ImageViewCreateInfo(vk::ImageViewCreateFlags(), *t->image,
				vk::ImageViewType::e2D, TextureFormat, vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, e.levels, 0, 1)));
		t->width = e.width;
		t->height = e.height;
		t->levels = e.levels;
	}

	std::vector<vk::BufferImageCopy> regions;
	const u32 *src;
	u32 size;
	if (!isNew && e.levels == 1 && e.dirtyY1 > e.dirtyY0)
	{
		// only the rows that changed (2D layers)
		src = &e.texels[e.dirtyY0 * e.width];
		size = (e.dirtyY1 - e.dirtyY0) * e.width * 4;
		regions.emplace_back(0, e.width, e.dirtyY1 - e.dirtyY0, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
				vk::Offset3D(0, (s32)e.dirtyY0, 0), vk::Extent3D(e.width, e.dirtyY1 - e.dirtyY0, 1));
	}
	else
	{
		src = e.texels.data();
		size = 0;
		for (u32 l = 0; l < e.levels; l++)
		{
			u32 w = std::max(e.width >> l, 1u);
			u32 h = std::max(e.height >> l, 1u);
			regions.emplace_back(size, w, h, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, l, 0, 1),
					vk::Offset3D(0, 0, 0), vk::Extent3D(w, h, 1));
			size += w * h * 4;
		}
		if (size > e.texels.size() * 4)
		{
			WARN_LOG(RENDERER, "Hikaru texture %d: not enough texels (%dx%d, %d levels)", id, e.width, e.height, e.levels);
			return false;
		}
	}
	BufferData *stagingBuffer = new BufferData(size, vk::BufferUsageFlagBits::eTransferSrc);
	stagingBuffer->upload(size, src);

	setImageLayout(cmdBuffer, *t->image, TextureFormat, t->levels,
			isNew ? vk::ImageLayout::eUndefined : vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eTransferDstOptimal);
	cmdBuffer.copyBufferToImage(*stagingBuffer->buffer, *t->image, vk::ImageLayout::eTransferDstOptimal, regions);
	setImageLayout(cmdBuffer, *t->image, TextureFormat, t->levels, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	commandPool->addToFlight(new Deleter(stagingBuffer));

	t->version = e.version;
	e.dirtyY0 = e.dirtyY1 = 0;
	return true;
}

vk::ImageView HikaruDrawer::textureView(int id) const
{
	if (id < 0 || id >= (int)textures.size() || !textures[id] || textures[id]->version == 0)
		return vk::ImageView();
	return *textures[id]->imageView;
}

void HikaruDrawer::uploadTextures(vk::CommandBuffer cmdBuffer, const DrawList& dl)
{
	TextureStore& store = textureStore();
	std::lock_guard<std::mutex> lock(store.mutex);
	// Textures freed by the emulation are released here too
	for (size_t id = 0; id < textures.size(); id++)
		if (id >= store.entries.size() || store.entries[id].version == 0)
			freeTexture(textures[id]);
	int last = -1;
	for (const hikaru::Draw& d : dl.draws)
		if (d.texture >= 0 && d.texture != last)
		{
			updateTexture(cmdBuffer, d.texture);
			last = d.texture;
		}
	updateTexture(cmdBuffer, dl.blend.texture);
	for (const CompositeSlot& slot : dl.composite.slots)
		updateTexture(cmdBuffer, slot.texture);
}

vk::Rect2D HikaruDrawer::scaleRect(int x0, int y0, int x1, int y1) const
{
	// screen pixels (640x480) to framebuffer pixels
	x0 = std::clamp((int)(x0 * scaleX + 0.5f), 0, (int)extent.width);
	y0 = std::clamp((int)(y0 * scaleY + 0.5f), 0, (int)extent.height);
	x1 = std::clamp((int)(x1 * scaleX + 0.5f), 0, (int)extent.width);
	y1 = std::clamp((int)(y1 * scaleY + 0.5f), 0, (int)extent.height);
	return vk::Rect2D(vk::Offset2D(x0, y0), vk::Extent2D(std::max(x1 - x0, 0), std::max(y1 - y0, 0)));
}

void HikaruDrawer::drawScreen(vk::CommandBuffer cmdBuffer, bool composite, const vk::ImageView views[3], const bool repeat[3],
		const HikaruScreenConstants& constants)
{
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.GetScreenPipeline(composite));
	SetScissor(cmdBuffer, vk::Rect2D({ 0, 0 }, extent));
	vk::DescriptorSet descSet = screenDescSetAlloc.alloc();
	std::array<vk::DescriptorImageInfo, 3> imageInfos;
	std::array<vk::WriteDescriptorSet, 3> writes;
	for (int i = 0; i < 3; i++)
	{
		imageInfos[i] = vk::DescriptorImageInfo(pipelineManager.GetScreenSampler(repeat[i]), views[i], vk::ImageLayout::eShaderReadOnlyOptimal);
		writes[i] = vk::WriteDescriptorSet(descSet, i, 0, vk::DescriptorType::eCombinedImageSampler, imageInfos[i]);
	}
	GetContext()->GetDevice().updateDescriptorSets(writes, nullptr);
	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineManager.GetScreenPipelineLayout(), 0, descSet, nullptr);
	cmdBuffer.pushConstants(pipelineManager.GetScreenPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, sizeof(constants), &constants);
	cmdBuffer.draw(4, 1, 0, 0);
}

void HikaruDrawer::Draw(const DrawList& dl, u32 width, u32 height)
{
	scaleX = (float)width / SCREEN_WIDTH;
	scaleY = (float)height / SCREEN_HEIGHT;
	initRenderTarget(width, height);
	imageIndex = (imageIndex + 1) % colorAttachments.size();
	descSetAlloc.nextFrame();
	screenDescSetAlloc.nextFrame();
	frameRendered = false;

	vk::CommandBuffer cmdBuffer = commandPool->Allocate(true);
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	record(cmdBuffer, dl);
	cmdBuffer.end();
	commandPool->EndFrame();
	frameRendered = true;
}

void HikaruDrawer::record(vk::CommandBuffer cmdBuffer, const DrawList& dl)
{
	static const float scopeColor[4] = { 0.75f, 0.75f, 0.75f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "Hikaru Draw", scopeColor);

	uploadTextures(cmdBuffer, dl);

	// Geometry and uniforms of the frame in one buffer: vertices, indices, matrices, shadings
	BufferPacker packer;
	packer.add(dl.vertices.data(), dl.vertices.size() * sizeof(DrawVertex));
	const vk::DeviceSize indexOffset = packer.add(dl.indices.data(), dl.indices.size() * sizeof(u32));
	std::vector<HikaruDrawUniforms> drawUniforms;
	std::vector<int> drawUniformIndex(dl.draws.size(), -1);
	{
		int lastMatrices[3] { -1, -1, -1 };
		int lastPattern = -1;
		for (size_t i = 0; i < dl.draws.size(); i++)
		{
			const hikaru::Draw& d = dl.draws[i];
			if (d.modelview[0] != lastMatrices[0] || d.modelview[1] != lastMatrices[1] || d.projection != lastMatrices[2]
					|| d.pattern != lastPattern)
			{
				HikaruDrawUniforms& u = drawUniforms.emplace_back();
				u.params[0] = (float)d.pattern;
				u.params[1] = u.params[2] = u.params[3] = 0.f;
				lastPattern = d.pattern;
				memcpy(u.modelview0, dl.matrices[d.modelview[0]].data(), sizeof(u.modelview0));
				memcpy(u.modelview1, dl.matrices[d.modelview[1]].data(), sizeof(u.modelview1));
				memcpy(u.projection, dl.matrices[d.projection].data(), sizeof(u.projection));
				lastMatrices[0] = d.modelview[0];
				lastMatrices[1] = d.modelview[1];
				lastMatrices[2] = d.projection;
			}
			drawUniformIndex[i] = (int)drawUniforms.size() - 1;
		}
	}
	std::vector<vk::DeviceSize> drawUniformOffsets;
	drawUniformOffsets.reserve(drawUniforms.size());
	for (const HikaruDrawUniforms& u : drawUniforms)
		drawUniformOffsets.push_back(packer.addUniform(&u, sizeof(u)));
	std::vector<vk::DeviceSize> shadingOffsets;
	shadingOffsets.reserve(dl.shadings.size());
	for (const Shading& s : dl.shadings)
		shadingOffsets.push_back(packer.addUniform(&s, sizeof(s)));
	BufferData *buffer = GetMainBuffer((u32)packer.size());
	packer.upload(*buffer);
	const vk::Buffer mainBuffer = *buffer->buffer;

	if (!buffersCleared)
	{
		// new 3D buffers: black, and readable by the final picture
		const std::array<vk::ClearValue, 2> blackValues = {
			vk::ClearColorValue(std::array<float, 4> { 0.f, 0.f, 0.f, 0.f }),
			vk::ClearDepthStencilValue { 0.f, 0 }
		};
		for (const vk::UniqueFramebuffer& fb : bufferFramebuffers)
		{
			cmdBuffer.beginRenderPass(vk::RenderPassBeginInfo(*renderPass, *fb, vk::Rect2D({ 0, 0 }, extent), blackValues),
					vk::SubpassContents::eInline);
			cmdBuffer.endRenderPass();
		}
		buffersCleared = true;
	}

	// The 3D picture goes into the target buffer, cleared with the clear color
	const int target = dl.target % HIKARU_3D_BUFFERS;
	const std::array<vk::ClearValue, 2> clearValues = {
		vk::ClearColorValue(std::array<float, 4> { dl.clearColor[0], dl.clearColor[1], dl.clearColor[2], dl.clearColor[3] }),
		vk::ClearDepthStencilValue { 0.f, 0 }
	};
	cmdBuffer.beginRenderPass(vk::RenderPassBeginInfo(*renderPass, *bufferFramebuffers[target],
			vk::Rect2D({ 0, 0 }, extent), clearValues), vk::SubpassContents::eInline);
	cmdBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f));
	currentScissor = vk::Rect2D();

	if (!dl.passes.empty() && !dl.vertices.empty() && !dl.indices.empty())
	{
		cmdBuffer.bindVertexBuffers(0, mainBuffer, { 0 });
		cmdBuffer.bindIndexBuffer(mainBuffer, indexOffset, vk::IndexType::eUint32);

		vk::Pipeline lastPipeline;
		vk::DescriptorSet descSet;
		int lastUniforms = -1;
		int lastShading = -1;
		vk::ImageView lastView;
		vk::Sampler lastSampler;
		for (const Pass& pass : dl.passes)
		{
			// higher viewport priorities draw over lower ones
			cmdBuffer.clearAttachments(vk::ClearAttachment(vk::ImageAspectFlagBits::eDepth, 0, vk::ClearDepthStencilValue{ 0.f, 0 }),
					vk::ClearRect(vk::Rect2D({ 0, 0 }, extent), 0, 1));
			for (u32 i = pass.firstDraw; i < pass.firstDraw + pass.drawCount; i++)
			{
				const hikaru::Draw& d = dl.draws[i];
				if (d.indexCount == 0 || d.clip[2] <= d.clip[0] || d.clip[3] <= d.clip[1])
					continue;
				vk::ImageView view = textureView(d.texture);
				if (d.texture >= 0 && !view)
					continue;
				const vk::Rect2D scissor = scaleRect(d.clip[0], d.clip[1], d.clip[2], d.clip[3]);
				if (scissor.extent.width == 0 || scissor.extent.height == 0)
					continue;
				vk::Sampler sampler;
				if (view)
					sampler = pipelineManager.GetSampler(d.linear, d.mipmapped, d.addressU, d.addressV);

				if (!descSet || drawUniformIndex[i] != lastUniforms || d.shading != lastShading || view != lastView || sampler != lastSampler)
				{
					descSet = descSetAlloc.alloc();
					vk::DescriptorBufferInfo drawBufferInfo(mainBuffer, drawUniformOffsets[drawUniformIndex[i]], sizeof(HikaruDrawUniforms));
					vk::DescriptorBufferInfo shadingBufferInfo(mainBuffer, shadingOffsets[d.shading], sizeof(Shading));
					vk::DescriptorImageInfo imageInfo(sampler, view, vk::ImageLayout::eShaderReadOnlyOptimal);
					std::vector<vk::WriteDescriptorSet> writes {
						vk::WriteDescriptorSet(descSet, 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, drawBufferInfo),
						vk::WriteDescriptorSet(descSet, 1, 0, vk::DescriptorType::eUniformBuffer, nullptr, shadingBufferInfo),
					};
					if (view)
						writes.emplace_back(descSet, 2, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
					GetContext()->GetDevice().updateDescriptorSets(writes, nullptr);
					cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineManager.GetPipelineLayout(), 0, descSet, nullptr);
					lastUniforms = drawUniformIndex[i];
					lastShading = d.shading;
					lastView = view;
					lastSampler = sampler;
				}
				vk::Pipeline pipeline = pipelineManager.GetPipeline((bool)view, d);
				if (pipeline != lastPipeline)
				{
					cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
					lastPipeline = pipeline;
				}
				SetScissor(cmdBuffer, scissor);
				cmdBuffer.drawIndexed(d.indexCount, 1, d.firstIndex, 0, 0);
			}
		}
	}

	HikaruScreenConstants constants {};
	constants.blendFactor[0] = dl.blend.factor;
	// Frame blender: target = factor * source + (1 - factor) * target, target alpha = factor.
	// A source that is the target itself reads as black.
	if (dl.blend.enabled)
	{
		vk::ImageView source;
		if (dl.blend.source < 2)
		{
			if (dl.blend.source != target)
				source = buffers[dl.blend.source]->GetImageView();
		}
		else
			source = textureView(dl.blend.texture);
		constants.slotKind[0] = (bool)source;
		// a valid image that is not drawn to now, never read when the source reads as black
		const vk::ImageView any = buffers[(target + 1) % HIKARU_3D_BUFFERS]->GetImageView();
		const vk::ImageView views[3] { source ? source : any, any, any };
		const bool repeat[3] {};
		drawScreen(cmdBuffer, false, views, repeat, constants);
	}
	cmdBuffer.endRenderPass();

	// Final picture: the shown 3D buffer and the 2D layers, in the 0x1B8 order
	cmdBuffer.beginRenderPass(vk::RenderPassBeginInfo(*compositePass, *framebuffers[imageIndex],
			vk::Rect2D({ 0, 0 }, extent), nullptr), vk::SubpassContents::eInline);
	cmdBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f));
	currentScissor = vk::Rect2D();
	const vk::ImageView shown = buffers[dl.displayed % HIKARU_3D_BUFFERS]->GetImageView();
	vk::ImageView views[3];
	bool repeat[3];
	for (int i = 0; i < 3; i++)
	{
		const CompositeSlot& slot = dl.composite.slots[i];
		vk::ImageView view = slot.is3D ? shown : textureView(slot.texture);
		constants.slotKind[i] = slot.is3D ? 1 : view ? 2 : 0;
		views[i] = view ? view : shown;
		repeat[i] = !slot.is3D;
		constants.scroll[i][0] = (float)slot.scrollX / SCREEN_WIDTH;
		constants.scroll[i][1] = (float)slot.scrollY / SCREEN_HEIGHT;
	}
	constants.keyed3D[0] = dl.composite.keyed3D ? 1.f : 0.f;
	drawScreen(cmdBuffer, true, views, repeat, constants);
	cmdBuffer.endRenderPass();
}

bool HikaruDrawer::PresentFrame()
{
	if (!frameRendered)
		return false;
	frameRendered = false;
	// The PVR video output registers (border color, video shift) don't apply to the Hikaru
	GetContext()->PresentFrame(colorAttachments[imageIndex]->GetImage(), colorAttachments[imageIndex]->GetImageView(),
			extent, 4.f / 3.f, false);
	return true;
}
