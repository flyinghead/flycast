#include "vulkan.h"
#include "oit_shaders.h"

const vk::DeviceSize PixelBufferSize = 512 * 1024 * 1024;

vk::UniquePipelineLayout pipelineLayout;
vk::UniqueDescriptorSetLayout perFrameLayout;
vk::UniqueDescriptorSetLayout perPolyLayout;
vk::UniqueRenderPass renderPass;

vk::ImageView colorImageView;
vk::ImageView depthImageView;
vk::ImageView stencilImageView;

std::unique_ptr<FramebufferAttachment> colorAttachment;
std::unique_ptr<FramebufferAttachment> depthAttachment;

std::vector<vk::UniqueFramebuffer> framebuffers;

std::unique_ptr<BufferData> pixelBuffer;
std::unique_ptr<BufferData> pixelCounter;
std::unique_ptr<FramebufferAttachment> abufferPointerAttachment;
std::unique_ptr<BufferData> trParamBuffer;

struct PushConstants
{
	glm::vec4 clipTest;
	glm::ivec2 blend_mode0;
	float trilinearAlpha;
	int pp_Number;

	// two volume mode
	glm::ivec2 blend_mode1;
	int shading_instr0;
	int shading_instr1;
	int fog_control0;
	int fog_control1;
	bool use_alpha0;
	bool use_alpha1;
	bool ignore_tex_alpha0;
	bool ignore_tex_alpha1;
};

struct QuadVertex
{
	f32 pos[3];
	f32 uv[2];
};

vk::PipelineVertexInputStateCreateInfo GetQuadInputStateCreateInfo(bool uv)
{
	// Vertex input state
	static const vk::VertexInputBindingDescription vertexBindingDescriptions[] =
	{
			{ 0, sizeof(QuadVertex) },
	};
	static const vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] =
	{
			vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(QuadVertex, pos)),	// pos
			vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32Sfloat, offsetof(QuadVertex, uv)),		// tex coord
	};
	return vk::PipelineVertexInputStateCreateInfo(
			vk::PipelineVertexInputStateCreateFlags(),
			ARRAY_SIZE(vertexBindingDescriptions),
			vertexBindingDescriptions,
			ARRAY_SIZE(vertexInputAttributeDescriptions) - (uv ? 0 : 1),
			vertexInputAttributeDescriptions);
}

static VulkanContext *GetContext()
{
	return VulkanContext::Instance();
}

void makeRenderPass()
{
    vk::AttachmentDescription attachmentDescriptions[] = {
    		// Swap chain image
    		vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetColorFormat(), vk::SampleCountFlagBits::e1,
    				vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR),
			// OP+PT color attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetColorFormat(), vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal),
			// OP+PT depth attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal),
			// new depth attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal),
    };
    vk::AttachmentReference swapChainReference(0, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference colorReference(1, vk::ImageLayout::eColorAttachmentOptimal);
    vk::AttachmentReference depthReference(2, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    vk::AttachmentReference depth2Reference(3, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentReference depthReadReference(2, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
    vk::AttachmentReference colorReadReference(1, vk::ImageLayout::eShaderReadOnlyOptimal);

    vk::SubpassDescription subpasses[] = {
    		// Depth and modvol pass
    	    vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 0, nullptr, 0, nullptr, nullptr, &depthReference),
    	    // Color pass
    	    vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 1, &depthReadReference, 1, &colorReference, nullptr, &depth2Reference),
    	    // Final pass
    	    vk::SubpassDescription(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 1, &colorReadReference, 1, &swapChainReference, nullptr, nullptr),
    };

    vk::SubpassDependency dependencies[] {
    	vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eBottomOfPipe, vk::PipelineStageFlagBits::eColorAttachmentOutput,
    			vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead, vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(0, 1, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(1, 2, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead, vk::DependencyFlagBits::eByRegion),
		vk::SubpassDependency(2, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eBottomOfPipe,
				vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eMemoryRead, vk::DependencyFlagBits::eByRegion),
    };

    renderPass = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
    		ARRAY_SIZE(attachmentDescriptions), attachmentDescriptions,
    		ARRAY_SIZE(subpasses), subpasses,
			ARRAY_SIZE(dependencies), dependencies));
}

void makeAttachments(int width, int height, Allocator *allocator)
{
	colorAttachment = std::unique_ptr<FramebufferAttachment>(new FramebufferAttachment(GetContext()->GetPhysicalDevice(),
			GetContext()->GetDevice(), allocator));
	colorAttachment->Init(width, height, GetContext()->GetColorFormat(), vk::ImageUsageFlagBits::eInputAttachment);
	colorImageView = colorAttachment->GetImageView();

	depthAttachment = std::unique_ptr<FramebufferAttachment>(new FramebufferAttachment(GetContext()->GetPhysicalDevice(),
				GetContext()->GetDevice(), allocator));
	depthAttachment->Init(width, height, GetContext()->GetDepthFormat(), vk::ImageUsageFlagBits::eInputAttachment);
	depthImageView = depthAttachment->GetImageView();
	stencilImageView = depthAttachment->GetStencilView();
}

void makeFramebuffers(int width, int height, vk::RenderPass renderPass)
{
	vk::ImageView attachments[] = {
			nullptr,	// swap chain image view, set later
			colorImageView,
			depthImageView,
			stencilImageView
	};
	framebuffers.reserve(GetContext()->GetSwapChainSize());
	for (int i = 0; i < GetContext()->GetSwapChainSize(); i++)
	{
		vk::FramebufferCreateInfo createInfo(vk::FramebufferCreateFlags(), renderPass, 4, attachments, width, height, 1);
		attachments[0] = GetContext()->GetSwapChainImageView(i);
		framebuffers.push_back(GetContext()->GetDevice().createFramebufferUnique(createInfo));
	}
}

void makeBuffers(int width, int height, int trParamBufSize, Allocator *allocator)
{
	pixelBuffer = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(), PixelBufferSize,
			vk::BufferUsageFlagBits::eStorageBuffer, &SimpleAllocator::instance, vk::MemoryPropertyFlagBits::eDeviceLocal));
	pixelCounter = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(), 4,
			vk::BufferUsageFlagBits::eStorageBuffer, allocator, vk::MemoryPropertyFlagBits::eDeviceLocal));
	trParamBuffer = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(), trParamBufSize,
			vk::BufferUsageFlagBits::eStorageBuffer, allocator));
	abufferPointerAttachment = std::unique_ptr<FramebufferAttachment>(new FramebufferAttachment(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(),
			allocator));
	abufferPointerAttachment->Init(width, height, vk::Format::eR32Uint);
}

void makeDescSetLayout()
{
	vk::DescriptorSetLayoutBinding perFrameBindings[] = {
			{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },			// vertex uniforms
			{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// fragment uniforms
			{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// fog texture
			{ 3, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// stencil input attachment
			{ 4, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// depth input attachment
			{ 5, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// pixel buffer
			{ 6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// pixel counter
			{ 7, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eFragment },		// a-buffer pointers
			{ 8, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// Tr poly params
	};
	perFrameLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perFrameBindings), perFrameBindings));

	vk::DescriptorSetLayoutBinding perPolyBindings[] = {
			{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture 0
			{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture 1 (for 2-volume mode)
	};
	perPolyLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
			vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perFrameBindings), perFrameBindings));
	vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(PushConstants));
	vk::DescriptorSetLayout layouts[] = { *perFrameLayout, *perPolyLayout };
	pipelineLayout = GetContext()->GetDevice().createPipelineLayoutUnique(
			vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), ARRAY_SIZE(layouts), layouts, 1, &pushConstant));
}

void makeFinalPipeline(bool depthSorted, OITShaderManager *shaderManager)
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetQuadInputStateCreateInfo();

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
	vk::ShaderModule fragment_module = shaderManager->GetFinalShader(depthSorted);

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

	// TODO store...
	GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo);

}

void makeClearPipeline(OITShaderManager *shaderManager)
{
	vk::PipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo = GetQuadInputStateCreateInfo();

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

	// TODO store...
	GetContext()->GetDevice().createGraphicsPipelineUnique(GetContext()->GetPipelineCache(), graphicsPipelineCreateInfo);

}

void InitOIT(Allocator *allocator)
{
	const int width = (int)lroundf((float)GetContext()->GetViewPort().width * settings.rend.ScreenScaling / 100);
	const int height = (int)lroundf((float)GetContext()->GetViewPort().height * settings.rend.ScreenScaling / 100);
	makeRenderPass();
	makeDescSetLayout();
	makeBuffers(width, height, 2 * 1024 * 1024, allocator);
	makeAttachments(width, height, allocator);
	makeFramebuffers(width, height, *renderPass);
	OITShaderManager shaderManager;
	makeFinalPipeline(true, &shaderManager);
	makeClearPipeline(&shaderManager);
	printf("YO BIATCH!!\n");
}
