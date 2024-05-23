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
#pragma once
#include "vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "commandpool.h"
#include "pipeline.h"
#include "shaders.h"

#include <memory>
#include <vector>

void os_VideoRoutingTermVk();

class BaseVulkanRenderer : public Renderer
{
protected:
	bool BaseInit(vk::RenderPass renderPass, int subpass = 0);

public:
	void Term() override;
	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override;
	void Process(TA_context* ctx) override;
	void ReInitOSD();
	void DrawOSD(bool clear_screen) override;
	void RenderFramebuffer(const FramebufferInfo& info) override;
	void RenderVideoRouting();

	bool GetLastFrame(std::vector<u8>& data, int& width, int& height) override {
		return GetContext()->GetLastFrame(data, width, height);
	}

protected:
	BaseVulkanRenderer() : viewport(640, 480) {}

	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	virtual void resize(int w, int h)
	{
		viewport.width = w;
		viewport.height = h;
	}

	void CheckFogTexture();
	void CheckPaletteTexture();
	bool presentFramebuffer();

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
