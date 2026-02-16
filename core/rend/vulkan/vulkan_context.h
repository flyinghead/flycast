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

#ifdef USE_VULKAN
#include "vulkan.h"

#include <stdexcept>

class InvalidVulkanContext : public std::runtime_error {
public:
	InvalidVulkanContext() : std::runtime_error("Invalid Vulkan context") {}
};

// RAII utility-object for adding debug-scopes to command buffers
#ifdef VK_DEBUG
class CommandBufferDebugScope {
private:
	const vk::CommandBuffer commandBuffer;

public:
	CommandBufferDebugScope(
		vk::CommandBuffer targetCommandBuffer,
		std::string_view scopeName, const float scopeColor[4]
	) : commandBuffer(targetCommandBuffer)
	{
		vk::DebugUtilsLabelEXT label{};
		label.pLabelName = scopeName.data();
		std::copy_n(scopeColor, 4, label.color.data());
		commandBuffer.beginDebugUtilsLabelEXT(label);
	}

	void operator()(std::string_view scopeName, const float scopeColor[4]) const
	{
		vk::DebugUtilsLabelEXT label{};
		label.pLabelName = scopeName.data();
		std::copy_n(scopeColor, 4, label.color.data());
		commandBuffer.insertDebugUtilsLabelEXT(label);
	}

	~CommandBufferDebugScope()
	{
		commandBuffer.endDebugUtilsLabelEXT();
	}
};
#else
class CommandBufferDebugScope {
public:
	CommandBufferDebugScope(
		vk::CommandBuffer targetCommandBuffer,
		std::string_view scopeName, const float scopeColor[4]
	)
	{
	}

	void operator()(std::string_view scopeName, const float scopeColor[4]) const
	{
	}

	~CommandBufferDebugScope()
	{
	}
};
#endif

#ifdef LIBRETRO
#include "vk_context_lr.h"
#else

#include "vmallocator.h"
#include "quad.h"
#include "rend/texconv.h"
#include "overlay.h"
#include "wsi/context.h"
#include <vector>

struct ImDrawData;

class VulkanContext : public GraphicsContext, public FlightManager
{
public:
	VulkanContext();
	~VulkanContext() override;

	bool init();
	void term() override;

	VkInstance GetInstance() const { return static_cast<VkInstance>(instance.get()); }
	u32 GetGraphicsQueueFamilyIndex() const { return graphicsQueueIndex; }
	void resize() override { resized = true; }
	bool IsValid() { return width != 0 && height != 0; }
	void NewFrame();
	void BeginRenderPass();
	void EndFrame(vk::CommandBuffer cmdBuffer = vk::CommandBuffer());
	void Present() noexcept;
	void PresentFrame(vk::Image image, vk::ImageView imageView, const vk::Extent2D& extent, float aspectRatio) noexcept;
	void PresentLastFrame();
	bool GetLastFrame(std::vector<u8>& data, int& width, int& height);

	vk::PhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	vk::Device GetDevice() const { return *device; }
	vk::PipelineCache GetPipelineCache() const { return *pipelineCache; }
	vk::RenderPass GetRenderPass() const { return *renderPass; }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return *commandBuffers[GetCurrentImageIndex()]; }
	vk::DescriptorPool GetDescriptorPool() const { return *descriptorPool; }
	vk::Extent2D GetViewPort() const { return { (u32)settings.display.width, (u32)settings.display.height }; }
	vk::SwapchainKHR GetSwapChain() const { return *swapChain; }
	u32 GetSwapChainSize() const { return (u32)imageViews.size(); }
	int GetCurrentImageIndex() const { return currentImage; }
	void WaitIdle() const;
	bool IsRendering() const { return rendering; }
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
	std::string getDriverName() override {
		return driverName;
	}
	std::string getDriverVersion() override {
		return driverVersion;
	}
	bool isAMD() override {
		return vendorID == VENDOR_ATI || vendorID == VENDOR_AMD;
	}
	vk::Format GetDepthFormat() const { return depthFormat; }
	static VulkanContext *Instance() { return contextInstance; }
	bool SupportsSamplerAnisotropy() const { return samplerAnisotropy; }
	float GetMaxSamplerAnisotropy() const { return samplerAnisotropy ? maxSamplerAnisotropy : 1.f; }
	bool SupportsDedicatedAllocation() const { return dedicatedAllocationSupported; }
	const VMAllocator& GetAllocator() const { return allocator; }
	vk::DeviceSize GetMaxMemoryAllocationSize() const { return maxMemoryAllocationSize; }
	u32 GetVendorID() const { return vendorID; }
	vk::CommandBuffer PrepareOverlay(bool vmu, bool crosshair);
	void DrawOverlay(float scaling, bool vmu, bool crosshair);
	void SubmitCommandBuffers(const std::vector<vk::CommandBuffer> &buffers, vk::Fence fence) {
		graphicsQueue.submit(
				vk::SubmitInfo(nullptr, nullptr, buffers), fence);
	}
	bool hasPerPixel() override { return fragmentStoresAndAtomics; }
	bool hasProvokingVertex() { return provokingVertexSupported; }
	bool recreateSwapChainIfNeeded();
	void addToFlight(Deletable *object) override {
		inFlightObjects[GetCurrentImageIndex()].emplace_back(object);
	}

#ifdef VK_DEBUG
	template<typename HandleType, typename = std::enable_if_t<vk::isVulkanHandleType<HandleType>::value>>
	void setObjectName(const HandleType& object, const std::string& name)
	{
		vk::DebugUtilsObjectNameInfoEXT nameInfo {};
		nameInfo.objectType = HandleType::objectType;
		nameInfo.objectHandle = reinterpret_cast<uint64_t>(static_cast<typename HandleType::NativeType>(object));
		nameInfo.pObjectName = name.c_str();
		if (device) {
			vk::Result e = device->setDebugUtilsObjectNameEXT(&nameInfo);
			(void)e;
		}
	}
#endif
	constexpr static int VENDOR_AMD = 0x1022;
	// AMD GPU products use the ATI vendor Id
	constexpr static int VENDOR_ATI = 0x1002;
	constexpr static int VENDOR_ARM = 0x13B5;
	constexpr static int VENDOR_INTEL = 0x8086;
	constexpr static int VENDOR_NVIDIA = 0x10DE;
	constexpr static int VENDOR_QUALCOMM = 0x5143;
	constexpr static int VENDOR_MESA = 0x10005;

private:
	void CreateSwapChain();
	bool InitDevice();
	bool InitInstance(const char** extensions, uint32_t extensions_count);
	void InitImgui();
	void DoSwapAutomation();
	void DrawFrame(vk::ImageView imageView, const vk::Extent2D& extent, float aspectRatio);
	vk::SurfaceKHR GetSurface() const { return *surface; }

	bool HasSurfaceDimensionChanged() const;
	void SetWindowSize(u32 width, u32 height);

	VMAllocator allocator;
	bool rendering = false;
	bool renderDone = false;
	u32 width = 0;
	u32 height = 0;
	bool resized = false;
	bool swapOnVSync = true;
	vk::UniqueInstance instance;
	vk::PhysicalDevice physicalDevice;

	u32 graphicsQueueIndex = 0;
	u32 presentQueueIndex = 0;
	vk::DeviceSize uniformBufferAlignment = 0;
	vk::DeviceSize storageBufferAlignment = 0;
	vk::DeviceSize maxMemoryAllocationSize = 0xFFFFFFFFu;
	bool optimalTilingSupported565 = false;
	bool optimalTilingSupported1555 = false;
	bool optimalTilingSupported4444 = false;
	bool fragmentStoresAndAtomics = false;
	bool samplerAnisotropy = false;
	float maxSamplerAnisotropy = 0.f;
	bool dedicatedAllocationSupported = false;
	bool provokingVertexSupported = false;
	u32 vendorID = 0;
	int swapInterval = 1;
	vk::UniqueDevice device;

	vk::UniqueSurfaceKHR surface;

	vk::UniqueSwapchainKHR swapChain;
	std::vector<vk::UniqueImageView> imageViews;
	u32 currentImage = 0;
	vk::Format presentFormat = vk::Format::eUndefined;

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

	std::unique_ptr<QuadPipeline> quadPipelineWithAlpha;
	std::unique_ptr<QuadPipeline> quadPipeline;
	std::unique_ptr<QuadPipeline> quadRotatePipeline;
	std::unique_ptr<QuadDrawer> quadDrawer;
	std::unique_ptr<QuadDrawer> quadRotateDrawer;
	std::unique_ptr<ShaderManager> shaderManager;

	vk::ImageView lastFrameView;
	vk::Extent2D lastFrameExtent;
	float lastFrameAR = 0.f;

	std::unique_ptr<VulkanOverlay> overlay;
	std::vector<std::vector<std::unique_ptr<Deletable>>> inFlightObjects;

	std::string driverName;
	std::string driverVersion;

#ifdef VK_DEBUG
#ifndef __ANDROID__
	vk::UniqueDebugUtilsMessengerEXT debugUtilsMessenger;
#else
	vk::UniqueDebugReportCallbackEXT debugReportCallback;
#endif
#endif
	static VulkanContext *contextInstance;
};
#endif // !LIBRETRO

static inline vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice)
{
	const vk::Format depthFormats[] = { vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint, vk::Format::eD16UnormS8Uint };
	vk::ImageTiling tiling;
	vk::Format depthFormat = vk::Format::eUndefined;
	for (size_t i = 0; i < std::size(depthFormats); i++)
	{
		vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(depthFormats[i]);

		if (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
		{
			tiling = vk::ImageTiling::eOptimal;
			depthFormat = depthFormats[i];
			break;
		}
	}
	if (depthFormat == vk::Format::eUndefined)
	{
		// Try to find a linear format
		for (size_t i = 0; i < std::size(depthFormats); i++)
		{
			vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(depthFormats[i]);

			if (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment)
			{
				tiling = vk::ImageTiling::eLinear;
				depthFormat = depthFormats[i];
				break;
			}
		}
		if (depthFormat == vk::Format::eUndefined) {
			ERROR_LOG(RENDERER, "No supported depth/stencil format found");
			return depthFormat;
		}
	}
	NOTICE_LOG(RENDERER, "Using depth format %s tiling %s", vk::to_string(depthFormat).c_str(), vk::to_string(tiling).c_str());

	return depthFormat;
}

#endif // USE_VULKAN
