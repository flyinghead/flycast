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
#include "texture.h"

#include <algorithm>
#include <memory>

void setImageLayout(vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, u32 mipmapLevels, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
	static const float scopeColor[4] = { 0.75f, 0.75f, 0.0f, 1.0f };
	CommandBufferDebugScope _(commandBuffer, "setImageLayout", scopeColor);

	vk::AccessFlags sourceAccessMask;
	switch (oldImageLayout)
	{
	case vk::ImageLayout::eTransferDstOptimal:
		sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;
	case vk::ImageLayout::eTransferSrcOptimal:
		sourceAccessMask = vk::AccessFlagBits::eTransferRead;
		break;
	case vk::ImageLayout::ePreinitialized:
		sourceAccessMask = vk::AccessFlagBits::eHostWrite;
		break;
	case vk::ImageLayout::eGeneral:     // sourceAccessMask is empty
	case vk::ImageLayout::eUndefined:
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		sourceAccessMask = vk::AccessFlagBits::eShaderRead;
		break;
	default:
		verify(false);
		break;
	}

	vk::PipelineStageFlags sourceStage;
	switch (oldImageLayout)
	{
	case vk::ImageLayout::eGeneral:
	case vk::ImageLayout::ePreinitialized:
		sourceStage = vk::PipelineStageFlagBits::eHost;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
	case vk::ImageLayout::eTransferSrcOptimal:
		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		break;
	case vk::ImageLayout::eUndefined:
		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		sourceStage = vk::PipelineStageFlagBits::eFragmentShader;
		break;
	default:
		verify(false);
		break;
	}

	vk::AccessFlags destinationAccessMask;
	switch (newImageLayout)
	{
	case vk::ImageLayout::eColorAttachmentOptimal:
		destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
		break;
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
		break;
	case vk::ImageLayout::eGeneral:   // empty destinationAccessMask
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		destinationAccessMask = vk::AccessFlagBits::eShaderRead;
		break;
	case vk::ImageLayout::eTransferSrcOptimal:
		destinationAccessMask = vk::AccessFlagBits::eTransferRead;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
		destinationAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;
	case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
		destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead;
		break;
	default:
		verify(false);
		break;
	}

	vk::PipelineStageFlags destinationStage;
	switch (newImageLayout)
	{
	case vk::ImageLayout::eColorAttachmentOptimal:
		destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
		break;
	case vk::ImageLayout::eDepthStencilAttachmentOptimal:
		destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
		break;
	case vk::ImageLayout::eGeneral:
		destinationStage = vk::PipelineStageFlagBits::eHost;
		break;
	case vk::ImageLayout::eShaderReadOnlyOptimal:
		destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		break;
	case vk::ImageLayout::eTransferDstOptimal:
	case vk::ImageLayout::eTransferSrcOptimal:
		destinationStage = vk::PipelineStageFlagBits::eTransfer;
		break;
	case vk::ImageLayout::eDepthStencilReadOnlyOptimal:
		destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
		break;
	default:
		verify(false);
		break;
	}

	vk::ImageAspectFlags aspectMask;
	if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal || newImageLayout == vk::ImageLayout::eDepthStencilReadOnlyOptimal)
	{
		aspectMask = vk::ImageAspectFlagBits::eDepth;
		if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint || format == vk::Format::eD16UnormS8Uint)
		{
			aspectMask |= vk::ImageAspectFlagBits::eStencil;
		}
	}
	else
	{
		aspectMask = vk::ImageAspectFlagBits::eColor;
	}

	vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, mipmapLevels, 0, 1);
	vk::ImageMemoryBarrier imageMemoryBarrier(sourceAccessMask, destinationAccessMask, oldImageLayout, newImageLayout, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored, image, imageSubresourceRange);
	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}

void Texture::UploadToGPU(int width, int height, const u8 *data, bool mipmapped, bool mipmapsIncluded)
{
	if (customTextureResource && image)
	{
		verify(flightManager != nullptr);
		deferDeleteResource(flightManager);
		customTextureResource = false;
	}
	vk::Format format = vk::Format::eUndefined;
	u32 dataSize = width * height * 2;
	switch (tex_type)
	{
	case TextureType::_5551:
		format = vk::Format::eR5G5B5A1UnormPack16;
		break;
	case TextureType::_565:
		format = vk::Format::eR5G6B5UnormPack16;
		break;
	case TextureType::_4444:
		format = vk::Format::eR4G4B4A4UnormPack16;
		break;
	case TextureType::_8888:
		format = vk::Format::eR8G8B8A8Unorm;
		dataSize *= 2;
		break;
	case TextureType::_8:
		format = vk::Format::eR8Unorm;
		dataSize /= 2;
		break;
	}
	if (mipmapsIncluded)
	{
		int w = width / 2;
		u32 size = dataSize / 4;
		while (w)
		{
			dataSize += ((size + 3) >> 2) << 2;		// offset must be a multiple of 4
			size /= 4;
			w /= 2;
		}
	}
	bool isNew = true;
	if (width != (int)extent.width || height != (int)extent.height
			|| format != this->format || !this->image)
		Init(width, height, format, dataSize, mipmapped, mipmapsIncluded);
	else
		isNew = false;
	SetImage(dataSize, data, isNew, mipmapped && !mipmapsIncluded);
}

namespace
{
vk::Format customVkFormat(NativeTextureFormat format)
{
	switch (format)
	{
	case NativeTextureFormat::Rgba8Unorm: return vk::Format::eR8G8B8A8Unorm;
	case NativeTextureFormat::Bc7Unorm: return vk::Format::eBc7UnormBlock;
	case NativeTextureFormat::Bc7Srgb: return vk::Format::eBc7SrgbBlock;
	case NativeTextureFormat::Bc3Unorm: return vk::Format::eBc3UnormBlock;
	case NativeTextureFormat::Etc2Rgba8Unorm: return vk::Format::eEtc2R8G8B8A8UnormBlock;
	case NativeTextureFormat::Astc4x4Unorm: return vk::Format::eAstc4x4UnormBlock;
	case NativeTextureFormat::Astc5x4Unorm: return vk::Format::eAstc5x4UnormBlock;
	case NativeTextureFormat::Astc5x5Unorm: return vk::Format::eAstc5x5UnormBlock;
	case NativeTextureFormat::Astc6x5Unorm: return vk::Format::eAstc6x5UnormBlock;
	case NativeTextureFormat::Astc6x6Unorm: return vk::Format::eAstc6x6UnormBlock;
	case NativeTextureFormat::Astc8x5Unorm: return vk::Format::eAstc8x5UnormBlock;
	case NativeTextureFormat::Astc8x6Unorm: return vk::Format::eAstc8x6UnormBlock;
	case NativeTextureFormat::Astc10x5Unorm: return vk::Format::eAstc10x5UnormBlock;
	case NativeTextureFormat::Astc10x6Unorm: return vk::Format::eAstc10x6UnormBlock;
	case NativeTextureFormat::Astc8x8Unorm: return vk::Format::eAstc8x8UnormBlock;
	case NativeTextureFormat::Astc10x8Unorm: return vk::Format::eAstc10x8UnormBlock;
	case NativeTextureFormat::Astc10x10Unorm: return vk::Format::eAstc10x10UnormBlock;
	case NativeTextureFormat::Astc12x10Unorm: return vk::Format::eAstc12x10UnormBlock;
	case NativeTextureFormat::Astc12x12Unorm: return vk::Format::eAstc12x12UnormBlock;
	case NativeTextureFormat::Count: break;
	}
	return vk::Format::eUndefined;
}
}

bool Texture::UploadCustomTexture(const PreparedCustomTexture& customTexture, bool mipmapped)
{
	std::string validationError;
	if (!(bool)commandBuffer || !validatePreparedCustomTexture(customTexture, validationError)
			|| customTexture.bytes.size() > UINT32_MAX)
		return false;
	const vk::Format newFormat = customVkFormat(customTexture.nativeFormat);
	if (newFormat == vk::Format::eUndefined || (image && flightManager == nullptr))
		return false;
	const bool generateMipmaps = mipmapped && customTexture.generateMipmaps;
	try
	{
		auto newStaging = std::make_unique<BufferData>(customTexture.bytes.size(),
				vk::BufferUsageFlagBits::eTransferSrc);
		newStaging->upload(static_cast<u32>(customTexture.bytes.size()), customTexture.bytes.data());
		Allocation newAllocation;
		const vk::Extent2D newExtent(customTexture.width, customTexture.height);
		const u32 sourceMipLevels = static_cast<u32>(customTexture.levels.size());
		const u32 newMipLevels = generateMipmaps
				? mipmapLevelCount(customTexture.width, customTexture.height)
				: sourceMipLevels;
		vk::ImageUsageFlags newUsageFlags = vk::ImageUsageFlagBits::eSampled
				| vk::ImageUsageFlagBits::eTransferDst;
		if (generateMipmaps)
			newUsageFlags |= vk::ImageUsageFlagBits::eTransferSrc;
		vk::ImageCreateInfo imageInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, newFormat,
				vk::Extent3D(newExtent, 1), newMipLevels, 1, vk::SampleCountFlagBits::e1,
				vk::ImageTiling::eOptimal, newUsageFlags,
				vk::SharingMode::eExclusive, nullptr, vk::ImageLayout::eUndefined);
		vk::UniqueImage newImage = device.createImageUnique(imageInfo);
		VmaAllocationCreateInfo allocInfo = { VmaAllocationCreateFlags(), VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY };
		newAllocation = VulkanContext::Instance()->GetAllocator().AllocateForImage(*newImage, allocInfo);
		vk::ImageViewCreateInfo viewInfo(vk::ImageViewCreateFlags(), newImage.get(), vk::ImageViewType::e2D,
				newFormat, vk::ComponentMapping(),
				vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, newMipLevels, 0, 1));
		vk::UniqueImageView newView = device.createImageViewUnique(viewInfo);
		std::vector<vk::BufferImageCopy> regions;
		regions.reserve(customTexture.levels.size());
		for (u32 levelIndex = 0; levelIndex < sourceMipLevels; ++levelIndex)
		{
			const PreparedMipLevel& level = customTexture.levels[levelIndex];
			regions.emplace_back(level.byteOffset, 0, 0,
					vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, levelIndex, 0, 1),
					vk::Offset3D(), vk::Extent3D(level.width, level.height, 1));
		}
		// Finish every potentially allocating operation before recording commands
		// that reference the new resource. The command buffer cannot be rolled back
		// if an allocation throws after this point.
		if (image)
			deferDeleteResource(flightManager);
		setImageLayout(commandBuffer, newImage.get(), newFormat, newMipLevels,
				vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		commandBuffer.copyBufferToImage(newStaging->buffer.get(), newImage.get(),
				vk::ImageLayout::eTransferDstOptimal, regions);
		if (generateMipmaps)
			GenerateMipmaps(newImage.get(), newExtent, newMipLevels, true);
		else
			setImageLayout(commandBuffer, newImage.get(), newFormat, newMipLevels,
					vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

		format = newFormat;
		extent = newExtent;
		mipmapLevels = newMipLevels;
		usageFlags = newUsageFlags;
		needsStaging = true;
		stagingBufferData = std::move(newStaging);
		allocation = std::move(newAllocation);
		image = std::move(newImage);
		imageView = std::move(newView);
		readOnlyImageView = vk::ImageView();
		customTextureResource = true;
		return true;
	}
	catch (const std::exception& exception)
	{
		WARN_LOG(RENDERER, "Vulkan custom texture upload failed: %s", exception.what());
		return false;
	}
}

CustomTextureCapabilities Texture::GetCustomTextureCapabilities()
{
	VulkanContext *context = VulkanContext::Instance();
	const vk::PhysicalDevice physicalDevice = context->GetPhysicalDevice();
	const u32 maxDimension = physicalDevice.getProperties().limits.maxImageDimension2D;
	CustomTextureCapabilities capabilities = CustomTextureCapabilities::rgbaOnly(
			CustomTextureBackend::Vulkan, maxDimension);
	const auto query = [physicalDevice](vk::Format format) {
		const vk::FormatFeatureFlags features = physicalDevice.getFormatProperties(format).optimalTilingFeatures;
		return (features & vk::FormatFeatureFlagBits::eSampledImage)
				&& (features & vk::FormatFeatureFlagBits::eTransferDst);
	};
	for (NativeTextureFormat format : { NativeTextureFormat::Bc7Unorm, NativeTextureFormat::Bc7Srgb,
			NativeTextureFormat::Bc3Unorm, NativeTextureFormat::Etc2Rgba8Unorm,
			NativeTextureFormat::Astc4x4Unorm, NativeTextureFormat::Astc5x4Unorm,
			NativeTextureFormat::Astc5x5Unorm, NativeTextureFormat::Astc6x5Unorm,
			NativeTextureFormat::Astc6x6Unorm, NativeTextureFormat::Astc8x5Unorm,
			NativeTextureFormat::Astc8x6Unorm, NativeTextureFormat::Astc10x5Unorm,
			NativeTextureFormat::Astc10x6Unorm, NativeTextureFormat::Astc8x8Unorm,
			NativeTextureFormat::Astc10x8Unorm, NativeTextureFormat::Astc10x10Unorm,
			NativeTextureFormat::Astc12x10Unorm, NativeTextureFormat::Astc12x12Unorm })
		capabilities.setSupported(format, query(customVkFormat(format)));
	return capabilities;
}

void Texture::Init(u32 width, u32 height, vk::Format format, u32 dataSize, bool mipmapped, bool mipmapsIncluded)
{
	this->extent = vk::Extent2D(width, height);
	this->format = format;
	mipmapLevels = 1;
	if (mipmapped)
		mipmapLevels += floor(log2(std::max(width, height)));

	vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(format);

	vk::ImageTiling imageTiling = (formatProperties.optimalTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage)
			== vk::FormatFeatureFlagBits::eSampledImage
			? vk::ImageTiling::eOptimal
			: vk::ImageTiling::eLinear;
#ifndef __APPLE__
	// Texture corruption with moltenvk. Perf improvement on other platforms
	if (height <= 32
			&& dataSize / height <= 64
			&& !mipmapped
			&& (formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) == vk::FormatFeatureFlagBits::eSampledImage)
		imageTiling = vk::ImageTiling::eLinear;
#endif
	needsStaging = imageTiling != vk::ImageTiling::eLinear;
	vk::ImageLayout initialLayout;
	vk::ImageUsageFlags usageFlags = vk::ImageUsageFlagBits::eSampled;
	if (needsStaging)
	{
		stagingBufferData = std::make_unique<BufferData>(dataSize, vk::BufferUsageFlagBits::eTransferSrc);
		usageFlags |= vk::ImageUsageFlagBits::eTransferDst;
		initialLayout = vk::ImageLayout::eUndefined;
	}
	else
	{
		verify((formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage) == vk::FormatFeatureFlagBits::eSampledImage);
		initialLayout = vk::ImageLayout::ePreinitialized;
	}
	if (mipmapped && !mipmapsIncluded)
		usageFlags |= vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst;
	CreateImage(imageTiling, usageFlags, initialLayout, vk::ImageAspectFlagBits::eColor);
}

void Texture::CreateImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout initialLayout,
		vk::ImageAspectFlags aspectMask)
{
	this->usageFlags = usage;
	vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, format, vk::Extent3D(extent, 1), mipmapLevels, 1,
										vk::SampleCountFlagBits::e1, tiling, usage,
										vk::SharingMode::eExclusive, nullptr, initialLayout);
	image = device.createImageUnique(imageCreateInfo);

	VmaAllocationCreateInfo allocCreateInfo = { VmaAllocationCreateFlags(), needsStaging ? VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY : VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU };
#ifndef __APPLE__
	if (!needsStaging)
		allocCreateInfo.flags = VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_MAPPED_BIT;
#endif
	allocation = VulkanContext::Instance()->GetAllocator().AllocateForImage(*image, allocCreateInfo);

	vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image.get(), vk::ImageViewType::e2D, format, vk::ComponentMapping(),
			vk::ImageSubresourceRange(aspectMask, 0, mipmapLevels, 0, 1));
	imageView = device.createImageViewUnique(imageViewCreateInfo);
#ifdef VK_DEBUG
	char name[128];
	snprintf(name, sizeof(name), "texture @ %x", startAddress);
	VulkanContext::Instance()->setObjectName(image.get(), name);
	VulkanContext::Instance()->setObjectName(imageView.get(), name);
#endif
}

void Texture::SetImage(u32 srcSize, const void *srcData, bool isNew, bool genMipmaps)
{
	verify((bool)commandBuffer);

	static const float scopeColor[4] = { 1.0f, 1.0f, 0.0f, 1.0f };
	CommandBufferDebugScope _(commandBuffer, "SetImage", scopeColor);

	if (!isNew && !needsStaging)
		setImageLayout(commandBuffer, image.get(), format, mipmapLevels, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral);

	void* data;
	if (needsStaging)
	{
		if (!stagingBufferData)
			// This can happen if a texture is first created for RTT, then later updated
			stagingBufferData = std::make_unique<BufferData>(srcSize, vk::BufferUsageFlagBits::eTransferSrc);
		data = stagingBufferData->MapMemory();
	}
	else
		data = allocation.MapMemory();
	verify(data != nullptr);

	if (mipmapLevels > 1 && !genMipmaps && tex_type != TextureType::_8888)
	{
		// Each mipmap level must start at a 4-byte boundary
		u8 *src = (u8 *)srcData;
		u8 *dst = (u8 *)data;
		for (u32 i = 0; i < mipmapLevels; i++)
		{
			const u32 size = (1 << (2 * i)) * 2;
			memcpy(dst, src, size);
			dst += ((size + 3) >> 2) << 2;
			src += size;
		}
	}
	else if (!needsStaging)
	{
		vk::SubresourceLayout layout = device.getImageSubresourceLayout(*image, vk::ImageSubresource(vk::ImageAspectFlagBits::eColor));
		if (layout.size != srcSize)
		{
			u8 *src = (u8 *)srcData;
			u8 *dst = (u8 *)data;
			u32 srcSz = extent.width * 2;
			if (tex_type == TextureType::_8888)
				srcSz *= 2;
			else if (tex_type == TextureType::_8)
				srcSz /= 2;
			u8 * const srcEnd = src + srcSz * extent.height;
			for (; src < srcEnd; src += srcSz, dst += layout.rowPitch)
				memcpy(dst, src, srcSz);
		}
		else
			memcpy(data, srcData, srcSize);
		allocation.UnmapMemory();
	}
	else
		memcpy(data, srcData, srcSize);

	if (needsStaging)
	{
		stagingBufferData->UnmapMemory();
		// Since we're going to blit to the texture image, set its layout to eTransferDstOptimal
		setImageLayout(commandBuffer, image.get(), format, mipmapLevels, isNew ? vk::ImageLayout::eUndefined : vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageLayout::eTransferDstOptimal);

		if (mipmapLevels > 1 && !genMipmaps)
		{
			vk::DeviceSize bufferOffset = 0;
			for (u32 i = 0; i < mipmapLevels; i++)
			{
				vk::BufferImageCopy copyRegion(bufferOffset, 1 << i, 1 << i, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, mipmapLevels - i - 1, 0, 1),
						vk::Offset3D(0, 0, 0), vk::Extent3D(1 << i, 1 << i, 1));
				commandBuffer.copyBufferToImage(stagingBufferData->buffer.get(), image.get(), vk::ImageLayout::eTransferDstOptimal, copyRegion);
				const u32 size = (1 << (2 * i)) * (tex_type == TextureType::_8888 ? 4 : 2);
				bufferOffset += ((size + 3) >> 2) << 2;
			}
		}
		else
		{
			vk::BufferImageCopy copyRegion(0, extent.width, extent.height, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1),
					vk::Offset3D(0, 0, 0), vk::Extent3D(extent, 1));
			commandBuffer.copyBufferToImage(stagingBufferData->buffer.get(), image.get(), vk::ImageLayout::eTransferDstOptimal, copyRegion);
			if (mipmapLevels > 1)
				GenerateMipmaps(image.get(), extent, mipmapLevels, true);
		}
		// Set the layout for the texture image from eTransferDstOptimal to SHADER_READ_ONLY
		if (mipmapLevels <= 1 || !genMipmaps)
			setImageLayout(commandBuffer, image.get(), format, mipmapLevels,
					vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	else
	{
		if (mipmapLevels > 1)
			GenerateMipmaps(image.get(), extent, mipmapLevels, false);
		else
			// If we can use the linear tiled image as a texture, just do it
			setImageLayout(commandBuffer, image.get(), format, mipmapLevels, isNew ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eGeneral,
					vk::ImageLayout::eShaderReadOnlyOptimal);
	}
}

void Texture::GenerateMipmaps(vk::Image image, vk::Extent2D extent, u32 mipmapLevels,
		bool usesStagingBuffer)
{
	static const float scopeColor[4] = { 0.75f, 0.75f, 0.0f, 1.0f };
	CommandBufferDebugScope _(commandBuffer, "GenerateMipmaps", scopeColor);

	u32 mipWidth = extent.width;
	u32 mipHeight = extent.height;
	vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, vk::QueueFamilyIgnored, vk::QueueFamilyIgnored,
			image, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

	for (u32 i = 1; i < mipmapLevels; i++)
	{
		// Transition previous mipmap level from dst optimal/preinit to src optimal
		barrier.subresourceRange.baseMipLevel = i - 1;
		if (i == 1 && !usesStagingBuffer)
		{
			barrier.oldLayout = vk::ImageLayout::ePreinitialized;
			barrier.srcAccessMask = vk::AccessFlagBits::eHostWrite;
		}
		else
		{
			barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
		}
		barrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eTransfer, {}, nullptr, nullptr, barrier);

		// Blit previous mipmap level on current
		vk::ImageBlit blit(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i - 1, 0, 1),
				 { { vk::Offset3D(0, 0, 0), vk::Offset3D(mipWidth, mipHeight, 1) } },
				 vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, i, 0, 1),
				 { { vk::Offset3D(0, 0, 0), vk::Offset3D(std::max(mipWidth / 2, 1u), std::max(mipHeight / 2, 1u), 1) } });
		commandBuffer.blitImage(image, vk::ImageLayout::eTransferSrcOptimal,
				image, vk::ImageLayout::eTransferDstOptimal, blit, vk::Filter::eLinear);

		// Transition previous mipmap level from src optimal to shader read-only optimal
		barrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
		barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		barrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
		barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
		commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier);

		mipWidth = std::max(mipWidth / 2, 1u);
		mipHeight = std::max(mipHeight / 2, 1u);
	}
	// Transition last mipmap level from dst optimal to shader read-only optimal
	barrier.subresourceRange.baseMipLevel = mipmapLevels - 1;
	barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
	barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader, {}, nullptr, nullptr, barrier);
}

void Texture::deferDeleteResource(FlightManager *manager)
{
	class ResourceDeleter : public Deletable
	{
	public:
		ResourceDeleter(Texture *texture)
		{
			std::swap(image, texture->image);
			std::swap(imageView, texture->imageView);
			std::swap(bufferData, texture->stagingBufferData);
			std::swap(allocation, texture->allocation);
		}

	private:
		vk::UniqueImage image;
		vk::UniqueImageView imageView;
		std::unique_ptr<BufferData> bufferData;
		Allocation allocation;
	};
	manager->addToFlight(new ResourceDeleter(this));
}

void FramebufferAttachment::Init(u32 width, u32 height, vk::Format format, const vk::ImageUsageFlags& usage, const std::string& name)
{
	this->format = format;
	this->extent = vk::Extent2D { width, height };
	bool depth = format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint || format == vk::Format::eD16UnormS8Uint;

	if (usage & vk::ImageUsageFlagBits::eTransferSrc)
	{
		stagingBufferData = std::make_unique<BufferData>(width * height * 4,
				vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached | vk::MemoryPropertyFlagBits::eHostCoherent);
	}
	vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, format, vk::Extent3D(extent, 1), 1, 1, vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal, usage,
			vk::SharingMode::eExclusive, nullptr, vk::ImageLayout::eUndefined);
	image = device.createImageUnique(imageCreateInfo);
#ifdef VK_DEBUG
	if (!name.empty())
		VulkanContext::Instance()->setObjectName(image.get(), name);
#endif

	VmaAllocationCreateInfo allocCreateInfo = { VmaAllocationCreateFlags(), VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY };
	if (usage & vk::ImageUsageFlagBits::eTransientAttachment)
		allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	allocation = VulkanContext::Instance()->GetAllocator().AllocateForImage(*image, allocCreateInfo);

	if ((usage & vk::ImageUsageFlagBits::eColorAttachment) || (usage & vk::ImageUsageFlagBits::eDepthStencilAttachment))
	{
		vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image.get(), vk::ImageViewType::e2D,
				format, vk::ComponentMapping(),	vk::ImageSubresourceRange(depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
		imageView = device.createImageViewUnique(imageViewCreateInfo);
#ifdef VK_DEBUG
		if (!name.empty())
			VulkanContext::Instance()->setObjectName(imageView.get(), name);
#endif

		if ((usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) && (usage & vk::ImageUsageFlagBits::eInputAttachment))
		{
			// Also create an imageView for the stencil
			imageViewCreateInfo.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1);
			stencilView = device.createImageViewUnique(imageViewCreateInfo);
#ifdef VK_DEBUG
			if (!name.empty())
				VulkanContext::Instance()->setObjectName(stencilView.get(), name);
#endif
		}
	}
}

void TextureCache::Cleanup()
{
	std::vector<u64> list;

	u32 TargetFrame = std::max((u32)120, FrameCount) - 120;

	for (const auto& [id, texture] : cache)
	{
		if (texture.dirty && texture.dirty < TargetFrame)
			list.push_back(id);

		if (list.size() > 5)
			break;
	}

	for (u64 id : list)
	{
		if (clearTexture(&cache[id]))
			cache.erase(id);
	}
}
