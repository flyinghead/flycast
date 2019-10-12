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

BufferData::BufferData(vk::PhysicalDevice const& physicalDevice, vk::Device const& device, vk::DeviceSize size,
		vk::BufferUsageFlags usage, Allocator *allocator, vk::MemoryPropertyFlags propertyFlags)
	: device(device), bufferSize(size), allocator(allocator)
#if !defined(NDEBUG)
					, m_usage(usage), m_propertyFlags(propertyFlags)
#endif
{
	buffer = device.createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), size, usage));
	vk::MemoryRequirements memoryRequirements = device.getBufferMemoryRequirements(buffer.get());
	memoryType = findMemoryType(physicalDevice.getMemoryProperties(), memoryRequirements.memoryTypeBits, propertyFlags);
	offset = allocator->Allocate(memoryRequirements.size, memoryRequirements.alignment, memoryType, sharedDeviceMemory);
	device.bindBufferMemory(buffer.get(), sharedDeviceMemory, offset);
}
