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
#include "utils.h"

#include <algorithm>
#include <memory>

void setImageLayout(vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, u32 mipmapLevels, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
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
	vk::ImageMemoryBarrier imageMemoryBarrier(sourceAccessMask, destinationAccessMask, oldImageLayout, newImageLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, imageSubresourceRange);
	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}

void Texture::UploadToGPU(int width, int height, u8 *data, bool mipmapped, bool mipmapsIncluded)
{
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
	if (width != (int)extent.width || height != (int)extent.height || format != this->format)
		Init(width, height, format, dataSize, mipmapped, mipmapsIncluded);
	else
		isNew = false;
	SetImage(dataSize, data, isNew, mipmapped && !mipmapsIncluded);
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
		stagingBufferData = std::unique_ptr<BufferData>(new BufferData(dataSize, vk::BufferUsageFlagBits::eTransferSrc));
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

void Texture::CreateImage(vk::ImageTiling tiling, const vk::ImageUsageFlags& usage, vk::ImageLayout initialLayout,
		const vk::ImageAspectFlags& aspectMask)
{
	vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, format, vk::Extent3D(extent, 1), mipmapLevels, 1,
										vk::SampleCountFlagBits::e1, tiling, usage,
										vk::SharingMode::eExclusive, 0, nullptr, initialLayout);
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
}

void Texture::SetImage(u32 srcSize, void *srcData, bool isNew, bool genMipmaps)
{
	verify((bool)commandBuffer);

	if (!isNew && !needsStaging)
		setImageLayout(commandBuffer, image.get(), format, mipmapLevels, vk::ImageLayout::eShaderReadOnlyOptimal, vk::ImageLayout::eGeneral);

	void* data;
	if (needsStaging)
	{
		if (!stagingBufferData)
			// This can happen if a texture is first created for RTT, then later updated
			stagingBufferData = std::unique_ptr<BufferData>(new BufferData(srcSize, vk::BufferUsageFlagBits::eTransferSrc));
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
		vk::SubresourceLayout layout = device.getImageSubresourceLayout(*image, vk::ImageSubresource(vk::ImageAspectFlagBits::eColor, 0, 0));
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
				GenerateMipmaps();
		}
		// Set the layout for the texture image from eTransferDstOptimal to SHADER_READ_ONLY
		setImageLayout(commandBuffer, image.get(), format, mipmapLevels, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	else
	{
		if (mipmapLevels > 1)
			GenerateMipmaps();
		else
			// If we can use the linear tiled image as a texture, just do it
			setImageLayout(commandBuffer, image.get(), format, mipmapLevels, isNew ? vk::ImageLayout::ePreinitialized : vk::ImageLayout::eGeneral,
					vk::ImageLayout::eShaderReadOnlyOptimal);
	}
}

void Texture::GenerateMipmaps()
{
	u32 mipWidth = extent.width;
	u32 mipHeight = extent.height;
	vk::ImageMemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eTransferRead,
			vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eTransferSrcOptimal, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
			*image, vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

	for (u32 i = 1; i < mipmapLevels; i++)
	{
		// Transition previous mipmap level from dst optimal/preinit to src optimal
		barrier.subresourceRange.baseMipLevel = i - 1;
		if (i == 1 && !needsStaging)
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
		commandBuffer.blitImage(*image, vk::ImageLayout::eTransferSrcOptimal, *image, vk::ImageLayout::eTransferDstOptimal, 1, &blit, vk::Filter::eLinear);

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

void FramebufferAttachment::Init(u32 width, u32 height, vk::Format format, const vk::ImageUsageFlags& usage)
{
	this->format = format;
	this->extent = vk::Extent2D { width, height };
	bool depth = format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint || format == vk::Format::eD16UnormS8Uint;

	if (usage & vk::ImageUsageFlagBits::eTransferSrc)
	{
		stagingBufferData = std::unique_ptr<BufferData>(new BufferData(width * height * 4,
				vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst));
	}
	vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, format, vk::Extent3D(extent, 1), 1, 1, vk::SampleCountFlagBits::e1,
			vk::ImageTiling::eOptimal, usage,
			vk::SharingMode::eExclusive, 0, nullptr, vk::ImageLayout::eUndefined);
	image = device.createImageUnique(imageCreateInfo);

	VmaAllocationCreateInfo allocCreateInfo = { VmaAllocationCreateFlags(), VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY };
	if (usage & vk::ImageUsageFlagBits::eTransientAttachment)
		allocCreateInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	allocation = VulkanContext::Instance()->GetAllocator().AllocateForImage(*image, allocCreateInfo);

	vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image.get(), vk::ImageViewType::e2D,
			format, vk::ComponentMapping(),	vk::ImageSubresourceRange(depth ? vk::ImageAspectFlagBits::eDepth : vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));
	imageView = device.createImageViewUnique(imageViewCreateInfo);

	if ((usage & vk::ImageUsageFlagBits::eDepthStencilAttachment) && (usage & vk::ImageUsageFlagBits::eInputAttachment))
	{
		// Also create an imageView for the stencil
		imageViewCreateInfo.subresourceRange = vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eStencil, 0, 1, 0, 1);
		stencilView = device.createImageViewUnique(imageViewCreateInfo);
	}
}

void TextureCache::Cleanup()
{
	std::vector<u64> list;

	u32 TargetFrame = std::max((u32)120, FrameCount) - 120;

	for (const auto& pair : cache)
	{
		if (pair.second.dirty && pair.second.dirty < TargetFrame)
			list.push_back(pair.first);

		if (list.size() > 5)
			break;
	}

	for (u64 id : list)
	{
		if (clearTexture(&cache[id]))
			cache.erase(id);
	}
}
