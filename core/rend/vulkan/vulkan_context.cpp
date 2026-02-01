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
#include "vulkan_renderer.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "ui/gui.h"
#ifdef USE_SDL
#include <sdl/sdl.h>
#include <SDL_vulkan.h>
#endif
#include "compiler.h"
#include "utils.h"
#include "emulator.h"
#include "oslib/oslib.h"
#include "vulkan_driver.h"
#include "rend/transform_matrix.h"
#if defined(__ANDROID__) && HOST_CPU == CPU_ARM64
#include "adreno.h"
#endif

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#include <memory>
#include <set>
#include <vulkan/vulkan_format_traits.hpp>

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
	try
	{
#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
		PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = nullptr;
#if defined(__ANDROID__) && HOST_CPU == CPU_ARM64
		vkGetInstanceProcAddr = loadVulkanDriver();
#else
		static vk::DynamicLoader dl;
		vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
#endif
		if (vkGetInstanceProcAddr == nullptr) {
			ERROR_LOG(RENDERER, "Vulkan entry point vkGetInstanceProcAddr not found");
			return false;
		}
		VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
#endif
		bool vulkan11 = false;
		if (VULKAN_HPP_DEFAULT_DISPATCHER.vkEnumerateInstanceVersion != nullptr)
		{
			const u32 apiVersion = vk::enumerateInstanceVersion();

			vulkan11 = (apiVersion >= VK_API_VERSION_1_1);
		}

		vk::ApplicationInfo applicationInfo("Flycast", 1, "Flycast", 1, vulkan11 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0);
		std::vector<const char *> vext;
		for (uint32_t i = 0; i < extensions_count; i++)
			vext.push_back(extensions[i]);

		std::vector<const char *> layer_names;
		//layer_names.push_back("VK_LAYER_ARM_AGA");
#ifdef VK_DEBUG
#ifndef __ANDROID__
		vext.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		vext.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		layer_names.push_back("VK_LAYER_KHRONOS_validation");
#else
		vext.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);	// NDK <= 19?
		layer_names.push_back("VK_LAYER_GOOGLE_threading");
		layer_names.push_back("VK_LAYER_LUNARG_parameter_validation");
		layer_names.push_back("VK_LAYER_LUNARG_core_validation");
		layer_names.push_back("VK_LAYER_GOOGLE_unique_objects");
#endif
#endif
		vk::InstanceCreateInfo instanceCreateInfo({}, &applicationInfo, layer_names, vext);
		// create a UniqueInstance
		instance = vk::createInstanceUnique(instanceCreateInfo);

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
		VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
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

		auto devices = instance->enumeratePhysicalDevices();
		if (devices.empty())
		{
			ERROR_LOG(RENDERER, "Vulkan error: no physical devices found");
			return false;
		}

		// The order of physical-devices provided by the driver should be somewhat preserved with stable-partitions/stable-sorts

		// Prefer GPUs that support optimal R5G5B5/R5G6B5A1/R4G4B4A4
		const auto supportsOptimalFormat = [](vk::Format format)
			{
				return [format](const vk::PhysicalDevice& physicalDevice) -> bool
					{
						const vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(format);
						return (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
							&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitDst)
							&& (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eBlitSrc);
					};
			};
		std::stable_partition(devices.begin(), devices.end(), supportsOptimalFormat(vk::Format::eR5G6B5UnormPack16));
		std::stable_partition(devices.begin(), devices.end(), supportsOptimalFormat(vk::Format::eR5G5B5A1UnormPack16));
		std::stable_partition(devices.begin(), devices.end(), supportsOptimalFormat(vk::Format::eR4G4B4A4UnormPack16));

		// Prefer GPUs that support fragmentStoresAndAtomics
		std::stable_partition(
			devices.begin(), devices.end(),
			[](const vk::PhysicalDevice& physicalDevice) -> bool
			{
				return !!physicalDevice.getFeatures().fragmentStoresAndAtomics;
			}
		);

		// Finally, prefer Discrete GPUs
		std::stable_partition(
			devices.begin(), devices.end(),
			[](const vk::PhysicalDevice& physicalDevice) -> bool
			{
				return physicalDevice.getProperties().deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
			}
		);

		// Top of the device-list is the _most_ qualified GPU
		physicalDevice = devices.front();

		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		if (vulkan11 && properties.apiVersion >= VK_API_VERSION_1_1)
		{
			const auto properties2 = physicalDevice.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceMaintenance3Properties>();
			properties = properties2.get<vk::PhysicalDeviceProperties2>().properties;
			maxMemoryAllocationSize = properties2.get<vk::PhysicalDeviceMaintenance3Properties>().maxMemoryAllocationSize;
			if (maxMemoryAllocationSize == 0)
				// Happens on Windows 7 with NVidia 376.33, ok on 441.66
				maxMemoryAllocationSize = 0xFFFFFFFFu;
		}

		uniformBufferAlignment = properties.limits.minUniformBufferOffsetAlignment;
		storageBufferAlignment = properties.limits.minStorageBufferOffsetAlignment;
		maxSamplerAnisotropy =  properties.limits.maxSamplerAnisotropy;
		vendorID = properties.vendorID;
		NOTICE_LOG(RENDERER, "Vulkan API %s. Device %s", vulkan11 ? "1.1" : "1.0", properties.deviceName.data());

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

		return true;
	}
	catch (const vk::SystemError& err)
	{
		ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
	}
	catch (const std::exception& err)
	{
		ERROR_LOG(RENDERER, "Vulkan instance init failed: %s", err.what());
	}
	catch (...)
	{
		ERROR_LOG(RENDERER, "Unknown error");
	}
	return false;
}

void VulkanContext::InitImgui()
{
	VulkanDriver *vkDriver = dynamic_cast<VulkanDriver *>(imguiDriver.get());
	if (vkDriver == nullptr) {
		imguiDriver.reset();
		imguiDriver = std::unique_ptr<ImGuiDriver>(new VulkanDriver());
	}
	else {
		vkDriver->reset();
	}
	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = (VkInstance)*instance;
	initInfo.PhysicalDevice = (VkPhysicalDevice)physicalDevice;
	initInfo.Device = (VkDevice)*device;
	initInfo.QueueFamily = graphicsQueueIndex;
	initInfo.Queue = (VkQueue)graphicsQueue;
	initInfo.PipelineCache = (VkPipelineCache)*pipelineCache;
	initInfo.DescriptorPool = (VkDescriptorPool)*descriptorPool;
	initInfo.RenderPass = (VkRenderPass)*renderPass;
	initInfo.MinImageCount = 2;
	initInfo.ImageCount = GetSwapChainSize();
#ifdef VK_DEBUG
	initInfo.CheckVkResultFn = &CheckImGuiResult;
#endif

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
	ImGui_ImplVulkan_LoadFunctions([](const char *function_name, void *) {
		return VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr((VkInstance) *contextInstance->instance, function_name);
	});
#endif

	if (!ImGui_ImplVulkan_Init(&initInfo))
		throw FlycastException("Vulkan ImGui initialization failed");
}

bool VulkanContext::InitDevice()
{
	if (!instance)
		return false;
	try
	{
		const vk::PhysicalDeviceProperties physicalDeviceProperties = physicalDevice.getProperties();

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
		if (graphicsQueueIndex == queueFamilyProperties.size() || presentQueueIndex == queueFamilyProperties.size()) {
			ERROR_LOG(RENDERER, "Could not find a queue for graphics or present");
			return false;
		}
		if (graphicsQueueIndex == presentQueueIndex)
			DEBUG_LOG(RENDERER, "Using Graphics+Present queue family");
		else
			DEBUG_LOG(RENDERER, "Using distinct Graphics and Present queue families");


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

#ifdef VK_ENABLE_BETA_EXTENSIONS
		tryAddDeviceExtension(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_METAL_EXT
		tryAddDeviceExtension(VK_EXT_METAL_OBJECTS_EXTENSION_NAME);
#endif
#ifdef VK_DEBUG
		tryAddDeviceExtension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif

		// Enable VK_KHR_dedicated_allocation if available
		if (physicalDeviceProperties.apiVersion >= VK_API_VERSION_1_1)
		{
			// Core in Vulkan 1.1
			dedicatedAllocationSupported = true;
		}
		else
		{
			const bool getMemReq2Supported = tryAddDeviceExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
			if (getMemReq2Supported)
			{
				dedicatedAllocationSupported = tryAddDeviceExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
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
			provokingVertexSupported = tryAddDeviceExtension(VK_EXT_PROVOKING_VERTEX_EXTENSION_NAME);
		}
		
		// Get device features

		vk::PhysicalDeviceFeatures2 featuresChain{};
		vk::PhysicalDeviceFeatures& features = featuresChain.features;

		vk::PhysicalDeviceProvokingVertexFeaturesEXT provokingVertexFeatures{};
		if (provokingVertexSupported)
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

		if (provokingVertexSupported)
		{
			provokingVertexSupported &= provokingVertexFeatures.provokingVertexLast;
		}

		samplerAnisotropy = features.samplerAnisotropy;
		fragmentStoresAndAtomics = features.fragmentStoresAndAtomics;
		if (!fragmentStoresAndAtomics)
			NOTICE_LOG(RENDERER, "Fragment stores & atomic not supported: no per-pixel sorting");

		// create a UniqueDevice
		float queuePriority = 1.0f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo(vk::DeviceQueueCreateFlags(), graphicsQueueIndex, 1, &queuePriority);

		if (getPhysicalDeviceProperties2Supported)
		{
			vk::DeviceCreateInfo deviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo,
				nullptr, enabledExtensions);
			deviceCreateInfo.pNext = &featuresChain;
			device = physicalDevice.createDeviceUnique(deviceCreateInfo);
		}
		else
		{
			device = physicalDevice.createDeviceUnique(vk::DeviceCreateInfo(vk::DeviceCreateFlags(), deviceQueueCreateInfo,
				nullptr, enabledExtensions, &features));
		}

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
		VULKAN_HPP_DEFAULT_DISPATCHER.init(*device);
#endif

	    // Queues
	    graphicsQueue = device->getQueue(graphicsQueueIndex, 0);
	    presentQueue = device->getQueue(presentQueueIndex, 0);

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
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 100),
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBufferDynamic, 2),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBufferDynamic, 2),
            vk::DescriptorPoolSize(vk::DescriptorType::eInputAttachment, 100)
        };
	    descriptorPool = device->createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
	    		40000, pool_sizes));


	    std::string cachePath = hostfs::getShaderCachePath("vulkan_pipeline.cache");
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
	    	try {
	    		pipelineCache = device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo(vk::PipelineCacheCreateFlags(), cacheSize, cacheData));
	    		INFO_LOG(RENDERER, "Vulkan pipeline cache loaded from %s: %zd bytes", cachePath.c_str(), cacheSize);
	    	}
	    	catch (const vk::SystemError& err) {
	    		WARN_LOG(RENDERER, "Error loading pipeline cache: %s", err.what());
	    		pipelineCache = device->createPipelineCacheUnique(vk::PipelineCacheCreateInfo());
	    	}
    		delete [] cacheData;
	    }
	    allocator.Init(physicalDevice, *device, *instance);

	    shaderManager = std::make_unique<ShaderManager>();
	    quadPipeline = std::make_unique<QuadPipeline>(true, false);
	    quadPipelineWithAlpha = std::make_unique<QuadPipeline>(false, false);
	    quadDrawer = std::make_unique<QuadDrawer>();
	    quadRotatePipeline = std::make_unique<QuadPipeline>(true, true);
	    quadRotateDrawer = std::make_unique<QuadDrawer>();

		vk::PhysicalDeviceProperties props = physicalDevice.getProperties();
		driverName = (const char *)props.deviceName;
#ifdef __APPLE__
		driverVersion = std::to_string(VK_API_VERSION_MAJOR(props.apiVersion)) + "."
				+ std::to_string(VK_API_VERSION_MINOR(props.apiVersion)) + "."
				+ std::to_string(VK_API_VERSION_PATCH(props.apiVersion)) + " MoltenVK-"
				// driverVersion = MoltenVK version, not using Vulkan apiVersion encoding
				+ std::to_string(props.driverVersion / 10000) + "."
				+ std::to_string((props.driverVersion % 10000) / 100) + "."
				+ std::to_string(props.driverVersion % 100);
#else
		driverVersion = std::to_string(VK_API_VERSION_MAJOR(props.driverVersion)) + "."
				+ std::to_string(VK_API_VERSION_MINOR(props.driverVersion)) + "."
				+ std::to_string(VK_API_VERSION_PATCH(props.driverVersion));
#endif

		CreateSwapChain();

		return true;
	}
	catch (const vk::SystemError& err)
	{
		ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
	}
	catch (const InvalidVulkanContext&)
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

		if (!drawFences.empty())
		{
			std::vector<vk::Fence> allFences = vk::uniqueToRaw(drawFences);
			vk::Result res = device->waitForFences(allFences, true, UINT64_MAX);
			if (res != vk::Result::eSuccess)
				WARN_LOG(RENDERER, "VulkanContext::CreateSwapChain: waitForFences failed %d", (int)res);
		}
		inFlightObjects.clear();
		overlay->Term();
		framebuffers.clear();
		drawFences.clear();
		imageAcquiredSemaphores.clear();
		renderCompleteSemaphores.clear();
		commandBuffers.clear();
		commandPools.clear();
		for (auto& img : imageViews)
			img.reset();
		rendering = false;
		renderDone = false;

		// Determine surface format and color-space
		std::vector<vk::SurfaceFormatKHR> surfaceFormats = physicalDevice.getSurfaceFormatsKHR(GetSurface());

		// Prefer a non-sRGB image format
		std::stable_partition(surfaceFormats.begin(), surfaceFormats.end(),
			[](const vk::SurfaceFormatKHR& surfaceFormat) -> bool
			{
				return std::string_view("SRGB").compare(vk::componentNumericFormat(surfaceFormat.format, 0)) != 0;
			}
		);

		// Prefer an sRGB presentation color-space
		std::stable_partition(surfaceFormats.begin(), surfaceFormats.end(),
			[](const vk::SurfaceFormatKHR& surfaceFormat) -> bool
			{
				return surfaceFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
			}
		);

		// Top of the list is the best candidate surface format/color-space
		const vk::SurfaceFormatKHR& targetSurfaceFormat = surfaceFormats[0];
		presentFormat = targetSurfaceFormat.format;

		int tries = 0;
		do {
			vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(GetSurface());
			DEBUG_LOG(RENDERER, "Surface capabilities: %d x %d, %s, image count: %d - %d", surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height,
					vk::to_string(surfaceCapabilities.currentTransform).c_str(), surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount);
			vk::Extent2D swapchainExtent;
			if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
			{
				// If the surface size is undefined, use the current display size
				swapchainExtent.width = std::min(std::max((u32)settings.display.width, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
				swapchainExtent.height = std::min(std::max((u32)settings.display.height, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);
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
#if defined(TEST_AUTOMATION) || (defined(VIDEO_ROUTING) && defined(TARGET_MAC))
			// for final screenshot or Syphon
			usage |= vk::ImageUsageFlagBits::eTransferSrc;
#endif
			vk::SwapchainCreateInfoKHR swapChainCreateInfo(vk::SwapchainCreateFlagsKHR(), GetSurface(), imageCount, targetSurfaceFormat.format, targetSurfaceFormat.colorSpace,
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
			vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image, vk::ImageViewType::e2D, presentFormat, componentMapping, subResourceRange);
			imageViews[imageIdx++] = device->createImageViewUnique(imageViewCreateInfo);

			// create a UniqueCommandPool to allocate a CommandBuffer from
			commandPools.push_back(device->createCommandPoolUnique(vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlagBits::eTransient, graphicsQueueIndex)));

		    // allocate a CommandBuffer from the CommandPool
		    commandBuffers.push_back(std::move(device->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo(*commandPools.back(), vk::CommandBufferLevel::ePrimary, 1)).front()));
		}

	    depthFormat = findDepthFormat(physicalDevice);
		if (depthFormat == vk::Format::eUndefined) {
			SetWindowSize(0, 0);
			throw InvalidVulkanContext();
		}

	    // Render pass
	    vk::AttachmentDescription attachmentDescription = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), presentFormat, vk::SampleCountFlagBits::e1,
	    		vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
				vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

	    vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, nullptr, colorReference,
	    		nullptr, nullptr);

	    renderPass = device->createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
	    		attachmentDescription,	subpass));

	    // Framebuffers, fences, semaphores

	    framebuffers.reserve(imageViews.size());
	    drawFences.reserve(imageViews.size());
	    for (auto const& view : imageViews)
	    {
	    	framebuffers.push_back(device->createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(), *renderPass,
	    			view.get(), width, height, 1)));
	    	drawFences.push_back(device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
	    }
	    renderCompleteSemaphores.reserve(imageViews.size() + 1);
	    imageAcquiredSemaphores.reserve(imageViews.size() + 1);
	    for (unsigned i = 0; i < imageViews.size() + 1; i++)
	    {
	    	renderCompleteSemaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
	    	imageAcquiredSemaphores.push_back(device->createSemaphoreUnique(vk::SemaphoreCreateInfo()));
	    }
	    inFlightObjects.resize(imageViews.size());
	    currentSemaphore = 0;
	    quadPipeline->Init(shaderManager.get(), *renderPass, 0);
	    quadPipelineWithAlpha->Init(shaderManager.get(), *renderPass, 0);
	    quadDrawer->Init(quadPipeline.get());
	    quadRotatePipeline->Init(shaderManager.get(), *renderPass, 0);
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
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
    extern void CreateMainWindow();
    CreateMainWindow();
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#endif
	if (!InitInstance(&extensions[0], extensions.size())) {
		term();
		return false;
	}

#if defined(USE_SDL)
    VkSurfaceKHR surface;
    if (SDL_Vulkan_CreateSurface((SDL_Window *)window, (VkInstance)*instance, &surface) == 0) {
		term();
    	return false;
    }
    this->surface.reset(vk::SurfaceKHR(surface));
    SDL_Window *sdlWin = (SDL_Window *)window;
    int w, h;
    SDL_GetWindowSize(sdlWin, &w, &h);
    SDL_Vulkan_GetDrawableSize(sdlWin, &settings.display.width, &settings.display.height);
    settings.display.pointScale = (float)settings.display.width / w;
	float hdpi, vdpi;
	if (!SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(sdlWin), nullptr, &hdpi, &vdpi))
		settings.display.dpi = roundf(std::max(hdpi, vdpi));

	sdl_fix_steamdeck_dpi(sdlWin);
#elif defined(VK_USE_PLATFORM_WIN32_KHR)
	vk::Win32SurfaceCreateInfoKHR createInfo(vk::Win32SurfaceCreateFlagsKHR(), GetModuleHandle(NULL), (HWND)window);
	surface = instance->createWin32SurfaceKHRUnique(createInfo);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
	vk::XlibSurfaceCreateInfoKHR createInfo(vk::XlibSurfaceCreateFlagsKHR(), (Display*)display, (Window)window);
	surface = instance->createXlibSurfaceKHRUnique(createInfo);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
	vk::AndroidSurfaceCreateInfoKHR createInfo(vk::AndroidSurfaceCreateFlagsKHR(), (struct ANativeWindow*)window);
	surface = instance->createAndroidSurfaceKHRUnique(createInfo);
#elif defined(VK_USE_PLATFORM_METAL_EXT)
	vk::MetalSurfaceCreateInfoEXT createInfo(vk::MetalSurfaceCreateFlagsEXT(), window);
	surface = instance->createMetalSurfaceEXTUnique(createInfo);
#else
#error "Unknown Vulkan platform"
#endif
	overlay = std::make_unique<VulkanOverlay>();

	if (!InitDevice()) {
		term();
		return false;
	}

	return true;
}

bool VulkanContext::recreateSwapChainIfNeeded()
{
	if (resized || HasSurfaceDimensionChanged())
	{
		CreateSwapChain();
		lastFrameView = vk::ImageView();
		return true;
	}
	else
		return false;
}

void VulkanContext::NewFrame()
{
	recreateSwapChainIfNeeded();
	if (!IsValid())
		throw InvalidVulkanContext();
	vk::Result res = device->acquireNextImageKHR(*swapChain, UINT64_MAX, *imageAcquiredSemaphores[currentSemaphore], nullptr, &currentImage);
	if (res != vk::Result::eSuccess)
		throw InvalidVulkanContext();
	try {
		res = device->waitForFences(*drawFences[currentImage], true, UINT64_MAX);
		if (res != vk::Result::eSuccess)
			throw InvalidVulkanContext();
	} catch (const vk::SystemError& e) {
		WARN_LOG(RENDERER, "vk:SystemError: %s", e.what());
		throw FlycastException("Vulkan system error");
	}
	device->resetCommandPool(*commandPools[currentImage], vk::CommandPoolResetFlagBits::eReleaseResources);
	inFlightObjects[currentImage].clear();
	vk::CommandBuffer commandBuffer = *commandBuffers[currentImage];
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	verify(!rendering);
	rendering = true;
}

void VulkanContext::BeginRenderPass()
{
	if (!IsValid())
		return;
	const std::array<vk::ClearValue, 2> clear_colors = { getBorderColor(), vk::ClearDepthStencilValue{ 0.f, 0 } };
	vk::CommandBuffer commandBuffer = *commandBuffers[currentImage];
	commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(*renderPass, *framebuffers[currentImage], vk::Rect2D({0, 0}, {width, height}), clear_colors),
			vk::SubpassContents::eInline);
}

void VulkanContext::EndFrame(vk::CommandBuffer overlayCmdBuffer)
{
	if (!IsValid())
		return;
	vk::CommandBuffer commandBuffer = *commandBuffers[currentImage];
	commandBuffer.endRenderPass();
	commandBuffer.end();
	vk::PipelineStageFlags wait_stage(vk::PipelineStageFlagBits::eColorAttachmentOutput);
	std::vector<vk::CommandBuffer> allCmdBuffers;
	if (overlayCmdBuffer)
		allCmdBuffers.push_back(overlayCmdBuffer);
	allCmdBuffers.push_back(commandBuffer);
	vk::SubmitInfo submitInfo(*imageAcquiredSemaphores[currentSemaphore], wait_stage, allCmdBuffers, *renderCompleteSemaphores[currentSemaphore]);
	device->resetFences(*drawFences[currentImage]);
	graphicsQueue.submit(submitInfo, *drawFences[currentImage]);
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
			vk::Result res = presentQueue.presentKHR(vk::PresentInfoKHR(1, &(*renderCompleteSemaphores[currentSemaphore]), 1, &(*swapChain), &currentImage));
			(void)res;
			currentSemaphore = (currentSemaphore + 1) % renderCompleteSemaphores.size();

			if (lastFrameView && IsValid() && !gui_is_open())
				for (int i = 1; i < swapInterval; i++)
				{
					PresentFrame(vk::Image(), lastFrameView, lastFrameExtent, lastFrameAR);
					res = presentQueue.presentKHR(vk::PresentInfoKHR(1, &(*renderCompleteSemaphores[currentSemaphore]), 1, &(*swapChain), &currentImage));
					currentSemaphore = (currentSemaphore + 1) % renderCompleteSemaphores.size();
				}
		} catch (const vk::SystemError& e) {
			// Happens when resizing the window
			INFO_LOG(RENDERER, "vk::SystemError %s", e.what());
			resized = true;
			width = height = 0;
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
		} catch (const InvalidVulkanContext&) {
		}
}

void VulkanContext::DrawFrame(vk::ImageView imageView, const vk::Extent2D& extent, float aspectRatio)
{
	QuadVertex vtx[4] {
		{ -1, -1, 0, 0, 0 },
		{  1, -1, 0, 1, 0 },
		{ -1,  1, 0, 0, 1 },
		{  1,  1, 0, 1, 1 },
	};
	float shiftX, shiftY;
	getVideoShift(shiftX, shiftY);
	vtx[0].x = vtx[2].x = -1.f + shiftX * 2.f / extent.width;
	vtx[1].x = vtx[3].x = vtx[0].x + 2;
	vtx[0].y = vtx[1].y = -1.f + shiftY * 2.f / extent.height;
	vtx[2].y = vtx[3].y = vtx[0].y + 2;

	vk::CommandBuffer commandBuffer = GetCurrentCommandBuffer();

	static const float scopeColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	CommandBufferDebugScope _(commandBuffer, "DrawFrame", scopeColor);

	if (config::Rotate90)
		quadRotatePipeline->BindPipeline(commandBuffer);
	else
		quadPipeline->BindPipeline(commandBuffer);

	int dx = 0;
	int dy = 0;
	getWindowboxDimensions(width, height, aspectRatio, dx, dy, config::Rotate90);
	
	vk::Viewport viewport(dx, dy, width - dx * 2, height - dy * 2);
	commandBuffer.setViewport(0, viewport);
	commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(dx, dy), vk::Extent2D(width - dx * 2, height - dy * 2)));
	if (config::Rotate90)
		quadRotateDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);
	else
		quadDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);
}

void VulkanContext::WaitIdle() const
{
	try {
		graphicsQueue.waitIdle();
	} catch (const vk::Error &err) {
		WARN_LOG(RENDERER, "WaitIdle: %s", err.what());
	}
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

void VulkanContext::PresentFrame(vk::Image image, vk::ImageView imageView, const vk::Extent2D& extent, float aspectRatio) noexcept
{
	lastFrameView = imageView;
	lastFrameExtent = extent;
	lastFrameAR = aspectRatio;

	if (imageView && IsValid())
	{
		try {
			NewFrame();
			auto overlayCmdBuffer = PrepareOverlay(config::FloatVMUs, true);
			gui_draw_osd();
			if (GetVendorID() == VulkanContext::VENDOR_NVIDIA && image)
			{
				vk::ImageMemoryBarrier barrier(
						vk::AccessFlagBits::eColorAttachmentWrite,
				        vk::AccessFlagBits::eShaderRead,
				        vk::ImageLayout::eShaderReadOnlyOptimal,
				        vk::ImageLayout::eShaderReadOnlyOptimal,
				        VK_QUEUE_FAMILY_IGNORED,
				        VK_QUEUE_FAMILY_IGNORED,
				        image,
				        vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
				GetCurrentCommandBuffer().pipelineBarrier(
						vk::PipelineStageFlagBits::eColorAttachmentOutput,
						vk::PipelineStageFlagBits::eFragmentShader,
						{},
						nullptr, nullptr,
						barrier
				);
			}
			BeginRenderPass();

			if (lastFrameView) // Might have been nullified if swap chain recreated
				DrawFrame(imageView, extent, aspectRatio);

			DrawOverlay(settings.display.uiScale, config::FloatVMUs, true);
			imguiDriver->renderDrawData(ImGui::GetDrawData(), false);
			EndFrame(overlayCmdBuffer);
			static_cast<BaseVulkanRenderer*>(renderer)->RenderVideoRouting();
			
		} catch (const InvalidVulkanContext&) {
			// Re-create swap chain
			resized = true;
		}
	}
}

void VulkanContext::PresentLastFrame()
{
	if (lastFrameView && IsValid())
		DrawFrame(lastFrameView, lastFrameExtent, lastFrameAR);
}

void VulkanContext::term()
{
	GraphicsContext::instance = nullptr;
	lastFrameView = nullptr;
	if (device && graphicsQueue)
		WaitIdle();
	if (device && !drawFences.empty())
	{
		std::vector<vk::Fence> allFences = vk::uniqueToRaw(drawFences);
		try {
			vk::Result res = device->waitForFences(allFences, true, UINT64_MAX);
			if (res != vk::Result::eSuccess)
				INFO_LOG(RENDERER, "VulkanContext::term: waitForFences failed %d", (int)res);
		} catch (const vk::SystemError& e) {
		}
	}
	inFlightObjects.clear();
	imguiDriver.reset();
	if (device && pipelineCache)
	{
		std::vector<u8> cacheData = device->getPipelineCacheData(*pipelineCache);
		if (!cacheData.empty())
		{
			std::string cachePath = hostfs::getShaderCachePath("vulkan_pipeline.cache");
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
	if (instance && surface)
		instance->destroySurfaceKHR(surface.release());
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
#if defined(__ANDROID__) && HOST_CPU == CPU_ARM64
	unloadVulkanDriver();
#endif
}

void VulkanContext::DoSwapAutomation()
{
#ifdef TEST_AUTOMATION
	extern bool do_screenshot;

	if (do_screenshot)
	{
		bool supportsBlit = true;
		vk::FormatProperties properties;
		physicalDevice.getFormatProperties(presentFormat, &properties);
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
					vk::SharingMode::eExclusive, nullptr, vk::ImageLayout::eUndefined);
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
			vk::SubmitInfo submitInfo(nullptr, nullptr, cmdBuffer.get(), nullptr);
			graphicsQueue.submit(submitInfo, nullptr);
			graphicsQueue.waitIdle();

			vk::ImageSubresource subresource(vk::ImageAspectFlagBits::eColor, 0, 0);
			vk::SubresourceLayout subresourceLayout;
			device->getImageSubresourceLayout(*dstImage, &subresource, &subresourceLayout);

			u8* img = (u8*)device->mapMemory(*deviceMemory, 0, VK_WHOLE_SIZE);
			img += subresourceLayout.offset;

			u8 *end = img + settings.display.width * settings.display.height * 4;
			if (!supportsBlit && presentFormat == vk::Format::eB8G8R8A8Unorm)
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
		flycast_term();
		exit(0);
	}
#endif
}

bool VulkanContext::HasSurfaceDimensionChanged() const
{
	vk::SurfaceCapabilitiesKHR surfaceCapabilities =
			physicalDevice.getSurfaceCapabilitiesKHR(GetSurface());
	vk::Extent2D swapchainExtent;
	if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
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

bool VulkanContext::GetLastFrame(std::vector<u8>& data, int& width, int& height)
{
	if (!lastFrameView)
		return false;

	if (width != 0) {
		height = width / lastFrameAR;
	}
	else if (height != 0) {
		width = lastFrameAR * height;
	}
	else
	{
		width = lastFrameExtent.width;
		height = lastFrameExtent.height;
		if (config::Rotate90)
			std::swap(width, height);
		// We need square pixels for PNG
		int w = lastFrameAR * height;
		if (width > w)
			height = width / lastFrameAR;
		else
			width = w;
	}

	vk::Format imageFormat = vk::Format::eR8G8B8A8Unorm;
	const vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;

	// Test if RGB8 is natively supported to avoid having to do a format conversion
	bool nativeRgb8 = false;
	vk::ImageFormatProperties rgb8Properties{};
	if (physicalDevice.getImageFormatProperties(vk::Format::eR8G8B8Unorm, vk::ImageType::e2D, vk::ImageTiling::eOptimal, imageUsage, {}, &rgb8Properties) == vk::Result::eSuccess)
	{
		nativeRgb8 = true;
		imageFormat = vk::Format::eR8G8B8Unorm;
	}

	// color attachment
	FramebufferAttachment attachment(physicalDevice, *device);
	attachment.Init(width, height, imageFormat, imageUsage, "screenshot");
	// command buffer
	vk::UniqueCommandBuffer commandBuffer = std::move(device->allocateCommandBuffersUnique(
			vk::CommandBufferAllocateInfo(*commandPools.back(), vk::CommandBufferLevel::ePrimary, 1)).front());
	commandBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	static const float scopeColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
	CommandBufferDebugScope _(commandBuffer.get(), "GetLastFrame", scopeColor);

	// render pass
	vk::AttachmentDescription attachmentDescription = vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), imageFormat, vk::SampleCountFlagBits::e1,
			vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
			vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferSrcOptimal);
	vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, nullptr, colorReference,
			nullptr, nullptr);
	vk::UniqueRenderPass renderPass = device->createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
    		attachmentDescription,	subpass));
	// framebuffer
	vk::ImageView imageView = attachment.GetImageView();
	vk::UniqueFramebuffer framebuffer = device->createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(),
				*renderPass, imageView, width, height, 1));
	vk::ClearValue clearValue;
	commandBuffer->beginRenderPass(vk::RenderPassBeginInfo(*renderPass, *framebuffer, vk::Rect2D({0, 0}, {(u32)width, (u32)height}), clearValue),
			vk::SubpassContents::eInline);

	// Pipeline
	QuadPipeline pipeline(true, config::Rotate90);
	pipeline.Init(shaderManager.get(), *renderPass, 0);
	pipeline.BindPipeline(*commandBuffer);

	// Draw
	QuadVertex vtx[4] {
		{ -1, -1, 0, 0, 0 },
		{  1, -1, 0, 1, 0 },
		{ -1,  1, 0, 0, 1 },
		{  1,  1, 0, 1, 1 },
	};

	vk::Viewport viewport(0, 0, width, height);
	commandBuffer->setViewport(0, viewport);
	commandBuffer->setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(width, height)));
	QuadDrawer drawer;
	drawer.Init(&pipeline);
	drawer.Draw(*commandBuffer, lastFrameView, vtx, false);
	commandBuffer->endRenderPass();

	if (vendorID == VENDOR_ARM)
	{
		// Mali GPUs need an extra mem barrier here for some reason
		vk::MemoryBarrier memoryBarrier(
				vk::AccessFlagBits::eColorAttachmentWrite,
				vk::AccessFlagBits::eTransferRead);
		commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
						vk::PipelineStageFlagBits::eTransfer, {}, memoryBarrier, nullptr, nullptr);
	}

	// Copy back
	vk::BufferImageCopy copyRegion(0, width, height, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0),
			vk::Extent3D(width, height, 1));
	commandBuffer->copyImageToBuffer(attachment.GetImage(), vk::ImageLayout::eTransferSrcOptimal,
			*attachment.GetBufferData()->buffer, copyRegion);

	vk::BufferMemoryBarrier bufferMemoryBarrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eHostRead,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			*attachment.GetBufferData()->buffer,
			0,
			VK_WHOLE_SIZE);
	commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eHost, {}, nullptr, bufferMemoryBarrier, nullptr);
	commandBuffer->end();

	vk::UniqueFence fence = device->createFenceUnique(vk::FenceCreateInfo());
	vk::SubmitInfo submitInfo(nullptr, nullptr, commandBuffer.get(), nullptr);
	graphicsQueue.submit(submitInfo, *fence);

	vk::Result res = device->waitForFences(fence.get(), true, UINT64_MAX);
	if (res != vk::Result::eSuccess)
		WARN_LOG(RENDERER, "VulkanContext::GetLastFrame: waitForFences failed %d", (int)res);

	const u8 *img = (const u8 *)attachment.GetBufferData()->MapMemory();
	data.clear();
	if (nativeRgb8)
	{
		// Format is already RGB, can be directly copied
		data.resize(width * height * 3);
		std::memcpy(data.data(), img, width * height * 3);
	}
	else
	{
		data.reserve(width * height * 3);
		// RGBA -> RGB
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				data.push_back(*img++);
				data.push_back(*img++);
				data.push_back(*img++);
				img++;
			}
		}
	}
	attachment.GetBufferData()->UnmapMemory();

	return true;
}
