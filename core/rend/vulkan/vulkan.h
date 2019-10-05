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

//#if defined(_WIN32)
//#   define VK_USE_PLATFORM_WIN32_KHR
//#elif defined(__linux__) || defined(__unix__)
//#   define VK_USE_PLATFORM_XLIB_KHR
//#elif defined(__APPLE__)
//#   define VK_USE_PLATFORM_MACOS_MVK
//#else
//#   error "Platform not supported"
//#endif

#include "volk/volk.h"
#undef VK_NO_PROTOTYPES
#include "vulkan/vulkan.hpp"

#define VK_DEBUG

extern int screen_width, screen_height;

class VulkanContext
{
public:
	VulkanContext() { verify(contextInstance == nullptr); contextInstance = this; }
	~VulkanContext();
	void InitInstance(const char** extensions, uint32_t extensions_count);
	void InitDevice();

	VkInstance GetInstance() const { return static_cast<VkInstance>(instance.get()); }
	u32 GetGraphicsQueueFamilyIndex() const { return graphicsQueueIndex; }
	void SetSurface(VkSurfaceKHR surface) { this->surface = surface; }
	void SetWindowSize(u32 width, u32 height) { this->width = screen_width = width; this->height = screen_height = height; }
	void NewFrame();
	void BeginRenderPass();
	void EndFrame();
	void Present();

	vk::PhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	vk::UniqueDevice& GetDevice() { return device; }
	vk::PipelineCache GetPipelineCache() const { return *pipelineCache; }
	vk::RenderPass GetRenderPass() const { return *renderPass; }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return *commandBuffers[currentImage]; }
	vk::DescriptorPool GetDescriptorPool() const { return *descriptorPool; }
	vk::Extent2D GetViewPort() const { return { width, height }; }
	int GetSwapChainSize() const { return (int)imageViews.size(); }
	int GetCurrentImageIndex() const { return currentImage; }
	void WaitIdle() const { graphicsQueue.waitIdle(); }

	static VulkanContext *Instance() { return contextInstance; }

private:
	void InitDepthBuffer();
	void InitImgui();

	u32 width = 0;
	u32 height = 0;
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;

	u32 graphicsQueueIndex = 0;
	u32 presentQueueIndex = 0;
	vk::UniqueDevice device;

	vk::SurfaceKHR surface;	// FIXME needs to be destroyed manually

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
	vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger;
#endif
	static VulkanContext *contextInstance;
};
