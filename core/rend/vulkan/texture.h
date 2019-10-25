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
#include "hw/pvr/Renderer_if.h"

void setImageLayout(vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, u32 mipmapLevels, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

struct Texture : BaseTextureCacheData
{
	~Texture() override
	{
		imageView.reset();
		image.reset();
		if (allocator)
			allocator->Free(memoryOffset, memoryType, sharedDeviceMemory);
	}
	void UploadToGPU(int width, int height, u8 *data) override;
	u64 GetIntId() { return (u64)reinterpret_cast<uintptr_t>(this); }
	std::string GetId() override { char s[20]; sprintf(s, "%p", this); return s; }
	bool IsNew() const { return !image.get(); }
	vk::ImageView GetImageView() const { return *imageView; }
	void SetCommandBuffer(vk::CommandBuffer commandBuffer) { this->commandBuffer = commandBuffer; }
	virtual bool Force32BitTexture(TextureType type) override { return !VulkanContext::Instance()->IsFormatSupported(type); }

	void SetAllocator(VulkanAllocator *allocator) { this->allocator = allocator; }
	void SetPhysicalDevice(vk::PhysicalDevice physicalDevice) { this->physicalDevice = physicalDevice; }
	void SetDevice(vk::Device device) { this->device = device; }

private:
	void Init(u32 width, u32 height, vk::Format format);
	void SetImage(u32 size, void *data, bool isNew);
	void CreateImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout initialLayout,
			vk::MemoryPropertyFlags memoryProperties, vk::ImageAspectFlags aspectMask);
	void GenerateMipmaps();

	vk::Format format = vk::Format::eUndefined;
	vk::Extent2D extent;
	u32 mipmapLevels = 1;
	bool needsStaging = false;
	std::unique_ptr<BufferData> stagingBufferData;
	vk::CommandBuffer commandBuffer;

	vk::UniqueDeviceMemory deviceMemory;
	vk::UniqueImageView imageView;
	vk::UniqueImage image;

	vk::PhysicalDevice physicalDevice;
	vk::Device device;
	VulkanAllocator *allocator = nullptr;
	vk::DeviceMemory sharedDeviceMemory;
	u32 memoryType = 0;
	vk::DeviceSize memoryOffset = 0;

	friend class TextureDrawer;
};

class SamplerManager
{
public:
	vk::Sampler GetSampler(TSP tsp)
	{
		u32 samplerHash = tsp.full & TSP_Mask;	// FilterMode, ClampU, ClampV, FlipU, FlipV
		const auto& it = samplers.find(samplerHash);
		vk::Sampler sampler;
		if (it != samplers.end())
			return it->second.get();
		vk::Filter filter = tsp.FilterMode == 0 ? vk::Filter::eNearest : vk::Filter::eLinear;
		vk::SamplerAddressMode uRepeat = tsp.ClampU ? vk::SamplerAddressMode::eClampToEdge
				: tsp.FlipU ? vk::SamplerAddressMode::eMirroredRepeat : vk::SamplerAddressMode::eRepeat;
		vk::SamplerAddressMode vRepeat = tsp.ClampV ? vk::SamplerAddressMode::eClampToEdge
				: tsp.FlipV ? vk::SamplerAddressMode::eMirroredRepeat : vk::SamplerAddressMode::eRepeat;

		return samplers.emplace(
					std::make_pair(samplerHash, VulkanContext::Instance()->GetDevice().createSamplerUnique(
						vk::SamplerCreateInfo(vk::SamplerCreateFlags(), filter, filter,
							vk::SamplerMipmapMode::eLinear, uRepeat, vRepeat, vk::SamplerAddressMode::eClampToEdge, 0.0f, false,
							16.0f, false, vk::CompareOp::eNever, 0.0f, 0.0f, vk::BorderColor::eFloatOpaqueBlack)))).first->second.get();
	}
	static const u32 TSP_Mask = 0x7e000;

private:
	std::map<u32, vk::UniqueSampler> samplers;
};

class FramebufferAttachment
{
public:
	FramebufferAttachment(vk::PhysicalDevice physicalDevice, vk::Device device)
		: physicalDevice(physicalDevice), device(device), format(vk::Format::eUndefined)
		{}
	void Init(u32 width, u32 height, vk::Format format);
	void Reset() { image.reset(); imageView.reset(); deviceMemory.reset(); }

	vk::UniqueImageView& GetImageView() { return imageView; }
	vk::UniqueImage& GetImage() { return image; }
	vk::UniqueDeviceMemory& GetDeviceMemory() { return deviceMemory; }
	const BufferData* GetBufferData() const { return stagingBufferData.get(); }

private:
	vk::Format format;
	vk::Extent2D extent;

	std::unique_ptr<BufferData> stagingBufferData;
	vk::UniqueDeviceMemory deviceMemory;
	vk::UniqueImageView imageView;
	vk::UniqueImage image;

	vk::PhysicalDevice physicalDevice;
	vk::Device device;
};

class TextureCache : public BaseTextureCache<Texture>
{
};
