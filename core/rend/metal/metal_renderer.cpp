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
}

void MetalRenderer::Process(TA_context *ctx) {

}

bool MetalRenderer::Render() {
    return true;
}

void MetalRenderer::RenderFramebuffer(const FramebufferInfo &info) {

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

        // TODO: Set & Bind Push Constants
    }

    encoder->setRenderPipelineState(pipelineManager.GetPipeline(listType, sortTriangles, poly, gpuPalette, dithering));
    if (poly.pcw.Texture || poly.isNaomi2())
    {
        u32 index = 0;
        if (poly.isNaomi2())
        {

        }

        // TODO: Bind Texture & Naomi2 Lights Buffers
    }

    // TODO: Bind Index Buffer
    // encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, count, MTL::IndexTypeUInt16, , 0, 1);
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
            MTL::ScissorRect scissorRect {};
            SetTileClip(encoder, polyParam.tileclip, scissorRect);
            // TODO: Bind Index Buffer
            // encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle, param.count, MTL::IndexTypeUInt16, , pvrrc.idx.size() + param.first, 1);
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

Renderer* rend_Metal()
{
    return new MetalRenderer();
}
