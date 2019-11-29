/*
    Created on: Oct 2, 2019

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
#include "vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "commandpool.h"
#include "drawer.h"
#include "shaders.h"
#include "rend/gui.h"
#include "rend/osd.h"
#include "quad.h"

class VulkanRenderer : public Renderer
{
public:
	bool Init() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Init");
		texCommandPool.Init();

		rttPipelineManager.Init(&shaderManager);
		if (textureDrawer.size() > GetContext()->GetSwapChainSize())
			textureDrawer.resize(GetContext()->GetSwapChainSize());
		else
		{
			while (textureDrawer.size() < GetContext()->GetSwapChainSize())
				textureDrawer.emplace_back();
		}
		for (auto& drawer : textureDrawer)
		{
			drawer.Init(&samplerManager, &rttPipelineManager, &textureCache);
			drawer.SetCommandPool(&texCommandPool);
		}

		screenDrawer.Init(&samplerManager, &shaderManager);
		screenDrawer.SetCommandPool(&texCommandPool);
		quadPipeline.Init(&shaderManager);
		quadBuffer = std::unique_ptr<QuadBuffer>(new QuadBuffer());

#ifdef __ANDROID__
		if (!vjoyTexture)
		{
			int w, h;
			u8 *image_data = loadPNGData(get_readonly_data_path(DATA_PATH "buttons.png"), w, h);
			if (image_data == nullptr)
			{
				WARN_LOG(RENDERER, "Cannot load buttons.png image");
			}
			else
			{
				vjoyTexture = std::unique_ptr<Texture>(new Texture());
				vjoyTexture->tex_type = TextureType::_8888;
				vjoyTexture->tcw.full = 0;
				vjoyTexture->tsp.full = 0;
				vjoyTexture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
				vjoyTexture->SetDevice(GetContext()->GetDevice());
				vjoyTexture->SetCommandBuffer(texCommandPool.Allocate());
				vjoyTexture->UploadToGPU(OSD_TEX_W, OSD_TEX_H, image_data);
				vjoyTexture->SetCommandBuffer(nullptr);
				texCommandPool.EndFrame();
				delete [] image_data;
				osdPipeline.Init(&shaderManager, vjoyTexture->GetImageView(), GetContext()->GetRenderPass());
			}
		}
		if (!osdBuffer)
		{
			osdBuffer = std::unique_ptr<BufferData>(new BufferData(sizeof(OSDVertex) * VJOY_VISIBLE * 4,
									vk::BufferUsageFlagBits::eVertexBuffer));
		}
#endif

		return true;
	}

	void Resize(int w, int h) override
	{
		texCommandPool.Init();
		screenDrawer.Init(&samplerManager, &shaderManager);
		quadPipeline.Init(&shaderManager);
#ifdef __ANDROID__
		osdPipeline.Init(&shaderManager, vjoyTexture->GetImageView(), GetContext()->GetRenderPass());
#endif
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Term");
		GetContext()->WaitIdle();
		quadBuffer = nullptr;
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
		curTexture->UploadToGPU(width, height, (u8*)pb.data());
		curTexture->SetCommandBuffer(nullptr);
		texCommandPool.EndFrame();

		GetContext()->PresentFrame(curTexture->GetImageView(), { 640, 480 });

		return true;
	}

	bool Process(TA_context* ctx) override
	{
		texCommandPool.BeginFrame();

		if (ctx->rend.isRenderFramebuffer)
		{
			return RenderFramebuffer();
		}

		ctx->rend_inuse.Lock();

		if (KillTex)
			textureCache.Clear();

		bool result = ta_parse_vdrc(ctx);

		textureCache.CollectCleanup();

		if (ctx->rend.Overrun)
			WARN_LOG(PVR, "ERROR: TA context overrun");

		result = result && !ctx->rend.Overrun;

		if (result)
			CheckFogTexture();

		if (!result)
			texCommandPool.EndFrame();

		return result;
	}

	// FIXME This needs to go in its own class
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

		Drawer *drawer;
		if (pvrrc.isRTT)
			drawer = &textureDrawer[GetContext()->GetCurrentImageIndex()];
		else
			drawer = &screenDrawer;

		drawer->Draw(fogTexture.get());

		drawer->EndRenderPass();

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
			tf->SetCommandBuffer(texCommandPool.Allocate());
			tf->Update();
			tf->SetCommandBuffer(nullptr);
		}
		else
			tf->CheckCustomTexture();

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

		fogTexture->UploadToGPU(128, 2, texData);

		fogTexture->SetCommandBuffer(nullptr);
	}

	std::unique_ptr<QuadBuffer> quadBuffer;
	std::unique_ptr<Texture> fogTexture;
	CommandPool texCommandPool;

	SamplerManager samplerManager;
	ShaderManager shaderManager;
	ScreenDrawer screenDrawer;
	RttPipelineManager rttPipelineManager;
	std::vector<TextureDrawer> textureDrawer;
	std::vector<std::unique_ptr<Texture>> framebufferTextures;
	QuadPipeline quadPipeline;
	OSDPipeline osdPipeline;
	std::unique_ptr<Texture> vjoyTexture;
	std::unique_ptr<BufferData> osdBuffer;
	TextureCache textureCache;
};

Renderer* rend_Vulkan()
{
	return new VulkanRenderer();
}
