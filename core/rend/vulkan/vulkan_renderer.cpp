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
		screenDrawer.Term();
		textureDrawer.Term();
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
				screenDrawer.Init(&samplerManager, &shaderManager, viewport);
				BaseInit(screenDrawer.GetRenderPass());
				emulateFramebuffer = config::EmulateFramebuffer;
			}
			Drawer *drawer;
			if (pvrrc.isRTT)
				drawer = &textureDrawer;
			else {
				resize(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
				drawer = &screenDrawer;
			}

			drawer->Draw(fogTexture.get(), paletteTexture.get());
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
