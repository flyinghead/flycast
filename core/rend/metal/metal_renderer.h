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
#pragma once
#include "metal_pipeline.h"
#include "metal_shaders.h"
#include "metal_texture.h"
#include "hw/pvr/Renderer_if.h"

#include "rend/tileclip.h"
#include "rend/transform_matrix.h"

class MetalRenderer final : public Renderer
{
public:
    bool Init() override;
    void Term() override;
    void Process(TA_context* ctx) override;
    bool Render() override;
    void RenderFramebuffer(const FramebufferInfo& info) override;
    MetalShaders* GetShaders() { return &shaders; }

private:
    void DrawPoly(MTL::RenderCommandEncoder *encoder, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
    void DrawSorted(MTL::RenderCommandEncoder *encoder, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass);
    void DrawList(MTL::RenderCommandEncoder *encoder, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last);
    void DrawModVols(MTL::RenderCommandEncoder *encoder, int first, int count);

protected:
    TileClipping SetTileClip(MTL::RenderCommandEncoder *encoder, u32 val, MTL::ScissorRect& clipRect);
    void SetBaseScissor(MTL::Viewport viewport);

    void SetScissor(MTL::RenderCommandEncoder *encoder, const MTL::ScissorRect& scissor)
    {
        if (scissor.x != currentScissor.x ||
            scissor.y != currentScissor.y ||
            scissor.width != currentScissor.width ||
            scissor.height != currentScissor.height)
        {
            encoder->setScissorRect(scissor);
            currentScissor = scissor;
        }
    }

    MTL::ScissorRect baseScissor {};
    MTL::ScissorRect currentScissor {};
    TransformMatrix<COORD_DIRECTX> matrices;

    MetalPipelineManager pipelineManager = MetalPipelineManager(this);
    MetalShaders shaders;
    MetalSamplers samplers;
    bool frameRendered = false;
    bool dithering = false;
};
