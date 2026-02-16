/*
    Created on: Oct 2, 2019

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
#include "vulkan_context.h"
#include "hw/pvr/Renderer_if.h"
#include "compiler.h"
#include "oslib/oslib.h"
#include "rend/transform_matrix.h"
#include "texture.h"
#include <set>

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

VulkanContext *VulkanContext::contextInstance;

const VkApplicationInfo* VkGetApplicationInfo()
{
	// apiVersion is the maximum vulkan version supported by the app
	static vk::ApplicationInfo applicationInfo("Flycast", 1, "Flycast", 1, VK_API_VERSION_1_1);
	return &(VkApplicationInfo&)applicationInfo;
}

bool VkCreateDevice(retro_vulkan_context* context, VkInstance instance, VkPhysicalDevice gpu,
		VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
		const char** required_device_extensions,
		unsigned num_required_device_extensions,
		const char** required_device_layers, unsigned num_required_device_layers,
		const VkPhysicalDeviceFeatures* required_features)
{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
	VULKAN_HPP_DEFAULT_DISPATCHER.init(get_instance_proc_addr);
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance);
#endif

	vk::PhysicalDevice physicalDevice(gpu);
	if (gpu == VK_NULL_HANDLE)
	{
		// Choose a discrete gpu if there's one, otherwise just pick the first one
		verify(instance != VK_NULL_HANDLE);
		vk::Instance vkinstance(instance);
		const auto devices = vkinstance.enumeratePhysicalDevices();
		for (const auto& phyDev : devices)
		{
			vk::PhysicalDeviceProperties props;
			phyDev.getProperties(&props);
			if (props.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
			{
				physicalDevice = phyDev;
				break;
			}
		}
		if (!physicalDevice)
			physicalDevice = vkinstance.enumeratePhysicalDevices().front();
	}
	context->gpu = (VkPhysicalDevice)physicalDevice;

	const vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();

	std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

	// get the first index into queueFamilyProperties which supports graphics and compute
	context->queue_family_index = (u32)std::distance(queueFamilyProperties.begin(),
			std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
					[](vk::QueueFamilyProperties const& qfp) { return (qfp.queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute))
							== (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute); }));
	verify(context->queue_family_index < queueFamilyProperties.size());

	if (surface != VK_NULL_HANDLE)
	{
		// determine a queue family index that supports present
		// first check if the queue_family_index is good enough
		vk::SurfaceKHR vksurface(surface);
		context->presentation_queue_family_index = physicalDevice.getSurfaceSupportKHR(context->queue_family_index, vksurface) ? context->queue_family_index : queueFamilyProperties.size();
		if (context->presentation_queue_family_index == queueFamilyProperties.size())
		{
			// the queue_family_index doesn't support present -> look for an other family index that supports both graphics, compute and present
			for (size_t i = 0; i < queueFamilyProperties.size(); i++)
			{
				if ((queueFamilyProperties[i].queueFlags & (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)) == (vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute)
						&& physicalDevice.getSurfaceSupportKHR((u32)i, vksurface))
				{
					context->queue_family_index = (u32)i;
					context->presentation_queue_family_index = (u32)i;
					break;
				}
			}
			if (context->presentation_queue_family_index == queueFamilyProperties.size())
			{
				// there's nothing like a single family index that supports both graphics/compute and present -> look for an other family index that supports present
				DEBUG_LOG(RENDERER, "Using separate Graphics and Present queue families");
				for (size_t i = 0; i < queueFamilyProperties.size(); i++)
				{
					if (physicalDevice.getSurfaceSupportKHR((u32)i, vksurface))
					{
						context->presentation_queue_family_index = (u32)i;
						break;
					}
				}
			}
		}
		if (context->queue_family_index == queueFamilyProperties.size() || context->presentation_queue_family_index == queueFamilyProperties.size())
		{
			ERROR_LOG(RENDERER, "Could not find a queue for graphics or present");
			return false;
		}
		if (context->queue_family_index == context->presentation_queue_family_index)
			DEBUG_LOG(RENDERER, "Using Graphics+Present queue family");
		else
			DEBUG_LOG(RENDERER, "Using distinct Graphics and Present queue families");
	}


	std::set<std::string> supportedExtensions;

	const auto deviceExtensionProperties = physicalDevice.enumerateDeviceExtensionProperties();
	for (const auto& property : deviceExtensionProperties)
	{
		supportedExtensions.insert(property.extensionName);
	}

	std::vector<const char*> enabledExtensions;

	const auto tryAddDeviceExtension = [&supportedExtensions = std::as_const(supportedExtensions), &enabledExtensions]
	(std::string_view extensionName) -> bool
		{
			if (supportedExtensions.count(extensionName.data()))
			{
				enabledExtensions.push_back(extensionName.data());
				NOTICE_LOG(RENDERER, "Device extension enabled: %s", extensionName.data());
				return true;
			}
			NOTICE_LOG(RENDERER, "Device extension unavailable: %s", extensionName.data());
			return false;
		};

	// Required swapchain extension
	tryAddDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	// Enable VK_KHR_dedicated_allocation if available
	if (physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1)
	{
		// Core in Vulkan 1.1
		VulkanContext::Instance()->dedicatedAllocationSupported = true;
	}
	else
	{
		const bool getMemReq2Supported = tryAddDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
		if (getMemReq2Supported)
		{
			VulkanContext::Instance()->dedicatedAllocationSupported = tryAddDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
		}
	}

	// Check for VK_KHR_get_physical_device_properties2
	// Core as of Vulkan 1.1
	const bool getPhysicalDeviceProperties2Supported =
		(physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1)
		? true : tryAddDeviceExtension(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

	if (getPhysicalDeviceProperties2Supported)
	{
		// Enable VK_EXT_provoking_vertex if available
		VulkanContext::Instance()->provokingVertexSupported = tryAddDeviceExtension(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
	}

	// Get device features

	vk::PhysicalDeviceFeatures2 featuresChain{};
	vk::PhysicalDeviceFeatures& features = featuresChain.features;

	vk::PhysicalDeviceProvokingVertexFeaturesEXT provokingVertexFeatures{};
	if (VulkanContext::Instance()->provokingVertexSupported)
	{
		featuresChain.pNext = &provokingVertexFeatures;
	}

	// Get the physical device's features
	if (getPhysicalDeviceProperties2Supported && featuresChain.pNext)
	{
		physicalDevice.getFeatures2(&featuresChain);
	}
	else
	{
		physicalDevice.getFeatures(&features);
	}

	if (VulkanContext::Instance()->provokingVertexSupported)
	{
		VulkanContext::Instance()->provokingVertexSupported &= provokingVertexFeatures.provokingVertexLast;
	}

	VulkanContext::Instance()->samplerAnisotropy = features.samplerAnisotropy;
	VulkanContext::Instance()->fragmentStoresAndAtomics = features.fragmentStoresAndAtomics;

	// create a Device
	float queuePriority = 1.0f;
	vk::DeviceQueueCreateInfo deviceQueueCreateInfo[] = {
			vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), context->queue_family_index, 1, &queuePriority),
			vk::DeviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), context->presentation_queue_family_index, 1, &queuePriority),
	};
	bool singleQueue = context->queue_family_index == context->presentation_queue_family_index;
	vk::DeviceCreateInfo deviceCreateInfo;
	if (singleQueue)
		deviceCreateInfo = vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo[0], nullptr, enabledExtensions);
	else
		deviceCreateInfo = vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo, nullptr, enabledExtensions);

	if (getPhysicalDeviceProperties2Supported)
		deviceCreateInfo.pNext = &featuresChain;
	else
		deviceCreateInfo.setPEnabledFeatures(&features);
	vk::Device newDevice = physicalDevice.createDevice(deviceCreateInfo);

	context->device = (VkDevice)newDevice;
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
	VULKAN_HPP_DEFAULT_DISPATCHER.init(context->device);
#endif

	// Queues
	context->queue = (VkQueue)newDevice.getQueue(context->queue_family_index, 0);
	context->presentation_queue = (VkQueue)newDevice.getQueue(context->presentation_queue_family_index, 0);

	return true;
}

bool VulkanContext::init(retro_hw_render_interface_vulkan *retro_render_if)
{
	if (retro_render_if->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN
			|| retro_render_if->interface_version != RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION)
		return false;
	this->retro_render_if = retro_render_if;
	GraphicsContext::instance = this;

	instance = vk::Instance(retro_render_if->instance);
	physicalDevice = vk::PhysicalDevice(retro_render_if->gpu);
	device = vk::Device(retro_render_if->device);
	queue = vk::Queue(retro_render_if->queue);

	vk::PhysicalDeviceProperties *properties;
	static vk::PhysicalDeviceProperties props;
	physicalDevice.getProperties(&props);

	NOTICE_LOG(RENDERER, "GPU Supports Vulkan API: %u.%u.%u",
			VK_API_VERSION_MAJOR(props.apiVersion),
			VK_API_VERSION_MINOR(props.apiVersion),
			VK_API_VERSION_PATCH(props.apiVersion));
	if (VK_VERSION_MINOR(props.apiVersion) >= 1)
	{
		NOTICE_LOG(RENDERER, "GPU Supports vkGetPhysicalDeviceProperties2");
		static vk::PhysicalDeviceProperties2 properties2;
		vk::PhysicalDeviceMaintenance3Properties properties3;
		properties2.pNext = &properties3;
		physicalDevice.getProperties2(&properties2);
		properties = &properties2.properties;
		maxMemoryAllocationSize = properties3.maxMemoryAllocationSize;
	}
	else
	{
		properties = &props;
		maxMemoryAllocationSize = 0xFFFFFFFFu;
	}
	uniformBufferAlignment = properties->limits.minUniformBufferOffsetAlignment;
	storageBufferAlignment = properties->limits.minStorageBufferOffsetAlignment;
	maxStorageBufferRange = properties->limits.maxStorageBufferRange;
	maxSamplerAnisotropy =  properties->limits.maxSamplerAnisotropy;
	vendorID = properties->vendorID;

	vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(vk::Format::eR5G5B5A1UnormPack16);
	if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
		optimalTilingSupported1555 = true;
	else
		NOTICE_LOG(RENDERER, "eR5G5B5A1UnormPack16 not supported for optimal tiling");
	formatProperties = physicalDevice.getFormatProperties(vk::Format::eR5G6B5UnormPack16);
	if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
		optimalTilingSupported565 = true;
	else
		NOTICE_LOG(RENDERER, "eR5G6B5UnormPack16 not supported for optimal tiling");
	formatProperties = physicalDevice.getFormatProperties(vk::Format::eR4G4B4A4UnormPack16);
	if ((formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
			&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
		optimalTilingSupported4444 = true;
	else
		NOTICE_LOG(RENDERER, "eR4G4B4A4UnormPack16 not supported for optimal tiling");

	ShaderCompiler::Init();

	// Descriptor pool
	std::array<vk::DescriptorPoolSize, 11> pool_sizes =
	{
			vk::DescriptorPoolSize(vk::DescriptorType::eSampler, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, 40000),
			vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageImage, 12),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformTexelBuffer, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageTexelBuffer, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 80000),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 50),
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, 2),
			vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 50)
	};
	descriptorPool = device.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			40000, pool_sizes));

	std::string cachePath = hostfs::getShaderCachePath("vulkan_pipeline.cache");
	FILE *f = fopen(cachePath.c_str(), "rb");
	if (f == nullptr)
		pipelineCache = device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
	else
	{
		fseek(f, 0, SEEK_END);
		size_t cacheSize = ftell(f);
		fseek(f, 0, SEEK_SET);
		u8 *cacheData = new u8[cacheSize];
		if (fread(cacheData, 1, cacheSize, f) != cacheSize)
			cacheSize = 0;
		fclose(f);
		pipelineCache = device.createPipelineCacheUnique(vk::PipelineCacheCreateInfo(vk::PipelineCacheCreateFlags(), cacheSize, cacheData));
		delete [] cacheData;
		INFO_LOG(RENDERER, "Vulkan pipeline cache loaded from %s: %zd bytes", cachePath.c_str(), cacheSize);
	}
	allocator.Init(physicalDevice, device, instance);
	depthFormat = findDepthFormat(physicalDevice);
	if (depthFormat == vk::Format::eUndefined)
		return false;

	retro_image.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	retro_image.create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	retro_image.create_info.pNext = nullptr;
	retro_image.create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
	retro_image.create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	retro_image.create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	retro_image.create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	retro_image.create_info.subresourceRange.baseArrayLayer = 0;
	retro_image.create_info.subresourceRange.layerCount = 1;
	retro_image.create_info.subresourceRange.baseMipLevel = 0;
	retro_image.create_info.subresourceRange.levelCount = 1;
	retro_image.create_info.flags = 0;

	int chainSize = GetSwapChainSize();
	commandPool.Init(chainSize);
	// Render pass
	vk::AttachmentDescription attachmentDescription = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, nullptr, colorReference,
			nullptr, nullptr);
	renderPass = device.createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
			attachmentDescription,	subpass));

	shaderManager = std::make_unique<ShaderManager>();
	quadPipeline = std::make_unique<QuadPipeline>(true, false);
	quadPipelineWithAlpha = std::make_unique<QuadPipeline>(false, false);
	quadDrawer = std::make_unique<QuadDrawer>();
	quadPipeline->Init(shaderManager.get(), *renderPass, 0);
	quadPipelineWithAlpha->Init(shaderManager.get(), *renderPass, 0);
	quadDrawer->Init(quadPipeline.get());
	overlay = std::make_unique<VulkanOverlay>();
	overlay->Init(quadPipelineWithAlpha.get());

	return true;
}

void VulkanContext::PresentFrame(vk::Image image, vk::ImageView imageView, const vk::Extent2D& extent, float aspectRatio)
{
	if (image == vk::Image())
		return;
	float shiftX, shiftY;
	getVideoShift(shiftX, shiftY);

	beginFrame(extent, image);
	QuadVertex vtx[4] {
		{ -1, -1, 0, 0, 0 },
		{  1, -1, 0, 1, 0 },
		{ -1,  1, 0, 0, 1 },
		{  1,  1, 0, 1, 1 },
	};
	vtx[0].x = vtx[2].x = -1.f + shiftX * 2.f / extent.width;
	vtx[1].x = vtx[3].x = vtx[0].x + 2;
	vtx[0].y = vtx[1].y = -1.f + shiftY * 2.f / extent.height;
	vtx[2].y = vtx[3].y = vtx[0].y + 2;

	quadPipeline->BindPipeline(cmdBuffer);
	vk::Viewport viewport(0, 0, extent.width, extent.height);
	cmdBuffer.setViewport(0, viewport);
	cmdBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), extent));
	quadDrawer->Draw(cmdBuffer, imageView, vtx, false);
	overlay->Draw(cmdBuffer, extent, config::EmulateFramebuffer ? 1 : (int)config::RenderResolution / 480.f,
			true, true);
	endFrame(image);

	retro_image.image_view = (VkImageView)colorAttachments[GetCurrentImageIndex()]->GetImageView();
	retro_image.create_info.image = (VkImage)colorAttachments[GetCurrentImageIndex()]->GetImage();
	retro_render_if->set_image(retro_render_if->handle, &retro_image, 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
}

void VulkanContext::beginFrame(vk::Extent2D extent, vk::Image barrierImage)
{
	int currentImage = GetCurrentImageIndex();
	if (currentImage >= (int)framebuffers.size())
	{
		framebuffers.resize(currentImage + 1);
		colorAttachments.resize(currentImage + 1);
	}
	if (colorAttachments[currentImage])
	{
		vk::Extent2D caExtent = colorAttachments[currentImage]->getExtent();
		if (extent != caExtent)
		{
			addToFlight(new Deleter(std::move(colorAttachments[currentImage])));
			addToFlight(new Deleter(std::move(framebuffers[currentImage])));
		}
	}
	commandPool.BeginFrame();
	const std::array<vk::ClearValue, 2> clear_colors = { getBorderColor(), vk::ClearDepthStencilValue{ 0.f, 0 } };
	cmdBuffer = commandPool.Allocate(true);
	cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	overlay->Prepare(cmdBuffer, true, true);

	if (colorAttachments[currentImage] == nullptr)
	{
		colorAttachments[currentImage] = std::make_unique<FramebufferAttachment>(physicalDevice, device);
		colorAttachments[currentImage]->Init(extent.width, extent.height, vk::Format::eR8G8B8A8Unorm,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
				"COLOR ATTACHMENT " + std::to_string(currentImage));
		vk::ImageView imageView = colorAttachments[currentImage]->GetImageView();
		vk::FramebufferCreateInfo createInfo(vk::FramebufferCreateFlags(), *renderPass,
				imageView, extent.width, extent.height, 1);
		framebuffers[currentImage] = device.createFramebufferUnique(createInfo);
		setImageLayout(cmdBuffer, colorAttachments[currentImage]->GetImage(), vk::Format::eR8G8B8A8Unorm,
				1, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	if (GetVendorID() == VulkanContext::VENDOR_NVIDIA && barrierImage)
	{
		vk::ImageMemoryBarrier barrier(
				vk::AccessFlagBits::eColorAttachmentWrite,
		        vk::AccessFlagBits::eShaderRead,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
				barrierImage,
		        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		cmdBuffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::PipelineStageFlagBits::eFragmentShader,
				{},
				nullptr, nullptr,
				barrier
		);
	}
	cmdBuffer.beginRenderPass(vk::RenderPassBeginInfo(*renderPass, *framebuffers[currentImage], vk::Rect2D({0, 0}, extent), clear_colors),
			vk::SubpassContents::eInline);
}

void VulkanContext::endFrame(vk::Image barrierImage)
{
	cmdBuffer.endRenderPass();
	if (GetVendorID() == VulkanContext::VENDOR_NVIDIA && barrierImage)
	{
		vk::ImageMemoryBarrier barrier(
				vk::AccessFlagBits::eShaderRead,
		        vk::AccessFlagBits::eMemoryWrite | vk::AccessFlagBits::eColorAttachmentWrite,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        vk::ImageLayout::eShaderReadOnlyOptimal,
		        VK_QUEUE_FAMILY_IGNORED,
		        VK_QUEUE_FAMILY_IGNORED,
				barrierImage,
		        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		cmdBuffer.pipelineBarrier(
				vk::PipelineStageFlagBits::eFragmentShader,
				vk::PipelineStageFlagBits::eAllCommands,
				{},
				nullptr, nullptr,
				barrier
		);
	}
	cmdBuffer.end();
	cmdBuffer = nullptr;
	commandPool.EndFrame();
}

void VulkanContext::term()
{
	GraphicsContext::instance = nullptr;
	if (device)
	{
		device.waitIdle();
		if (pipelineCache)
		{
			std::vector<u8> cacheData = device.getPipelineCacheData(*pipelineCache);
			if (!cacheData.empty())
			{
				std::string cachePath = hostfs::getShaderCachePath("vulkan_pipeline.cache");
				FILE *f = fopen(cachePath.c_str(), "wb");
				if (f != nullptr)
				{
					(void)fwrite(&cacheData[0], 1, cacheData.size(), f);
					fclose(f);
				}
			}
		}
	}
	overlay.reset();
	framebuffers.clear();
	colorAttachments.clear();
	commandPool.Term();
	quadDrawer.reset();
	quadPipeline.reset();
	quadPipelineWithAlpha.reset();
	renderPass.reset();
	shaderManager.reset();
	ShaderCompiler::Term();
	descriptorPool.reset();
	allocator.Term();
	pipelineCache.reset();
}

VulkanContext::VulkanContext()
{
	verify(contextInstance == nullptr);
	contextInstance = this;
}

VulkanContext::~VulkanContext()
{
	verify(contextInstance == this);
	contextInstance = nullptr;
}
