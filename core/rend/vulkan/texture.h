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
#include "vulkan_context.h"
#include "buffer.h"
#include "rend/TexCache.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

void setImageLayout(vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, u32 mipmapLevels, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

class Texture final : public BaseTextureCacheData
{
public:
	Texture(TSP tsp = {}, TCW tcw = {}) : BaseTextureCacheData(tsp, tcw) {
		this->physicalDevice = VulkanContext::Instance()->GetPhysicalDevice();
		this->device = VulkanContext::Instance()->GetDevice();
	}
	Texture(Texture&& other) : BaseTextureCacheData(std::move(other)) {
		std::swap(format, other.format);
		std::swap(extent, other.extent);
		std::swap(mipmapLevels, other.mipmapLevels);
		std::swap(needsStaging, other.needsStaging);
		std::swap(stagingBufferData, other.stagingBufferData);
		std::swap(commandBuffer, other.commandBuffer);
		std::swap(allocation, other.allocation);
		std::swap(image, other.image);
		std::swap(imageView, other.imageView);
		std::swap(readOnlyImageView, other.readOnlyImageView);
		std::swap(physicalDevice, other.physicalDevice);
		std::swap(device, other.device);
	}

	void UploadToGPU(int width, int height, const u8 *data, bool mipmapped, bool mipmapsIncluded = false) override;
	u64 GetIntId() { return (u64)reinterpret_cast<uintptr_t>(this); }
	std::string GetId() override { char s[20]; sprintf(s, "%p", this); return s; }
	vk::ImageView GetImageView() const { return *imageView; }
	vk::Image GetImage() const { return *image; }
	vk::ImageView GetReadOnlyImageView() const { return readOnlyImageView ? readOnlyImageView : *imageView; }
	void SetCommandBuffer(vk::CommandBuffer commandBuffer) { this->commandBuffer = commandBuffer; }
	bool Force32BitTexture(TextureType type) const override { return !VulkanContext::Instance()->IsFormatSupported(type); }
	vk::Extent2D getSize() const { return extent; }
	void deferDeleteResource(FlightManager *manager);

private:
	void Init(u32 width, u32 height, vk::Format format ,u32 dataSize, bool mipmapped, bool mipmapsIncluded);
	void SetImage(u32 size, const void *data, bool isNew, bool genMipmaps);
	void CreateImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout initialLayout,
			vk::ImageAspectFlags aspectMask);
	void GenerateMipmaps();

	vk::Format format = vk::Format::eUndefined;
	vk::Extent2D extent;
	u32 mipmapLevels = 1;
	vk::ImageUsageFlags usageFlags;
	bool needsStaging = false;
	std::unique_ptr<BufferData> stagingBufferData;
	vk::CommandBuffer commandBuffer;

	Allocation allocation;
	vk::UniqueImage image;
	vk::UniqueImageView imageView;
	vk::ImageView readOnlyImageView;

	vk::PhysicalDevice physicalDevice;
	vk::Device device;

	friend class TextureDrawer;
	friend class OITTextureDrawer;
	friend class TextureCache;
};

class SamplerManager
{
public:
	void term() {
		samplers.clear();
	}

	vk::Sampler GetSampler(const PolyParam& poly, bool punchThrough, bool texture1 = false)
	{
		TSP tsp = texture1 ? poly.tsp1 : poly.tsp;
		if (poly.texture != nullptr && poly.texture->gpuPalette)
			tsp.FilterMode = 0;
		else if (config::TextureFiltering == 1)
			tsp.FilterMode = 0;
		else if (config::TextureFiltering == 2)
			tsp.FilterMode = 1;
		return GetSampler(tsp, punchThrough);
	}

	vk::Sampler GetSampler(TSP tsp, bool punchThrough = false)
	{
		const u32 samplerHash = (tsp.full & TSP_Mask) | punchThrough;	// MipMapD, FilterMode, ClampU, ClampV, FlipU, FlipV
		const auto& it = samplers.find(samplerHash);
		if (it != samplers.end())
			return it->second.get();
		vk::Filter filter = tsp.FilterMode == 0 ? vk::Filter::eNearest : vk::Filter::eLinear;
		const vk::SamplerAddressMode uRepeat = tsp.ClampU ? vk::SamplerAddressMode::eClampToEdge
				: tsp.FlipU ? vk::SamplerAddressMode::eMirroredRepeat : vk::SamplerAddressMode::eRepeat;
		const vk::SamplerAddressMode vRepeat = tsp.ClampV ? vk::SamplerAddressMode::eClampToEdge
				: tsp.FlipV ? vk::SamplerAddressMode::eMirroredRepeat : vk::SamplerAddressMode::eRepeat;

		// The W-axis is unused for 2D textures
		// Try to keep all three of the wrapping-modes the same by just repeating vRepeat for wRepeat
		// BestPractices-Arm-vkCreateSampler-different-wrapping-modes
		const vk::SamplerAddressMode wRepeat = vRepeat;

		const bool anisotropicFiltering = config::AnisotropicFiltering > 1 && VulkanContext::Instance()->SupportsSamplerAnisotropy()
				&& filter == vk::Filter::eLinear && !punchThrough;
#ifndef __APPLE__
		const float mipLodBias = D_Adjust_LoD_Bias[tsp.MipMapD] - 1.f;
#else
		// not supported by metal
		const float mipLodBias = 0;
#endif
		const vk::SamplerMipmapMode mipmapMode = !punchThrough && filter == vk::Filter::eLinear ? vk::SamplerMipmapMode::eLinear : vk::SamplerMipmapMode::eNearest;
		return samplers.emplace(
					std::make_pair(samplerHash, VulkanContext::Instance()->GetDevice().createSamplerUnique(
						vk::SamplerCreateInfo(vk::SamplerCreateFlags(), filter, filter,
							mipmapMode, uRepeat, vRepeat, wRepeat, mipLodBias,
							anisotropicFiltering, std::min((float)config::AnisotropicFiltering, VulkanContext::Instance()->GetMaxSamplerAnisotropy()),
							false, vk::CompareOp::eNever,
							0.0f, vk::LodClampNone, vk::BorderColor::eFloatOpaqueBlack)))).first->second.get();
	}
	static const u32 TSP_Mask = 0x7ef00;

private:
	std::map<u32, vk::UniqueSampler> samplers;
};

class FramebufferAttachment
{
public:
	FramebufferAttachment(vk::PhysicalDevice physicalDevice, vk::Device device)
		: format(vk::Format::eUndefined), physicalDevice(physicalDevice), device(device)
		{}
	void Init(u32 width, u32 height, vk::Format format, const vk::ImageUsageFlags& usage, const std::string& name = "");
	void Reset() { image.reset(); imageView.reset(); }

	vk::ImageView GetImageView() const { return *imageView; }
	vk::Image GetImage() const { return *image; }
	const BufferData* GetBufferData() const { return stagingBufferData.get(); }
	vk::ImageView GetStencilView() const { return *stencilView; }
	vk::Extent2D getExtent() const { return extent; }

private:
	vk::Format format;
	vk::Extent2D extent;

	std::unique_ptr<BufferData> stagingBufferData;
	Allocation allocation;
	vk::UniqueImage image;
	vk::UniqueImageView imageView;
	vk::UniqueImageView stencilView;

	vk::PhysicalDevice physicalDevice;
	vk::Device device;
};

class TextureCache final : public BaseTextureCache<Texture>
{
public:
	TextureCache() {
		Texture::SetDirectXColorOrder(false);
	}
	void SetCurrentIndex(int index)
	{
		if (index == (int)currentIndex)
			return;
		if (currentIndex < inFlightTextures.size())
			std::for_each(inFlightTextures[currentIndex].begin(), inFlightTextures[currentIndex].end(),
				[](Texture *texture) { texture->readOnlyImageView = vk::ImageView(); });
		currentIndex = index;
		EmptyTrash(inFlightTextures);
	}

	// Checks whether a given texture is in use by a previous frame, including the current one if previous is false
	bool IsInFlight(Texture *texture, bool previous)
	{
		for (u32 i = 0; i < inFlightTextures.size(); i++)
			if ((!previous || i != currentIndex)
					&& inFlightTextures[i].find(texture) != inFlightTextures[i].end())
				return true;
		return false;
	}

	void SetInFlight(Texture *texture)
	{
		inFlightTextures[currentIndex].insert(texture);
	}

	void Cleanup();

	void Clear()
	{
		VulkanContext *context = VulkanContext::Instance();
		for (auto& set : inFlightTextures)
		{
			for (Texture *tex : set)
				tex->deferDeleteResource(context);
			set.clear();
		}
		BaseTextureCache::Clear();
	}

private:
	bool clearTexture(Texture *tex)
	{
		for (auto& set : inFlightTextures)
			set.erase(tex);

		return tex->Delete();
	}

	template<typename T>
	void EmptyTrash(T& v)
	{
		if (v.size() < currentIndex + 1)
			v.resize(currentIndex + 1);
		else
			v[currentIndex].clear();
	}

	std::vector<std::unordered_set<Texture *>> inFlightTextures;
	u32 currentIndex = ~0;
};
