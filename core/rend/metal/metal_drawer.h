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

#include "metal_texture.h"
#include "metal_shaders.h"
#include "metal_pipeline.h"
#include "metal_buffer.h"
#include "rend/tileclip.h"
#include "rend/transform_matrix.h"
#include "rend/sorter.h"
#include "hw/pvr/pvr_mem.h"

class MetalBaseDrawer
{
protected:
    TileClipping SetTileClip(id<MTLRenderCommandEncoder> encoder, u32 val, MTLScissorRect& clipRect);
    void SetBaseScissor(MTLViewport viewport);

    void SetScissor(id<MTLRenderCommandEncoder> encoder, const MTLScissorRect& scissor)
    {
        if (scissor.x != currentScissor.x ||
            scissor.y != currentScissor.y ||
            scissor.width != currentScissor.width ||
            scissor.height != currentScissor.height)
        {
            [encoder setScissorRect:scissor];
            currentScissor = scissor;
        }
    }

    MetalBufferData* GetMainBuffer(u32 size)
    {
        MetalBufferData *buffer;
        if (!mainBuffers.empty())
        {
            buffer = mainBuffers.back().release();
            mainBuffers.pop_back();
            if (buffer->bufferSize < size) {
                u32 newSize = (u32)buffer->bufferSize;
                while (newSize < size)
                    newSize *= 2;

                INFO_LOG(RENDERER, "Increasing main buffer size %zd -> %d", buffer->bufferSize, newSize);
                [buffer->buffer setPurgeableState: MTLPurgeableStateEmpty];

                buffer = new MetalBufferData(newSize);
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

    MTLScissorRect baseScissor {};
    MTLScissorRect currentScissor {};
    TransformMatrix<COORD_DIRECTX> matrices;
    std::vector<std::unique_ptr<MetalBufferData>> mainBuffers;
};

class MetalDrawer : public MetalBaseDrawer
{
public:
    virtual ~MetalDrawer() = default;

    bool Draw(const MetalTexture *fogTexture, const MetalTexture *paletteTexture, id<MTLCommandBuffer> commandBuffer);
    virtual void EndRenderPass() {
        renderPassStarted = false;
    }

    virtual void Term() {

    }

protected:
    virtual id<MTLRenderCommandEncoder> BeginRenderPass(id<MTLCommandBuffer> commandBuffer) = 0;
    void Init(MetalSamplers *samplers, MetalPipelineManager pipelineManager) {
        this->samplers = samplers;
        this->pipelineManager = std::make_unique<MetalPipelineManager>(pipelineManager);
    }

    int GetCurrentImage() const { return imageIndex; }

    id<MTLRenderCommandEncoder> currentEncoder = nil;
    MetalSamplers *samplers = nullptr;
    bool renderPassStarted = false;

private:
    void DrawPoly(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
    void DrawSorted(id<MTLRenderCommandEncoder> encoder, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass);
    void DrawList(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last);
    void DrawModVols(id<MTLRenderCommandEncoder> encoder, int first, int count);
    void UploadMainBuffer(const MetalVertexShaderUniforms& vertexUniforms, const MetalFragmentShaderUniforms& fragmentUniforms);

    int imageIndex = 0;
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
    id<MTLBuffer> curMainBuffer = nil;
    std::unique_ptr<MetalPipelineManager> pipelineManager = nullptr;
    bool dithering = false;
};

class MetalScreenDrawer : public MetalDrawer
{
public:
    void Init(MetalSamplers *samplers, MetalShaders *shaders, const MTLViewport& viewport);

    void EndRenderPass() override;
    bool PresentFrame()
    {
        EndRenderPass();
        if (!frameRendered)
            return false;
        frameRendered = false;
        MetalContext::Instance()->PresentFrame(framebuffers[GetCurrentImage()], viewport, aspectRatio);

        return true;
    }

protected:
    id<MTLRenderCommandEncoder> BeginRenderPass(id<MTLCommandBuffer> commandBuffer) override;

private:
    std::vector<id<MTLTexture>> framebuffers;
    std::vector<MTLRenderPassDescriptor*> loadPassDescriptors;
    std::vector<MTLRenderPassDescriptor*> clearPassDescriptors;
    id<MTLTexture> depthAttachment;
    MTLViewport viewport;
    MetalShaders *shaderManager = nullptr;
    std::vector<bool> clearNeeded;
    bool frameRendered = false;
    float aspectRatio = 0.f;
    bool emulateFramebuffer = false;
};

class MetalTextureDrawer : public MetalDrawer
{
public:
    void Init(MetalSamplers *samplers, MetalShaders *shaders, MetalTextureCache *textureCache);

    void EndRenderPass() override;

protected:
    id<MTLRenderCommandEncoder> BeginRenderPass(id<MTLCommandBuffer> commandBuffer) override;

private:
    u32 width = 0;
    u32 height = 0;
    u32 textureAddr = 0;

    MetalTexture *texture = nullptr;
    std::vector<id<MTLTexture>> framebuffers;
    MTLRenderPassDescriptor *rttPassDescriptor = nil;
    id<MTLTexture> colorAttachment;
    id<MTLTexture> depthAttachment;
    MetalTextureCache *textureCache = nullptr;
};