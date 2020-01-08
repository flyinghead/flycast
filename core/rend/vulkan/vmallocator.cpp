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

#if HOST_CPU == CPU_ARM
__attribute__((pcs("aapcs-vfp")))
#endif
static void vmaAllocateDeviceMemoryCallback(
    VmaAllocator      allocator,
    uint32_t          memoryType,
    VkDeviceMemory    memory,
    VkDeviceSize      size)
{
	DEBUG_LOG(RENDERER, "VMAAllocator: %llu bytes allocated (type %d)", (unsigned long long)size, memoryType);
}

#if HOST_CPU == CPU_ARM
__attribute__((pcs("aapcs-vfp")))
#endif
static void vmaFreeDeviceMemoryCallback(
    VmaAllocator      allocator,
    uint32_t          memoryType,
    VkDeviceMemory    memory,
    VkDeviceSize      size)
{
	DEBUG_LOG(RENDERER, "VMAAllocator: %llu bytes freed (type %d)", (unsigned long long)size, memoryType);
}

static const VmaDeviceMemoryCallbacks memoryCallbacks = { vmaAllocateDeviceMemoryCallback, vmaFreeDeviceMemoryCallback };

void VMAllocator::Init(vk::PhysicalDevice physicalDevice, vk::Device device)
{
	verify(allocator == VK_NULL_HANDLE);
	VmaAllocatorCreateInfo allocatorInfo = { VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT };
	if (VulkanContext::Instance()->SupportsDedicatedAllocation())
		allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

	allocatorInfo.physicalDevice = (VkPhysicalDevice)physicalDevice;
	allocatorInfo.device = (VkDevice)device;
#if !defined(RELEASE) || defined(DEBUGFAST)
	allocatorInfo.pDeviceMemoryCallbacks = &memoryCallbacks;
#endif
	VkResult rc = vmaCreateAllocator(&allocatorInfo, &allocator);
	if (rc != VK_SUCCESS)
		throwResultException((vk::Result)rc, "vmaCreateAllocator failed");
}
