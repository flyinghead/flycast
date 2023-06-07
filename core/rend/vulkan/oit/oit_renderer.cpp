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
		}
		catch (const vk::SystemError& err)
		{
			ERROR_LOG(RENDERER, "Vulkan error: %s", err.what());
		}
		return false;
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "OITVulkanRenderer::Term");
		GetContext()->WaitIdle();
		screenDrawer.Term();
		textureDrawer.Term();
		oitBuffers.Term();
		oitShaderManager.term();
		samplerManager.term();
		BaseVulkanRenderer::Term();
	}

	bool Render() override
	{
		try {
			if (emulateFramebuffer != config::EmulateFramebuffer)
			{
				VulkanContext::Instance()->WaitIdle();
				screenDrawer.Term();
				screenDrawer.Init(&samplerManager, &oitShaderManager, &oitBuffers, viewport);
				BaseInit(screenDrawer.GetRenderPass(), 2);
				emulateFramebuffer = config::EmulateFramebuffer;
			}
			OITDrawer *drawer;
			if (pvrrc.isRTT)
				drawer = &textureDrawer;
			else {
				resize(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
				drawer = &screenDrawer;
			}

			drawer->Draw(fogTexture.get(), paletteTexture.get());
			drawer->EndFrame();

			return !pvrrc.isRTT;
		} catch (const vk::SystemError& e) {
			// Sometimes happens when resizing the window
			WARN_LOG(RENDERER, "Vulkan system error %s", e.what());

			return false;
		}
	}

	bool Present() override
	{
		if (config::EmulateFramebuffer || framebufferRendered)
			return presentFramebuffer();
		else
			return screenDrawer.PresentFrame();
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
