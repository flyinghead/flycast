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
#include "rend/shader_util.h"
#include "hw/pvr/pvr_regs.h"

enum class ModVolMode { Xor, Or, Inclusion, Exclusion, Final };

static const vk::CompareOp depthOps[] =
{
	vk::CompareOp::eNever,          //0 Never
	vk::CompareOp::eLess,           //1 Less
	vk::CompareOp::eEqual,          //2 Equal
	vk::CompareOp::eLessOrEqual,    //3 Less Or Equal
	vk::CompareOp::eGreater,        //4 Greater
	vk::CompareOp::eNotEqual,       //5 Not Equal
	vk::CompareOp::eGreaterOrEqual, //6 Greater Or Equal
	vk::CompareOp::eAlways,         //7 Always
};

static inline vk::BlendFactor getBlendFactor(u32 instr, bool src)
{
	switch (instr) {
	case 0:	// zero
		return vk::BlendFactor::eZero;
	case 1: // one
		return vk::BlendFactor::eOne;
	case 2: // other color
		return src ? vk::BlendFactor::eDstColor : vk::BlendFactor::eSrcColor;
	case 3: // inverse other color
		return src ? vk::BlendFactor::eOneMinusDstColor : vk::BlendFactor::eOneMinusSrcColor;
	case 4: // src alpha
		return vk::BlendFactor::eSrcAlpha;
	case 5: // inverse src alpha
		return vk::BlendFactor::eOneMinusSrcAlpha;
	case 6: // dst alpha
		return vk::BlendFactor::eDstAlpha;
	case 7: // inverse dst alpha
		return vk::BlendFactor::eOneMinusDstAlpha;
	default:
		die("Unsupported blend instruction");
		return vk::BlendFactor::eZero;
	}
}

static inline u32 findMemoryType(vk::PhysicalDeviceMemoryProperties const& memoryProperties, u32 typeBits, const vk::MemoryPropertyFlags& requirementsMask)
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
	verify(typeIndex != u32(~0));
	return typeIndex;
}

static inline void addPointerToChain(void* head, const void* ptr)
{
	vk::BaseInStructure* prevInStructure = static_cast<vk::BaseInStructure*>(head);
	while (prevInStructure->pNext)
	{
		// Structure already in chain
		if (prevInStructure->pNext == ptr)
		{
			return;
		}
		prevInStructure = const_cast<vk::BaseInStructure*>(prevInStructure->pNext);
	}

	// Add structure to end
	prevInStructure->pNext = static_cast<const vk::BaseInStructure*>(ptr);
}

static const char GouraudSource[] = R"(
#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION
#endif
)";

class VulkanSource : public ShaderSource
{
public:
	VulkanSource() : ShaderSource("#version 430") {}
};


static inline vk::ClearColorValue getBorderColor() {
	return vk::ClearColorValue(std::array<float, 4>{ VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f });
}

static inline u32 align(vk::DeviceSize offset, u32 alignment)
{
	u32 pad = (u32)(offset & (alignment - 1));
	return pad == 0 ? 0 : alignment - pad;
}
