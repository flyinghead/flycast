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
#include "libretro_vulkan.h"
#include "wsi/context.h"

static vk::Format findDepthFormat(vk::PhysicalDevice physicalDevice);

class VulkanContext : public GraphicsContext
{
public:
	VulkanContext() { verify(contextInstance == nullptr); contextInstance = this; }
	~VulkanContext() { verify(contextInstance == this); contextInstance = nullptr; }

	bool init(retro_hw_render_interface_vulkan *render_if);
	void term() override;

	u32 GetGraphicsQueueFamilyIndex() const { return retro_render_if->queue_index; }
	void PresentFrame(vk::Image image, vk::ImageView imageView, const vk::Extent2D& extent);

	vk::PhysicalDevice GetPhysicalDevice() const { return physicalDevice; }
	vk::Device GetDevice() const { return device; }
	vk::PipelineCache GetPipelineCache() const { return *pipelineCache; }
	vk::DescriptorPool GetDescriptorPool() const { return *descriptorPool; }
	size_t GetSwapChainSize() const { u32 m = retro_render_if->get_sync_index_mask(retro_render_if->handle); u32 n = 1; while (m >>= 1) n++; return n; }
	int GetCurrentImageIndex() const { return retro_render_if->get_sync_index(retro_render_if->handle); }
	// FIXME that's not quite correct
	void WaitIdle() const { retro_render_if->wait_sync_index(retro_render_if->handle); }
	void SubmitCommandBuffers(u32 bufferCount, vk::CommandBuffer *buffers, vk::Fence fence) {
		retro_render_if->lock_queue(retro_render_if->handle);
		queue.submit(vk::SubmitInfo(0, nullptr, nullptr, bufferCount, buffers, 0, nullptr), fence);
		retro_render_if->unlock_queue(retro_render_if->handle);
	}
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
	std::string getDriverName() override { vk::PhysicalDeviceProperties props; physicalDevice.getProperties(&props); return props.deviceName; }
	std::string getDriverVersion() override {
		vk::PhysicalDeviceProperties props;
		physicalDevice.getProperties(&props);

		return std::to_string(VK_VERSION_MAJOR(props.driverVersion))
			+ "." + std::to_string(VK_VERSION_MINOR(props.driverVersion))
			+ "." + std::to_string(VK_VERSION_PATCH(props.driverVersion));
	}
	vk::Format GetColorFormat() const { return colorFormat; }
	vk::Format GetDepthFormat() const { return depthFormat; }
	static VulkanContext *Instance() { return contextInstance; }
	bool SupportsSamplerAnisotropy() const { return samplerAnisotropy; }
	bool SupportsDedicatedAllocation() const { return dedicatedAllocationSupported; }
	const VMAllocator& GetAllocator() const { return allocator; }
	vk::DeviceSize GetMaxMemoryAllocationSize() const { return maxMemoryAllocationSize; }
	f32 GetMaxSamplerAnisotropy() const { return samplerAnisotropy ? maxSamplerAnisotropy : 1.f; }
	u32 GetVendorID() const { return vendorID; }

private:
	VMAllocator allocator;

	vk::DeviceSize uniformBufferAlignment = 0;
	vk::DeviceSize storageBufferAlignment = 0;
	u32 maxStorageBufferRange = 0;
	vk::DeviceSize maxMemoryAllocationSize = 0;
	bool optimalTilingSupported565 = false;
	bool optimalTilingSupported1555 = false;
	bool optimalTilingSupported4444 = false;
public:
	bool samplerAnisotropy = false;
	f32 maxSamplerAnisotropy = 0.f;
	bool dedicatedAllocationSupported = false;
private:
	u32 vendorID = 0;
	vk::Format colorFormat = vk::Format::eR8G8B8A8Unorm;

	vk::UniqueDescriptorPool descriptorPool;

	vk::Format depthFormat = vk::Format::eUndefined;

	vk::UniquePipelineCache pipelineCache;

	retro_hw_render_interface_vulkan *retro_render_if = nullptr;
	vk::Instance instance;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	vk::Queue queue;

	retro_vulkan_image retro_image;

	static VulkanContext *contextInstance;
};

const VkApplicationInfo* VkGetApplicationInfo();
bool VkCreateDevice(retro_vulkan_context* context, VkInstance instance, VkPhysicalDevice gpu,
                         VkSurfaceKHR surface, PFN_vkGetInstanceProcAddr get_instance_proc_addr,
                         const char** required_device_extensions,
                         unsigned num_required_device_extensions,
                         const char** required_device_layers, unsigned num_required_device_layers,
                         const VkPhysicalDeviceFeatures* required_features);
