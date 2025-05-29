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

#include <Foundation/Foundation.h>
#include <Metal/Metal.h>

#include "metal_renderer.h"
#include "hw/aica/dsp.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/sorter.h"

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
    WaitIdle();
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

    // TODO: Don't hardcode these values
    matrices.CalcMatrices(&pvrrc, 1920, 1080);
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

void MetalRenderer::WaitIdle() {
    [commandBuffer waitUntilCompleted];
    commandBuffer = nil;
}

TileClipping MetalRenderer::SetTileClip(id<MTLRenderCommandEncoder> encoder, u32 val, MTLScissorRect& clipRect) {
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


void MetalRenderer::SetBaseScissor(MTLViewport viewport) {
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

        baseScissor = MTLScissorRect();
        baseScissor.x = std::max(lroundf(min_x), 0L);
        baseScissor.y = std::max(lroundf(min_y), 0L);
        baseScissor.width = std::max(lroundf(width), 0L);
        baseScissor.height = std::max(lroundf(height), 0L);
    }
    else
    {
        baseScissor = MTLScissorRect();
        baseScissor.x = 0;
        baseScissor.y = 0;
        baseScissor.width = viewport.width;
        baseScissor.height = viewport.height;
    }
}

void MetalRenderer::DrawPoly(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const PolyParam &poly, u32 first, u32 count)
{
    MTLScissorRect scissorRect {};
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

        [encoder setFragmentBytes:pushConstants.data() length:sizeof(pushConstants) + MetalBufferPacker::align(sizeof(pushConstants), 16) atIndex:1];
    }

    bool shadowed = listType == ListType_Opaque || listType == ListType_Punch_Through;

    [encoder setRenderPipelineState:pipelineManager.GetPipeline(listType, sortTriangles, poly, gpuPalette, dithering)];
    [encoder setDepthStencilState:pipelineManager.GetDepthStencilStates(listType, sortTriangles, shadowed, poly)];

    if (shadowed) {
        if (poly.pcw.Shadow != 0) {
            [encoder setStencilReferenceValue:0x80];
        } else {
            [encoder setStencilReferenceValue:0];
        }
    }

    if (poly.texture != nullptr) {
        auto texture = ((MetalTexture *)poly.texture)->texture;
        [encoder setFragmentTexture:texture atIndex:0];

        // Texture sampler
        [encoder setFragmentSamplerState:samplers.GetSampler(poly, listType == ListType_Punch_Through) atIndex:0];
    }

    if (poly.pcw.Texture || poly.isNaomi2())
    {
        u32 index = 0;
        if (poly.isNaomi2())
        {

        }

        // TODO: Bind Texture & Naomi2 Lights Buffers
    }

    MTLPrimitiveType primitive = sortTriangles && !config::PerStripSorting ? MTLPrimitiveTypeTriangle : MTLPrimitiveTypeTriangleStrip;

    [encoder drawIndexedPrimitives:primitive
                        indexCount:count
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:curMainBuffer
                 indexBufferOffset:offsets.indexOffset + first * sizeof(u32)];
}

void MetalRenderer::DrawSorted(id<MTLRenderCommandEncoder> encoder, const std::vector<SortedTriangle> &polys, u32 first, u32 last, bool multipass)
{
    if (first == last)
        return;

    [encoder pushDebugGroup:@"DrawSorted"];

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
            [encoder setRenderPipelineState:pipelineManager.GetDepthPassPipeline(polyParam.isp.CullMode, polyParam.isNaomi2())];
            [encoder setDepthStencilState:pipelineManager.GetDepthPassDepthStencilStates(polyParam.isp.CullMode, polyParam.isNaomi2())];
            MTLScissorRect scissorRect {};
            SetTileClip(encoder, polyParam.tileclip, scissorRect);
            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:param.count
                                 indexType:MTLIndexTypeUInt32
                               indexBuffer:curMainBuffer
                         indexBufferOffset:offsets.indexOffset + param.first * sizeof(u32)];
        }
    }

    [encoder popDebugGroup];
}

void MetalRenderer::DrawList(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const std::vector<PolyParam> &polys, u32 first, u32 last)
{
    if (first == last)
        return;

    [encoder pushDebugGroup:@"DrawList"];

    const PolyParam *pp_end = polys.data() + last;
    for (const PolyParam *pp = &polys[first]; pp != pp_end; pp++)
        if (pp->count > 2)
            DrawPoly(encoder, listType, sortTriangles, *pp, pp->first, pp->count);

    [encoder popDebugGroup];
}

void MetalRenderer::DrawModVols(id<MTLRenderCommandEncoder> encoder, int first, int count)
{
    if (count == 0 || pvrrc.modtrig.empty() || !config::ModifierVolumes)
        return;

    [encoder pushDebugGroup:@"DrawModVols"];
    [encoder setVertexBufferOffset:offsets.modVolOffset atIndex:30];

    ModifierVolumeParam* params = &pvrrc.global_param_mvo[first];

    int mod_base = -1;
    id<MTLRenderPipelineState> state;
    id<MTLDepthStencilState> depth_state;

    const std::array<float, 1> pushConstants = { 1 - FPU_SHAD_SCALE.scale_factor / 256.f };
    [encoder setFragmentBytes:pushConstants.data() length:sizeof(pushConstants) + MetalBufferPacker::align(sizeof(pushConstants), 16) atIndex:1];

    for (int cmv = 0; cmv < count; cmv++) {
        ModifierVolumeParam& param = params[cmv];
        MTLCullMode cull_mode = param.isp.CullMode == 3 ? MTLCullModeBack : param.isp.CullMode == 2 ? MTLCullModeFront : MTLCullModeNone;
        [encoder setCullMode:cull_mode];
        [encoder setFrontFacingWinding:MTLWindingCounterClockwise];

        if (param.count == 0)
            continue;

        u32 mv_mode = param.isp.DepthMode;

        if (mod_base == -1)
            mod_base = param.first;

        if (!param.isp.VolumeLast && mv_mode > 0) {
            state = pipelineManager.GetModifierVolumePipeline(ModVolMode::Or, param.isp.CullMode, param.isNaomi2());  // OR'ing (open volume or quad)
            depth_state = pipelineManager.GetModVolDepthStencilStates(ModVolMode::Or, param.isp.CullMode, param.isNaomi2());
        } else {
            state = pipelineManager.GetModifierVolumePipeline(ModVolMode::Xor, param.isp.CullMode, param.isNaomi2()); // XOR'ing (closed volume)
            depth_state = pipelineManager.GetModVolDepthStencilStates(ModVolMode::Xor, param.isp.CullMode, param.isNaomi2());
        }

        [encoder setRenderPipelineState:state];
        [encoder setDepthStencilState:depth_state];
        [encoder setStencilReferenceValue:2];
        MTLScissorRect scissorRect {};
        SetTileClip(encoder, param.tileclip, scissorRect);
        // TODO inside clipping

        [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                    vertexStart:param.first * 3
                    vertexCount:param.count * 3];

        if (mv_mode == 1 || mv_mode == 2)
        {
            // Sum the area
            state = pipelineManager.GetModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isp.CullMode, param.isNaomi2());
            depth_state = pipelineManager.GetModVolDepthStencilStates(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isp.CullMode, param.isNaomi2());
            [encoder setRenderPipelineState:state];
            [encoder setDepthStencilState:depth_state];
            [encoder setStencilReferenceValue:1];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle
                        vertexStart: mod_base * 3
                        vertexCount: (param.first + param.count - mod_base) * 3];
            mod_base = -1;
        }
    }
    [encoder setVertexBufferOffset:0 atIndex:30];

    state = pipelineManager.GetModifierVolumePipeline(ModVolMode::Final, 0, false);
    depth_state = pipelineManager.GetModVolDepthStencilStates(ModVolMode::Final, 0, false);
    [encoder setRenderPipelineState:state];
    [encoder setDepthStencilState:depth_state];
    [encoder setStencilReferenceValue:0x81];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip
                        indexCount:4
                         indexType:MTLIndexTypeUInt32
                       indexBuffer:curMainBuffer
                 indexBufferOffset:offsets.indexOffset];

    [encoder popDebugGroup];
}

void MetalRenderer::UploadMainBuffer(const VertexShaderUniforms &vertexUniforms, const FragmentShaderUniforms &fragmentUniforms) {
    MetalBufferPacker packer;

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
    matrices.CalcMatrices(&pvrrc);
    u32 origWidth = pvrrc.getFramebufferWidth();
    u32 origHeight = pvrrc.getFramebufferHeight();
    u32 upscaledWidth = origWidth;
    u32 upscaledHeight = origHeight;
    u32 widthPow2;
    u32 heightPow2;
    getRenderToTextureDimensions(upscaledWidth, upscaledHeight, widthPow2, heightPow2);

    FragmentShaderUniforms fragUniforms = MakeFragmentUniforms<FragmentShaderUniforms>();
    dithering = config::EmulateFramebuffer && pvrrc.fb_W_CTRL.fb_dither && pvrrc.fb_W_CTRL.fb_packmode <= 3;
    if (dithering) {
        switch (pvrrc.fb_W_CTRL.fb_packmode)
        {
            case 0: // 0555 KRGB 16 bit
            case 3: // 1555 ARGB 16 bit
                fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[1] = fragUniforms.ditherDivisor[2] = 2.f;
                break;
            case 1: // 565 RGB 16 bit
                fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[2] = 2.f;
                fragUniforms.ditherDivisor[1] = 4.f;
                break;
            case 2: // 4444 ARGB 16 bit
                fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[1] = fragUniforms.ditherDivisor[2] = 1.f;
                break;
            default:
                break;
        }
        fragUniforms.ditherDivisor[3] = 1.f;
    }

    currentScissor = MTLScissorRect {};

    if (!frameBuffer || widthPow2 > frameBuffer.width || heightPow2 > frameBuffer.height) {
        if (frameBuffer) {
            WaitIdle();
            [frameBuffer setPurgeableState:MTLPurgeableStateEmpty];
            frameBuffer = nil;
        }

        MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
        [desc setPixelFormat:MTLPixelFormatBGRA8Unorm];
        [desc setWidth:widthPow2];
        [desc setHeight:heightPow2];
        [desc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];

        frameBuffer = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:desc];
    }

    if (!depthBuffer || widthPow2 > depthBuffer.width || heightPow2 > depthBuffer.height) {
        if (depthBuffer) {
            WaitIdle();
            [depthBuffer setPurgeableState:MTLPurgeableStateEmpty];
            depthBuffer = nil;
        }

        MTLTextureDescriptor *depthDesc = [[MTLTextureDescriptor alloc] init];
        [depthDesc setPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
        [depthDesc setWidth:widthPow2];
        [depthDesc setHeight:heightPow2];
        [depthDesc setUsage:MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget];

        depthBuffer = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:depthDesc];
    }

    auto drawable = [MetalContext::Instance()->GetLayer() nextDrawable];

    commandBuffer = [MetalContext::Instance()->GetQueue() commandBuffer];
    MTLRenderPassDescriptor *descriptor = [[MTLRenderPassDescriptor alloc] init];
    auto color = [descriptor colorAttachments][0];
    [color setTexture:frameBuffer];
    [color setLoadAction:MTLLoadActionClear];
    [color setStoreAction:MTLStoreActionStore];

    MTLRenderPassDepthAttachmentDescriptor *depthAttachmentDescriptor = [[MTLRenderPassDepthAttachmentDescriptor alloc] init];
    [depthAttachmentDescriptor setTexture:depthBuffer];
    [depthAttachmentDescriptor setLoadAction:MTLLoadActionClear];
    [depthAttachmentDescriptor setStoreAction:MTLStoreActionDontCare];

    MTLRenderPassStencilAttachmentDescriptor *stencilAttachmentDescriptor = [[MTLRenderPassStencilAttachmentDescriptor alloc] init];
    [stencilAttachmentDescriptor setTexture:depthBuffer];
    [stencilAttachmentDescriptor setLoadAction:MTLLoadActionClear];
    [stencilAttachmentDescriptor setStoreAction:MTLStoreActionDontCare];

    [descriptor setDepthAttachment:depthAttachmentDescriptor];
    [descriptor setStencilAttachment:stencilAttachmentDescriptor];

    @autoreleasepool {
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:descriptor];

        [renderEncoder setFragmentTexture:fogTexture->texture atIndex:2];
        [renderEncoder setFragmentTexture:paletteTexture->texture atIndex:3];

        // Fog sampler
        TSP fogTsp = {};
        fogTsp.FilterMode = 1;
        fogTsp.ClampU = 1;
        fogTsp.ClampV = 1;
        [renderEncoder setFragmentSamplerState:samplers.GetSampler(fogTsp) atIndex:2];

        // Palette sampler
        TSP palTsp = {};
        palTsp.FilterMode = 0;
        palTsp.ClampU = 1;
        palTsp.ClampV = 1;
        [renderEncoder setFragmentSamplerState:samplers.GetSampler(palTsp) atIndex:3];

        setFirstProvokingVertex(pvrrc);

        // Upload vertex and index buffers
        VertexShaderUniforms vtxUniforms {};
        vtxUniforms.ndcMat = matrices.GetNormalMatrix();

        UploadMainBuffer(vtxUniforms, fragUniforms);

        [renderEncoder setVertexBuffer:curMainBuffer offset:0 atIndex:30];
        [renderEncoder setVertexBuffer:curMainBuffer offset:offsets.vertexUniformOffset atIndex:0];
        [renderEncoder setFragmentBuffer:curMainBuffer offset:offsets.fragmentUniformOffset atIndex:0];

        RenderPass previous_pass {};
        for (int render_pass = 0; render_pass < (int)pvrrc.render_passes.size(); render_pass++) {
            const RenderPass& current_pass = pvrrc.render_passes[render_pass];

            DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
                      current_pass.op_count - previous_pass.op_count,
                      current_pass.pt_count - previous_pass.pt_count,
                      current_pass.tr_count - previous_pass.tr_count,
                      current_pass.mvo_count - previous_pass.mvo_count, current_pass.autosort);
            DrawList(renderEncoder, ListType_Opaque, false, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count);
            DrawList(renderEncoder, ListType_Punch_Through, false, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count);
            DrawModVols(renderEncoder, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);
            if (current_pass.autosort) {
                if (!config::PerStripSorting)
                    DrawSorted(renderEncoder, pvrrc.sortedTriangles, previous_pass.sorted_tr_count, current_pass.sorted_tr_count, render_pass + 1 < (int)pvrrc.render_passes.size());
                else
                    DrawList(renderEncoder, ListType_Translucent, true, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
            } else {
                // TODO: This breaking?
                // DrawList(renderEncoder, ListType_Translucent, false, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
            }
            previous_pass = current_pass;
        }

        [renderEncoder endEncoding];
    }

    // Blit to framebuffer
    descriptor = [[MTLRenderPassDescriptor alloc] init];
    color = [descriptor colorAttachments][0];
    [color setTexture:[drawable texture]];
    [color setLoadAction:MTLLoadActionClear];
    [color setStoreAction:MTLStoreActionStore];

    @autoreleasepool {
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:descriptor];

        [renderEncoder setRenderPipelineState:pipelineManager.GetBlitPassPipeline()];
        [renderEncoder setFragmentTexture:frameBuffer atIndex:0];
        [renderEncoder drawPrimitives: MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [renderEncoder endEncoding];
    }

    [commandBuffer presentDrawable:drawable];
    [commandBuffer commit];
    // TODO: Properly handle wait/vsync/buffering!
    WaitIdle();

    DEBUG_LOG(RENDERER, "Render command buffer released");
    return !pvrrc.isRTT;
}

Renderer* rend_Metal()
{
    return new MetalRenderer();
}
