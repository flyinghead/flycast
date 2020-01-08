/*
    Created on: Nov 29, 2019

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
#include "vulkan.h"
#include "vmallocator.h"
#include "quad.h"
#include "rend/TexCache.h"
#include "vmu.h"

extern int screen_width, screen_height;

#define VENDOR_AMD 0x1022
#define VENDOR_ARM 0x13B5
#define VENDOR_INTEL 0x8086
#define VENDOR_NVIDIA 0x10DE
#define VENDOR_QUALCOMM 0x5143

class VulkanContext
{
public:
	VulkanContext() { verify(contextInstance == nullptr); contextInstance = this; }
	~VulkanContext() { verify(contextInstance == this); contextInstance = nullptr; }

	bool Init();
	bool InitInstance(const char** extensions, uint32_t extensions_count);
	bool InitDevice();
	void CreateSwapChain();
	void Term();
	void SetWindow(void *window, void *display) { this->window = window; this->display = display; }

	VkInstance GetInstance() const { return static_cast<VkInstance>(instance.get()); }
	u32 GetGraphicsQueueFamilyIndex() const { return graphicsQueueIndex; }
	void SetWindowSize(u32 width, u32 height) { this->width = screen_width = width; this->height = screen_height = height; }
	void NewFrame();
	void BeginRenderPass();
	void EndFrame(const std::vector<vk::UniqueCommandBuffer> *cmdBuffers = nullptr);
	void Present();
	void PresentFrame(vk::ImageView imageView, vk::Offset2D extent);
	void PresentLastFrame();

	vk::PhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	vk::Device GetDevice() const { return *device; }
	vk::PipelineCache GetPipelineCache() const { return *pipelineCache; }
	vk::RenderPass GetRenderPass() const { return *renderPass; }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return *commandBuffers[GetCurrentImageIndex()]; }
	vk::DescriptorPool GetDescriptorPool() const { return *descriptorPool; }
	vk::Extent2D GetViewPort() const { return { width, height }; }
	int GetSwapChainSize() const { return (int)imageViews.size(); }
	int GetCurrentImageIndex() const { return currentImage; }
	void WaitIdle() const { graphicsQueue.waitIdle(); }
	bool IsRendering() const { return rendering; }
	vk::Queue GetGraphicsQueue() const { return graphicsQueue; }
	vk::DeviceSize GetUniformBufferAlignment() const { return uniformBufferAlignment; }
	vk::DeviceSize GetStorageBufferAlignment() const { return storageBufferAlignment; }
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
	std::string GetDriverName() const { vk::PhysicalDeviceProperties props; physicalDevice.getProperties(&props); return std::string(props.deviceName); }
	std::string GetDriverVersion() const {
		vk::PhysicalDeviceProperties props;
		physicalDevice.getProperties(&props);

		return std::to_string(VK_VERSION_MAJOR(props.driverVersion))
			+ "." + std::to_string(VK_VERSION_MINOR(props.driverVersion))
			+ "." + std::to_string(VK_VERSION_PATCH(props.driverVersion));
	}
	vk::Format GetColorFormat() const { return colorFormat; }
	vk::Format GetDepthFormat() const { return depthFormat; }
	static VulkanContext *Instance() { return contextInstance; }
	bool SupportsFragmentShaderStoresAndAtomics() const { return fragmentStoresAndAtomics; }
	bool SupportsSamplerAnisotropy() const { return samplerAnisotropy; }
	bool SupportsDedicatedAllocation() const { return dedicatedAllocationSupported; }
	const VMAllocator& GetAllocator() const { return allocator; }
	bool IsUnifiedMemory() const { return unifiedMemory; }
	u32 GetMaxStorageBufferRange() const { return maxStorageBufferRange; }
	vk::DeviceSize GetMaxMemoryAllocationSize() const { return maxMemoryAllocationSize; }
	u32 GetVendorID() const { return vendorID; }
	const std::vector<vk::UniqueCommandBuffer> *PrepareVMUs();
	void DrawVMUs(float scaling);

private:
	vk::Format FindDepthFormat();
	void InitImgui();
	void DoSwapAutomation();
	void DrawFrame(vk::ImageView imageView, vk::Offset2D extent);
	vk::SurfaceKHR GetSurface() {
#ifdef USE_SDL
		return surface;
#else
		return *surface;
#endif
	}

	bool HasSurfaceDimensionChanged()
	{
		vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(GetSurface());
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

	VMAllocator allocator;
	void *window = nullptr;
	void *display = nullptr;
	bool rendering = false;
	bool renderDone = false;
	u32 width = 0;
	u32 height = 0;
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;

	u32 graphicsQueueIndex = 0;
	u32 presentQueueIndex = 0;
	vk::DeviceSize uniformBufferAlignment = 0;
	vk::DeviceSize storageBufferAlignment = 0;
	u32 maxStorageBufferRange = 0;
	vk::DeviceSize maxMemoryAllocationSize = 0xFFFFFFFFu;
	bool optimalTilingSupported565 = false;
	bool optimalTilingSupported1555 = false;
	bool optimalTilingSupported4444 = false;
	bool fragmentStoresAndAtomics = false;
	bool samplerAnisotropy = false;
	bool dedicatedAllocationSupported = false;
	bool unifiedMemory = false;
	u32 vendorID = 0;
	vk::UniqueDevice device;

#ifdef USE_SDL
	vk::SurfaceKHR surface;
#else
	vk::UniqueSurfaceKHR surface;
#endif

	vk::UniqueSwapchainKHR swapChain;
	std::vector<vk::UniqueImageView> imageViews;
	u32 currentImage = 0;
	vk::Format colorFormat = vk::Format::eUndefined;

	vk::Queue graphicsQueue;
	vk::Queue presentQueue;

	vk::UniqueDescriptorPool descriptorPool;
	vk::UniqueRenderPass renderPass;

	vk::Format depthFormat = vk::Format::eUndefined;

	std::vector<vk::UniqueCommandPool> commandPools;
	std::vector<vk::UniqueCommandBuffer> commandBuffers;

	std::vector<vk::UniqueFramebuffer> framebuffers;

	std::vector<vk::UniqueFence> drawFences;
	std::vector<vk::UniqueSemaphore> renderCompleteSemaphores;
	std::vector<vk::UniqueSemaphore> imageAcquiredSemaphores;
	u32 currentSemaphore = 0;

	vk::UniquePipelineCache pipelineCache;

	std::unique_ptr<QuadPipeline> quadPipeline;
	std::unique_ptr<QuadDrawer> quadDrawer;
	std::unique_ptr<ShaderManager> shaderManager;

	vk::ImageView lastFrameView;
	vk::Offset2D lastFrameExtent;

	std::unique_ptr<VulkanVMUs> vmus;

#ifdef VK_DEBUG
#ifndef __ANDROID__
	vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger;
#else
	vk::UniqueDebugReportCallbackEXT debugReportCallback;
#endif
#endif
	static VulkanContext *contextInstance;
};
