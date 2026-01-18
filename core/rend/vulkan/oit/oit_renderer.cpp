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
#include "../vulkan.h"
#include "../vulkan_renderer.h"
#include "oit_drawer.h"
#include "oit_shaders.h"
#include "oit_buffer.h"

class OITVulkanRenderer final : public BaseVulkanRenderer
{
public:
	bool Init() override
	{
		NOTICE_LOG(RENDERER, "OITVulkanRenderer::Init");
		try {
			oitBuffers.Init(viewport.width, viewport.height);
			textureDrawer.Init(&samplerManager, &oitShaderManager, &textureCache, &oitBuffers);
			textureDrawer.SetCommandPool(&texCommandPool);

			screenDrawer.Init(&samplerManager, &oitShaderManager, &oitBuffers, viewport);
			screenDrawer.SetCommandPool(&texCommandPool);
			BaseInit(screenDrawer.GetRenderPass(), 2);
			emulateFramebuffer = config::EmulateFramebuffer;

			return true;
		} catch (const vk::SystemError& err) {
			ERROR_LOG(RENDERER, "Vulkan system error: %s", err.what());
			return false;
		}
	}

	void Term() override
	{
		try {
			DEBUG_LOG(RENDERER, "OITVulkanRenderer::Term");
			GetContext()->WaitIdle();
			texCommandPool.Term();
			screenDrawer.Term();
			textureDrawer.Term();
			oitBuffers.Term();
			oitShaderManager.term();
			samplerManager.term();
			BaseVulkanRenderer::Term();
		} catch (const vk::SystemError& err) {
			ERROR_LOG(RENDERER, "Vulkan system error: %s", err.what());
		}
	}

	void Process(TA_context* ctx) override
	{
		try {
			if (emulateFramebuffer != config::EmulateFramebuffer)
			{
				screenDrawer.EndFrame();
				VulkanContext::Instance()->WaitIdle();
				screenDrawer.Term();
				screenDrawer.Init(&samplerManager, &oitShaderManager, &oitBuffers, viewport);
				BaseInit(screenDrawer.GetRenderPass(), 2);
				emulateFramebuffer = config::EmulateFramebuffer;
			}
			else if (ctx->rend.isRTT) {
				screenDrawer.EndFrame();
			}
			BaseVulkanRenderer::Process(ctx);
		} catch (const vk::OutOfDeviceMemoryError& e) {
			ERROR_LOG(RENDERER, "Vulkan out of device memory: %s", e.what());
			throw RendererException("Out of device memory");
		} catch (const vk::SystemError& e) {
			ERROR_LOG(RENDERER, "Vulkan system error %s", e.what());
			throw RendererException("Vulkan system error");
		}
	}

	bool Render() override
	{
		try {
			OITDrawer *drawer;
			if (rendContext->isRTT)
				drawer = &textureDrawer;
			else {
				resize(rendContext->framebufferWidth, rendContext->framebufferHeight);
				drawer = &screenDrawer;
			}

			drawer->setRendContext(rendContext);
			drawer->Draw(fogTexture.get(), paletteTexture.get());
			if (config::EmulateFramebuffer || rendContext->isRTT)
				drawer->EndFrame();

			return !rendContext->isRTT;
		} catch (const vk::OutOfDeviceMemoryError& e) {
			ERROR_LOG(RENDERER, "Vulkan out of device memory: %s", e.what());
			throw RendererException("Out of device memory");
		} catch (const vk::SystemError& e) {
			// Sometimes happens when resizing the window
			ERROR_LOG(RENDERER, "Vulkan system error %s", e.what());
			throw RendererException("Vulkan system error");
		}
	}

	bool Present() override
	{
		if (clearLastFrame)
			return false;
		try {
			if (config::EmulateFramebuffer || framebufferRendered)
				return presentFramebuffer();
			else
				return screenDrawer.PresentFrame();
		} catch (const vk::SystemError& e) {
			ERROR_LOG(RENDERER, "Vulkan system error %s", e.what());
			throw RendererException("Vulkan system error");
		}
	}

protected:
	void resize(int w, int h) override
	{
		if ((u32)w == viewport.width && (u32)h == viewport.height)
			return;
		BaseVulkanRenderer::resize(w, h);
		GetContext()->WaitIdle();
		screenDrawer.Init(&samplerManager, &oitShaderManager, &oitBuffers, viewport);
	}

private:
	OITBuffers oitBuffers;
	SamplerManager samplerManager;
	OITShaderManager oitShaderManager;
	OITScreenDrawer screenDrawer;
	OITTextureDrawer textureDrawer;
	bool emulateFramebuffer = false;
};

Renderer* rend_OITVulkan()
{
	return new OITVulkanRenderer();
}
