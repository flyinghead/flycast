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
#pragma once
#include "types.h"

#include "volk/volk.h"
#undef VK_NO_PROTOTYPES
#include "vulkan/vulkan.hpp"
#include "rend/TexCache.h"

//#define VK_DEBUG

extern int screen_width, screen_height;

class VulkanContext
{
public:
	VulkanContext() { verify(contextInstance == nullptr); contextInstance = this; }
	~VulkanContext();
	bool Init();
	bool InitInstance(const char** extensions, uint32_t extensions_count);
	bool InitDevice();
	void CreateSwapChain();

	VkInstance GetInstance() const { return static_cast<VkInstance>(instance.get()); }
	u32 GetGraphicsQueueFamilyIndex() const { return graphicsQueueIndex; }
	VkSurfaceKHR GetSurface() { return (VkSurfaceKHR)this->surface; }
	void SetSurface(VkSurfaceKHR surface) { this->surface = vk::SurfaceKHR(surface); }
	void SetWindowSize(u32 width, u32 height) { this->width = screen_width = width; this->height = screen_height = height; }
	void NewFrame();
	void BeginRenderPass();
	void EndFrame();
	void Present();

	vk::PhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	vk::UniqueDevice& GetDevice() { return device; }
	vk::PipelineCache GetPipelineCache() const { return *pipelineCache; }
	vk::RenderPass GetRenderPass() const { return *renderPass; }
	vk::CommandPool GetCurrentCommandPool() const { return *commandPools[GetCurrentImageIndex()]; }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return *commandBuffers[GetCurrentImageIndex()]; }
	vk::DescriptorPool GetDescriptorPool() const { return *descriptorPool; }
	vk::Extent2D GetViewPort() const { return { width, height }; }
	int GetSwapChainSize() const { return (int)imageViews.size(); }
	int GetCurrentImageIndex() const { return currentImage; }
	void WaitIdle() const { graphicsQueue.waitIdle(); }
	bool IsRendering() const { return rendering; }
	vk::Queue GetGraphicsQueue() const { return graphicsQueue; }
	vk::DeviceSize GetUniformBufferAlignment() const { return uniformBufferAlignment; }
	bool IsFormatSupported(TextureType textureType)
	{
		switch (textureType)
		{
		case TextureType::_4444:
			return optimalTilingSupported4444;
		case TextureType::_565:
			return optimalTilingSupported565;
		case TextureType::_5551:
			return optimalTilingSupported1555;
		default:
			return true;
		}
	}
	std::string GetDriverName() const { vk::PhysicalDeviceProperties props; physicalDevice.getProperties(&props); return props.deviceName; }
	std::string GetDriverVersion() const { vk::PhysicalDeviceProperties props; physicalDevice.getProperties(&props); return std::to_string(props.driverVersion); }

	static VulkanContext *Instance() { return contextInstance; }

private:
	vk::Format InitDepthBuffer();
	void InitImgui();

	bool HasSurfaceDimensionChanged()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
		VkExtent2D swapchainExtent;
		if (surfaceCapabilities.currentExtent.width == std::numeric_limits<uint32_t>::max())
		{
			// If the surface size is undefined, the size is set to the size of the images requested.
			swapchainExtent.width = std::min(std::max(width, surfaceCapabilities.minImageExtent.width), surfaceCapabilities.maxImageExtent.width);
			swapchainExtent.height = std::min(std::max(height, surfaceCapabilities.minImageExtent.height), surfaceCapabilities.maxImageExtent.height);
		}
		else
		{
			// If the surface size is defined, the swap chain size must match
			swapchainExtent = surfaceCapabilities.currentExtent;
		}
		if (width == swapchainExtent.width && height == swapchainExtent.height)
			return false;

		screen_width = width = swapchainExtent.width;
		screen_height = height = swapchainExtent.height;

		return true;
	}

	bool rendering = false;
	bool renderDone = false;
	u32 width = 0;
	u32 height = 0;
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;

	u32 graphicsQueueIndex = 0;
	u32 presentQueueIndex = 0;
	vk::DeviceSize uniformBufferAlignment = 0;
	bool optimalTilingSupported565 = false;
	bool optimalTilingSupported1555 = false;
	bool optimalTilingSupported4444 = false;
	vk::UniqueDevice device;

	vk::SurfaceKHR surface;

	vk::UniqueSwapchainKHR swapChain;
	std::vector<vk::UniqueImageView> imageViews;
	u32 currentImage = 0;

	vk::Queue graphicsQueue;
	vk::Queue presentQueue;

	vk::UniqueDescriptorPool descriptorPool;
	vk::UniqueRenderPass renderPass;

	vk::UniqueImage depthImage;
	vk::UniqueImageView depthView;
	vk::UniqueDeviceMemory depthMemory;

	std::vector<vk::UniqueCommandPool> commandPools;
	std::vector<vk::UniqueCommandBuffer> commandBuffers;

	std::vector<vk::UniqueFramebuffer> framebuffers;

	std::vector<vk::UniqueFence> drawFences;
	std::vector<vk::UniqueSemaphore> renderCompleteSemaphores;
	std::vector<vk::UniqueSemaphore> imageAcquiredSemaphores;
	u32 currentSemaphore = 0;

	vk::UniquePipelineCache pipelineCache;
#ifdef VK_DEBUG
#ifndef __ANDROID__
	vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger;
#else
	vk::UniqueDebugReportCallbackEXT debugReportCallback;
#endif
#endif
	static VulkanContext *contextInstance;
};
