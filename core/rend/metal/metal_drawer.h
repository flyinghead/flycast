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
#include <glm/gtc/type_ptr.hpp>

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

    template<typename Offsets>
    void packNaomi2Uniforms(MetalBufferPacker& packer, Offsets& offsets, std::vector<u8>& n2uniforms, bool trModVolIncluded)
    {
        size_t n2UniformSize = sizeof(MetalN2VertexShaderUniforms) + MetalBufferPacker::align(sizeof(MetalN2VertexShaderUniforms), 16);
        int items = pvrrc.global_param_op.size() + pvrrc.global_param_pt.size() + pvrrc.global_param_tr.size() + pvrrc.global_param_mvo.size();
        if (trModVolIncluded)
            items += pvrrc.global_param_mvo_tr.size();
        n2uniforms.resize(items * n2UniformSize);
        size_t bufIdx = 0;
        auto addUniform = [&](const PolyParam& pp, int polyNumber) {
            if (pp.isNaomi2())
            {
                MetalN2VertexShaderUniforms& uni = *(MetalN2VertexShaderUniforms *)&n2uniforms[bufIdx];
                memcpy(glm::value_ptr(uni.mvMat), pvrrc.matrices[pp.mvMatrix].mat, sizeof(uni.mvMat));
                memcpy(glm::value_ptr(uni.normalMat), pvrrc.matrices[pp.normalMatrix].mat, sizeof(uni.normalMat));
                memcpy(glm::value_ptr(uni.projMat), pvrrc.matrices[pp.projMatrix].mat, sizeof(uni.projMat));
                uni.bumpMapping = pp.pcw.Texture == 1 && pp.tcw.PixelFmt == PixelBumpMap;
                uni.polyNumber = polyNumber;
                for (size_t i = 0; i < 2; i++)
                {
                    uni.envMapping[i] = pp.envMapping[i];
                    uni.glossCoef[i] = pp.glossCoef[i];
                    uni.constantColor[i] = pp.constantColor[i];
                }
            }
            bufIdx += n2UniformSize;
        };
        for (const PolyParam& pp : pvrrc.global_param_op)
            addUniform(pp, 0);
        size_t ptOffset = bufIdx;
        for (const PolyParam& pp : pvrrc.global_param_pt)
            addUniform(pp, 0);
        size_t trOffset = bufIdx;
        if (!pvrrc.global_param_tr.empty())
        {
            u32 firstVertexIdx = pvrrc.idx[pvrrc.global_param_tr[0].first];
            for (const PolyParam& pp : pvrrc.global_param_tr)
                addUniform(pp, ((&pp - &pvrrc.global_param_tr[0]) << 17) - firstVertexIdx);
        }
        size_t mvOffset = bufIdx;
        for (const ModifierVolumeParam& mvp : pvrrc.global_param_mvo)
        {
            if (mvp.isNaomi2())
            {
                MetalN2VertexShaderUniforms& uni = *(MetalN2VertexShaderUniforms *)&n2uniforms[bufIdx];
                memcpy(glm::value_ptr(uni.mvMat), pvrrc.matrices[mvp.mvMatrix].mat, sizeof(uni.mvMat));
                memcpy(glm::value_ptr(uni.projMat), pvrrc.matrices[mvp.projMatrix].mat, sizeof(uni.projMat));
            }
            bufIdx += n2UniformSize;
        }
        size_t trMvOffset = bufIdx;
        if (trModVolIncluded)
            for (const ModifierVolumeParam& mvp : pvrrc.global_param_mvo_tr)
            {
                if (mvp.isNaomi2())
                {
                    MetalN2VertexShaderUniforms& uni = *(MetalN2VertexShaderUniforms *)&n2uniforms[bufIdx];
                    memcpy(glm::value_ptr(uni.mvMat), pvrrc.matrices[mvp.mvMatrix].mat, sizeof(uni.mvMat));
                    memcpy(glm::value_ptr(uni.projMat), pvrrc.matrices[mvp.projMatrix].mat, sizeof(uni.projMat));
                }
                bufIdx += n2UniformSize;
            }
        offsets.naomi2OpaqueOffset = packer.addUniform(n2uniforms.data(), bufIdx);
        offsets.naomi2PunchThroughOffset = offsets.naomi2OpaqueOffset + ptOffset;
        offsets.naomi2TranslucentOffset = offsets.naomi2OpaqueOffset + trOffset;
        offsets.naomi2ModVolOffset = offsets.naomi2OpaqueOffset + mvOffset;
        offsets.naomi2TrModVolOffset = offsets.naomi2OpaqueOffset + trMvOffset;
    }

    u64 packNaomi2Lights(MetalBufferPacker& packer)
    {
        u64 offset = -1;

        size_t n2LightSize = sizeof(N2LightModel) + MetalBufferPacker::align(sizeof(N2LightModel), 16);
        if (n2LightSize == sizeof(N2LightModel) && !pvrrc.lightModels.empty())
        {
            offset = packer.addUniform(&pvrrc.lightModels[0], pvrrc.lightModels.size() * sizeof(decltype(pvrrc.lightModels[0])));
        }
        else
        {
            for (const N2LightModel& model : pvrrc.lightModels)
            {
                u64 ioffset = packer.addUniform(&model, sizeof(N2LightModel));
                if (offset == (u64)-1)
                    offset = ioffset;
            }
        }

        return offset;
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