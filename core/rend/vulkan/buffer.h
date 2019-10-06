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
#pragma once
#include "vulkan.h"

struct BufferData
{
	BufferData(vk::PhysicalDevice const& physicalDevice, vk::Device const& device, vk::DeviceSize size, vk::BufferUsageFlags usage,
			vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

	void upload(vk::Device const& device, u32 size, void *data) const
	{
		verify((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
		verify(size <= m_size);

		void* dataPtr = device.mapMemory(*this->deviceMemory, 0, size);
		memcpy(dataPtr, data, size);
		device.unmapMemory(*this->deviceMemory);
	}

	vk::UniqueDeviceMemory  deviceMemory;
	vk::UniqueBuffer        buffer;
	vk::DeviceSize          m_size;

#if !defined(NDEBUG)
private:
	vk::BufferUsageFlags    m_usage;
	vk::MemoryPropertyFlags m_propertyFlags;
#endif
};
