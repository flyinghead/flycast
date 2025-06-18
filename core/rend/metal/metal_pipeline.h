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
#include "types.h"
#include <Metal/Metal.h>
#include <map>

#include "cfg/option.h"
#include "hw/pvr/ta_ctx.h"
#include "metal_shaders.h"

class MetalRenderer;

enum class ModVolMode { Xor, Or, Inclusion, Exclusion, Final };

class MetalPipelineManager
{
public:
    explicit MetalPipelineManager(MetalShaders *shaderManager);
    virtual ~MetalPipelineManager() = default;

    void term()
    {
        pipelines.clear();
        depthPassPipelines.clear();
        depthStencilStates.clear();
        depthPassDepthStencilStates.clear();
    }

    id<MTLRenderPipelineState> GetBlitPassPipeline() {
        if (blitPassPipeline != nullptr)
            return blitPassPipeline;

        CreateBlitPassPipeline();

        return blitPassPipeline;
    }

    id<MTLRenderPipelineState> GetDepthPassPipeline(bool naomi2)
    {
        u32 pipehash = hash(naomi2);
        const auto &pipeline = depthPassPipelines.find(pipehash);
        if (pipeline != depthPassPipelines.end() && pipeline->second != nullptr)
            return pipeline->second;
        CreateDepthPassPipeline(naomi2);

        return depthPassPipelines[pipehash];
    }

    id<MTLRenderPipelineState> GetModifierVolumePipeline(ModVolMode mode, bool naomi2)
    {
        u32 pipehash = hash(mode, naomi2);
        const auto &pipeline = modVolPipelines.find(pipehash);
        if (pipeline != modVolPipelines.end() && pipeline->second != nullptr)
            return pipeline->second;
        CreateModVolPipeline(mode, naomi2);

        return modVolPipelines[pipehash];
    }

    id<MTLRenderPipelineState> GetPipeline(u32 listType, bool sortTriangles, const PolyParam& pp, int gpuPalette, bool dithering)
    {
        u64 pipehash = hash(listType, sortTriangles, &pp, gpuPalette, dithering);
        const auto &pipeline = pipelines.find(pipehash);
        if (pipeline != pipelines.end() && pipeline->second != nullptr)
            return pipeline->second;
        CreatePipeline(listType, sortTriangles, pp, gpuPalette, dithering);

        return pipelines[pipehash];
    }

    id<MTLDepthStencilState> GetModVolDepthStencilStates(ModVolMode mode, bool naomi2)
    {
        u32 pipehash = hash(mode, naomi2);
        const auto &state = modVolStencilStates.find(pipehash);
        if (state != modVolStencilStates.end() && state->second != nullptr)
            return state->second;
        CreateModVolDepthStencilState(mode, naomi2);

        return modVolStencilStates[pipehash];
    }

    id<MTLDepthStencilState> GetDepthPassDepthStencilStates(bool naomi2)
    {
        u32 pipehash = hash(naomi2);
        const auto &state = depthPassDepthStencilStates.find(pipehash);
        if (state != depthPassDepthStencilStates.end() && state->second != nullptr)
            return state->second;
        CreateDepthPassDepthStencilState(naomi2);

        return depthPassDepthStencilStates[pipehash];
    }

    id<MTLDepthStencilState> GetDepthStencilStates(u32 listType, bool sortTriangles, bool shadowed, const PolyParam& pp)
    {
        u64 pipehash = hash(listType, sortTriangles, shadowed, &pp);

        const auto &state = depthStencilStates.find(pipehash);
        if (state != depthStencilStates.end() && state->second != nullptr)
            return state->second;
        CreateDepthStencilState(listType, sortTriangles, shadowed, pp);

        return depthStencilStates[pipehash];
    }

private:
    void CreateBlitPassPipeline();
    void CreateModVolPipeline(ModVolMode mode, bool naomi2);
    void CreateDepthPassPipeline(bool naomi2);
    void CreatePipeline(u32 listType, bool sortTriangles, const PolyParam& pp, int gpuPalette, bool dithering);

    void CreateModVolDepthStencilState(ModVolMode mode, bool naomi2);
    void CreateDepthPassDepthStencilState(bool naomi2);
    void CreateDepthStencilState(u32 listType, bool sortTriangles, bool shadowed, const PolyParam& pp);

    u64 hash(u32 listType, bool sortTriangles, const PolyParam *pp, int gpuPalette, bool dithering) const
    {
        u64 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
            | (((pp->tileclip >> 28) == 3) << 4);
        hash |= ((listType >> 1) << 5);
        bool ignoreTexAlpha = pp->tsp.IgnoreTexA || pp->tcw.PixelFmt == Pixel565;
        hash |= (pp->tsp.ShadInstr << 7) | (ignoreTexAlpha << 9) | (pp->tsp.UseAlpha << 10)
            | (pp->tsp.ColorClamp << 11) | ((config::Fog ? pp->tsp.FogCtrl : 2) << 12) | (pp->tsp.SrcInstr << 14)
            | (pp->tsp.DstInstr << 17);
        hash |= (pp->isp.ZWriteDis << 20) | (pp->isp.CullMode << 21) | (pp->isp.DepthMode << 23);
        hash |= ((u64)sortTriangles << 26) | ((u64)gpuPalette << 27) | ((u64)pp->isNaomi2() << 29);
        hash |= (u64)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 30;
        hash |= (u64)(pp->tcw.PixelFmt == PixelBumpMap) << 31;
        hash |= (u64)dithering << 32;

        return hash;
    }
    u64 hash(u32 listType, bool sortTriangles, bool shadowed, const PolyParam *pp) const
    {
        u64 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
            | (((pp->tileclip >> 28) == 3) << 4);
        hash |= ((listType >> 1) << 5);
        bool ignoreTexAlpha = pp->tsp.IgnoreTexA || pp->tcw.PixelFmt == Pixel565;
        hash |= (pp->tsp.ShadInstr << 7) | (ignoreTexAlpha << 9) | (pp->tsp.UseAlpha << 10)
            | (pp->tsp.ColorClamp << 11) | ((config::Fog ? pp->tsp.FogCtrl : 2) << 12) | (pp->tsp.SrcInstr << 14)
            | (pp->tsp.DstInstr << 17);
        hash |= (pp->isp.ZWriteDis << 20) | (pp->isp.CullMode << 21) | (pp->isp.DepthMode << 23);
        hash |= ((u64)sortTriangles << 26) | ((u64)pp->isNaomi2() << 29);
        hash |= (u64)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 30;
        hash |= (u64)(pp->tcw.PixelFmt == PixelBumpMap) << 31;

        return hash;
    }
    u32 hash(ModVolMode mode, bool naomi2) const
    {
        return ((int)mode << 2) | ((int)naomi2 << 5) | ((int)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 6);
    }
    u32 hash(bool naomi2) const
    {
        return ((int)naomi2 << 2) | ((int)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 3);
    }

    MTLVertexDescriptor* GetMainVertexInputDescriptor(bool full = true, bool naomi2 = false) const
    {
        MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];

        auto pos = [vertexDesc attributes][0]; // pos
        [pos setFormat:MTLVertexFormatFloat3];
        [pos setOffset:offsetof(Vertex, x)];
        [pos setBufferIndex:30];

        if (full) {
            auto col = [vertexDesc attributes][1]; // base color
            [col setFormat:MTLVertexFormatUChar4Normalized];
            [col setOffset:offsetof(Vertex, col)];
            [col setBufferIndex:30];

            auto spc = [vertexDesc attributes][2]; // offset color
            [spc setFormat:MTLVertexFormatUChar4Normalized];
            [spc setOffset:offsetof(Vertex, spc)];
            [spc setBufferIndex:30];

            auto u = [vertexDesc attributes][3]; // tex coord
            [u setFormat:MTLVertexFormatFloat2];
            [u setOffset:offsetof(Vertex, u)];
            [u setBufferIndex:30];

            auto col1 = [vertexDesc attributes][4];
            [col1 setFormat:MTLVertexFormatUChar4Normalized];
            [col1 setOffset:offsetof(Vertex, col1)];
            [col1 setBufferIndex:30];

            auto spc1 = [vertexDesc attributes][5];
            [spc1 setFormat:MTLVertexFormatUChar4Normalized];
            [spc1 setOffset:offsetof(Vertex, spc1)];
            [spc1 setBufferIndex:30];

            auto u1 = [vertexDesc attributes][6]; // tex coord
            [u1 setFormat:MTLVertexFormatFloat2];
            [u1 setOffset:offsetof(Vertex, u1)];
            [u1 setBufferIndex:30];

            if (naomi2) {
                auto nx = [vertexDesc attributes][7]; // naomi2 normal
                [nx setFormat:MTLVertexFormatFloat3];
                [nx setOffset:offsetof(Vertex, nx)];
                [nx setBufferIndex:30];
            }
        }

        auto layout = [vertexDesc layouts][30];
        [layout setStride:sizeof(Vertex)];
        [layout setStepRate:1];
        [layout setStepFunction:MTLVertexStepFunctionPerVertex];

        return vertexDesc;
    }

    static inline MTLBlendFactor GetBlendFactor(u32 instr, bool src) {
        switch (instr) {
        case 0:	// zero
			return MTLBlendFactorZero;
        case 1: // one
			return MTLBlendFactorOne;
        case 2: // other color
			return src ? MTLBlendFactorDestinationColor : MTLBlendFactorSourceColor;
		case 3: // inverse other color
			return src ? MTLBlendFactorOneMinusDestinationColor : MTLBlendFactorOneMinusSourceColor;
        case 4: // src alpha
			return MTLBlendFactorSourceAlpha;
		case 5: // inverse src alpha
			return MTLBlendFactorOneMinusSourceAlpha;
		case 6: // dst alpha
		    return MTLBlendFactorDestinationAlpha;
		case 7: // inverse dst alpha
		    return MTLBlendFactorOneMinusDestinationAlpha;
		default:
			die("Unsupported blend instruction");
			return MTLBlendFactorZero;
		}
    }

    id<MTLRenderPipelineState> blitPassPipeline = nil;
    std::map<u64, id<MTLRenderPipelineState>> pipelines;
    std::map<u32, id<MTLRenderPipelineState>> modVolPipelines;
    std::map<u32, id<MTLRenderPipelineState>> depthPassPipelines;

    std::map<u32, id<MTLDepthStencilState>> modVolStencilStates;
    std::map<u64, id<MTLDepthStencilState>> depthStencilStates;
    std::map<u32, id<MTLDepthStencilState>> depthPassDepthStencilStates;

protected:
    MetalShaders *shaderManager;
};

static const MTLCompareFunction depthOps[] =
{
    MTLCompareFunctionNever,        // 0 Never
    MTLCompareFunctionLess,         // 1 Less
    MTLCompareFunctionEqual,        // 2 Equal
    MTLCompareFunctionLessEqual,    // 3 Less Or Equal
    MTLCompareFunctionGreater,      // 4 Greater
    MTLCompareFunctionNotEqual,     // 5 Not Equal
    MTLCompareFunctionGreaterEqual, // 6 Greater Or Equal
    MTLCompareFunctionAlways,       // 7 Always
};