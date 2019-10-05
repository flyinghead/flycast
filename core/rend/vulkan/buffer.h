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

	template <typename DataType>
	void upload(vk::Device const& device, DataType const& data) const
	{
		assert((m_propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent) && (m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
		assert(sizeof(DataType) <= m_size);

		void* dataPtr = device.mapMemory(*this->deviceMemory, 0, sizeof(DataType));
		memcpy(dataPtr, &data, sizeof(DataType));
		device.unmapMemory(*this->deviceMemory);
	}

	template <typename DataType>
	void upload(vk::Device const& device, std::vector<DataType> const& data, size_t stride = 0) const
	{
		assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible);

		size_t elementSize = stride ? stride : sizeof(DataType);
		assert(sizeof(DataType) <= elementSize);

		copyToDevice(device, deviceMemory, data.data(), data.size(), elementSize);
	}

	template <typename DataType>
	void upload(vk::PhysicalDevice const& physicalDevice, vk::Device const& device, vk::CommandPool const& commandPool, vk::Queue queue, std::vector<DataType> const& data,
			size_t stride) const
	{
		assert(m_usage & vk::BufferUsageFlagBits::eTransferDst);
		assert(m_propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal);

		size_t elementSize = stride ? stride : sizeof(DataType);
		assert(sizeof(DataType) <= elementSize);

		size_t dataSize = data.size() * elementSize;
		assert(dataSize <= m_size);

		BufferData stagingBuffer(physicalDevice, device, dataSize, vk::BufferUsageFlagBits::eTransferSrc);
		copyToDevice(device, stagingBuffer.deviceMemory, data.data(), data.size(), elementSize);

		oneTimeSubmit(device, commandPool, queue,
				[&](vk::CommandBuffer const& commandBuffer) { commandBuffer.copyBuffer(*stagingBuffer.buffer, *this->buffer, vk::BufferCopy(0, 0, dataSize)); });
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
