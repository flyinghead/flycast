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
		vk::BufferUsageFlags usage, vk::MemoryPropertyFlags propertyFlags)
	: m_size(size)
#if !defined(NDEBUG)
					, m_usage(usage), m_propertyFlags(propertyFlags)
#endif
{
	buffer = device.createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(), size, usage));
	deviceMemory = allocateMemory(device, physicalDevice.getMemoryProperties(), device.getBufferMemoryRequirements(buffer.get()), propertyFlags);
	device.bindBufferMemory(buffer.get(), deviceMemory.get(), 0);
}
