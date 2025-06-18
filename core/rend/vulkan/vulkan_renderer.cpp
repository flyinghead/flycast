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
#include "vulkan.h"
#include "vulkan_renderer.h"
#include "drawer.h"
#include "hw/pvr/ta.h"
#include "rend/transform_matrix.h"

bool BaseVulkanRenderer::BaseInit(vk::RenderPass renderPass, int subpass)
{
	texCommandPool.Init();
	fbCommandPool.Init();
	quadPipeline = std::make_unique<QuadPipeline>(false, false);
	quadPipeline->Init(&shaderManager, renderPass, subpass);
	framebufferDrawer = std::make_unique<QuadDrawer>();
	framebufferDrawer->Init(quadPipeline.get());

	return true;
}

void BaseVulkanRenderer::Term()
{
	GetContext()->WaitIdle();
	GetContext()->PresentFrame(nullptr, nullptr, vk::Extent2D(), 0);
#if defined(VIDEO_ROUTING) && defined(TARGET_MAC)
	os_VideoRoutingTermVk();
#endif
	framebufferDrawer.reset();
	quadPipeline.reset();
	textureCache.Clear();
	fogTexture = nullptr;
	paletteTexture = nullptr;
	texCommandPool.Term();
	fbCommandPool.Term();
	framebufferTextures.clear();
	framebufferTexIndex = 0;
	shaderManager.term();
}

BaseTextureCacheData *BaseVulkanRenderer::GetTexture(TSP tsp, TCW tcw)
{
	Texture* tf = textureCache.getTextureCacheData(tsp, tcw);

	//update if needed
	if (tf->NeedsUpdate())
	{
		// This kills performance when a frame is skipped and lots of texture updated each frame
		//if (textureCache.IsInFlight(tf, true))
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
		tf->deferDeleteResource(&texCommandPool);
		tf->SetCommandBuffer(texCommandBuffer);
		tf->CheckCustomTexture();
	}
	tf->SetCommandBuffer(nullptr);
	textureCache.SetInFlight(tf);

	return tf;
}

void BaseVulkanRenderer::Process(TA_context* ctx)
{
	if (!ctx->rend.isRTT) {
		framebufferRendered = false;
		if (!config::EmulateFramebuffer)
			clearLastFrame = false;
	}
	if (resetTextureCache) {
		textureCache.Clear();
		resetTextureCache = false;
	}

	texCommandPool.BeginFrame();
	textureCache.SetCurrentIndex(texCommandPool.GetIndex());
	textureCache.Cleanup();

	texCommandBuffer = texCommandPool.Allocate();
	texCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	ta_parse(ctx, true);

	// TODO can't update fog or palette twice in multi render
	CheckFogTexture();
	CheckPaletteTexture();
	texCommandBuffer.end();
}

void BaseVulkanRenderer::ReInitOSD()
{
	texCommandPool.Init();
	fbCommandPool.Init();
}

void BaseVulkanRenderer::RenderFramebuffer(const FramebufferInfo& info)
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
	{
		static const float scopeColor[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
		CommandBufferDebugScope _(commandBuffer, "RenderFramebuffer", scopeColor);

		if (info.fb_r_ctrl.fb_enable == 0 || info.vo_control.blank_video == 1)
		{
			// Video output disabled
			u8 rgba[]{ (u8)info.vo_border_col._red, (u8)info.vo_border_col._green, (u8)info.vo_border_col._blue, 255 };
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

	}
	curTexture->SetCommandBuffer(nullptr);
	commandBuffer.end();
	fbCommandPool.EndFrame();
	framebufferRendered = true;
	clearLastFrame = false;
}

void BaseVulkanRenderer::RenderVideoRouting()
{
#if defined(VIDEO_ROUTING) && defined(TARGET_MAC)
	if (config::VideoRouting)
	{
		auto device = GetContext()->GetDevice();
		auto srcImage = device.getSwapchainImagesKHR(GetContext()->GetSwapChain())[GetContext()->GetCurrentImageIndex()];
		auto graphicsQueue = device.getQueue(GetContext()->GetGraphicsQueueFamilyIndex(), 0);

		int targetWidth = (config::VideoRoutingScale ? config::VideoRoutingVRes * settings.display.width / settings.display.height : settings.display.width);
		int targetHeight = (config::VideoRoutingScale ? config::VideoRoutingVRes : settings.display.height);

		extern void os_VideoRoutingPublishFrameTexture(const vk::Device& device, const vk::Image& image, const vk::Queue& queue, float x, float y, float w, float h);
		os_VideoRoutingPublishFrameTexture(device, srcImage, graphicsQueue, 0, 0, targetWidth, targetHeight);
	}
	else
	{
		os_VideoRoutingTermVk();
	}
#endif
}

void BaseVulkanRenderer::CheckFogTexture()
{
	if (!fogTexture)
	{
		fogTexture = std::make_unique<Texture>();
		fogTexture->tex_type = TextureType::_8;
		updateFogTable = true;
	}
	if (!updateFogTable || !config::Fog)
		return;
	updateFogTable = false;
	u8 texData[256];
	MakeFogTexture(texData);

	fogTexture->SetCommandBuffer(texCommandBuffer);
	fogTexture->UploadToGPU(128, 2, texData, false);
	fogTexture->SetCommandBuffer(nullptr);
}

void BaseVulkanRenderer::CheckPaletteTexture()
{
	if (!paletteTexture) {
		paletteTexture = std::make_unique<Texture>();
		paletteTexture->tex_type = TextureType::_8888;
	}
	else if (!updatePalette) {
		return;
	}
	updatePalette = false;

	paletteTexture->SetCommandBuffer(texCommandBuffer);
	paletteTexture->UploadToGPU(1024, 1, (u8 *)palette32_ram, false);
	paletteTexture->SetCommandBuffer(nullptr);
}

bool BaseVulkanRenderer::presentFramebuffer()
{
	if (framebufferTexIndex >= (int)framebufferTextures.size())
		return false;
	Texture *fbTexture = framebufferTextures[framebufferTexIndex].get();
	if (fbTexture == nullptr)
		return false;
	GetContext()->PresentFrame(fbTexture->GetImage(), fbTexture->GetImageView(), fbTexture->getSize(),
			getDCFramebufferAspectRatio());
	return true;
}

class VulkanRenderer final : public BaseVulkanRenderer
{
public:
	bool Init() override
	{
		NOTICE_LOG(RENDERER, "VulkanRenderer::Init");

		textureDrawer.Init(&samplerManager, &shaderManager, &textureCache);
		textureDrawer.SetCommandPool(&texCommandPool);

		screenDrawer.Init(&samplerManager, &shaderManager, viewport);
		screenDrawer.SetCommandPool(&texCommandPool);
		BaseInit(screenDrawer.GetRenderPass());
		emulateFramebuffer = config::EmulateFramebuffer;

		return true;
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Term");
		GetContext()->WaitIdle();
		texCommandPool.Term(); // make sure all in-flight buffers are returned
		screenDrawer.Term();
		textureDrawer.Term();
		samplerManager.term();
		BaseVulkanRenderer::Term();
	}

	void Process(TA_context* ctx) override
	{
		if (emulateFramebuffer != config::EmulateFramebuffer)
		{
			screenDrawer.EndRenderPass();
			VulkanContext::Instance()->WaitIdle();
			screenDrawer.Term();
			screenDrawer.Init(&samplerManager, &shaderManager, viewport);
			BaseInit(screenDrawer.GetRenderPass());
			emulateFramebuffer = config::EmulateFramebuffer;
		}
		else if (ctx->rend.isRTT) {
			screenDrawer.EndRenderPass();
		}
		BaseVulkanRenderer::Process(ctx);
	}

	bool Render() override
	{
		try {
			Drawer *drawer;
			if (pvrrc.isRTT)
				drawer = &textureDrawer;
			else {
				resize(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
				drawer = &screenDrawer;
			}

			drawer->Draw(fogTexture.get(), paletteTexture.get());
			if (config::EmulateFramebuffer || pvrrc.isRTT)
				// delay ending the render pass in case of multi render
				drawer->EndRenderPass();

			return !pvrrc.isRTT;
		} catch (const vk::SystemError& e) {
			// Sometimes happens when resizing the window
			WARN_LOG(RENDERER, "Vulkan system error %s", e.what());

			return false;
		}
	}

	bool Present() override
	{
		if (clearLastFrame)
			return false;
		if (config::EmulateFramebuffer || framebufferRendered)
			return presentFramebuffer();
		else
			return screenDrawer.PresentFrame();
	}

protected:
	void resize(int w, int h) override
	{
        ERROR_LOG(RENDERER, "VulkanRenderer::resize %d %d", w, h);
		if ((u32)w == viewport.width && (u32)h == viewport.height)
			return;
		BaseVulkanRenderer::resize(w, h);
		GetContext()->WaitIdle();
		screenDrawer.Init(&samplerManager, &shaderManager, viewport);
	}

private:
	SamplerManager samplerManager;
	ScreenDrawer screenDrawer;
	TextureDrawer textureDrawer;
	bool emulateFramebuffer = false;
};

Renderer* rend_Vulkan()
{
	return new VulkanRenderer();
}

void ReInitOSD()
{
	if (renderer != nullptr) {
		BaseVulkanRenderer *vkrenderer = dynamic_cast<BaseVulkanRenderer*>(renderer);
		if (vkrenderer != nullptr)
			vkrenderer->ReInitOSD();
	}
}
