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
#include <memory>
#include "vulkan.h"
#include "buffer.h"
#include "rend/TexCache.h"

struct Texture : BaseTextureCacheData
{
	Texture(vk::PhysicalDevice physicalDevice, vk::Device device)
		: physicalDevice(physicalDevice), device(device), format(vk::Format::eR8G8B8A8Unorm)
		{}
	void UploadToGPU(int width, int height, u8 *data) override;
	u64 GetIntId() { return (u64)reinterpret_cast<uintptr_t>(this); }
	std::string GetId() override { char s[20]; sprintf(s, "%p", this); return s; }
	bool IsNew() const { return !image.get(); }
	vk::DescriptorImageInfo GetDescriptorImageInfo(TSP tsp)
	{
		const auto& it = samplers.find(tsp.full & 0x7e000);
		vk::Sampler sampler;
		if (it != samplers.end())
			sampler = it->second.get();
		else
		{
			vk::Filter filter = tsp.FilterMode == 0 ? vk::Filter::eNearest : vk::Filter::eLinear;
			vk::SamplerAddressMode uRepeat = tsp.ClampU ? vk::SamplerAddressMode::eClampToEdge
					: tsp.FlipU ? vk::SamplerAddressMode::eMirroredRepeat : vk::SamplerAddressMode::eRepeat;
			vk::SamplerAddressMode vRepeat = tsp.ClampV ? vk::SamplerAddressMode::eClampToEdge
					: tsp.FlipV ? vk::SamplerAddressMode::eMirroredRepeat : vk::SamplerAddressMode::eRepeat;
			samplers[tsp.full & 0x7e000]
				= device.createSamplerUnique(vk::SamplerCreateInfo(vk::SamplerCreateFlags(), filter, filter, vk::SamplerMipmapMode::eLinear,
						uRepeat, vRepeat, vk::SamplerAddressMode::eClampToEdge, 0.0f, false,
					16.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f, vk::BorderColor::eFloatOpaqueBlack));
			sampler = *samplers[tsp.full & 0x7e000];
		}
		return vk::DescriptorImageInfo(sampler, *imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
	}

private:
	void Init(u32 width, u32 height, vk::Format format);
	void SetImage(vk::CommandBuffer const& commandBuffer, u32 size, void *data);
	void CreateImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout initialLayout,
			vk::MemoryPropertyFlags memoryProperties, vk::ImageAspectFlags aspectMask);

	vk::Format                  format;
	vk::Extent2D                extent;
	bool                        needsStaging = false;
	std::unique_ptr<BufferData> stagingBufferData;
	std::map<u32, vk::UniqueSampler> samplers;

	vk::UniqueImage image;
	vk::UniqueDeviceMemory deviceMemory;
	vk::UniqueImageView imageView;
	vk::PhysicalDevice physicalDevice;
	vk::Device device;
};
