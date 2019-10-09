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

void setImageLayout(vk::CommandBuffer const& commandBuffer, vk::Image image, vk::Format format, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout);

struct Texture : BaseTextureCacheData
{
	Texture(vk::PhysicalDevice physicalDevice, vk::Device device)
		: physicalDevice(physicalDevice), device(device), format(vk::Format::eUndefined)
		{}
	void UploadToGPU(int width, int height, u8 *data) override;
	u64 GetIntId() { return (u64)reinterpret_cast<uintptr_t>(this); }
	std::string GetId() override { char s[20]; sprintf(s, "%p", this); return s; }
	bool IsNew() const { return !image.get(); }
	vk::ImageView GetImageView() const { return *imageView; }
	void SetCommandBuffer(vk::CommandBuffer commandBuffer) { this->commandBuffer = commandBuffer; }

private:
	void Init(u32 width, u32 height, vk::Format format);
	void SetImage(u32 size, void *data, bool isNew);
	void CreateImage(vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::ImageLayout initialLayout,
			vk::MemoryPropertyFlags memoryProperties, vk::ImageAspectFlags aspectMask);

	vk::Format                  format;
	vk::Extent2D                extent;
	bool                        needsStaging = false;
	std::unique_ptr<BufferData> stagingBufferData;
	vk::CommandBuffer commandBuffer;

	vk::UniqueDeviceMemory deviceMemory;
	vk::UniqueImageView imageView;
	vk::UniqueImage image;

	vk::PhysicalDevice physicalDevice;
	vk::Device device;

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
					std::make_pair(samplerHash, VulkanContext::Instance()->GetDevice()->createSamplerUnique(
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

class RenderToTexture
{
public:
	RenderToTexture() : colorAttachment(VulkanContext::Instance()->GetPhysicalDevice(), *VulkanContext::Instance()->GetDevice()),
		depthAttachment(VulkanContext::Instance()->GetPhysicalDevice(), *VulkanContext::Instance()->GetDevice())
	{}

	void PrepareRender(vk::RenderPass rttRenderPass)
	{
		DEBUG_LOG(RENDERER, "RenderToTexture packmode=%d stride=%d - %d,%d -> %d,%d", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
				FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		u32 newWidth = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
		u32 newHeight = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
		int fbh = 2;
		while (fbh < newHeight)
			fbh *= 2;
		int fbw = 2;
		while (fbw < newWidth)
			fbw *= 2;

		if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
		{
			newWidth *= settings.rend.RenderToTextureUpscale;
			newHeight *= settings.rend.RenderToTextureUpscale;
			fbw *= settings.rend.RenderToTextureUpscale;
			fbh *= settings.rend.RenderToTextureUpscale;
		}

		VulkanContext *context = VulkanContext::Instance();
		if (newWidth != this->width || newHeight != this->height)
		{
			width = newWidth;
			height = newHeight;
			colorAttachment.Init(width, height, vk::Format::eR8G8B8A8Unorm);
			depthAttachment.Init(width, height, vk::Format::eD32SfloatS8Uint);
			vk::ImageView imageViews[] = {
				*colorAttachment.GetImageView(),
				*depthAttachment.GetImageView(),
			};
			framebuffer = context->GetDevice()->createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(),
					rttRenderPass, ARRAY_SIZE(imageViews), imageViews, newWidth, newHeight, 1));
		}
		const vk::ClearValue clear_colors[] = { vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}), vk::ClearDepthStencilValue{ 0.f, 0 } };
		VulkanContext::Instance()->GetCurrentCommandBuffer().beginRenderPass(vk::RenderPassBeginInfo(rttRenderPass, *framebuffer,
				vk::Rect2D({0, 0}, {width, height}), 2, clear_colors), vk::SubpassContents::eInline);
		VulkanContext::Instance()->GetCurrentCommandBuffer().setViewport(0, vk::Viewport(0.0f, 0.0f, (float)width, (float)height, 1.0f, 0.0f));
		// FIXME if the texture exists, need to transition it first
	}

	void EndRender()
	{
		VulkanContext::Instance()->GetCurrentCommandBuffer().endRenderPass();
		u32 w = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
		u32 h = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;

		u32 stride = FB_W_LINESTRIDE.stride * 8;
		if (stride == 0)
			stride = w * 2;
		else if (w * 2 > stride) {
			// Happens for Virtua Tennis
			w = stride / 2;
		}
		u32 size = w * h * 2;

		const u8 fb_packmode = FB_W_CTRL.fb_packmode;

		if (settings.rend.RenderToTextureBuffer)
		{
			// wait! need to synchronize buddy!
			// FIXME wtf? need a different cmd buffer if writing back to vram PITA
			u32 tex_addr = FB_W_SOF1 & VRAM_MASK;

			// Remove all vram locks before calling glReadPixels
			// (deadlock on rpi)
			u32 page_tex_addr = tex_addr & PAGE_MASK;
			u32 page_size = size + tex_addr - page_tex_addr;
			page_size = ((page_size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;
			for (u32 page = page_tex_addr; page < page_tex_addr + page_size; page += PAGE_SIZE)
				VramLockedWriteOffset(page);

			die("Not implemented");
		}
		else
		{
			//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);
		}

		if (w > 1024 || h > 1024 || settings.rend.RenderToTextureBuffer) {
			// TODO glcache.DeleteTextures(1, &gl.rtt.tex);
		}
		else
		{
			// TexAddr : fb_rtt.TexAddr, Reserved : 0, StrideSel : 0, ScanOrder : 1
			TCW tcw = { { (FB_W_SOF1 & VRAM_MASK) >> 3, 0, 0, 1 } };
			switch (fb_packmode) {
			case 0:
			case 3:
				tcw.PixelFmt = Pixel1555;
				break;
			case 1:
				tcw.PixelFmt = Pixel565;
				break;
			case 2:
				tcw.PixelFmt = Pixel4444;
				break;
			}
			TSP tsp = { 0 };
			for (tsp.TexU = 0; tsp.TexU <= 7 && (8 << tsp.TexU) < w; tsp.TexU++);
			for (tsp.TexV = 0; tsp.TexV <= 7 && (8 << tsp.TexV) < h; tsp.TexV++);

			Texture* texture = static_cast<Texture*>(getTextureCacheData(tsp, tcw, [](){
				return (BaseTextureCacheData *)new Texture(VulkanContext::Instance()->GetPhysicalDevice(), *VulkanContext::Instance()->GetDevice());
			}));
			if (texture->IsNew())
				texture->Create();
			// TODO replace tex vk:: stuff
			texture->dirty = 0;
			if (texture->lock_block == NULL)
				texture->lock_block = libCore_vramlock_Lock(texture->sa_tex, texture->sa + texture->size - 1, texture);
		}
	}

private:
	u32 width = 0;
	u32 height = 0;

	vk::UniqueFramebuffer framebuffer;
	FramebufferAttachment colorAttachment;
	FramebufferAttachment depthAttachment;
};
