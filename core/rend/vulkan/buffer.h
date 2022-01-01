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
#include "vmallocator.h"

struct BufferData
{
	BufferData(vk::DeviceSize size, const vk::BufferUsageFlags& usage,
			const vk::MemoryPropertyFlags& propertyFlags =
				vk::MemoryPropertyFlagBits::eHostVisible
#ifndef __APPLE__
				// host coherent memory not supported on apple platforms
				| vk::MemoryPropertyFlagBits::eHostCoherent
#endif
				);
	~BufferData()
	{
		buffer.reset();
	}

	void upload(u32 size, const void *data, u32 bufOffset = 0) const
	{
		verify((bool)(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
		verify(bufOffset + size <= bufferSize);

		void* dataPtr = (u8 *)allocation.MapMemory() + bufOffset;
		memcpy(dataPtr, data, size);
		allocation.UnmapMemory();
	}

	void upload(size_t count, const u32 *sizes, const void * const *data, u32 bufOffset = 0) const
	{
		verify((bool)(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));

		u32 totalSize = 0;
		for (size_t i = 0; i < count; i++)
			totalSize += sizes[i];
		verify(bufOffset + totalSize <= bufferSize);
		void* dataPtr = (u8 *)allocation.MapMemory() + bufOffset;
		for (size_t i = 0; i < count; i++)
		{
			if (data[i] != nullptr)
				memcpy(dataPtr, data[i], sizes[i]);
			dataPtr = (u8 *)dataPtr + sizes[i];
		}
		allocation.UnmapMemory();
	}

	void download(u32 size, void *data, u32 bufOffset = 0) const
	{
		verify((bool)(m_propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible));
		verify(bufOffset + size <= bufferSize);

		void* dataPtr = (u8 *)allocation.MapMemory() + bufOffset;
		memcpy(data, dataPtr, size);
		allocation.UnmapMemory();
	}

	void *MapMemory()
	{
		return allocation.MapMemory();
	}
	void UnmapMemory()
	{
		allocation.UnmapMemory();
	}

	vk::UniqueBuffer buffer;
	vk::DeviceSize bufferSize;
	Allocation allocation;

private:
	vk::BufferUsageFlags    m_usage;
	vk::MemoryPropertyFlags m_propertyFlags;
};
