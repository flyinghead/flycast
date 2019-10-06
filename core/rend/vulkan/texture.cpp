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

static void setImageLayout(vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout)
{
	vk::AccessFlags sourceAccessMask;
	switch (oldImageLayout)
	{
	case vk::ImageLayout::eTransferDstOptimal:
		sourceAccessMask = vk::AccessFlagBits::eTransferWrite;
		break;
	case vk::ImageLayout::ePreinitialized:
		sourceAccessMask = vk::AccessFlagBits::eHostWrite;
		break;
	case vk::ImageLayout::eGeneral:     // sourceAccessMask is empty
	case vk::ImageLayout::eUndefined:
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
		sourceStage = vk::PipelineStageFlagBits::eTransfer;
		break;
	case vk::ImageLayout::eUndefined:
		sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
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
	default:
		verify(false);
		break;
	}

	vk::ImageAspectFlags aspectMask;
	if (newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal)
	{
		aspectMask = vk::ImageAspectFlagBits::eDepth;
		if (format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint)
		{
			aspectMask |= vk::ImageAspectFlagBits::eStencil;
		}
	}
	else
	{
		aspectMask = vk::ImageAspectFlagBits::eColor;
	}

	vk::ImageSubresourceRange imageSubresourceRange(aspectMask, 0, 1, 0, 1);
	vk::ImageMemoryBarrier imageMemoryBarrier(sourceAccessMask, destinationAccessMask, oldImageLayout, newImageLayout, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, image, imageSubresourceRange);
	commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier);
}

void Texture::UploadToGPU(int width, int height, u8 *data)
{
	vk::Format format;
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
	Init(width, height, format);
	SetImage(VulkanContext::Instance()->GetCurrentCommandPool(), dataSize, data);
}

void Texture::Init(u32 width, u32 height, vk::Format format)
{
	this->extent = vk::Extent2D(width, height);
	this->format = format;
	vk::PhysicalDeviceMemoryProperties memoryProperties = physicalDevice.getMemoryProperties();
	vk::FormatProperties formatProperties = physicalDevice.getFormatProperties(format);

	vk::FormatFeatureFlags formatFeatureFlags = vk::FormatFeatureFlagBits::eSampledImage;
	// Forcing staging since it fixes texture glitches
	needsStaging = true; //(formatProperties.linearTilingFeatures & formatFeatureFlags) != formatFeatureFlags;
	vk::ImageTiling imageTiling;
	vk::ImageLayout initialLayout;
	vk::MemoryPropertyFlags requirements;
	vk::ImageUsageFlags usageFlags = vk::ImageUsageFlagBits::eSampled;
	if (needsStaging)
	{
		verify((formatProperties.optimalTilingFeatures & formatFeatureFlags) == formatFeatureFlags);
		stagingBufferData = std::unique_ptr<BufferData>(new BufferData(physicalDevice, device, extent.width * extent.height * 4, vk::BufferUsageFlagBits::eTransferSrc));
		imageTiling = vk::ImageTiling::eOptimal;
		usageFlags |= vk::ImageUsageFlagBits::eTransferDst;
		initialLayout = vk::ImageLayout::eUndefined;
	}
	else
	{
		imageTiling = vk::ImageTiling::eLinear;
		initialLayout = vk::ImageLayout::ePreinitialized;
		requirements = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
	}
	CreateImage(imageTiling, usageFlags | vk::ImageUsageFlagBits::eSampled, initialLayout, requirements,
			vk::ImageAspectFlagBits::eColor);
}

void Texture::CreateImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout initialLayout,
		vk::MemoryPropertyFlags memoryProperties, vk::ImageAspectFlags aspectMask)
{
	vk::ImageCreateInfo imageCreateInfo(vk::ImageCreateFlags(), vk::ImageType::e2D, format, vk::Extent3D(extent, 1), 1, 1,
										vk::SampleCountFlagBits::e1, tiling, usage,
										vk::SharingMode::eExclusive, 0, nullptr, initialLayout);
	image = device.createImageUnique(imageCreateInfo);

	vk::MemoryRequirements memReq = device.getImageMemoryRequirements(image.get());
	u32 memoryTypeIndex = findMemoryType(physicalDevice.getMemoryProperties(), memReq.memoryTypeBits, memoryProperties);
	deviceMemory = device.allocateMemoryUnique(vk::MemoryAllocateInfo(memReq.size, memoryTypeIndex));

	device.bindImageMemory(image.get(), deviceMemory.get(), 0);

	vk::ImageViewCreateInfo imageViewCreateInfo(vk::ImageViewCreateFlags(), image.get(), vk::ImageViewType::e2D, format, vk::ComponentMapping(),
			vk::ImageSubresourceRange(aspectMask, 0, 1, 0, 1));
	imageView = device.createImageViewUnique(imageViewCreateInfo);
}

void Texture::SetImage(const vk::CommandPool& commandPool, u32 srcSize, void *srcData)
{
	vk::UniqueCommandBuffer commandBuffer = std::move(device.allocateCommandBuffersUnique(
			vk::CommandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, 1)).front());
	commandBuffer->begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	vk::DeviceSize size = needsStaging
			? device.getBufferMemoryRequirements(stagingBufferData->buffer.get()).size
					: device.getImageMemoryRequirements(image.get()).size;
	void* data = needsStaging
			? device.mapMemory(stagingBufferData->deviceMemory.get(), 0, size)
					: device.mapMemory(deviceMemory.get(), 0, size);
	memcpy(data, srcData, srcSize);
	device.unmapMemory(needsStaging ? stagingBufferData->deviceMemory.get() : deviceMemory.get());

	if (needsStaging)
	{
		// Since we're going to blit to the texture image, set its layout to eTransferDstOptimal
		setImageLayout(*commandBuffer, image.get(), format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		vk::BufferImageCopy copyRegion(0, extent.width, extent.height, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0), vk::Extent3D(extent, 1));
		commandBuffer->copyBufferToImage(stagingBufferData->buffer.get(), image.get(), vk::ImageLayout::eTransferDstOptimal, copyRegion);
		// Set the layout for the texture image from eTransferDstOptimal to SHADER_READ_ONLY
		setImageLayout(*commandBuffer, image.get(), format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	else
	{
		// If we can use the linear tiled image as a texture, just do it
		setImageLayout(*commandBuffer, image.get(), format, vk::ImageLayout::ePreinitialized, vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	commandBuffer->end();
	VulkanContext::Instance()->GetGraphicsQueue().submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &(*commandBuffer)), nullptr);

	// FIXME we need to wait for the command buffer to finish executing before freeing the staging and command buffers
	VulkanContext::Instance()->GetGraphicsQueue().waitIdle();
	if (needsStaging)
	{
		// Free staging buffer
		stagingBufferData = nullptr;
	}
}
