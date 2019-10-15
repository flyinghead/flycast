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
#include "vulkan.h"
#include "imgui/imgui.h"
#include "imgui_impl_vulkan.h"
#include "../gui.h"
#include "hw/pvr/Renderer_if.h"

VulkanContext *VulkanContext::contextInstance;

static const char *PipelineCacheFileName = DATA_PATH "vulkan_pipeline.cache";

#if HOST_CPU == CPU_ARM
__attribute__((pcs("aapcs-vfp")))
#endif
#ifndef __ANDROID__
static VkBool32 debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
									 VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData, void * /*pUserData*/)
{
	std::string msg = vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) + ": "
			+ vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) + ": ";
	msg += std::string("messageIDName=") + pCallbackData->pMessageIdName + " ";
//	msg += std::string("messageIdNumber=") + pCallbackData->messageIdNumber + " ";
	msg += pCallbackData->pMessage;
	/* TODO
	if (0 < pCallbackData->queueLabelCount)
	{
		std::cerr << "\t" << "Queue Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->queueLabelCount; i++)
		{
			std::cerr << "\t\t" << "lableName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->cmdBufLabelCount)
	{
		std::cerr << "\t" << "CommandBuffer Labels:\n";
		for (uint8_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
		{
			std::cerr << "\t\t" << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->objectCount)
	{
		std::cerr << "\t" << "Objects:\n";
		for (uint8_t i = 0; i < pCallbackData->objectCount; i++)
		{
			std::cerr << "\t\t" << "Object " << i << "\n";
			std::cerr << "\t\t\t" << "objectType   = " << vk::to_string(static_cast<vk::ObjectType>(pCallbackData->pObjects[i].objectType)) << "\n";
			std::cerr << "\t\t\t" << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
			if (pCallbackData->pObjects[i].pObjectName)
			{
				std::cerr << "\t\t\t" << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
			}
		}
	}
	*/
	switch (static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity))
	{
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose:
		DEBUG_LOG(RENDERER, "%s", msg.c_str());
		break;
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo:
		INFO_LOG(RENDERER, "%s", msg.c_str());
		break;
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning:
		WARN_LOG(RENDERER, "%s", msg.c_str());
		break;
	case vk::DebugUtilsMessageSeverityFlagBitsEXT::eError:
	default:
		ERROR_LOG(RENDERER, "%s", msg.c_str());
		break;
	}
	return VK_TRUE;
}
#else
static VkBool32 debugReportCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode,
		const char* pLayerPrefix, const char* pMessage, void* /*pUserData*/)
{
	std::string msg = pMessage;
	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		ERROR_LOG(RENDERER, "%s", msg.c_str());
	else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		WARN_LOG(RENDERER, "%s", msg.c_str());
	else if (flags & (VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT))
		NOTICE_LOG(RENDERER, "%s", msg.c_str());
	else
		NOTICE_LOG(RENDERER, "(d) %s", msg.c_str());

	return VK_FALSE;
}
#endif

static void CheckImGuiResult(VkResult err)
{
	if (err != VK_SUCCESS)
		WARN_LOG(RENDERER, "ImGui Vulkan error %d", err);
}

bool VulkanContext::InitInstance(const char** extensions, uint32_t extensions_count)
{
	if (volkInitialize() != VK_SUCCESS)
	{
		ERROR_LOG(RENDERER, "Cannot load Vulkan libraries");
		return false;
	}
	try
	{
		vk::ApplicationInfo applicationInfo("Flycast", 1, "Flycast", 1, VK_API_VERSION_1_0);
		std::vector<const char *> vext;
		for (int i = 0; i < extensions_count; i++)
			vext.push_back(extensions[i]);

		std::vector<const char *> layer_names;
#ifdef VK_DEBUG
#ifndef __ANDROID__
		vext.push_back("VK_EXT_debug_utils");
		extensions_count += 1;
//		layer_names.push_back("VK_LAYER_LUNARG_api_dump");
		layer_names.push_back("VK_LAYER_LUNARG_standard_validation");
#else
		vext.push_back("VK_EXT_debug_report");	// NDK <= 19?
		extensions_count += 1;
		layer_names.push_back("VK_LAYER_GOOGLE_threading");
		layer_names.push_back("VK_LAYER_LUNARG_parameter_validation");
		layer_names.push_back("VK_LAYER_LUNARG_object_tracker");
		layer_names.push_back("VK_LAYER_LUNARG_core_validation");
		layer_names.push_back("VK_LAYER_GOOGLE_unique_objects");
#endif
#endif
		extensions = &vext[0];
		vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, layer_names.size(), &layer_names[0], extensions_count, extensions);
		// create a UniqueInstance
		instance = vk::createInstanceUnique(instanceCreateInfo);

		volkLoadInstance(static_cast<VkInstance>(*instance));

#ifdef VK_DEBUG
#ifndef __ANDROID__
		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo
				| vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral
				| vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		debugUtilsMessenger = instance->createDebugUtilsMessengerEXTUnique(vk::DebugUtilsMessengerCreateInfoEXT({}, severityFlags, messageTypeFlags, debugUtilsMessengerCallback));
#else
		vk::DebugReportCallbackCreateInfoEXT createInfo(vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eInformation
				| vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eWarning
				| vk::DebugReportFlagBitsEXT::eError, &::debugReportCallback);
		debugReportCallback = instance->createDebugReportCallbackEXTUnique(createInfo);
#endif
#endif

		physicalDevice = instance->enumeratePhysicalDevices().front();
		vk::PhysicalDeviceProperties properties;
		physicalDevice.getProperties(&properties);
		uniformBufferAlignment = properties.limits.minUniformBufferOffsetAlignment;

		vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(vk::Format::eR5G5B5A1UnormPack16);
		if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			optimalTilingSupported1555 = true;
		else
			NOTICE_LOG(RENDERER, "eR5G5B5A1UnormPack16 not supported for optimal tiling");
		formatProperties = physicalDevice.getFormatProperties(vk::Format::eR5G6B5UnormPack16);
		if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			optimalTilingSupported565 = true;
		else
			NOTICE_LOG(RENDERER, "eR5G6B5UnormPack16 not supported for optimal tiling");
		formatProperties = physicalDevice.getFormatProperties(vk::Format::eR4G4B4A4UnormPack16);
		if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			optimalTilingSupported4444 = true;
		else
			NOTICE_LOG(RENDERER, "eR4G4B4A4UnormPack16 not supported for optimal tiling");

		return true;
	}
	catch (const vk::SystemError& err)
	{
		ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
	}
	catch (...)
	{
		ERROR_LOG(RENDERER, "Unknown error");
	}
	return false;
}

vk::Format VulkanContext::InitDepthBuffer()
{
	const vk::Format depthFormats[] = { vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint };
	vk::ImageTiling tiling;
	vk::Format depthFormat = vk::Format::eUndefined;
	for (int i = 0; i < ARRAY_SIZE(depthFormats); i++)
	{
		vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(depthFormats[i]);

		if (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
		{
			tiling = vk::ImageTiling::eLinear;
			depthFormat = depthFormats[i];
			break;
		}
		else if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
		{
			tiling = vk::ImageTiling::eOptimal;
			depthFormat = depthFormats[i];
			break;
		}
	}
	if (depthFormat == vk::Format::eUndefined)
	{
		die("No supported depth/stencil format found");
	}

	vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, depthFormat, vk::Extent3D(width, height, 1), 1, 1, vk::SampleCountFlagBits::e1, tiling, vk::ImageUsageFlagBits::eDepthStencilAttachment);
	depthImage = device->createImageUnique(imageCreateInfo);

	vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
	vk::MemoryRequirements memoryRequirements = device->getImageMemoryRequirements(*depthImage);
	uint32_t typeBits = memoryRequirements.memoryTypeBits;
	uint32_t typeIndex = uint32_t(~0);
	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) && ((memoryProperties.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) == vk::MemoryPropertyFlagBits::eDeviceLocal))
		{
			typeIndex = i;
			break;
		}
		typeBits >>= 1;
	}
	verify(typeIndex != ~0);
	depthMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, typeIndex));

	device->bindImageMemory(*depthImage, *depthMemory, 0);

	vk::ComponentMapping componentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
	vk::ImageSubresourceRange subResourceRange(vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1);
	depthView = device->createImageViewUnique(vk::ImageViewCreateInfo(vk::ImageViewCreateFlags(), *depthImage, vk::ImageViewType::e2D, depthFormat, componentMapping, subResourceRange));

	return depthFormat;
}

void VulkanContext::InitImgui()
{
	gui_init();
	ImGui_ImplVulkan_InitInfo initInfo = {};
	initInfo.Instance = (VkInstance)*instance;
	initInfo.PhysicalDevice = (VkPhysicalDevice)physicalDevice;
	initInfo.Device = (VkDevice)*device;
	initInfo.QueueFamily = graphicsQueueIndex;
	initInfo.Queue = (VkQueue)graphicsQueue;
	initInfo.PipelineCache = (VkPipelineCache)*pipelineCache;
	initInfo.DescriptorPool = (VkDescriptorPool)*descriptorPool;
#ifdef VK_DEBUG
	initInfo.CheckVkResultFn = &CheckImGuiResult;
#endif

	if (!ImGui_ImplVulkan_Init(&initInfo, (VkRenderPass)*renderPass))
	{
		die("ImGui initialization failed");
	}
    // Upload Fonts
	device->resetFences(1, &(*drawFences.front()));
	device->resetCommandPool(*commandPools.front(), vk::CommandPoolResetFlagBits::eReleaseResources);
	vk::CommandBuffer& commandBuffer = *commandBuffers.front();
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	ImGui_ImplVulkan_CreateFontsTexture((VkCommandBuffer)commandBuffer);
	commandBuffer.end();
	vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &commandBuffer);
	graphicsQueue.submit(1, &submitInfo, *drawFences.front());

	device->waitIdle();
	ImGui_ImplVulkan_InvalidateFontUploadObjects();
}

bool VulkanContext::InitDevice()
{
	if (!instance)
		return false;
	try
	{
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
#ifdef VK_DEBUG
		std::for_each(queueFamilyProperties.begin(), queueFamilyProperties.end(),
				[](vk::QueueFamilyProperties const& qfp) { INFO_LOG(RENDERER, "Queue Family: count %d flags %s minImgGranularity %d x %d x %d",
						qfp.queueCount, vk::to_string(qfp.queueFlags).c_str(), qfp.minImageTransferGranularity.width, qfp.minImageTransferGranularity.height,
						qfp.minImageTransferGranularity.depth); });
#endif
		// get the first index into queueFamiliyProperties which supports graphics
		graphicsQueueIndex = (u32)std::distance(queueFamilyProperties.begin(),
				std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
						[](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; }));
		verify(graphicsQueueIndex < queueFamilyProperties.size());

		// determine a queueFamilyIndex that supports present
		// first check if the graphicsQueueFamilyIndex is good enough
		presentQueueIndex = physicalDevice.getSurfaceSupportKHR(graphicsQueueIndex, surface) ? graphicsQueueIndex : queueFamilyProperties.size();
		if (presentQueueIndex == queueFamilyProperties.size())
		{
			// the graphicsQueueFamilyIndex doesn't support present -> look for an other family index that supports both graphics and present
			for (size_t i = 0; i < queueFamilyProperties.size(); i++)
			{
				if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR((u32)i, surface))
				{
					graphicsQueueIndex = (u32)i;
					presentQueueIndex = (u32)i;
					break;
				}
			}
			if (presentQueueIndex == queueFamilyProperties.size())
			{
				// there's nothing like a single family index that supports both graphics and present -> look for an other family index that supports present
				DEBUG_LOG(RENDERER, "Using separate Graphics and Present queue families");
				for (size_t i = 0; i < queueFamilyProperties.size(); i++)
				{
					if (physicalDevice.getSurfaceSupportKHR((u32)i, surface))
					{
						presentQueueIndex = (u32)i;
						break;
					}
				}
			}
		}
		if (graphicsQueueIndex == queueFamilyProperties.size() || presentQueueIndex == queueFamilyProperties.size())
			die("Could not find a queue for graphics or present -> terminating");
		if (graphicsQueueIndex == presentQueueIndex)
			DEBUG_LOG(RENDERER, "Using Graphics+Present queue family");
		else
			DEBUG_LOG(RENDERER, "Using distinct Graphics and Present queue families");

		// create a UniqueDevice
		float queuePriority = 1.0f;
		const char *dev_extensions[] = { "VK_KHR_swapchain" };
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), graphicsQueueIndex, 1, &queuePriority);
		device = physicalDevice.createDeviceUnique(vk::DeviceCreateInfo(vk::DeviceCreateFlags(), 1, &deviceQueueCreateInfo, 0, nullptr, 1, dev_extensions));

		// This links entry points directly from the driver and isn't absolutely necessary
		volkLoadDevice(static_cast<VkDevice>(*device));

	    // Queues
	    graphicsQueue = device->getQueue(graphicsQueueIndex, 0);
	    presentQueue = device->getQueue(presentQueueIndex, 0);

	    // Descriptor pool
        vk::DescriptorPoolSize pool_sizes[] =
        {
            { vk::DescriptorType::eSampler, 2 },
            { vk::DescriptorType::eCombinedImageSampler, 4000 },
            { vk::DescriptorType::eSampledImage, 2 },
            { vk::DescriptorType::eStorageImage, 2 },
            { vk::DescriptorType::eUniformTexelBuffer, 2 },
			{ vk::DescriptorType::eStorageTexelBuffer, 2 },
            { vk::DescriptorType::eUniformBuffer, 12 },
            { vk::DescriptorType::eStorageBuffer, 2 },
            { vk::DescriptorType::eUniformBufferDynamic, 2 },
            { vk::DescriptorType::eStorageBufferDynamic, 2 },
            { vk::DescriptorType::eInputAttachment, 2 }
        };
	    descriptorPool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
	    		10000, ARRAY_SIZE(pool_sizes), pool_sizes));


	    std::string cachePath = get_writable_data_path(PipelineCacheFileName);
	    FILE *f = fopen(cachePath.c_str(), "rb");
	    if (f == nullptr)
	    	pipelineCache = device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
	    else
	    {
	    	fseek(f, 0, SEEK_END);
	    	size_t cacheSize = ftell(f);
	    	fseek(f, 0, SEEK_SET);
	    	u8 *cacheData = new u8[cacheSize];
	    	if (fread(cacheData, 1, cacheSize, f) != cacheSize)
	    		cacheSize = 0;
	    	fclose(f);
    		pipelineCache = device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo(vk::PipelineCacheCreateFlags(), cacheSize, cacheData));
    		INFO_LOG(RENDERER, "Vulkan pipeline cache loaded from %s: %zd bytes", cachePath.c_str(), cacheSize);
	    }

		CreateSwapChain();

		return true;
	}
	catch (const vk::SystemError& err)
	{
		ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
	}
	catch (...)
	{
		ERROR_LOG(RENDERER, "Unknown error");
	}
	return false;
}

void VulkanContext::CreateSwapChain()
{
	try
	{
		device->waitIdle();

		framebuffers.clear();
		drawFences.clear();
		imageAcquiredSemaphores.clear();
		renderCompleteSemaphores.clear();
		commandBuffers.clear();
		commandPools.clear();
		imageViews.clear();

		// get the supported VkFormats
		std::vector<vk::SurfaceFormatKHR> formats = physicalDevice.getSurfaceFormatsKHR(surface);
		assert(!formats.empty());
		vk::Format colorFormat = vk::Format::eUndefined;
		for (const auto& f : formats)
		{
			DEBUG_LOG(RENDERER, "Supported surface format: %s", vk::to_string(f.format).c_str());
			// Try to find an non-sRGB color format
			if (f.format == vk::Format::eB8G8R8A8Unorm || f.format == vk::Format::eR8G8B8A8Unorm)
			{
				colorFormat = f.format;
				break;
			}
		}
		if (colorFormat == vk::Format::eUndefined)
		{
			colorFormat = (formats[0].format == vk::Format::eUndefined) ? vk::Format::eB8G8R8A8Unorm : formats[0].format;
		}

		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
		DEBUG_LOG(RENDERER, "Surface capabilities: %d x %d, %s, image count: %d - %d", surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height,
				vk::to_string(surfaceCapabilities.currentTransform).c_str(), surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
		VkExtent2D swapchainExtent;
		if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
		{
			// If the surface size is undefined, the size is set to the size of the images requested.
			swapchainExtent.width = std::min(std::max(640u, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
			swapchainExtent.height = std::min(std::max(480u, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);
		}
		else
		{
			// If the surface size is defined, the swap chain size must match
			swapchainExtent = surfaceCapabilities.currentExtent;
		}
		SetWindowSize(swapchainExtent.width, swapchainExtent.height);

		// The FIFO present mode is guaranteed by the spec to be supported
		vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;
		// Use FIFO on mobile, prefer Mailbox on desktop
#if HOST_CPU != CPU_ARM && HOST_CPU != CPU_ARM64 && !defined(__ANDROID__)
		for (auto& presentMode : physicalDevice.getSurfacePresentModesKHR(surface))
		{
			if (presentMode == vk::PresentModeKHR::eMailbox)
			{
				INFO_LOG(RENDERER, "Using mailbox present mode");
				swapchainPresentMode = vk::PresentModeKHR::eMailbox;
				break;
			}
		}
#endif

		vk::SurfaceTransformFlagBitsKHR preTransform = (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) ? vk::SurfaceTransformFlagBitsKHR::eIdentity : surfaceCapabilities.currentTransform;

		vk::CompositeAlphaFlagBitsKHR compositeAlpha =
				(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied :
				(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied) ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied :
				(surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit) ? vk::CompositeAlphaFlagBitsKHR::eInherit : vk::CompositeAlphaFlagBitsKHR::eOpaque;
		u32 imageCount = std::max(3u, surfaceCapabilities.minImageCount);
		if (surfaceCapabilities.maxImageCount != 0)
			imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
		vk::SwapchainCreateInfoKHR swapChainCreateInfo(vk::SwapchainCreateFlagsKHR(), surface, imageCount, colorFormat, vk::ColorSpaceKHR::eSrgbNonlinear,
				swapchainExtent, 1, vk::ImageUsageFlagBits::eColorAttachment, vk::SharingMode::eExclusive, 0, nullptr, preTransform, compositeAlpha, swapchainPresentMode, true, nullptr);

		u32 queueFamilyIndices[2] = { graphicsQueueIndex, presentQueueIndex };
		if (graphicsQueueIndex != presentQueueIndex)
		{
			// If the graphics and present queues are from different queue families, we either have to explicitly transfer ownership of images between
			// the queues, or we have to create the swapchain with imageSharingMode as VK_SHARING_MODE_CONCURRENT
			swapChainCreateInfo.imageSharingMode = vk::SharingMode::eConcurrent;
			swapChainCreateInfo.queueFamilyIndexCount = 2;
			swapChainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
		}

		swapChain.reset();
		swapChain = device->createSwapchainKHRUnique(swapChainCreateInfo);

		std::vector<vk::Image> swapChainImages = device->getSwapchainImagesKHR(*swapChain);

		imageViews.reserve(swapChainImages.size());
		commandPools.reserve(swapChainImages.size());
		commandBuffers.reserve(swapChainImages.size());
		vk::ComponentMapping componentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
		vk::ImageSubresourceRange subResourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
		for (auto image : swapChainImages)
		{
			vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, colorFormat, componentMapping, subResourceRange);
			imageViews.push_back(device->createImageViewUnique(imageViewCreateInfo));

			// create a UniqueCommandPool to allocate a CommandBuffer from
			commandPools.push_back(device->createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient, graphicsQueueIndex)));

		    // allocate a CommandBuffer from the CommandPool
		    commandBuffers.push_back(std::move(device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*commandPools.back(), vk::CommandBufferLevel::ePrimary, 1)).front()));
		}

	    vk::Format depthFormat = InitDepthBuffer();

	    // Render pass
	    vk::AttachmentDescription attachmentDescriptions[2];
	    // FIXME we should use vk::AttachmentLoadOp::eLoad for loadOp to preserve previous framebuffer but it fails on the first render
	    attachmentDescriptions[0] = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), colorFormat, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
	    	vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
	    attachmentDescriptions[1] = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), depthFormat, vk::SampleCountFlagBits::e1, vk::AttachmentLoadOp::eClear,
	    	vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

	    vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	    vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
	    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorReference, nullptr, &depthReference);

	    renderPass = device->createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), 2, attachmentDescriptions, 1, &subpass));

	    // Framebuffers, fences, semaphores
	    vk::ImageView attachments[2];
	    attachments[1] = *depthView;

	    framebuffers.reserve(imageViews.size());
	    drawFences.reserve(imageViews.size());
	    renderCompleteSemaphores.reserve(imageViews.size());
	    imageAcquiredSemaphores.reserve(imageViews.size());
	    for (auto const& view : imageViews)
	    {
	    	attachments[0] = *view;
	    	framebuffers.push_back(device->createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass, 2, attachments, width, height, 1)));
	    	drawFences.push_back(device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
	    	renderCompleteSemaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
	    	imageAcquiredSemaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
	    }

	    InitImgui();

	    INFO_LOG(RENDERER, "Vulkan swap chain created: %d x %d, swap chain size %d", width, height, (int)imageViews.size());
	}
	catch (const vk::SystemError& err)
	{
		ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
	}
	catch (...)
	{
		ERROR_LOG(RENDERER, "Unknown error");
	}
}

bool VulkanContext::Init()
{
	std::vector<const char *> extensions;
	extensions.push_back("VK_KHR_surface");
#if defined(_WIN32)
	extensions.push_back("VK_KHR_win32_surface");
#elif defined(__MACH__)
	extensions.push_back("VK_MVK_macos_surface");
#elif defined(SUPPORT_X11)
	extensions.push_back("VK_KHR_xlib_surface");
#endif
	if (!InitInstance(&extensions[0], extensions.size()))
		return false;

	VkSurfaceKHR surface = nullptr;
#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createInfo.hinstance = GetModuleHandle(NULL);
	createInfo.hwnd = (HWND)libPvr_GetRenderTarget();
	if (vkCreateWin32SurfaceKHR(*instance, &createInfo, nullptr, &surface) != VK_SUCCESS)
	{
		ERROR_LOG(RENDERER, "Windows surface creation failed");
		return false;
	}
#endif

	SetSurface(surface);
	return InitDevice();
}

void VulkanContext::NewFrame()
{
	if (HasSurfaceDimensionChanged())
	{
		CreateSwapChain();
		rend_resize(width, height);
	}
	device->acquireNextImageKHR(*swapChain, UINT64_MAX, *imageAcquiredSemaphores[currentSemaphore], nullptr, &currentImage);
	device->waitForFences(1, &(*drawFences[currentImage]), true, UINT64_MAX);
	device->resetFences(1, &(*drawFences[currentImage]));
	device->resetCommandPool(*commandPools[currentImage], vk::CommandPoolResetFlagBits::eReleaseResources);
	vk::CommandBuffer& commandBuffer = *commandBuffers[currentImage];
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	verify(!rendering);
	rendering = true;
}

void VulkanContext::BeginRenderPass()
{
	const vk::ClearValue clear_colors[] = { vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}), vk::ClearDepthStencilValue{ 0.f, 0 } };
	vk::CommandBuffer& commandBuffer = *commandBuffers[currentImage];
	commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(*renderPass, *framebuffers[currentImage], vk::Rect2D({0, 0}, {width, height}), 2, clear_colors),
			vk::SubpassContents::eInline);
}

void VulkanContext::EndFrame()
{
	vk::CommandBuffer& commandBuffer = *commandBuffers[currentImage];
	commandBuffer.endRenderPass();
	commandBuffer.end();
	vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	vk::SubmitInfo submitInfo(1, &(*imageAcquiredSemaphores[currentSemaphore]), &wait_stage, 1, &commandBuffer, 1, &(*renderCompleteSemaphores[currentSemaphore]));
	graphicsQueue.submit(1, &submitInfo, *drawFences[currentImage]);
	verify(rendering);
	rendering = false;
	renderDone = true;
}

void VulkanContext::Present()
{
	if (renderDone)
	{
		try {
			presentQueue.presentKHR(vk::PresentInfoKHR(1, &(*renderCompleteSemaphores[currentSemaphore]), 1, &(*swapChain), &currentImage));
			currentSemaphore = (currentSemaphore + 1) % imageViews.size();
		} catch (const vk::OutOfDateKHRError& e) {
			// Sometimes happens when resizing the window
			INFO_LOG(RENDERER, "vk::OutOfDateKHRError");
		}
		renderDone = false;
	}
}

VulkanContext::~VulkanContext()
{
	ImGui_ImplVulkan_Shutdown();
	if (device)
    {
        std::vector<u8> cacheData = device->getPipelineCacheData(*pipelineCache);
        if (!cacheData.empty())
        {
            std::string cachePath = get_writable_data_path(PipelineCacheFileName);
            FILE *f = fopen(cachePath.c_str(), "wb");
            if (f != nullptr)
            {
                (void)fwrite(&cacheData[0], 1, cacheData.size(), f);
                fclose(f);
            }
        }
    }
	swapChain.reset();
	imageViews.clear();
	framebuffers.clear();
	renderPass.reset();
	descriptorPool.reset();
	depthView.reset();
	depthMemory.reset();
	depthImage.reset();
	commandBuffers.clear();
	commandPools.clear();
	imageAcquiredSemaphores.clear();
	renderCompleteSemaphores.clear();
	drawFences.clear();
	if (surface)
    	vkDestroySurfaceKHR((VkInstance)*instance, (VkSurfaceKHR)surface, nullptr);

	verify(contextInstance == this);
	contextInstance = nullptr;
}
