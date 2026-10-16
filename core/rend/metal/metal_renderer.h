/*
	Copyright 2025 flyinghead

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
#include "metal_apple.h"
#include "metal_pipeline.h"
#include "metal_shaders.h"
#include "metal_texture.h"
#include "metal_buffer.h"
#include "metal_drawer.h"
#include "metal_commandpool.h"

#include "hw/pvr/Renderer_if.h"
#include "rend/texconv.h"
#include "rend/transform_matrix.h"

class BaseMetalRenderer : public Renderer
{
protected:
	bool BaseInit();

public:
	void Term() override;
	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw, int area) override;
	void Process(TA_context* ctx) override;
	void RenderFramebuffer(const FramebufferInfo& info) override;
	void WaitIdle();

	bool RenderLastFrame() override {
		return !clearLastFrame;
	}

	bool GetLastFrame(std::vector<u8>& data, int& width, int& height) override {
		return MetalContext::Instance()->GetLastFrame(data, width, height);
	}

protected:
	virtual void resize(int w, int h)
	{
		viewport.width = w;
		viewport.height = h;
	}

	void clearTextureCache() override { textureCache.Clear(); }

	void CheckFogTexture();
	void CheckPaletteTexture();
	bool presentFramebuffer();

	MetalShaders shaderManager;
	std::unique_ptr<MetalTexture> fogTexture;
	std::unique_ptr<MetalTexture> paletteTexture;
	MetalCommandPool texCommandPool;
	MetalCommandPool fbCommandPool;
	id<MTLCommandBuffer> texCommandBuffer = nil;
	std::vector<std::unique_ptr<MetalTexture>> framebufferTextures;
	int framebufferTexIndex = 0;
	// Scratch buffer for RenderFramebuffer(). A member, not a local, so the
	// ~1.2MB (640x480x4) allocation is made once instead of every vblank --
	// RenderFramebuffer runs per-vblank under EmulateFramebuffer and on every
	// direct VRAM framebuffer write (BIOS, menus, load screens, 2D titles).
	// Render-thread only; holds no game state across a reload.
	PixelBuffer<u32> framebufferScratch;
	MetalTextureCache textureCache;
	MTLViewport viewport = MTLViewport { 0, 0, 640, 480, 0, 0 };
	bool framebufferRendered = false;
	// The context being rendered (set in Process, handed to the drawers in
	// Render). Replaces the removed _pvrrc global (90301d5c3).
	rend_context *rendContext = nullptr;
};

