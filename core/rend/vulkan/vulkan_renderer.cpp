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
#include "vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "commandpool.h"
#include "drawer.h"
#include "shaders.h"

extern bool ProcessFrame(TA_context* ctx);

class VulkanRenderer : public Renderer
{
public:
	bool Init() override
	{
		printf("VulkanRenderer::Init\n");
		shaderManager.Init();
		texCommandPool.Init();

		textureDrawer.Init(&samplerManager, &shaderManager);
		textureDrawer.SetCommandPool(&texCommandPool);
		screenDrawer.Init(&samplerManager, &shaderManager);

		return true;
	}

	void Resize(int w, int h) override
	{
	}

	void Term() override
	{
		printf("VulkanRenderer::Term\n");
		GetContext()->WaitIdle();
		killtex();
		texCommandPool.Term();
		shaderManager.Term();
	}

	bool Process(TA_context* ctx) override
	{
		if (ctx->rend.isRenderFramebuffer)
		{
			// TODO		RenderFramebuffer();
			return false;
		}

		texCommandPool.BeginFrame();

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
		if (pvrrc.isRTT)
			return textureDrawer.Draw(fogTexture.get());
		else
			return screenDrawer.Draw(fogTexture.get());
	}

	void Present() override
	{
		GetContext()->Present();
	}

	virtual u64 GetTexture(TSP tsp, TCW tcw) override
	{
		Texture* tf = static_cast<Texture*>(getTextureCacheData(tsp, tcw, [](){
			return (BaseTextureCacheData *)new Texture(VulkanContext::Instance()->GetPhysicalDevice(), *VulkanContext::Instance()->GetDevice());
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
	TextureDrawer textureDrawer;
};

Renderer* rend_Vulkan()
{
	return new VulkanRenderer();
}
