/*
 *  Created on: Oct 3, 2019

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
#include "buffer.h"
#include "utils.h"
#include "vulkan_context.h"

BufferData::BufferData(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags propertyFlags)
	: bufferSize(size), m_usage(usage), m_propertyFlags(propertyFlags)
{
	VulkanContext *context = VulkanContext::Instance();
	buffer = context->GetDevice().createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), size, usage));
	VmaAllocationCreateInfo allocInfo = {
			VMA_ALLOCATION_CREATE_MAPPED_BIT,
			(propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal) ? VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY : VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU
	};
	allocation = context->GetAllocator().AllocateForBuffer(*buffer, allocInfo);
}
