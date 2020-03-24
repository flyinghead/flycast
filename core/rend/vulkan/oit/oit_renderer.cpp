/*
    Created on: Nov 7, 2019

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
#include <memory>
#include <math.h>
#include "../vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "../commandpool.h"
#include "oit_drawer.h"
#include "oit_shaders.h"
#include "rend/gui.h"
#include "rend/osd.h"
#include "../pipeline.h"
#include "oit_buffer.h"

class OITVulkanRenderer : public Renderer
{
public:
	bool Init() override
	{
		DEBUG_LOG(RENDERER, "OITVulkanRenderer::Init");
		try {
			texCommandPool.Init();

			oitBuffers.Init(0, 0);
			textureDrawer.Init(&samplerManager, &shaderManager, &textureCache, &oitBuffers);
			textureDrawer.SetCommandPool(&texCommandPool);

			screenDrawer.Init(&samplerManager, &shaderManager, &oitBuffers);
			screenDrawer.SetCommandPool(&texCommandPool);

#ifdef __ANDROID__
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
				vjoyTexture->SetCommandBuffer(texCommandPool.Allocate());
				vjoyTexture->UploadToGPU(OSD_TEX_W, OSD_TEX_H, image_data, false);
				vjoyTexture->SetCommandBuffer(nullptr);
				texCommandPool.EndFrame();
				delete [] image_data;
				osdPipeline.Init(&normalShaderManager, vjoyTexture->GetImageView(), GetContext()->GetRenderPass());
			}
			if (!osdBuffer)
			{
				osdBuffer = std::unique_ptr<BufferData>(new BufferData(sizeof(OSDVertex) * VJOY_VISIBLE * 4,
										vk::BufferUsageFlagBits::eVertexBuffer));
			}
#endif

			return true;
		}
		catch (const vk::SystemError& err)
		{
			ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
		}
		return false;
	}

	void Resize(int w, int h) override
	{
		NOTICE_LOG(RENDERER, "OIT Resize %d x %d", w, h);
		texCommandPool.Init();
		screenDrawer.Init(&samplerManager, &shaderManager, &oitBuffers);
#ifdef __ANDROID__
		osdPipeline.Init(&normalShaderManager, vjoyTexture->GetImageView(), GetContext()->GetRenderPass());
#endif
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Term");
		GetContext()->WaitIdle();
		screenDrawer.Term();
		textureDrawer.Term();
		oitBuffers.Term();
		osdBuffer.reset();
		textureCache.Clear();
		fogTexture = nullptr;
		texCommandPool.Term();
		framebufferTextures.clear();
	}

	bool RenderFramebuffer()
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
		curTexture->SetCommandBuffer(texCommandPool.Allocate());
		curTexture->UploadToGPU(width, height, (u8*)pb.data(), false);
		curTexture->SetCommandBuffer(nullptr);
		texCommandPool.EndFrame();

		GetContext()->PresentFrame(curTexture->GetImageView(), { 640, 480 });

		return true;
	}

	bool Process(TA_context* ctx) override
	{
		texCommandPool.BeginFrame();
		textureCache.SetCurrentIndex(texCommandPool.GetIndex());

		if (ctx->rend.isRenderFramebuffer)
		{
			return RenderFramebuffer();
		}

		ctx->rend_inuse.Lock();

		if (KillTex)
			textureCache.Clear();

		bool result = ta_parse_vdrc(ctx);

		textureCache.CollectCleanup();

		if (result)
			CheckFogTexture();
		else
			texCommandPool.EndFrame();

		return result;
	}

	void DrawOSD(bool clear_screen) override
	{
		gui_display_osd();
		if (!vjoyTexture)
			return;
		if (clear_screen)
		{
			GetContext()->NewFrame();
			GetContext()->BeginRenderPass();
		}
		const float dc2s_scale_h = screen_height / 480.0f;
		const float sidebarWidth =  (screen_width - dc2s_scale_h * 640.0f) / 2;

		std::vector<OSDVertex> osdVertices = GetOSDVertices();
		const float x1 = 2.0f / (screen_width / dc2s_scale_h);
		const float y1 = 2.0f / 480;
		const float x2 = 1 - 2 * sidebarWidth / screen_width;
		const float y2 = 1;
		for (OSDVertex& vtx : osdVertices)
		{
			vtx.x = vtx.x * x1 - x2;
			vtx.y = vtx.y * y1 - y2;
		}

		const vk::CommandBuffer cmdBuffer = GetContext()->GetCurrentCommandBuffer();
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, osdPipeline.GetPipeline());

		osdPipeline.BindDescriptorSets(cmdBuffer);
		const vk::Viewport viewport(0, 0, (float)screen_width, (float)screen_height, 0, 1.f);
		cmdBuffer.setViewport(0, 1, &viewport);
		const vk::Rect2D scissor({ 0, 0 }, { (u32)screen_width, (u32)screen_height });
		cmdBuffer.setScissor(0, 1, &scissor);
		osdBuffer->upload(osdVertices.size() * sizeof(OSDVertex), osdVertices.data());
		const vk::DeviceSize zero = 0;
		cmdBuffer.bindVertexBuffers(0, 1, &osdBuffer->buffer.get(), &zero);
		for (int i = 0; i < osdVertices.size(); i += 4)
			cmdBuffer.draw(4, 1, i, 0);
		if (clear_screen)
			GetContext()->EndFrame();
	}

	bool Render() override
	{
		if (pvrrc.isRenderFramebuffer)
			return true;

		OITDrawer *drawer;
		if (pvrrc.isRTT)
			drawer = &textureDrawer;
		else
			drawer = &screenDrawer;

		drawer->Draw(fogTexture.get());

		drawer->EndFrame();

		return !pvrrc.isRTT;
	}

	void Present() override
	{
		GetContext()->Present();
	}

	virtual u64 GetTexture(TSP tsp, TCW tcw) override
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
			textureCache.DestroyLater(tf);
			tf->SetCommandBuffer(texCommandPool.Allocate());
			tf->Update();
		}
		else if (tf->IsCustomTextureAvailable())
		{
			textureCache.DestroyLater(tf);
			tf->SetCommandBuffer(texCommandPool.Allocate());
			tf->CheckCustomTexture();
		}
		tf->SetCommandBuffer(nullptr);

		return tf->GetIntId();
	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

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
		if (!fog_needs_update || !settings.rend.Fog)
			return;
		fog_needs_update = false;
		u8 texData[256];
		MakeFogTexture(texData);
		fogTexture->SetCommandBuffer(texCommandPool.Allocate());

		fogTexture->UploadToGPU(128, 2, texData, false);

		fogTexture->SetCommandBuffer(nullptr);
	}

	OITBuffers oitBuffers;
	std::unique_ptr<Texture> fogTexture;
	CommandPool texCommandPool;

	SamplerManager samplerManager;
	OITShaderManager shaderManager;
	ShaderManager normalShaderManager;
	OITScreenDrawer screenDrawer;
	OITTextureDrawer textureDrawer;
	std::vector<std::unique_ptr<Texture>> framebufferTextures;
	OSDPipeline osdPipeline;
	std::unique_ptr<Texture> vjoyTexture;
	std::unique_ptr<BufferData> osdBuffer;
	TextureCache textureCache;
};

Renderer* rend_OITVulkan()
{
	return new OITVulkanRenderer();
}
