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
#include "allocator.h"
#include "commandpool.h"
#include "drawer.h"
#include "shaders.h"
#include "../gui.h"

extern bool ProcessFrame(TA_context* ctx);

class VulkanRenderer : public Renderer
{
public:
	bool Init() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Init");
		shaderManager.Init();
		texCommandPool.Init();

		// FIXME this might be called after initial init
		texAllocator.SetChunkSize(16 * 1024 * 1024);
		while (textureDrawer.size() < 2)
			textureDrawer.emplace_back();
		textureDrawer[0].Init(&samplerManager, &shaderManager, &texAllocator);
		textureDrawer[0].SetCommandPool(&texCommandPool);
		textureDrawer[1].Init(&samplerManager, &shaderManager, &texAllocator);
		textureDrawer[1].SetCommandPool(&texCommandPool);
		screenDrawer.Init(&samplerManager, &shaderManager);
		quadPipeline.Init(&shaderManager);

		return true;
	}

	void Resize(int w, int h) override
	{
		texCommandPool.Init();
		screenDrawer.Init(&samplerManager, &shaderManager);
		quadPipeline.Init(&shaderManager);
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Term");
		GetContext()->WaitIdle();
		killtex();
		fogTexture = nullptr;
		texCommandPool.Term();
		shaderManager.Term();
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
			curTexture = std::unique_ptr<Texture>(new Texture(GetContext()->GetPhysicalDevice(), *GetContext()->GetDevice(), &texAllocator));
			curTexture->tex_type = TextureType::_8888;
			curTexture->tcw.full = 0;
			curTexture->tsp.full = 0;
		}
		curTexture->SetCommandBuffer(texCommandPool.Allocate());
		curTexture->UploadToGPU(width, height, (u8*)pb.data());
		curTexture->SetCommandBuffer(nullptr);
		texCommandPool.EndFrame();

		float screen_stretching = settings.rend.ScreenStretching / 100.f;
		float dc2s_scale_h, ds2s_offs_x;
		if (settings.rend.Rotate90)
		{
			dc2s_scale_h = screen_height / 640.0f;
			ds2s_offs_x =  (screen_width - dc2s_scale_h * 480.0f * screen_stretching) / 2;
		}
		else
		{
			dc2s_scale_h = screen_height / 480.0f;
			ds2s_offs_x =  (screen_width - dc2s_scale_h * 640.0f * screen_stretching) / 2;
		}

		vk::CommandBuffer cmdBuffer = screenDrawer.BeginRenderPass();

		vk::Pipeline pipeline = quadPipeline.GetPipeline();
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

		quadPipeline.SetTexture(curTexture.get());
		quadPipeline.BindDescriptorSets(cmdBuffer);

		// FIXME scaling, stretching...
		vk::Viewport viewport(ds2s_offs_x, 0.f, screen_width - ds2s_offs_x * 2, (float)screen_height);
		cmdBuffer.setViewport(0, 1, &viewport);
		cmdBuffer.draw(3, 1, 0, 0);

    	gui_display_osd();

		screenDrawer.EndRenderPass();

		return true;
	}

	bool Process(TA_context* ctx) override
	{
		texCommandPool.BeginFrame();

		if (ctx->rend.isRenderFramebuffer)
		{
			return RenderFramebuffer();
		}

		bool result = ProcessFrame(ctx);

		if (result)
			CheckFogTexture();

		if (!result || !ctx->rend.isRTT)
			texCommandPool.EndFrame();

		return result;
	}

	void DrawOSD(bool clear_screen) override
	{
	}

	bool Render() override
	{
		if (pvrrc.isRenderFramebuffer)
			return true;

		if (pvrrc.isRTT)
		{
			textureDrawer[curTextureDrawer].Draw(fogTexture.get());
			curTextureDrawer ^= 1;
			return false;
		}
		else
			return screenDrawer.Draw(fogTexture.get());
	}

	void Present() override
	{
		GetContext()->Present();
	}

	virtual u64 GetTexture(TSP tsp, TCW tcw) override
	{
		Texture* tf = static_cast<Texture*>(getTextureCacheData(tsp, tcw, [this](){
			return (BaseTextureCacheData *)new Texture(VulkanContext::Instance()->GetPhysicalDevice(), *VulkanContext::Instance()->GetDevice(), &this->texAllocator);
		}));

		if (tf->IsNew())
			tf->Create();

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
			fogTexture = std::unique_ptr<Texture>(new Texture(GetContext()->GetPhysicalDevice(), *GetContext()->GetDevice()));
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

	std::unique_ptr<Texture> fogTexture;
	CommandPool texCommandPool;

	SamplerManager samplerManager;
	ShaderManager shaderManager;
	ScreenDrawer screenDrawer;
	std::vector<TextureDrawer> textureDrawer;
	int curTextureDrawer = 0;
	VulkanAllocator texAllocator;
	std::vector<std::unique_ptr<Texture>> framebufferTextures;
	QuadPipeline quadPipeline;
};

Renderer* rend_Vulkan()
{
	return new VulkanRenderer();
}
