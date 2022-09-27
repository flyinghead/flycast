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
#include "shaders.h"

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

		return true;
	}

	void Resize(int w, int h) override
	{
		if ((u32)w == viewport.width && (u32)h == viewport.height)
			return;
		BaseVulkanRenderer::Resize(w, h);
		GetContext()->WaitIdle();
		screenDrawer.Init(&samplerManager, &shaderManager, viewport);
	}

	void Term() override
	{
		DEBUG_LOG(RENDERER, "VulkanRenderer::Term");
		GetContext()->WaitIdle();
		BaseVulkanRenderer::Term();
	}

	bool Render() override
	{
		try {
			Drawer *drawer;
			if (pvrrc.isRTT)
				drawer = &textureDrawer;
			else
				drawer = &screenDrawer;

			drawer->Draw(fogTexture.get(), paletteTexture.get());

#ifdef LIBRETRO
			if (!pvrrc.isRTT)
				overlay->Draw(screenDrawer.GetCurrentCommandBuffer(), viewport, (int)config::RenderResolution / 480.f, true, true);
#endif

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
		return screenDrawer.PresentFrame();
	}

private:
	SamplerManager samplerManager;
	ScreenDrawer screenDrawer;
	TextureDrawer textureDrawer;
};

Renderer* rend_Vulkan()
{
	return new VulkanRenderer();
}

void ReInitOSD()
{
	if (renderer != nullptr)
		((BaseVulkanRenderer *)renderer)->ReInitOSD();
}
