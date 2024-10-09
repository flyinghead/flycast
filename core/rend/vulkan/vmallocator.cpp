/*
	Created on: Nov 24, 2019

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
#define VMA_IMPLEMENTATION
#include "vulkan.h"
#include "vmallocator.h"
#include "vulkan_context.h"

#if !defined(NDEBUG) || defined(DEBUGFAST)
VKAPI_ATTR static void VKAPI_CALL vmaAllocateDeviceMemoryCallback(
    VmaAllocator      allocator,
    uint32_t          memoryType,
    VkDeviceMemory    memory,
    VkDeviceSize      size,
	void *            userData)
{
	DEBUG_LOG(RENDERER, "VMAAllocator: %" PRIu64 " bytes allocated (type %d)", size, memoryType);
}

VKAPI_ATTR static void VKAPI_CALL vmaFreeDeviceMemoryCallback(
    VmaAllocator      allocator,
    uint32_t          memoryType,
    VkDeviceMemory    memory,
    VkDeviceSize      size,
	void *            userData)
{
	DEBUG_LOG(RENDERER, "VMAAllocator: %" PRIu64 " bytes freed (type %d)", size, memoryType);
}

static const VmaDeviceMemoryCallbacks memoryCallbacks = { vmaAllocateDeviceMemoryCallback, vmaFreeDeviceMemoryCallback };
#endif

void VMAllocator::Init(vk::PhysicalDevice physicalDevice, vk::Device device, vk::Instance instance)
{
	verify(allocator == VK_NULL_HANDLE);
	VmaAllocatorCreateInfo allocatorInfo = { VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT };
	if (VulkanContext::Instance()->SupportsDedicatedAllocation())
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

	allocatorInfo.physicalDevice = (VkPhysicalDevice)physicalDevice;
	// Top-out at vulkan 1.1
	allocatorInfo.vulkanApiVersion = (physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_1) ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
	allocatorInfo.device = (VkDevice)device;
	allocatorInfo.instance = (VkInstance)instance;
#if !defined(NDEBUG) || defined(DEBUGFAST)
	allocatorInfo.pDeviceMemoryCallbacks = &memoryCallbacks;
#endif

#if VMA_DYNAMIC_VULKAN_FUNCTIONS
	VmaVulkanFunctions vulkanFunctions = {};
	vulkanFunctions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
	vulkanFunctions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;
	allocatorInfo.pVulkanFunctions = &vulkanFunctions;
#endif

	VkResult rc = vmaCreateAllocator(&allocatorInfo, &allocator);
	vk::resultCheck(static_cast<vk::Result>(rc), "vmaCreateAllocator failed");
}
