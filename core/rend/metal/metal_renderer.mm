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

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>

#include "metal_renderer.h"
#include "hw/aica/dsp.h"
#include "hw/pvr/ta.h"

bool BaseMetalRenderer::BaseInit(id<MTLRenderCommandEncoder> commandEncoder)
{
    return true;
}

void BaseMetalRenderer::Term()
{
    WaitIdle();
    MetalContext::Instance()->PresentFrame(nil, MTLViewport {}, 0);
    textureCache.Clear();
    fogTexture = nil;
    paletteTexture = nil;
    framebufferTextures.clear();
    framebufferTexIndex = 0;
    shaderManager.term();
}

BaseTextureCacheData *BaseMetalRenderer::GetTexture(TSP tsp, TCW tcw)
{
    MetalTexture* tf = textureCache.getTextureCacheData(tsp, tcw);

    if (tf->NeedsUpdate()) {
        tf->SetCommandBuffer(texCommandBuffer);

        if (!tf->Update()) {
            tf = nullptr;
            return nullptr;
        }
    }
    else if (tf->IsCustomTextureAvailable()) {
        // TODO
        tf->SetCommandBuffer(texCommandBuffer);
    }
    tf->SetCommandBuffer(nil);

    return tf;
}

void BaseMetalRenderer::Process(TA_context *ctx)
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

    texCommandBuffer = [MetalContext::Instance()->GetQueue() commandBuffer];

    ta_parse(ctx, true);

    // TODO can't update fog or palette twice in multi render
    CheckFogTexture();
    CheckPaletteTexture();
    [texCommandBuffer commit];
    texCommandBuffer = nil;
}

void BaseMetalRenderer::ReInitOSD()
{

}

void BaseMetalRenderer::RenderFramebuffer(const FramebufferInfo &info)
{
    framebufferTexIndex = (framebufferTexIndex + 1) % 3;

    if (framebufferTextures.size() != 3)
        framebufferTextures.resize(3);

    std::unique_ptr<MetalTexture>& curTexture = framebufferTextures[framebufferTexIndex];
    if (!curTexture)
    {
        curTexture = std::make_unique<MetalTexture>();
        curTexture->tex_type = TextureType::_8888;
    }
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

    framebufferRendered = true;
    clearLastFrame = false;
}

void BaseMetalRenderer::WaitIdle()
{
    [commandBuffer waitUntilCompleted];
    commandBuffer = nil;
}

void BaseMetalRenderer::CheckFogTexture() {
    if (!fogTexture)
    {
        fogTexture = std::make_unique<MetalTexture>();
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
    fogTexture->SetCommandBuffer(nil);
}

void BaseMetalRenderer::CheckPaletteTexture() {
    if (!paletteTexture)
    {
        paletteTexture = std::make_unique<MetalTexture>();
        paletteTexture->tex_type = TextureType::_8888;
    }
    else if (!updatePalette)
        return;
    updatePalette = false;

    paletteTexture->SetCommandBuffer(texCommandBuffer);
    paletteTexture->UploadToGPU(1024, 1, (u8 *)palette32_ram, false);
    paletteTexture->SetCommandBuffer(nil);
}

bool BaseMetalRenderer::presentFramebuffer()
{
    if (framebufferTexIndex >= (int)framebufferTextures.size())
        return false;
    MetalTexture *fbTexture = framebufferTextures[framebufferTexIndex].get();
    if (fbTexture == nullptr)
        return false;

    MTLViewport viewport = { 0, 0, (float)fbTexture->GetTexture().width, (float)fbTexture->GetTexture().height, 1.0, 0 };

    MetalContext::Instance()->PresentFrame(fbTexture->GetTexture(), viewport,
                                           getDCFramebufferAspectRatio());
    return true;
}

class MetalRenderer final : public BaseMetalRenderer
{
public:
    bool Init()
    {
        NOTICE_LOG(RENDERER, "MetalRenderer::Init");

        textureDrawer.Init(&samplerManager, &shaderManager, &textureCache);
        screenDrawer.Init(&samplerManager, &shaderManager, viewport);
        // BaseInit(screenDrawer.GetRenderPass());
        emulateFramebuffer = config::EmulateFramebuffer;

        return true;
    }

    void Term()
    {
        NOTICE_LOG(RENDERER, "MetalRenderer::Term");
        WaitIdle();
        screenDrawer.Term();
        textureDrawer.Term();
        samplerManager.term();
        BaseMetalRenderer::Term();
    }

    void Process(TA_context* ctx) override
    {
        if (emulateFramebuffer != config::EmulateFramebuffer)
        {
            screenDrawer.EndRenderPass();
            WaitIdle();
            screenDrawer.Term();
            screenDrawer.Init(&samplerManager, &shaderManager, viewport);
            // BaseInit(screenDrawer.GetRenderPass());
            emulateFramebuffer = config::EmulateFramebuffer;
        }
        else if (ctx->rend.isRTT) {
            screenDrawer.EndRenderPass();
        }
        BaseMetalRenderer::Process(ctx);
    }

    bool Render() override
    {
        id<MTLCommandBuffer> commandBuffer = [MetalContext::Instance()->GetQueue() commandBuffer];

        MetalDrawer *drawer;
        if (pvrrc.isRTT)
            drawer = &textureDrawer;
        else {
            resize(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
            drawer = &screenDrawer;
        }

        drawer->Draw(fogTexture.get(), paletteTexture.get(), commandBuffer);
        // TODO: ENABLE LATER WHEN WE CAN
        //if (config::EmulateFramebuffer || pvrrc.isRTT)
            // delay ending the render pass in case of multi render
        drawer->EndRenderPass();

        [commandBuffer commit];

        return !pvrrc.isRTT;
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
        if ((u32)w == viewport.width && (u32)h == viewport.height)
            return;
        BaseMetalRenderer::resize(w, h);
        WaitIdle();
        screenDrawer.Init(&samplerManager, &shaderManager, viewport);
    }

private:
    MetalSamplers samplerManager;
    MetalScreenDrawer screenDrawer;
    MetalTextureDrawer textureDrawer;
    bool emulateFramebuffer = false;
};

Renderer* rend_Metal()
{
    return new MetalRenderer();
}

void MetalReInitOSD()
{
    if (renderer != nullptr) {
        BaseMetalRenderer *mtlrenderer = dynamic_cast<BaseMetalRenderer*>(renderer);
        if (mtlrenderer != nullptr)
            mtlrenderer->ReInitOSD();
    }
}