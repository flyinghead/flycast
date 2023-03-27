/*
	Copyright 2020 flyinghead

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
#include "vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/ta.h"
#include "commandpool.h"
#include "pipeline.h"
#include "rend/osd.h"
#include "rend/transform_matrix.h"
#ifndef LIBRETRO
#include "rend/gui.h"
#endif

#include <memory>
#include <vector>

class BaseVulkanRenderer : public Renderer
{
protected:
	bool BaseInit(vk::RenderPass renderPass, int subpass = 0)
	{
		texCommandPool.Init();
		fbCommandPool.Init();

#if defined(__ANDROID__) && !defined(LIBRETRO)
		if (!vjoyTexture)
		{
			int w, h;
			u8 *image_data = loadOSDButtons(w, h);
			texCommandPool.BeginFrame();
			vjoyTexture = std::make_unique<Texture>();
			vjoyTexture->tex_type = TextureType::_8888;
			vk::CommandBuffer cmdBuffer = texCommandPool.Allocate();
			cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
			vjoyTexture->SetCommandBuffer(cmdBuffer);
			vjoyTexture->UploadToGPU(OSD_TEX_W, OSD_TEX_H, image_data, false);
			vjoyTexture->SetCommandBuffer(nullptr);
			cmdBuffer.end();
			texCommandPool.EndFrame();
			delete [] image_data;
			osdPipeline.Init(&shaderManager, vjoyTexture->GetImageView(), GetContext()->GetRenderPass());
		}
		if (!osdBuffer)
		{
			osdBuffer = std::make_unique<BufferData>(sizeof(OSDVertex) * VJOY_VISIBLE * 4,
									vk::BufferUsageFlagBits::eVertexBuffer);
		}
#endif
		quadPipeline = std::make_unique<QuadPipeline>(false, false);
		quadPipeline->Init(&shaderManager, renderPass, subpass);
		framebufferDrawer = std::make_unique<QuadDrawer>();
		framebufferDrawer->Init(quadPipeline.get());

		return true;
	}

public:
	void Term() override
	{
		GetContext()->WaitIdle();
		GetContext()->PresentFrame(nullptr, nullptr, vk::Extent2D(), 0);
		framebufferDrawer.reset();
		quadPipeline.reset();
		osdBuffer.reset();
		osdPipeline.Term();
		vjoyTexture.reset();
		textureCache.Clear();
		fogTexture = nullptr;
		paletteTexture = nullptr;
		texCommandPool.Term();
		fbCommandPool.Term();
		framebufferTextures.clear();
		framebufferTexIndex = 0;
		shaderManager.term();
	}

	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override
	{
		Texture* tf = textureCache.getTextureCacheData(tsp, tcw);

		//update if needed
		if (tf->NeedsUpdate())
		{
			// This kills performance when a frame is skipped and lots of texture updated each frame
			//if (textureCache.IsInFlight(tf))
			//	textureCache.DestroyLater(tf);
			tf->SetCommandBuffer(texCommandBuffer);
			if (!tf->Update())
			{
				tf->SetCommandBuffer(nullptr);
				return nullptr;
			}
		}
		else if (tf->IsCustomTextureAvailable())
		{
			textureCache.DestroyLater(tf);
			tf->SetCommandBuffer(texCommandBuffer);
			tf->CheckCustomTexture();
		}
		tf->SetCommandBuffer(nullptr);
		textureCache.SetInFlight(tf);

		return tf;
	}

	void Process(TA_context* ctx) override
	{
		if (KillTex)
			textureCache.Clear();

		texCommandPool.BeginFrame();
		textureCache.SetCurrentIndex(texCommandPool.GetIndex());
		textureCache.Cleanup();

		texCommandBuffer = texCommandPool.Allocate();
		texCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

		ta_parse(ctx, true);

		CheckFogTexture();
		CheckPaletteTexture();
		texCommandBuffer.end();
	}

	void ReInitOSD()
	{
		texCommandPool.Init();
		fbCommandPool.Init();
#if defined(__ANDROID__) && !defined(LIBRETRO)
		osdPipeline.Init(&shaderManager, vjoyTexture->GetImageView(), GetContext()->GetRenderPass());
#endif
	}

	void DrawOSD(bool clear_screen) override
	{
#ifndef LIBRETRO
		gui_display_osd();
		if (!vjoyTexture)
			return;
		try {
			if (clear_screen)
			{
				GetContext()->NewFrame();
				GetContext()->BeginRenderPass();
				GetContext()->PresentLastFrame();
			}
			const float dc2s_scale_h = settings.display.height / 480.0f;
			const float sidebarWidth =  (settings.display.width - dc2s_scale_h * 640.0f) / 2;

			std::vector<OSDVertex> osdVertices = GetOSDVertices();
			const float x1 = 2.0f / (settings.display.width / dc2s_scale_h);
			const float y1 = 2.0f / 480;
			const float x2 = 1 - 2 * sidebarWidth / settings.display.width;
			const float y2 = 1;
			for (OSDVertex& vtx : osdVertices)
			{
				vtx.x = vtx.x * x1 - x2;
				vtx.y = vtx.y * y1 - y2;
			}

			const vk::CommandBuffer cmdBuffer = GetContext()->GetCurrentCommandBuffer();
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, osdPipeline.GetPipeline());

			osdPipeline.BindDescriptorSets(cmdBuffer);
			const vk::Viewport viewport(0, 0, (float)settings.display.width, (float)settings.display.height, 0, 1.f);
			cmdBuffer.setViewport(0, viewport);
			const vk::Rect2D scissor({ 0, 0 }, { (u32)settings.display.width, (u32)settings.display.height });
			cmdBuffer.setScissor(0, scissor);
			osdBuffer->upload((u32)(osdVertices.size() * sizeof(OSDVertex)), osdVertices.data());
			cmdBuffer.bindVertexBuffers(0, osdBuffer->buffer.get(), {0});
			for (u32 i = 0; i < (u32)osdVertices.size(); i += 4)
				cmdBuffer.draw(4, 1, i, 0);
			if (clear_screen)
				GetContext()->EndFrame();
		} catch (const InvalidVulkanContext&) {
		}
#endif
	}

	void RenderFramebuffer(const FramebufferInfo& info) override
	{
		framebufferTexIndex = (framebufferTexIndex + 1) % GetContext()->GetSwapChainSize();

		if (framebufferTextures.size() != GetContext()->GetSwapChainSize())
			framebufferTextures.resize(GetContext()->GetSwapChainSize());
		std::unique_ptr<Texture>& curTexture = framebufferTextures[framebufferTexIndex];
		if (!curTexture)
		{
			curTexture = std::make_unique<Texture>();
			curTexture->tex_type = TextureType::_8888;
		}

		fbCommandPool.BeginFrame();
		vk::CommandBuffer commandBuffer = fbCommandPool.Allocate();
		commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
		curTexture->SetCommandBuffer(commandBuffer);

		if (info.fb_r_ctrl.fb_enable == 0 || info.vo_control.blank_video == 1)
		{
			// Video output disabled
			u8 rgba[] { (u8)info.vo_border_col._red, (u8)info.vo_border_col._green, (u8)info.vo_border_col._blue, 255 };
			curTexture->UploadToGPU(1, 1, rgba, false);
		}
		else
		{
			PixelBuffer<u32> pb;
			int width;
			int height;
			ReadFramebuffer(info, pb, width, height);

			curTexture->UploadToGPU(width, height, (u8*)pb.data(), false);
		}

		curTexture->SetCommandBuffer(nullptr);
		commandBuffer.end();
		fbCommandPool.EndFrame();
		framebufferRendered = true;
	}

protected:
	BaseVulkanRenderer() : viewport(640, 480) {}

	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	virtual void resize(int w, int h)
	{
		viewport.width = w;
		viewport.height = h;
	}

	void CheckFogTexture()
	{
		if (!fogTexture)
		{
			fogTexture = std::make_unique<Texture>();
			fogTexture->tex_type = TextureType::_8;
			fog_needs_update = true;
		}
		if (!fog_needs_update || !config::Fog)
			return;
		fog_needs_update = false;
		u8 texData[256];
		MakeFogTexture(texData);
		fogTexture->SetCommandBuffer(texCommandBuffer);

		fogTexture->UploadToGPU(128, 2, texData, false);

		fogTexture->SetCommandBuffer(nullptr);
	}

	void CheckPaletteTexture()
	{
		if (!paletteTexture)
		{
			paletteTexture = std::make_unique<Texture>();
			paletteTexture->tex_type = TextureType::_8888;
			forcePaletteUpdate();
		}
		if (!palette_updated)
			return;
		palette_updated = false;

		paletteTexture->SetCommandBuffer(texCommandBuffer);

		paletteTexture->UploadToGPU(1024, 1, (u8 *)palette32_ram, false);

		paletteTexture->SetCommandBuffer(nullptr);
	}

	bool presentFramebuffer()
	{
		if (framebufferTexIndex >= (int)framebufferTextures.size())
			return false;
		Texture *fbTexture = framebufferTextures[framebufferTexIndex].get();
		if (fbTexture == nullptr)
			return false;
		GetContext()->PresentFrame(fbTexture->GetImage(), fbTexture->GetImageView(), fbTexture->getSize(),
				getDCFramebufferAspectRatio());
		framebufferRendered = false;
		return true;
	}

	ShaderManager shaderManager;
	std::unique_ptr<Texture> fogTexture;
	std::unique_ptr<Texture> paletteTexture;
	CommandPool texCommandPool;
	std::vector<std::unique_ptr<Texture>> framebufferTextures;
	int framebufferTexIndex = 0;
	OSDPipeline osdPipeline;
	std::unique_ptr<Texture> vjoyTexture;
	std::unique_ptr<BufferData> osdBuffer;
	TextureCache textureCache;
	vk::Extent2D viewport;
	vk::CommandBuffer texCommandBuffer;
	std::unique_ptr<QuadPipeline> quadPipeline;
	std::unique_ptr<QuadDrawer> framebufferDrawer;
	CommandPool fbCommandPool;
	bool framebufferRendered = false;
};
