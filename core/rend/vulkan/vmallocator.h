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
#pragma once
#include <cinttypes>
#include "vulkan.h"
#ifndef _MSC_VER
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include "vk_mem_alloc.h"
#ifndef _MSC_VER
#pragma GCC diagnostic pop
#endif

#if !defined(PRIu64) && defined(_WIN32)
#define PRIu64 "I64u"
#endif

class VMAllocator;

class Allocation
{
public:
	Allocation() = default;
	Allocation(const Allocation& other) = delete;
	Allocation& operator=(const Allocation& other) = delete;

	Allocation(Allocation&& other) : allocator(other.allocator), allocation(other.allocation),
			allocInfo(other.allocInfo) {
		other.allocator = VK_NULL_HANDLE;
		other.allocation = VK_NULL_HANDLE;
	}

	Allocation& operator=(Allocation&& other) {
		std::swap(this->allocator, other.allocator);
		std::swap(this->allocation, other.allocation);
		std::swap(this->allocInfo, other.allocInfo);
		return *this;
	}

	~Allocation() {
		if (allocator != VK_NULL_HANDLE)
			vmaFreeMemory(allocator, allocation);
	}
	bool IsHostVisible() const {
		VkMemoryPropertyFlags flags;
		vmaGetMemoryTypeProperties(allocator, allocInfo.memoryType, &flags);
		return flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}
	void *MapMemory() const
	{
		if (allocInfo.pMappedData != nullptr)
			return allocInfo.pMappedData;
		void *p;
		VkResult res = vmaMapMemory(allocator, allocation, &p);
		vk::resultCheck(static_cast<vk::Result>(res), "vmaMapMemory failed");
		VkMemoryPropertyFlags flags;
		vmaGetMemoryTypeProperties(allocator, allocInfo.memoryType, &flags);
		if ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) && (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			vmaInvalidateAllocation(allocator, allocation, allocInfo.offset, allocInfo.size);
		return p;
	}
	void UnmapMemory() const
	{
		if (allocInfo.pMappedData != nullptr)
			return;
		VkMemoryPropertyFlags flags;
		vmaGetMemoryTypeProperties(allocator, allocInfo.memoryType, &flags);
		if ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) && (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			vmaFlushAllocation(allocator, allocation, allocInfo.offset, allocInfo.size);
		vmaUnmapMemory(allocator, allocation);
	}

private:
	Allocation(VmaAllocator allocator, VmaAllocation allocation, VmaAllocationInfo allocInfo)
		: allocator(allocator), allocation(allocation), allocInfo(allocInfo)
	{
	}

	VmaAllocator allocator = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo allocInfo;

	friend class VMAllocator;
};

class VMAllocator
{
public:
	void Init(vk::PhysicalDevice physicalDevice, vk::Device device, vk::Instance instance);

	void Term()
	{
		if (allocator != VK_NULL_HANDLE)
		{
			vmaDestroyAllocator(allocator);
			allocator = VK_NULL_HANDLE;
		}
	}

	Allocation AllocateMemory(const vk::MemoryRequirements& memoryRequirements, const VmaAllocationCreateInfo& allocCreateInfo) const
	{
		VmaAllocation vmaAllocation;
		VmaAllocationInfo allocInfo;
		VkResult rc = vmaAllocateMemory(allocator, (VkMemoryRequirements*)&memoryRequirements, &allocCreateInfo, &vmaAllocation, &allocInfo);
		vk::resultCheck(static_cast<vk::Result>(rc), "vmaAllocateMemory failed");
		return Allocation(allocator, vmaAllocation, allocInfo);
	}

	Allocation AllocateForImage(const vk::Image image, const VmaAllocationCreateInfo& allocCreateInfo) const
	{
		VmaAllocation vmaAllocation;
		VmaAllocationInfo allocInfo;
		VkResult rc = vmaAllocateMemoryForImage(allocator, (VkImage)image, &allocCreateInfo, &vmaAllocation, &allocInfo);
		vk::resultCheck(static_cast<vk::Result>(rc), "vmaAllocateMemoryForImage failed");
		vmaBindImageMemory(allocator, vmaAllocation, (VkImage)image);

		return Allocation(allocator, vmaAllocation, allocInfo);
	}

	Allocation AllocateForBuffer(const vk::Buffer buffer, const VmaAllocationCreateInfo& allocCreateInfo) const
	{
		VmaAllocation vmaAllocation;
		VmaAllocationInfo allocInfo;
		VkResult rc = vmaAllocateMemoryForBuffer(allocator, (VkBuffer)buffer, &allocCreateInfo, &vmaAllocation, &allocInfo);
		vk::resultCheck(static_cast<vk::Result>(rc), "vmaAllocateMemoryForBuffer failed");
		vmaBindBufferMemory(allocator, vmaAllocation, (VkBuffer)buffer);

		return Allocation(allocator, vmaAllocation, allocInfo);
	}

private:
	VmaAllocator allocator = VK_NULL_HANDLE;
};
