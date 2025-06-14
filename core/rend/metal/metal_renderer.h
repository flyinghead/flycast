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
#include "metal_pipeline.h"
#include "metal_shaders.h"
#include "metal_texture.h"
#include "metal_buffer.h"
#include "metal_drawer.h"

#include "hw/pvr/Renderer_if.h"
#include "rend/tileclip.h"
#include "rend/transform_matrix.h"

class BaseMetalRenderer : public Renderer
{
protected:
    bool BaseInit(id<MTLRenderCommandEncoder> commandEncoder);

public:
    void Term() override;
    BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override;
    void Process(TA_context* ctx) override;
    void ReInitOSD();
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

    void CheckFogTexture();
    void CheckPaletteTexture();
    bool presentFramebuffer();

    MetalShaders shaderManager;
    std::unique_ptr<MetalTexture> fogTexture;
    std::unique_ptr<MetalTexture> paletteTexture;
    id<MTLCommandBuffer> commandBuffer = nil;
    id<MTLCommandBuffer> texCommandBuffer = nil;
    std::vector<std::unique_ptr<MetalTexture>> framebufferTextures;
    int framebufferTexIndex = 0;
    MetalTextureCache textureCache;
    MTLViewport viewport = MTLViewport { 0, 0, 640, 480, 0, 0 };
    bool framebufferRendered = false;
};

