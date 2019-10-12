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

static inline u32 findMemoryType(vk::PhysicalDeviceMemoryProperties const& memoryProperties, u32 typeBits, vk::MemoryPropertyFlags requirementsMask)
{
	u32 typeIndex = u32(~0);
	for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) && (memoryProperties.memoryTypes[i].propertyFlags & requirementsMask) == requirementsMask)
		{
			typeIndex = i;
			break;
		}
		typeBits >>= 1;
	}
	verify(typeIndex != ~0);
	return typeIndex;
}

static inline vk::UniqueDeviceMemory allocateMemory(vk::Device const& device, vk::PhysicalDeviceMemoryProperties const& memoryProperties,
		vk::MemoryRequirements const& memoryRequirements,
		vk::MemoryPropertyFlags memoryPropertyFlags)
{
	u32 memoryTypeIndex = findMemoryType(memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags);

	return device.allocateMemoryUnique(vk::MemoryAllocateInfo(memoryRequirements.size, memoryTypeIndex));
}
