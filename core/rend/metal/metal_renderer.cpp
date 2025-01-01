/*
    Copyright 2024 flyinghead

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

#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

#include "metal_renderer.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_mem.h"

bool MetalRenderer::Init()
{
    NOTICE_LOG(RENDERER, "Metal renderer initializing");

    pipelineManager = MetalPipelineManager(this);
    shaders = MetalShaders();
    samplers = MetalSamplers();

    frameRendered = false;

    return true;
}

void MetalRenderer::Term() {
    pipelineManager.term();
    shaders.term();
    samplers.term();
    fogTexture = nullptr;
    paletteTexture = nullptr;
}

void MetalRenderer::Process(TA_context *ctx) {
    if (!ctx->rend.isRTT) {
        frameRendered = false;
        if (!config::EmulateFramebuffer)
            clearLastFrame = false;
    }

    ta_parse(ctx, true);

    // TODO can't update fog or palette twice in multi render
    CheckFogTexture();
    CheckPaletteTexture();
}

bool MetalRenderer::Render() {
    if (pvrrc.isRTT) {

    }
    else {

    }

    Draw(fogTexture.get(), paletteTexture.get());
    // if (config::EmulateFramebuffer || pvrrc.isRTT)
    //     // delay ending the render pass in case of multi render
    //     EndRenderPass();

    return true;
}

void MetalRenderer::EndRenderPass() {
    if (!renderPassStarted)
        return;

    frameRendered = true;
}

void MetalRenderer::RenderFramebuffer(const FramebufferInfo &info) {

}

BaseTextureCacheData *MetalRenderer::GetTexture(TSP tsp, TCW tcw) {
    MetalTexture* tf = textureCache.getTextureCacheData(tsp, tcw);

    if (tf->NeedsUpdate()) {
        if (!tf->Update()) {
            tf= nullptr;
        }
    }
    else if (tf->IsCustomTextureAvailable()) {
        // TODO
    }

    return tf;
}

void MetalRenderer::CheckFogTexture() {
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

    fogTexture->UploadToGPU(128, 2, texData, false);
}

void MetalRenderer::CheckPaletteTexture() {
    if (!paletteTexture)
    {
        paletteTexture = std::make_unique<MetalTexture>();
        paletteTexture->tex_type = TextureType::_8;
    }
    else if (!updatePalette)
        return;
    updatePalette = false;

    paletteTexture->UploadToGPU(1024, 1, (u8 *)palette32_ram, false);
}

TileClipping MetalRenderer::SetTileClip(MTL::RenderCommandEncoder *encoder, u32 val, MTL::ScissorRect& clipRect) {
    int rect[4] = {};
    TileClipping clipMode = GetTileClip(val, matrices.GetViewportMatrix(), rect);
    if (clipMode != TileClipping::Off)
    {
        clipRect.x = rect[0];
        clipRect.y = rect[1];
        clipRect.width = rect[2];
        clipRect.height = rect[3];
    }
    if (clipMode == TileClipping::Outside)
        SetScissor(encoder, clipRect);
    else
        SetScissor(encoder, baseScissor);

    return clipMode;
}


void MetalRenderer::SetBaseScissor(MTL::Viewport viewport) {
    bool wide_screen_on = config::Widescreen
            && !matrices.IsClipped() && !config::Rotate90 && !config::EmulateFramebuffer;
    if (!wide_screen_on)
    {
        float width;
        float height;
        float min_x;
        float min_y;
        glm::vec4 clip_min(pvrrc.fb_X_CLIP.min, pvrrc.fb_Y_CLIP.min, 0, 1);
        glm::vec4 clip_dim(pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1,
        pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1, 0, 0);
        clip_min = matrices.GetScissorMatrix() * clip_min;
        clip_dim = matrices.GetScissorMatrix() * clip_dim;

        min_x = clip_min[0];
        min_y = clip_min[1];
        width = clip_dim[0];
        height = clip_dim[1];
        if (width < 0)
        {
            min_x += width;
            width = -width;
        }
        if (height < 0)
        {
            min_y += height;
            height = -height;
        }

        baseScissor = MTL::ScissorRect();
        baseScissor.x = std::max(lroundf(min_x), 0L);
        baseScissor.y = std::max(lroundf(min_y), 0L);
        baseScissor.width = std::max(lroundf(width), 0L);
        baseScissor.height = std::max(lroundf(height), 0L);
    }
    else
    {
        baseScissor = MTL::ScissorRect();
        baseScissor.x = 0;
        baseScissor.y = 0;
        baseScissor.width = viewport.width;
        baseScissor.height = viewport.height;
    }
}

void MetalRenderer::DrawPoly(MTL::RenderCommandEncoder *encoder, u32 listType, bool sortTriangles, const PolyParam &poly, u32 first, u32 count)
{
    encoder->pushDebugGroup(NS::String::string("DrawPoly", NS::UTF8StringEncoding));

    MTL::ScissorRect scissorRect {};
    TileClipping tileClip = SetTileClip(encoder, poly.tileclip, scissorRect);

    float trilinearAlpha = 1.0f;
    if (poly.tsp.FilterMode > 1 && poly.pcw.Texture && listType != ListType_Punch_Through && poly.tcw.MipMapped == 1)
    {
        trilinearAlpha = 0.25f * (poly.tsp.MipMapD & 0x3);
        if (poly.tsp.FilterMode == 2)
            // Trilinear pass A
            trilinearAlpha = 1.0f - trilinearAlpha;
    }
    int gpuPalette = poly.texture == nullptr || !poly.texture->gpuPalette ? 0
            : poly.tsp.FilterMode + 1;
    float palette_index = 0.0f;
    if (gpuPalette != 0)
    {
        if (config::TextureFiltering == 1)
            gpuPalette = 1;
        else if (config::TextureFiltering == 2)
            gpuPalette = 2;
        if (poly.tcw.PixelFmt == PixelPal4)
            palette_index = float(poly.tcw.PalSelect << 4) / 1023.0f;
        else
            palette_index = float(poly.tcw.PalSelect >> 4 << 8) / 1023.0f;
    }

    if (tileClip == TileClipping::Inside || trilinearAlpha != 1.0f || gpuPalette != 0)
    {
        const std::array<float, 6> pushConstants = {
            (float)scissorRect.x,
            (float)scissorRect.y,
            (float)scissorRect.x + (float)scissorRect.width,
            (float)scissorRect.y + (float)scissorRect.height,
            trilinearAlpha,
            palette_index
        };

        encoder->setFragmentBytes(pushConstants.data(), sizeof(pushConstants), 1);
    }

    encoder->setRenderPipelineState(pipelineManager.GetPipeline(listType, sortTriangles, poly, gpuPalette, dithering));
    encoder->setDepthStencilState(pipelineManager.GetDepthStencilStates(listType, sortTriangles, poly, gpuPalette, dithering));

    bool shadowed = listType == ListType_Opaque || listType == ListType_Punch_Through;
    if (shadowed) {
        if (poly.pcw.Shadow != 0) {
            encoder->setStencilReferenceValue(0x80);
        } else {
            encoder->setStencilReferenceValue(0);
        }
    }

    if (poly.texture != nullptr) {
        auto texture = ((MetalTexture *)poly.texture)->texture;
        encoder->setFragmentTexture(texture, 0);

        // Texture sampler
        encoder->setFragmentSamplerState(samplers.GetSampler(poly, listType == ListType_Punch_Through), 0);
    }

    // Fog sampler
    TSP fogTsp = {};
    fogTsp.FilterMode = 1;
    fogTsp.ClampU = 1;
    fogTsp.ClampV = 1;
    encoder->setFragmentSamplerState(samplers.GetSampler(fogTsp), 2);

    // Palette sampler
    TSP palTsp = {};
    palTsp.FilterMode = 0;
    palTsp.ClampU = 1;
    palTsp.ClampV = 1;
    encoder->setFragmentSamplerState(samplers.GetSampler(palTsp), 3);

    if (poly.pcw.Texture || poly.isNaomi2())
    {
        u32 index = 0;
        if (poly.isNaomi2())
        {

        }

        // TODO: Bind Texture & Naomi2 Lights Buffers
    }

    MTL::PrimitiveType primitive = sortTriangles && !config::PerStripSorting ? MTL::PrimitiveTypeTriangle : MTL::PrimitiveTypeTriangleStrip;

    encoder->drawIndexedPrimitives(primitive, count, MTL::IndexTypeUInt32, curMainBuffer, offsets.indexOffset, 1);
    encoder->popDebugGroup();
}

void MetalRenderer::DrawSorted(MTL::RenderCommandEncoder *encoder, const std::vector<SortedTriangle> &polys, u32 first, u32 last, bool multipass)
{
    if (first == last)
        return;

    encoder->pushDebugGroup(NS::String::string("DrawSorted", NS::UTF8StringEncoding));

    for (u32 idx = first; idx < last; idx++)
        DrawPoly(encoder, ListType_Translucent, true, pvrrc.global_param_tr[polys[idx].polyIndex], polys[idx].first, polys[idx].count);
    if (multipass && config::TranslucentPolygonDepthMask)
    {
        // Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
        for (u32 idx = first; idx < last; idx++)
        {
            const SortedTriangle& param = polys[idx];
            const PolyParam& polyParam = pvrrc.global_param_tr[param.polyIndex];
            if (polyParam.isp.ZWriteDis)
                continue;
            encoder->setRenderPipelineState(pipelineManager.GetDepthPassPipeline(polyParam.isp.CullMode, polyParam.isNaomi2()));
            encoder->setDepthStencilState(pipelineManager.GetDepthPassDepthStencilStates(polyParam.isp.CullMode, polyParam.isNaomi2()));
            MTL::ScissorRect scissorRect {};
            SetTileClip(encoder, polyParam.tileclip, scissorRect);
            encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, param.count, MTL::IndexTypeUInt32, curMainBuffer, offsets.indexOffset + pvrrc.idx.size() + param.first, 1);
        }
    }

    encoder->popDebugGroup();
}

void MetalRenderer::DrawList(MTL::RenderCommandEncoder *encoder, u32 listType, bool sortTriangles, const std::vector<PolyParam> &polys, u32 first, u32 last)
{
    if (first == last)
        return;

    encoder->pushDebugGroup(NS::String::string("DrawList", NS::UTF8StringEncoding));

    const PolyParam *pp_end = polys.data() + last;
    for (const PolyParam *pp = &polys[first]; pp != pp_end; pp++)
        if (pp->count > 2)
            DrawPoly(encoder, listType, sortTriangles, *pp, pp->first, pp->count);

    encoder->popDebugGroup();
}

void MetalRenderer::DrawModVols(MTL::RenderCommandEncoder *encoder, int first, int count)
{
    if (count == 0 || pvrrc.modtrig.empty() || !config::ModifierVolumes)
        return;

    encoder->pushDebugGroup(NS::String::string("DrawModVols", NS::UTF8StringEncoding));

    ModifierVolumeParam* params = &pvrrc.global_param_mvo[first];

    int mod_base = -1;

    encoder->popDebugGroup();
}

void MetalRenderer::UploadMainBuffer(const VertexShaderUniforms &vertexUniforms, const FragmentShaderUniforms &fragmentUniforms) {
    BufferPacker packer;

    // Vertex
    packer.add(pvrrc.verts.data(), pvrrc.verts.size() * sizeof(decltype(*pvrrc.verts.data())));
    // Modifier Volumes
    offsets.modVolOffset = packer.add(pvrrc.modtrig.data(), pvrrc.modtrig.size() * sizeof(decltype(*pvrrc.modtrig.data())));
    // Index
    offsets.indexOffset = packer.add(pvrrc.idx.data(), pvrrc.idx.size() * sizeof(decltype(*pvrrc.idx.data())));
    // Uniform buffers
    offsets.vertexUniformOffset = packer.addUniform(&vertexUniforms, sizeof(vertexUniforms));
    offsets.fragmentUniformOffset = packer.addUniform(&fragmentUniforms, sizeof(fragmentUniforms));

    std::vector<u8> n2uniforms;
    if (settings.platform.isNaomi2())
    {
        // packNaomi2Uniforms(packer, offsets, n2uniforms, false);
        // offsets.lightsOffset = packNaomi2Lights(packer);
    }

    MetalBufferData *buffer = GetMainBuffer(packer.size());
    packer.upload(*buffer);
    curMainBuffer = buffer->buffer;
}

bool MetalRenderer::Draw(const MetalTexture *fogTexture, const MetalTexture *paletteTexture) {
    FragmentShaderUniforms fragUniforms = MakeFragmentUniforms<FragmentShaderUniforms>();
    dithering = config::EmulateFramebuffer && pvrrc.fb_W_CTRL.fb_dither && pvrrc.fb_W_CTRL.fb_packmode <= 3;
    if (dithering) {
        switch (pvrrc.fb_W_CTRL.fb_packmode)
        {
            case 0: // 0555 KRGB 16 bit
            case 3: // 1555 ARGB 16 bit
                fragUniforms.ditherColorMax[0] = fragUniforms.ditherColorMax[1] = fragUniforms.ditherColorMax[2] = 31.f;
            fragUniforms.ditherColorMax[3] = 255.f;
            break;
            case 1: // 565 RGB 16 bit
                fragUniforms.ditherColorMax[0] = fragUniforms.ditherColorMax[2] = 31.f;
            fragUniforms.ditherColorMax[1] = 63.f;
            fragUniforms.ditherColorMax[3] = 255.f;
            break;
            case 2: // 4444 ARGB 16 bit
                fragUniforms.ditherColorMax[0] = fragUniforms.ditherColorMax[1]
                    = fragUniforms.ditherColorMax[2] = fragUniforms.ditherColorMax[3] = 15.f;
            break;
            default:
                break;
        }
    }

    currentScissor = MTL::ScissorRect {};

    if (frameBuffer != nullptr) {
        frameBuffer->setPurgeableState(MTL::PurgeableStateEmpty);
        frameBuffer->release();
        frameBuffer = nullptr;
    }

    if (depthBuffer != nullptr) {
        depthBuffer->setPurgeableState(MTL::PurgeableStateEmpty);
        depthBuffer->release();
        depthBuffer = nullptr;
    }

    MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();
    desc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    desc->setWidth(pvrrc.framebufferWidth);
    desc->setHeight(pvrrc.framebufferHeight);
    desc->setUsage(MTL::TextureUsagePixelFormatView | MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite | MTL::TextureUsageRenderTarget);

    frameBuffer = MetalContext::Instance()->GetDevice()->newTexture(desc);
    desc->release();

    MTL::TextureDescriptor *depthDesc = MTL::TextureDescriptor::alloc()->init();
    depthDesc->setPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
    depthDesc->setWidth(pvrrc.framebufferWidth);
    depthDesc->setHeight(pvrrc.framebufferHeight);
    depthDesc->setUsage(MTL::TextureUsagePixelFormatView | MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite | MTL::TextureUsageRenderTarget);

    depthBuffer = MetalContext::Instance()->GetDevice()->newTexture(depthDesc);
    depthDesc->release();

    auto drawable = MetalContext::Instance()->GetLayer()->nextDrawable();

    MTL::CommandBuffer *buffer = MetalContext::Instance()->commandBuffer;
    MTL::RenderPassDescriptor *descriptor = MTL::RenderPassDescriptor::alloc()->init();
    auto color = descriptor->colorAttachments()->object(0);
    color->setTexture(frameBuffer);
    color->setLoadAction(MTL::LoadActionClear);
    color->setStoreAction(MTL::StoreActionStore);

    MTL::RenderPassDepthAttachmentDescriptor *depthAttachmentDescriptor = MTL::RenderPassDepthAttachmentDescriptor::alloc()->init();
    depthAttachmentDescriptor->setTexture(depthBuffer);
    depthAttachmentDescriptor->setLoadAction(MTL::LoadActionClear);
    depthAttachmentDescriptor->setStoreAction(MTL::StoreActionStore);

    MTL::RenderPassStencilAttachmentDescriptor *stencilAttachmentDescriptor = MTL::RenderPassStencilAttachmentDescriptor::alloc()->init();
    stencilAttachmentDescriptor->setTexture(depthBuffer);
    stencilAttachmentDescriptor->setLoadAction(MTL::LoadActionClear);
    stencilAttachmentDescriptor->setStoreAction(MTL::StoreActionStore);

    descriptor->setDepthAttachment(depthAttachmentDescriptor);
    descriptor->setStencilAttachment(stencilAttachmentDescriptor);

    depthAttachmentDescriptor->release();
    stencilAttachmentDescriptor->release();

    MTL::RenderCommandEncoder *encoder = buffer->renderCommandEncoder(descriptor);

    descriptor->release();

    if (fogTexture == nullptr) {
        encoder->setFragmentTexture(fogTexture->texture, 2);
    }

    if (paletteTexture == nullptr) {
        encoder->setFragmentTexture(paletteTexture->texture, 3);
    }

    MTL::CaptureManager *captureManager;

    // if (pvrrc.render_passes.size() > 0 && frameIndex >= 0) {
    //     MTL::CaptureDescriptor *capture = MTL::CaptureDescriptor::alloc()->init();
    //     capture->setCaptureObject(MetalContext::Instance()->GetDevice());
    //     capture->setDestination(MTL::CaptureDestinationGPUTraceDocument);
    //     std::string filePath = "/Users/isaacmarovitz/Documents/TRACES/flycast-" + std::to_string(frameIndex) + ".gputrace";
    //     capture->setOutputURL(NS::URL::fileURLWithPath(NS::String::string(filePath.c_str(), NS::UTF8StringEncoding)));
    //
    //     captureManager = MTL::CaptureManager::sharedCaptureManager();
    //
    //     NS::Error *error = nullptr;
    //     if (!captureManager->startCapture(capture, &error)) {
    //         ERROR_LOG(RENDERER, "Failed to start capture, %s", error->localizedDescription()->utf8String());
    //     }
    // }
    //
    // frameIndex++;

    // Upload vertex and index buffers
    VertexShaderUniforms vtxUniforms;
    vtxUniforms.ndcMat = matrices.GetNormalMatrix();

    UploadMainBuffer(vtxUniforms, fragUniforms);

    encoder->setVertexBuffer(curMainBuffer, offsets.vertexUniformOffset, 0);
    encoder->setFragmentBuffer(curMainBuffer, offsets.fragmentUniformOffset, 0);

    RenderPass previous_pass {};
    for (int render_pass = 0; render_pass < (int)pvrrc.render_passes.size(); render_pass++) {
        const RenderPass& current_pass = pvrrc.render_passes[render_pass];

        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
                current_pass.op_count - previous_pass.op_count,
                current_pass.pt_count - previous_pass.pt_count,
                current_pass.tr_count - previous_pass.tr_count,
                current_pass.mvo_count - previous_pass.mvo_count, current_pass.autosort);
        DrawList(encoder, ListType_Opaque, false, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count);
        DrawList(encoder, ListType_Punch_Through, false, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count);
        DrawModVols(encoder, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);
        if (current_pass.autosort) {
            if (!config::PerStripSorting)
                DrawSorted(encoder, pvrrc.sortedTriangles, previous_pass.sorted_tr_count, current_pass.sorted_tr_count, render_pass + 1 < (int)pvrrc.render_passes.size());
        } else {
            DrawList(encoder, ListType_Translucent, false, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
        }
    }

    encoder->endEncoding();
    buffer->presentDrawable(drawable);
    buffer->commit();

    //
    // if (pvrrc.render_passes.size() > 0) {
    //     captureManager->stopCapture();
    // }

    buffer->release();
    encoder->release();

    DEBUG_LOG(RENDERER, "Render command buffer released");

    MetalContext::Instance()->commandBuffer = MetalContext::Instance()->GetQueue()->commandBuffer();
    return !pvrrc.isRTT;
}

Renderer* rend_Metal()
{
    return new MetalRenderer();
}
