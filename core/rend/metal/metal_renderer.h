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
#include "metal_buffer.h"
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
    bool Draw(const MetalTexture *fogTexture, const MetalTexture *paletteTexture);
    void DrawPoly(MTL::RenderCommandEncoder *encoder, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
    void DrawSorted(MTL::RenderCommandEncoder *encoder, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass);
    void DrawList(MTL::RenderCommandEncoder *encoder, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last);
    void DrawModVols(MTL::RenderCommandEncoder *encoder, int first, int count);
    void UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms);

    virtual void EndRenderPass() {
        renderPassStarted = false;
    }

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

    MetalBufferData* GetMainBuffer(u32 size)
    {
        MetalBufferData* buffer = nullptr;

        if (!mainBuffers.empty())
        {
            buffer = mainBuffers.back().release();
            mainBuffers.pop_back();
            if (buffer->bufferSize < size)
                {

            }
        }
        else
        {
            buffer = new MetalBufferData(std::max(512 * 1024u, size));
        }

        return buffer;
    }

    template<typename T>
    T MakeFragmentUniforms()
    {
        T fragUniforms;

        //VERT and RAM fog color constants
        FOG_COL_VERT.getRGBColor(fragUniforms.sp_FOG_COL_VERT);
        FOG_COL_RAM.getRGBColor(fragUniforms.sp_FOG_COL_RAM);

        //Fog density constant
        fragUniforms.sp_FOG_DENSITY = FOG_DENSITY.get() * config::ExtraDepthScale;

        pvrrc.fog_clamp_min.getRGBAColor(fragUniforms.colorClampMin);
        pvrrc.fog_clamp_max.getRGBAColor(fragUniforms.colorClampMax);

        fragUniforms.cp_AlphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

        return fragUniforms;
    }

    void CheckFogTexture();
    void CheckPaletteTexture();

    struct {
        u64 indexOffset = 0;
        u64 modVolOffset = 0;
        u64 vertexUniformOffset = 0;
        u64 fragmentUniformOffset = 0;
        u64 naomi2OpaqueOffset = 0;
        u64 naomi2PunchThroughOffset = 0;
        u64 naomi2TranslucentOffset = 0;
        u64 naomi2ModVolOffset = 0;
        u64 naomi2TrModVolOffset = 0;
        u64 lightsOffset = 0;
    } offsets;

    MTL::CommandQueue *queue = nullptr;
    MTL::CommandBuffer *commandBuffer = nullptr;
    bool renderPassStarted = false;

    MTL::ScissorRect baseScissor {};
    MTL::ScissorRect currentScissor {};
    TransformMatrix<COORD_DIRECTX> matrices;

    MTL::Buffer *curMainBuffer = nullptr;
    std::vector<std::unique_ptr<MetalBufferData>> mainBuffers;
    MetalPipelineManager pipelineManager = MetalPipelineManager(this);
    MetalShaders shaders;
    std::unique_ptr<MetalTexture> fogTexture;
    std::unique_ptr<MetalTexture> paletteTexture;
    MetalSamplers samplers;
    bool frameRendered = false;
    bool dithering = false;
};
