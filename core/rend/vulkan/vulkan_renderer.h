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
#include "overlay.h"
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

#if defined(__ANDROID__) && !defined(LIBRETRO)
		if (!vjoyTexture)
		{
			int w, h;
			u8 *image_data = loadOSDButtons(w, h);
			texCommandPool.BeginFrame();
			vjoyTexture = std::unique_ptr<Texture>(new Texture());
			vjoyTexture->tex_type = TextureType::_8888;
			vjoyTexture->tcw.full = 0;
			vjoyTexture->tsp.full = 0;
			vjoyTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			vjoyTexture->SetDevice(GetContext()->GetDevice());
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
			osdBuffer = std::unique_ptr<BufferData>(new BufferData(sizeof(OSDVertex) * VJOY_VISIBLE * 4,
									vk::BufferUsageFlagBits::eVertexBuffer));
		}
#endif
#ifdef LIBRETRO
		quadPipeline = std::unique_ptr<QuadPipeline>(new QuadPipeline(false, false));
		quadPipeline->Init(&shaderManager, renderPass);
		overlay = std::unique_ptr<VulkanOverlay>(new VulkanOverlay());
		overlay->Init(quadPipeline.get());
#endif
		return true;
	}

public:
	void Term() override
	{
		GetContext()->WaitIdle();
		GetContext()->PresentFrame(nullptr, nullptr, vk::Extent2D());
#ifdef LIBRETRO
		overlay->Term();
		overlay.reset();
		quadPipeline.reset();
#endif
		osdBuffer.reset();
		vjoyTexture.reset();
		textureCache.Clear();
		fogTexture = nullptr;
		paletteTexture = nullptr;
		texCommandPool.Term();
		framebufferTextures.clear();
	}

	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override
	{
		Texture* tf = textureCache.getTextureCacheData(tsp, tcw);

		if (tf->IsNew())
		{
			tf->Create();
			tf->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			tf->SetDevice(GetContext()->GetDevice());
		}

		//update if needed
		if (tf->NeedsUpdate())
		{
			// This kills performance when a frame is skipped and lots of texture updated each frame
			//if (textureCache.IsInFlight(tf))
			//	textureCache.DestroyLater(tf);
			tf->SetCommandBuffer(texCommandBuffer);
			tf->Update();
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

	bool Process(TA_context* ctx) override
	{
		if (KillTex)
			textureCache.Clear();

		texCommandPool.BeginFrame();
		textureCache.SetCurrentIndex(texCommandPool.GetIndex());
		textureCache.Cleanup();

		texCommandBuffer = texCommandPool.Allocate();
		texCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

		bool result;
		if (ctx->rend.isRenderFramebuffer)
			result = RenderFramebuffer(ctx);
		else
			result = ta_parse_vdrc(ctx);

		if (result)
		{
#ifdef LIBRETRO
			if (!ctx->rend.isRTT)
				overlay->Prepare(texCommandBuffer, true, true);
#endif
			CheckFogTexture();
			CheckPaletteTexture();
			texCommandBuffer.end();
		}
		else
		{
			texCommandBuffer.end();
			texCommandPool.EndFrame();
		}

		return result;
	}

	void Resize(int w, int h) override
	{
		if ((u32)w == viewport.width && (u32)h == viewport.height)
			return;
		viewport.width = w;
		viewport.height = h;
	}

	void ReInitOSD()
	{
		texCommandPool.Init();
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
			cmdBuffer.setViewport(0, 1, &viewport);
			const vk::Rect2D scissor({ 0, 0 }, { (u32)settings.display.width, (u32)settings.display.height });
			cmdBuffer.setScissor(0, 1, &scissor);
			osdBuffer->upload(osdVertices.size() * sizeof(OSDVertex), osdVertices.data());
			const vk::DeviceSize zero = 0;
			cmdBuffer.bindVertexBuffers(0, 1, &osdBuffer->buffer.get(), &zero);
			for (size_t i = 0; i < osdVertices.size(); i += 4)
				cmdBuffer.draw(4, 1, i, 0);
			if (clear_screen)
				GetContext()->EndFrame();
		} catch (const InvalidVulkanContext&) {
		}
#endif
	}

protected:
	BaseVulkanRenderer() : viewport(640, 480) {}

	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	bool RenderFramebuffer(TA_context* ctx)
	{
		if (FB_R_SIZE.fb_x_size == 0 || FB_R_SIZE.fb_y_size == 0)
			return false;

		PixelBuffer<u32> pb;
		int width;
		int height;
		ReadFramebuffer(pb, width, height);

		if (framebufferTextures.size() != GetContext()->GetSwapChainSize())
			framebufferTextures.resize(GetContext()->GetSwapChainSize());
		std::unique_ptr<Texture>& curTexture = framebufferTextures[GetContext()->GetCurrentImageIndex()];
		if (!curTexture)
		{
			curTexture = std::unique_ptr<Texture>(new Texture());
			curTexture->tex_type = TextureType::_8888;
			curTexture->tcw.full = 0;
			curTexture->tsp.full = 0;
			curTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			curTexture->SetDevice(GetContext()->GetDevice());
		}
		curTexture->SetCommandBuffer(texCommandBuffer);
		curTexture->UploadToGPU(width, height, (u8*)pb.data(), false);
		curTexture->SetCommandBuffer(nullptr);

		Vertex *vtx = ctx->rend.verts.Append(4);
		vtx[0].x = 0.f;
		vtx[0].y = 0.f;
		vtx[0].z = 0.1f;
		vtx[0].u = 0.f;
		vtx[0].v = 0.f;

		vtx[1] = vtx[0];
		vtx[1].x = 640.f;
		vtx[1].u = 1.f;

		vtx[2] = vtx[0];
		vtx[2].y = 480.f;
		vtx[2].v = 1.f;

		vtx[3] = vtx[0];
		vtx[3].x = 640.f;
		vtx[3].y = 480.f;
		vtx[3].u = 1.f;
		vtx[3].v = 1.f;

		u32 *idx = ctx->rend.idx.Append(4);
		idx[0] = ctx->rend.verts.used() - 4;
		idx[1] = idx[0] + 1;
		idx[2] = idx[1] + 1;
		idx[3] = idx[2] + 1;

		PolyParam *pp = ctx->rend.global_param_op.Append(1);
		pp->first = ctx->rend.idx.used() - 4;
		pp->count = 4;

		pp->isp.full = 0;
		pp->isp.DepthMode = 7;

		pp->pcw.full = 0;
		pp->pcw.Gouraud = 1;
		pp->pcw.Texture = 1;

		pp->tcw.full = 0;
		pp->tcw.TexAddr = 0x1fffff;
		pp->tcw1.full = (u32)-1;

		pp->tsp.full = 0;
		pp->tsp.FilterMode = 1;
		pp->tsp.FogCtrl = 2;
		pp->tsp.SrcInstr = 1;
		pp->tsp1.full = (u32)-1;

		pp->texture = curTexture.get();
		pp->texture1 = nullptr;
		pp->tileclip = 0;

		RenderPass *pass = ctx->rend.render_passes.Append(1);
		pass->autosort = false;
		pass->mvo_count = 0;
		pass->mvo_tr_count = 0;
		pass->op_count = ctx->rend.global_param_op.used();
		pass->pt_count = 0;
		pass->tr_count = 0;

		return true;
	}

	void CheckFogTexture()
	{
		if (!fogTexture)
		{
			fogTexture = std::unique_ptr<Texture>(new Texture());
			fogTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			fogTexture->SetDevice(GetContext()->GetDevice());
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
			paletteTexture = std::unique_ptr<Texture>(new Texture());
			paletteTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			paletteTexture->SetDevice(GetContext()->GetDevice());
			paletteTexture->tex_type = TextureType::_8888;
			palette_updated = true;
		}
		if (!palette_updated)
			return;
		palette_updated = false;

		paletteTexture->SetCommandBuffer(texCommandBuffer);

		paletteTexture->UploadToGPU(1024, 1, (u8 *)palette32_ram, false);

		paletteTexture->SetCommandBuffer(nullptr);
	}

	ShaderManager shaderManager;
	std::unique_ptr<Texture> fogTexture;
	std::unique_ptr<Texture> paletteTexture;
	CommandPool texCommandPool;
	std::vector<std::unique_ptr<Texture>> framebufferTextures;
	OSDPipeline osdPipeline;
	std::unique_ptr<Texture> vjoyTexture;
	std::unique_ptr<BufferData> osdBuffer;
	TextureCache textureCache;
	vk::Extent2D viewport;
	vk::CommandBuffer texCommandBuffer;
#ifdef LIBRETRO
	std::unique_ptr<VulkanOverlay> overlay;
	std::unique_ptr<QuadPipeline> quadPipeline;
#endif
};
