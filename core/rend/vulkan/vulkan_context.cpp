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
#include "imgui/imgui.h"
#include "imgui_impl_vulkan.h"
#include "../gui.h"
#ifdef USE_SDL
#include <sdl/sdl.h>
#include <SDL_vulkan.h>
#endif
#include "compiler.h"
#include "texture.h"
#include "utils.h"
#include "emulator.h"
#include "oslib/oslib.h"
#include "vulkan_driver.h"

void ReInitOSD();

VulkanContext *VulkanContext::contextInstance;

#ifdef VK_DEBUG
#ifndef __ANDROID__
VKAPI_ATTR static VkBool32 VKAPI_CALL debugUtilsMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageTypes,
									 VkDebugUtilsMessengerCallbackDataEXT const * pCallbackData, void * /*pUserData*/)
{
	std::string msg = vk::to_string(static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity)) + ": "
			+ vk::to_string(static_cast<vk::DebugUtilsMessageTypeFlagsEXT>(messageTypes)) + ": ";
	if (pCallbackData->pMessageIdName)
		msg += std::string("messageIDName=") + pCallbackData->pMessageIdName + " ";
//	msg += std::string("messageIdNumber=") + pCallbackData->messageIdNumber + " ";
	if (pCallbackData->pMessage)
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
	return VK_FALSE;
}
#else
#if HOST_CPU == CPU_ARM
__attribute__((pcs("aapcs-vfp")))
#endif
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
#endif

bool VulkanContext::InitInstance(const char** extensions, uint32_t extensions_count)
{
#ifndef TARGET_IPHONE
	if (volkInitialize() != VK_SUCCESS)
	{
		ERROR_LOG(RENDERER, "Cannot load Vulkan libraries");
		return false;
	}
#endif
	try
	{
		bool vulkan11 = false;
		if (::vkEnumerateInstanceVersion != nullptr)
		{
			u32 apiVersion;
			if (vk::enumerateInstanceVersion(&apiVersion) == vk::Result::eSuccess)
				vulkan11 = VK_VERSION_MAJOR(apiVersion) > 1
					|| (VK_VERSION_MAJOR(apiVersion) == 1 && VK_VERSION_MINOR(apiVersion) >= 1);
		}
		vk::ApplicationInfo applicationInfo("Flycast", 1, "Flycast", 1, vulkan11 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0);
		std::vector<const char *> vext;
		for (uint32_t i = 0; i < extensions_count; i++)
			vext.push_back(extensions[i]);

		std::vector<const char *> layer_names;
		//layer_names.push_back("VK_LAYER_ARM_AGA");
#ifdef VK_DEBUG
#ifndef __ANDROID__
		vext.push_back("VK_EXT_debug_utils");
		vext.push_back("VK_EXT_debug_report");
		layer_names.push_back("VK_LAYER_KHRONOS_validation");
#else
		vext.push_back("VK_EXT_debug_report");	// NDK <= 19?
		layer_names.push_back("VK_LAYER_GOOGLE_threading");
		layer_names.push_back("VK_LAYER_LUNARG_parameter_validation");
		layer_names.push_back("VK_LAYER_LUNARG_core_validation");
		layer_names.push_back("VK_LAYER_GOOGLE_unique_objects");
#endif
#endif
		vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, layer_names.size(), layer_names.data(),
				vext.size(), vext.data());
		// create a UniqueInstance
		instance = vk::createInstanceUnique(instanceCreateInfo);

#ifndef TARGET_IPHONE
		volkLoadInstance(static_cast<VkInstance>(*instance));
#endif

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

		// Choose a discrete gpu if there's one, otherwise just pick the first one
		physicalDevice = nullptr;
		const auto devices = instance->enumeratePhysicalDevices();
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
			physicalDevice = instance->enumeratePhysicalDevices().front();

		const vk::PhysicalDeviceProperties *properties;
		if (vulkan11 && ::vkGetPhysicalDeviceProperties2 != nullptr)
		{
			static vk::PhysicalDeviceProperties2 properties2;
			vk::PhysicalDeviceMaintenance3Properties properties3;
			properties2.pNext = &properties3;
			physicalDevice.getProperties2(&properties2);
			properties = &properties2.properties;
			maxMemoryAllocationSize = properties3.maxMemoryAllocationSize;
			if (maxMemoryAllocationSize == 0)
				// Happens on Windows 7 with NVidia 376.33, ok on 441.66
				maxMemoryAllocationSize = 0xFFFFFFFFu;
		}
		else
		{
			static vk::PhysicalDeviceProperties phyProperties;
			physicalDevice.getProperties(&phyProperties);
			properties = &phyProperties;
		}
		uniformBufferAlignment = properties->limits.minUniformBufferOffsetAlignment;
		storageBufferAlignment = properties->limits.minStorageBufferOffsetAlignment;
		maxStorageBufferRange = properties->limits.maxStorageBufferRange;
		maxSamplerAnisotropy =  properties->limits.maxSamplerAnisotropy;
		unifiedMemory = properties->deviceType == vk::PhysicalDeviceType::eIntegratedGpu;
		vendorID = properties->vendorID;
		NOTICE_LOG(RENDERER, "Vulkan API %s. Device %s", vulkan11 ? "1.1" : "1.0", properties->deviceName);

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
		vk::PhysicalDeviceFeatures features;
		physicalDevice.getFeatures(&features);
		fragmentStoresAndAtomics = features.fragmentStoresAndAtomics;
		samplerAnisotropy = features.samplerAnisotropy;
		if (!fragmentStoresAndAtomics)
			NOTICE_LOG(RENDERER, "Fragment stores & atomic not supported: no per-pixel sorting");

		ShaderCompiler::Init();

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

void VulkanContext::InitImgui()
{
	imguiDriver = std::unique_ptr<ImGuiDriver>(new VulkanDriver());
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

	if (!ImGui_ImplVulkan_Init(&initInfo, (VkRenderPass)*renderPass, 0))
	{
		die("ImGui initialization failed");
	}
	if (ImGui::GetIO().Fonts->TexID == 0)
	{
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
		presentQueueIndex = physicalDevice.getSurfaceSupportKHR(graphicsQueueIndex, GetSurface()) ? graphicsQueueIndex : queueFamilyProperties.size();
		if (presentQueueIndex == queueFamilyProperties.size())
		{
			// the graphicsQueueFamilyIndex doesn't support present -> look for an other family index that supports both graphics and present
			for (size_t i = 0; i < queueFamilyProperties.size(); i++)
			{
				if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && physicalDevice.getSurfaceSupportKHR((u32)i, GetSurface()))
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
					if (physicalDevice.getSurfaceSupportKHR((u32)i, GetSurface()))
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

		// Enable VK_KHR_dedicated_allocation if available
		bool getMemReq2Supported = false;
		dedicatedAllocationSupported = false;
		std::vector<const char *> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		for (const auto& property : physicalDevice.enumerateDeviceExtensionProperties())
		{
			if (!strcmp(property.extensionName, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME))
			{
				deviceExtensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
				getMemReq2Supported = true;
			}
			else if (!strcmp(property.extensionName, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME))
			{
				deviceExtensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
				dedicatedAllocationSupported = true;
			}
			else if (!strcmp(property.extensionName, "VK_KHR_portability_subset"))
				deviceExtensions.push_back("VK_KHR_portability_subset");
#ifdef VK_DEBUG
			else if (!strcmp(property.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
			{
				NOTICE_LOG(RENDERER, "Debug extension %s available", property.extensionName);
				deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			}
			else if(!strcmp(property.extensionName, VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
			{
				NOTICE_LOG(RENDERER, "Debug extension %s available", property.extensionName);
				deviceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
			}
			else if (!strcmp(property.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
			{
				NOTICE_LOG(RENDERER, "Debug extension %s available", property.extensionName);
				deviceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			}
#endif
		}
		dedicatedAllocationSupported &= getMemReq2Supported;

		// create a UniqueDevice
		float queuePriority = 1.0f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), graphicsQueueIndex, 1, &queuePriority);
		vk::PhysicalDeviceFeatures features;
		if (fragmentStoresAndAtomics)
			features.fragmentStoresAndAtomics = true;
		if (samplerAnisotropy)
			features.samplerAnisotropy = true;
		const char *layers[] = { "VK_LAYER_ARM_AGA" };
		device = physicalDevice.createDeviceUnique(vk::DeviceCreateInfo(vk::DeviceCreateFlags(), 1, &deviceQueueCreateInfo,
				0, layers, deviceExtensions.size(), &deviceExtensions[0], &features));

#ifndef TARGET_IPHONE
		// This links entry points directly from the driver and isn't absolutely necessary
		volkLoadDevice(static_cast<VkDevice>(*device));
#endif

	    // Queues
	    graphicsQueue = device->getQueue(graphicsQueueIndex, 0);
	    presentQueue = device->getQueue(presentQueueIndex, 0);

	    // Descriptor pool
        vk::DescriptorPoolSize pool_sizes[] =
        {
            { vk::DescriptorType::eSampler, 2 },
            { vk::DescriptorType::eCombinedImageSampler, 15000 },
            { vk::DescriptorType::eSampledImage, 2 },
            { vk::DescriptorType::eStorageImage, 12 },
            { vk::DescriptorType::eUniformTexelBuffer, 2 },
			{ vk::DescriptorType::eStorageTexelBuffer, 2 },
            { vk::DescriptorType::eUniformBuffer, 36 },
            { vk::DescriptorType::eStorageBuffer, 36 },
            { vk::DescriptorType::eUniformBufferDynamic, 2 },
            { vk::DescriptorType::eStorageBufferDynamic, 2 },
            { vk::DescriptorType::eInputAttachment, 36 }
        };
	    descriptorPool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
	    		10000, ARRAY_SIZE(pool_sizes), pool_sizes));


	    std::string cachePath = hostfs::getVulkanCachePath();
	    FILE *f = nowide::fopen(cachePath.c_str(), "rb");
	    if (f == nullptr)
	    	pipelineCache = device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
	    else
	    {
	    	std::fseek(f, 0, SEEK_END);
	    	size_t cacheSize = std::ftell(f);
	    	std::fseek(f, 0, SEEK_SET);
	    	u8 *cacheData = new u8[cacheSize];
	    	if (std::fread(cacheData, 1, cacheSize, f) != cacheSize)
	    		cacheSize = 0;
	    	std::fclose(f);
    		pipelineCache = device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo(vk::PipelineCacheCreateFlags(), cacheSize, cacheData));
    		delete [] cacheData;
    		INFO_LOG(RENDERER, "Vulkan pipeline cache loaded from %s: %zd bytes", cachePath.c_str(), cacheSize);
	    }
	    allocator.Init(physicalDevice, *device, *instance);

	    shaderManager = std::unique_ptr<ShaderManager>(new ShaderManager());
	    quadPipeline = std::unique_ptr<QuadPipeline>(new QuadPipeline(true, false));
	    quadPipelineWithAlpha = std::unique_ptr<QuadPipeline>(new QuadPipeline(false, false));
	    quadDrawer = std::unique_ptr<QuadDrawer>(new QuadDrawer());
	    quadRotatePipeline = std::unique_ptr<QuadPipeline>(new QuadPipeline(true, true));
	    quadRotateDrawer = std::unique_ptr<QuadDrawer>(new QuadDrawer());

		CreateSwapChain();

		return true;
	}
	catch (const vk::SystemError& err)
	{
		ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
	}
	catch (const InvalidVulkanContext& err)
	{
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

		overlay->Term();
		framebuffers.clear();
		drawFences.clear();
		imageAcquiredSemaphores.clear();
		renderCompleteSemaphores.clear();
		commandBuffers.clear();
		commandPools.clear();
		for (auto& img : imageViews)
			img.reset();

		// get the supported VkFormats
		std::vector<vk::SurfaceFormatKHR> formats = physicalDevice.getSurfaceFormatsKHR(GetSurface());
		assert(!formats.empty());
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

		int tries = 0;
		do {
			vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(GetSurface());
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
			resized = false;
			if (!IsValid())
				throw InvalidVulkanContext();

			// The FIFO present mode is guaranteed by the spec to be supported
			vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;
			// Use FIFO on mobile, prefer Mailbox on desktop
			for (auto& presentMode : physicalDevice.getSurfacePresentModesKHR(GetSurface()))
			{
#if HOST_CPU != CPU_ARM && HOST_CPU != CPU_ARM64 && !defined(__ANDROID__)
				if (swapOnVSync && presentMode == vk::PresentModeKHR::eMailbox
						&& vendorID != VENDOR_ATI && vendorID != VENDOR_AMD)
				{
					INFO_LOG(RENDERER, "Using mailbox present mode");
					swapchainPresentMode = vk::PresentModeKHR::eMailbox;
					break;
				}
#endif
				if (!swapOnVSync && presentMode == vk::PresentModeKHR::eImmediate)
				{
					INFO_LOG(RENDERER, "Using immediate present mode");
					swapchainPresentMode = vk::PresentModeKHR::eImmediate;
					break;
				}
			}
			if (swapOnVSync && config::DupeFrames && settings.display.refreshRate > 60.f)
				swapInterval = settings.display.refreshRate / 60.f;
			else
				swapInterval = 1;

			vk::SurfaceTransformFlagBitsKHR preTransform = (surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity) ? vk::SurfaceTransformFlagBitsKHR::eIdentity : surfaceCapabilities.currentTransform;

			u32 imageCount = std::max(3u * swapInterval, surfaceCapabilities.minImageCount);
			if (surfaceCapabilities.maxImageCount != 0)
				imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment;
#ifdef TEST_AUTOMATION
			// for final screenshot
			usage |= vk::ImageUsageFlagBits::eTransferSrc;
#endif
			vk::SwapchainCreateInfoKHR swapChainCreateInfo(vk::SwapchainCreateFlagsKHR(), GetSurface(), imageCount, colorFormat, vk::ColorSpaceKHR::eSrgbNonlinear,
					swapchainExtent, 1, usage, vk::SharingMode::eExclusive, 0, nullptr, preTransform, vk::CompositeAlphaFlagBitsKHR::eOpaque, swapchainPresentMode, true, nullptr);

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
			try {
				swapChain = device->createSwapchainKHRUnique(swapChainCreateInfo);
			}
			catch (const vk::SystemError& err)
			{
				DEBUG_LOG(RENDERER, "createSwapchainKHRUnique failed: %s", err.what());
				if (++tries > 10)
					throw InvalidVulkanContext();
			}
		}
		while (!swapChain);

		std::vector<vk::Image> swapChainImages = device->getSwapchainImagesKHR(*swapChain);

		imageViews.resize(swapChainImages.size());
		commandPools.reserve(swapChainImages.size());
		commandBuffers.reserve(swapChainImages.size());
		vk::ComponentMapping componentMapping(vk::ComponentSwizzle::eR, vk::ComponentSwizzle::eG, vk::ComponentSwizzle::eB, vk::ComponentSwizzle::eA);
		vk::ImageSubresourceRange subResourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1);
		u32 imageIdx = 0;
		for (auto image : swapChainImages)
		{
			vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, colorFormat, componentMapping, subResourceRange);
			imageViews[imageIdx++] = device->createImageViewUnique(imageViewCreateInfo);

			// create a UniqueCommandPool to allocate a CommandBuffer from
			commandPools.push_back(device->createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient, graphicsQueueIndex)));

		    // allocate a CommandBuffer from the CommandPool
		    commandBuffers.push_back(std::move(device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*commandPools.back(), vk::CommandBufferLevel::ePrimary, 1)).front()));
		}

	    depthFormat = findDepthFormat(physicalDevice);

	    // Render pass
	    vk::AttachmentDescription attachmentDescription = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), colorFormat, vk::SampleCountFlagBits::e1,
	    		vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

	    vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorReference,
	    		nullptr, nullptr);

	    renderPass = device->createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
	    		1, &attachmentDescription,	1, &subpass));

	    // Framebuffers, fences, semaphores

	    framebuffers.reserve(imageViews.size());
	    drawFences.reserve(imageViews.size());
	    renderCompleteSemaphores.reserve(imageViews.size());
	    imageAcquiredSemaphores.reserve(imageViews.size());
	    for (auto const& view : imageViews)
	    {
	    	framebuffers.push_back(device->createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
	    			1, &view.get(), width, height, 1)));
	    	drawFences.push_back(device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
	    	renderCompleteSemaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
	    	imageAcquiredSemaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
	    }
	    quadPipeline->Init(shaderManager.get(), *renderPass);
	    quadPipelineWithAlpha->Init(shaderManager.get(), *renderPass);
	    quadDrawer->Init(quadPipeline.get());
	    quadRotatePipeline->Init(shaderManager.get(), *renderPass);
	    quadRotateDrawer->Init(quadRotatePipeline.get());
	    overlay->Init(quadPipelineWithAlpha.get());

	    InitImgui();

	    currentImage = GetSwapChainSize() - 1;
	    ReInitOSD();

	    INFO_LOG(RENDERER, "Vulkan swap chain created: %d x %d, swap chain size %d", width, height, (int)imageViews.size());
	}
	catch (const vk::SystemError& err)
	{
		ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
		SetWindowSize(0, 0);
		throw InvalidVulkanContext();
	}
}

bool VulkanContext::init()
{
	GraphicsContext::instance = this;

	std::vector<const char *> extensions;
	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(USE_SDL)
	if (!sdl_recreate_window(SDL_WINDOW_VULKAN))
		return false;
    uint32_t extensionsCount = 0;
    SDL_Vulkan_GetInstanceExtensions((SDL_Window *)window, &extensionsCount, NULL);
    extensions.resize(extensionsCount + extensions.size());
    SDL_Vulkan_GetInstanceExtensions((SDL_Window *)window, &extensionsCount, &extensions[extensions.size() - extensionsCount]);
#elif defined(_WIN32)
    extern void CreateMainWindow();
    CreateMainWindow();
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	extensions.push_back(VK_MVK_IOS_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(SUPPORT_X11)
	extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
	extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif
	if (!InitInstance(&extensions[0], extensions.size()))
		return false;

#if defined(USE_SDL)
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface((SDL_Window *)window, (VkInstance)*instance, &surface) == 0)
    	return false;
    this->surface.reset(vk::SurfaceKHR(surface));
    SDL_Window *sdlWin = (SDL_Window *)window;
    int w, h;
    SDL_GetWindowSize(sdlWin, &w, &h);
    SDL_Vulkan_GetDrawableSize(sdlWin, &settings.display.width, &settings.display.height);
    settings.display.pointScale = (float)settings.display.width / w;
	float hdpi, vdpi;
	if (!SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(sdlWin), nullptr, &hdpi, &vdpi))
		screen_dpi = (int)roundf(std::max(hdpi, vdpi));
#elif defined(_WIN32)
	vk::Win32SurfaceCreateInfoKHR createInfo(vk::Win32SurfaceCreateFlagsKHR(), GetModuleHandle(NULL), (HWND)window);
	surface = instance->createWin32SurfaceKHRUnique(createInfo);
#elif defined(SUPPORT_X11)
	vk::XlibSurfaceCreateInfoKHR createInfo(vk::XlibSurfaceCreateFlagsKHR(), (Display*)display, (Window)window);
	surface = instance->createXlibSurfaceKHRUnique(createInfo);
#elif defined(__ANDROID__)
	vk::AndroidSurfaceCreateInfoKHR createInfo(vk::AndroidSurfaceCreateFlagsKHR(), (struct ANativeWindow*)window);
	surface = instance->createAndroidSurfaceKHRUnique(createInfo);
#elif defined(VK_USE_PLATFORM_IOS_MVK)
	vk::IOSSurfaceCreateInfoMVK createInfo(vk::IOSSurfaceCreateFlagsMVK(), window);
	surface = instance->createIOSSurfaceMVKUnique(createInfo);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	vk::MetalSurfaceCreateInfoEXT createInfo(vk::MetalSurfaceCreateFlagsEXT(), window);
	surface = instance->createMetalSurfaceEXTUnique(createInfo);
#else
#error "Unknown Vulkan platform"
#endif
	overlay = std::unique_ptr<VulkanOverlay>(new VulkanOverlay());

	return InitDevice();
}

void VulkanContext::NewFrame()
{
	if (resized || HasSurfaceDimensionChanged())
	{
		CreateSwapChain();
		lastFrameView = vk::ImageView();
	}
	if (!IsValid())
		throw InvalidVulkanContext();
	device->acquireNextImageKHR(*swapChain, UINT64_MAX, *imageAcquiredSemaphores[currentSemaphore], nullptr, &currentImage);
	device->waitForFences(1, &(*drawFences[currentImage]), true, UINT64_MAX);
	device->resetFences(1, &(*drawFences[currentImage]));
	device->resetCommandPool(*commandPools[currentImage], vk::CommandPoolResetFlagBits::eReleaseResources);
	vk::CommandBuffer commandBuffer = *commandBuffers[currentImage];
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	verify(!rendering);
	rendering = true;
}

void VulkanContext::BeginRenderPass()
{
	if (!IsValid())
		return;
	const vk::ClearValue clear_colors[] = { getBorderColor(), vk::ClearDepthStencilValue{ 0.f, 0 } };
	vk::CommandBuffer commandBuffer = *commandBuffers[currentImage];
	commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(*renderPass, *framebuffers[currentImage], vk::Rect2D({0, 0}, {width, height}), 2, clear_colors),
			vk::SubpassContents::eInline);
}

void VulkanContext::EndFrame(vk::CommandBuffer overlayCmdBuffer)
{
	if (!IsValid())
		return;
	vk::CommandBuffer commandBuffer = *commandBuffers[currentImage];
	commandBuffer.endRenderPass();
	commandBuffer.end();
	vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	std::vector<vk::CommandBuffer> allCmdBuffers;
	if (overlayCmdBuffer)
		allCmdBuffers.push_back(overlayCmdBuffer);
	allCmdBuffers.push_back(commandBuffer);
	vk::SubmitInfo submitInfo(1, &(*imageAcquiredSemaphores[currentSemaphore]), &wait_stage, allCmdBuffers.size(),allCmdBuffers.data(),
			1, &(*renderCompleteSemaphores[currentSemaphore]));
	graphicsQueue.submit(1, &submitInfo, *drawFences[currentImage]);
	verify(rendering);
	rendering = false;
	renderDone = true;
}

void VulkanContext::Present() noexcept
{
	if (renderDone)
	{
		try {
			DoSwapAutomation();
			presentQueue.presentKHR(vk::PresentInfoKHR(1, &(*renderCompleteSemaphores[currentSemaphore]), 1, &(*swapChain), &currentImage));
			currentSemaphore = (currentSemaphore + 1) % imageViews.size();

			if (lastFrameView && IsValid() && !gui_is_open())
				for (int i = 1; i < swapInterval; i++)
				{
					PresentFrame(vk::Image(), lastFrameView, lastFrameExtent);
					presentQueue.presentKHR(vk::PresentInfoKHR(1, &(*renderCompleteSemaphores[currentSemaphore]), 1, &(*swapChain), &currentImage));
					currentSemaphore = (currentSemaphore + 1) % imageViews.size();
				}
		} catch (const vk::SystemError& e) {
			// Happens when resizing the window
			INFO_LOG(RENDERER, "vk::SystemError %s", e.what());
			resized = true;
		}
		renderDone = false;
	}
	if (swapOnVSync == (settings.input.fastForwardMode || !config::VSync))
	{
		swapOnVSync = (!settings.input.fastForwardMode && config::VSync);
		resized = true;
	}
	if (resized)
		try {
			CreateSwapChain();
			lastFrameView = vk::ImageView();
		} catch (const InvalidVulkanContext& err) {
		}
}

void VulkanContext::DrawFrame(vk::ImageView imageView, const vk::Extent2D& extent)
{
	QuadVertex vtx[] = {
		{ { -1, -1, 0 }, { 0, 0 } },
		{ {  1, -1, 0 }, { 1, 0 } },
		{ { -1,  1, 0 }, { 0, 1 } },
		{ {  1,  1, 0 }, { 1, 1 } },
	};

	vk::CommandBuffer commandBuffer = GetCurrentCommandBuffer();
	if (config::Rotate90)
		quadRotatePipeline->BindPipeline(commandBuffer);
	else
		quadPipeline->BindPipeline(commandBuffer);

	float marginWidth;
	if (config::Rotate90)
		marginWidth = ((float)width - (float)extent.height / extent.width * height) / 2.f;
	else
		marginWidth = ((float)width - (float)extent.width / extent.height * height) / 2.f;
	vk::Viewport viewport(marginWidth, 0, width - marginWidth * 2.f, height);
	commandBuffer.setViewport(0, 1, &viewport);
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(std::max(0.f, marginWidth), 0), vk::Extent2D(width - marginWidth * 2.f, height)));
	if (config::Rotate90)
		quadRotateDrawer->Draw(commandBuffer, imageView, vtx);
	else
		quadDrawer->Draw(commandBuffer, imageView, vtx);
}

void VulkanContext::WaitIdle() const
{
	try {
		graphicsQueue.waitIdle();
	} catch (const vk::Error &err) {
		WARN_LOG(RENDERER, "WaitIdle: %s", err.what());
	}
}

std::string VulkanContext::getDriverName()
{
	vk::PhysicalDeviceProperties props;
	physicalDevice.getProperties(&props);
	return std::string(props.deviceName);
}

std::string VulkanContext::getDriverVersion()
{
	vk::PhysicalDeviceProperties props;
	physicalDevice.getProperties(&props);
	return std::to_string(VK_VERSION_MAJOR(props.driverVersion)) + "."
			+ std::to_string(VK_VERSION_MINOR(props.driverVersion)) + "."
			+ std::to_string(VK_VERSION_PATCH(props.driverVersion));
}

vk::CommandBuffer VulkanContext::PrepareOverlay(bool vmu, bool crosshair)
{
	return overlay->Prepare(*commandPools[GetCurrentImageIndex()], vmu, crosshair);
}

 void VulkanContext::DrawOverlay(float scaling, bool vmu, bool crosshair)
{
	 if (IsValid())
		 overlay->Draw(GetCurrentCommandBuffer(), vk::Extent2D(width, height), scaling, vmu, crosshair);
}

extern Renderer *renderer;

void VulkanContext::PresentFrame(vk::Image image, vk::ImageView imageView, const vk::Extent2D& extent) noexcept
{
	lastFrameView = imageView;
	lastFrameExtent = extent;

	if (imageView && IsValid())
	{
		try {
			NewFrame();
			auto overlayCmdBuffer = PrepareOverlay(config::FloatVMUs, true);

			BeginRenderPass();

			if (lastFrameView) // Might have been nullified if swap chain recreated
				DrawFrame(imageView, extent);

			DrawOverlay(gui_get_scaling(), config::FloatVMUs, true);
			renderer->DrawOSD(false);
			EndFrame(overlayCmdBuffer);
		} catch (const InvalidVulkanContext& err) {
		}
	}
}

void VulkanContext::PresentLastFrame()
{
	if (lastFrameView && IsValid())
		DrawFrame(lastFrameView, lastFrameExtent);
}

void VulkanContext::term()
{
	GraphicsContext::instance = nullptr;
	lastFrameView = nullptr;
	if (!device)
		return;
	WaitIdle();
	imguiDriver.reset();
	ImGui_ImplVulkan_Shutdown();
	gui_term();
	if (device && pipelineCache)
    {
        std::vector<u8> cacheData = device->getPipelineCacheData(*pipelineCache);
        if (!cacheData.empty())
        {
            std::string cachePath = hostfs::getVulkanCachePath();
            FILE *f = nowide::fopen(cachePath.c_str(), "wb");
            if (f != nullptr)
            {
                (void)std::fwrite(&cacheData[0], 1, cacheData.size(), f);
                std::fclose(f);
            }
        }
    }
	overlay.reset();
	ShaderCompiler::Term();
	swapChain.reset();
	imageViews.clear();
	framebuffers.clear();
	renderPass.reset();
	quadDrawer.reset();
	quadPipeline.reset();
	quadPipelineWithAlpha.reset();
	quadRotateDrawer.reset();
	quadRotatePipeline.reset();
	shaderManager.reset();
	descriptorPool.reset();
	commandBuffers.clear();
	commandPools.clear();
	imageAcquiredSemaphores.clear();
	renderCompleteSemaphores.clear();
	drawFences.clear();
	allocator.Term();
#ifndef USE_SDL
	surface.reset();
#else
	::vkDestroySurfaceKHR((VkInstance)*instance, (VkSurfaceKHR)surface.release(), nullptr);
#endif
	pipelineCache.reset();
	device.reset();
#ifdef VK_DEBUG
#ifndef __ANDROID__
	debugUtilsMessenger.reset();
#else
	debugReportCallback.reset();
#endif
#endif
	instance.reset();
}

void VulkanContext::DoSwapAutomation()
{
#ifdef TEST_AUTOMATION
	extern bool do_screenshot;

	if (do_screenshot)
	{
		bool supportsBlit = true;
		vk::FormatProperties properties;
		physicalDevice.getFormatProperties(colorFormat, &properties);
		if (!(properties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc))
			supportsBlit = false;
		physicalDevice.getFormatProperties(vk::Format::eR8G8B8A8Unorm, &properties);
		if (!(properties.linearTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst))
			supportsBlit = false;

		{
			vk::Image srcImage = device->getSwapchainImagesKHR(*swapChain)[currentImage];

			vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, vk::Format::eR8G8B8A8Unorm,
					vk::Extent3D(width, height, 1), 1, 1,
					vk::SampleCountFlagBits::e1, vk::ImageTiling::eLinear, vk::ImageUsageFlagBits::eTransferDst,
					vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined);
			vk::UniqueImage dstImage = device->createImageUnique(imageCreateInfo);

			vk::MemoryRequirements memReq = device->getImageMemoryRequirements(*dstImage);
			u32 memoryType = findMemoryType(physicalDevice.getMemoryProperties(), memReq.memoryTypeBits,
					vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible);
			vk::UniqueDeviceMemory deviceMemory = device->allocateMemoryUnique(vk::MemoryAllocateInfo(memReq.size, memoryType));
			device->bindImageMemory(dstImage.get(), *deviceMemory, 0);

			vk::UniqueCommandBuffer cmdBuffer = std::move(device->allocateCommandBuffersUnique(
					vk::CommandBufferAllocateInfo(*commandPools.back(), vk::CommandBufferLevel::ePrimary, 1)).front());
			cmdBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

			// Transition destination image to transfer destination layout
			vk::ImageMemoryBarrier barrier(vk::AccessFlags(), vk::AccessFlagBits::eTransferWrite,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal,
					VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
					*dstImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			cmdBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
					vk::DependencyFlags(), nullptr, nullptr, barrier);
			// Transition swapchain image from present to transfer source layout
			barrier = vk::ImageMemoryBarrier(vk::AccessFlagBits::eMemoryRead, vk::AccessFlagBits::eTransferRead,
								vk::ImageLayout::ePresentSrcKHR, vk::ImageLayout::eTransferSrcOptimal,
								VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
								srcImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			cmdBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
								vk::DependencyFlags(), nullptr, nullptr, barrier);

			if (supportsBlit)
			{
				vk::Offset3D blitSize(width, height, 1);
				vk::ImageBlit imageBlit(
						vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), { vk::Offset3D(), blitSize },
						vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), { vk::Offset3D(), blitSize });
				cmdBuffer->blitImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, *dstImage, vk::ImageLayout::eTransferDstOptimal,
						imageBlit, vk::Filter::eNearest);
			}
			else
			{
				vk::ImageCopy imageCopy(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(),
						vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(), { width, height, 1 });
				cmdBuffer->copyImage(srcImage, vk::ImageLayout::eTransferSrcOptimal, *dstImage, vk::ImageLayout::eTransferDstOptimal,
										imageCopy);
			}
			// Transition destination image to general layout, which is the required layout for mapping the image memory later on
			barrier = vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eMemoryRead,
											vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eGeneral,
											VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
											*dstImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			cmdBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
					vk::DependencyFlags(), nullptr, nullptr, barrier);
			// Transition back the swap chain image after the blit is done
			barrier = vk::ImageMemoryBarrier(vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eMemoryRead,
											vk::ImageLayout::eTransferSrcOptimal, vk::ImageLayout::ePresentSrcKHR,
											VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
											srcImage, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
			cmdBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer,
					vk::DependencyFlags(), nullptr, nullptr, barrier);
			cmdBuffer->end();
			vk::PipelineStageFlags wait_stage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
			vk::SubmitInfo submitInfo(0, nullptr, nullptr, 1, &cmdBuffer.get(), 0, nullptr);
			graphicsQueue.submit(1, &submitInfo, nullptr);
			graphicsQueue.waitIdle();

			vk::ImageSubresource subresource(vk::ImageAspectFlagBits::eColor, 0, 0);
			vk::SubresourceLayout subresourceLayout;
			device->getImageSubresourceLayout(*dstImage, &subresource, &subresourceLayout);

			u8* img = (u8*)device->mapMemory(*deviceMemory, 0, VK_WHOLE_SIZE);
			img += subresourceLayout.offset;

			u8 *end = img + settings.display.width * settings.display.height * 4;
			if (!supportsBlit && colorFormat == vk::Format::eB8G8R8A8Unorm)
			{
				for (u8 *p = img; p < end; p += 4)
				{
					u8 b = p[0];
					p[0] = p[2];
					p[2] = b;
					p[3] = 0xff;
				}
			}
			else
			{
				for (u8 *p = img; p < end; p += 4)
					p[3] = 0xff;
			}
			dump_screenshot(img, settings.display.width, settings.display.height, true, subresourceLayout.rowPitch, false);

			device->unmapMemory(*deviceMemory);
		}
		dc_exit();
		Term();
		exit(0);
	}
#endif
}

bool VulkanContext::HasSurfaceDimensionChanged() const
{
	vk::SurfaceCapabilitiesKHR surfaceCapabilities =
			physicalDevice.getSurfaceCapabilitiesKHR(GetSurface());
	VkExtent2D swapchainExtent;
	if (surfaceCapabilities.currentExtent.width == std::numeric_limits < uint32_t > ::max())
	{
		// If the surface size is undefined, the size is set to the size of the images requested.
		swapchainExtent.width = std::min(
				std::max(width, surfaceCapabilities.minImageExtent.width),
				surfaceCapabilities.maxImageExtent.width);
		swapchainExtent.height = std::min(
				std::max(height, surfaceCapabilities.minImageExtent.height),
				surfaceCapabilities.maxImageExtent.height);
	}
	else
	{
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfaceCapabilities.currentExtent;
	}
	return width != swapchainExtent.width || height != swapchainExtent.height;
}

void VulkanContext::SetWindowSize(u32 width, u32 height)
{
	if (width != this->width || height != this->height)
	{
		this->width = width;
		this->height = height;
		// When the window is minimized, it can happen that the max surface dimension is 0,0
		// In this case, the context becomes invalid but we keep the previous
		// dimensions to not confuse the renderer and imgui
		if (width != 0)
			settings.display.width = width;

		if (height != 0)
			settings.display.height = height;

		resize();
	}
}

void ImGui_ImplVulkan_RenderDrawData(ImDrawData *draw_data)
{
	VulkanContext *context = VulkanContext::Instance();
	if (!context->IsValid())
		return;
	try {
		bool rendering = context->IsRendering();
		vk::CommandBuffer vmuCmdBuffer;
		if (!rendering)
		{
			context->NewFrame();
			vmuCmdBuffer = context->PrepareOverlay(true, false);
			context->BeginRenderPass();
			context->PresentLastFrame();
			context->DrawOverlay(gui_get_scaling(), true, false);
		}
		// Record Imgui Draw Data and draw funcs into command buffer
		ImGui_ImplVulkan_RenderDrawData(draw_data, (VkCommandBuffer)context->GetCurrentCommandBuffer());
		if (!rendering)
			context->EndFrame(vmuCmdBuffer);
	} catch (const InvalidVulkanContext& err) {
	}
}
