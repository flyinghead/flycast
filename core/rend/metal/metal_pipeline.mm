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
#include "metal_pipeline.h"

#include "metal_context.h"
#include "metal_shaders.h"
#include "metal_renderer.h"

MetalPipelineManager::MetalPipelineManager(MetalRenderer *renderer) {
    this->renderer = renderer;
}

void MetalPipelineManager::CreateBlitPassPipeline() {
    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    [descriptor setLabel:@"Blit Pass"];

    auto attachment = [descriptor colorAttachments][0];
    [attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

    [descriptor setVertexFunction:renderer->GetShaders()->GetBlitVertexShader()];
    [descriptor setFragmentFunction:renderer->GetShaders()->GetBlitFragmentShader()];

    NSError *error = nil;
    auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

    if (state == nil) {
        ERROR_LOG(RENDERER, "Failed to create Blit Pipeline State: %s", [[error localizedDescription] UTF8String]);;
    }

    blitPassPipeline = state;
}

void MetalPipelineManager::CreateModVolPipeline(ModVolMode mode, int cullMode, bool naomi2) {
    MTLVertexDescriptor *vertexDesc = nil;
    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];

    [descriptor setLabel:@"Mod Vol Pass"];

    if (mode == ModVolMode::Final) {
        [descriptor setVertexDescriptor:GetMainVertexInputDescriptor(false, naomi2)];
    }
    else {
        vertexDesc = [[MTLVertexDescriptor alloc] init];

        auto layout = [vertexDesc layouts][30];
        [layout setStride:sizeof(float) * 3];

        auto attribute = [vertexDesc attributes][0];
        [attribute setOffset:0];
        [attribute setBufferIndex:30];
        [attribute setFormat:MTLVertexFormatFloat3];

        [descriptor setVertexDescriptor:vertexDesc];
    }

    auto attachment = [descriptor colorAttachments][0];
    [attachment setBlendingEnabled:mode == ModVolMode::Final];
    [attachment setSourceRGBBlendFactor:MTLBlendFactorSourceAlpha];
    [attachment setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
    [attachment setRgbBlendOperation:MTLBlendOperationAdd];
    [attachment setSourceAlphaBlendFactor:MTLBlendFactorSourceAlpha];
    [attachment setDestinationAlphaBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
    [attachment setAlphaBlendOperation:MTLBlendOperationAdd];
    [attachment setWriteMask:mode != ModVolMode::Final ? MTLColorWriteMaskNone : MTLColorWriteMaskAll];
    [attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

    [descriptor setDepthAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
    [descriptor setStencilAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];

    ModVolShaderParams shaderParams { naomi2, !settings.platform.isNaomi2() && config::NativeDepthInterpolation };
    [descriptor setVertexFunction:renderer->GetShaders()->GetModVolVertexShader(shaderParams)];
    [descriptor setFragmentFunction:renderer->GetShaders()->GetModVolFragmentShader(!settings.platform.isNaomi2() && config::NativeDepthInterpolation)];

    NSError *error = nil;
    auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

    if (state == nullptr) {
        ERROR_LOG(RENDERER, "Failed to create Depth Render Pipeline State: %s", [[error localizedDescription] UTF8String]);
    }

    modVolPipelines[hash(mode, cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthPassPipeline(int cullMode, bool naomi2)
{
    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    [descriptor setLabel:@"Depth Pass"];
    [descriptor setVertexDescriptor:GetMainVertexInputDescriptor(false, false)];

    auto attachment = [descriptor colorAttachments][0];
    [attachment setBlendingEnabled:false];
    [attachment setSourceRGBBlendFactor:MTLBlendFactorZero];
    [attachment setDestinationRGBBlendFactor:MTLBlendFactorZero];
    [attachment setRgbBlendOperation:MTLBlendOperationAdd];
    [attachment setSourceAlphaBlendFactor:MTLBlendFactorZero];
    [attachment setDestinationAlphaBlendFactor:MTLBlendFactorZero];
    [attachment setAlphaBlendOperation:MTLBlendOperationAdd];
    [attachment setWriteMask:MTLColorWriteMaskNone];

    // TODO: Need functions here
    // descriptor->setVertexFunction();
    // descriptor->setFragmentFunction();

    NSError *error = nil;
    auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

    if (state == nil) {
        ERROR_LOG(RENDERER, "Failed to create Depth Render Pipeline State: %s", [[error localizedDescription] UTF8String]);
    }

    depthPassPipelines[hash(cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreatePipeline(u32 listType, bool sortTriangles, const PolyParam &pp, int gpuPalette, bool dithering) {
    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    [descriptor setLabel:@"Main Draw"];
    [descriptor setVertexDescriptor:GetMainVertexInputDescriptor(true, pp.isNaomi2())];

    u32 src = pp.tsp.SrcInstr;
    u32 dst = pp.tsp.DstInstr;

    auto attachment = [descriptor colorAttachments][0];
    [attachment setBlendingEnabled:true];
    [attachment setSourceRGBBlendFactor:GetBlendFactor(src, true)];
    [attachment setDestinationRGBBlendFactor:GetBlendFactor(dst, false)];
    [attachment setRgbBlendOperation:MTLBlendOperationAdd];
    [attachment setSourceAlphaBlendFactor:GetBlendFactor(src, true)];
    [attachment setDestinationAlphaBlendFactor:GetBlendFactor(dst, false)];
    [attachment setAlphaBlendOperation:MTLBlendOperationAdd];
    [attachment setWriteMask:MTLColorWriteMaskAll];
    [attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

    [descriptor setDepthAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
    [descriptor setStencilAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];

    bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;

    VertexShaderParams vertParams = {};
    vertParams.gouraud = pp.pcw.Gouraud == 1;
    vertParams.naomi2 = pp.isNaomi2();
    vertParams.divPosZ = divPosZ;

    FragmentShaderParams fragParams = {};
    fragParams.alphaTest = listType == ListType_Punch_Through;
    fragParams.bumpmap = pp.tcw.PixelFmt == PixelBumpMap;
    fragParams.clamping = pp.tsp.ColorClamp;
    fragParams.insideClipTest = (pp.tileclip >> 28) == 3;
    fragParams.fog = config::Fog ? pp.tsp.FogCtrl : 2;
    fragParams.gouraud = pp.pcw.Gouraud;
    fragParams.ignoreTexAlpha = pp.tsp.IgnoreTexA || pp.tcw.PixelFmt == Pixel565;
    fragParams.offset = pp.pcw.Offset;
    fragParams.shaderInstr = pp.tsp.ShadInstr;
    fragParams.texture = pp.pcw.Texture;
    fragParams.trilinear = pp.pcw.Texture && pp.tsp.FilterMode > 1 && listType != ListType_Punch_Through && pp.tcw.MipMapped == 1;
    fragParams.useAlpha = pp.tsp.UseAlpha;
    fragParams.palette = gpuPalette;
    fragParams.divPosZ = divPosZ;
    fragParams.dithering = dithering;

    [descriptor setVertexFunction:renderer->GetShaders()->GetVertexShader(vertParams)];
    [descriptor setFragmentFunction:renderer->GetShaders()->GetFragmentShader(fragParams)];

    NSError *error = nil;
    auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

    if (state == nullptr) {
        ERROR_LOG(RENDERER, "Failed to create Render Pipeline State: %s", [[error localizedDescription] UTF8String]);
    }

    pipelines[hash(listType, sortTriangles, &pp, gpuPalette, dithering)] = state;
}

void MetalPipelineManager::CreateModVolDepthStencilState(ModVolMode mode, int cullMode, bool naomi2) {
    MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
    [descriptor setDepthWriteEnabled:false];
    [descriptor setDepthCompareFunction:mode == ModVolMode::Xor || mode == ModVolMode::Or ? MTLCompareFunctionGreater : MTLCompareFunctionAlways];

    MTLStencilDescriptor *stencilDescriptor = [[MTLStencilDescriptor alloc] init];
    switch (mode)
    {
    case ModVolMode::Xor:
        [stencilDescriptor setStencilFailureOperation:MTLStencilOperationKeep];
        [stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationInvert];
        [stencilDescriptor setDepthFailureOperation:MTLStencilOperationKeep];
        [stencilDescriptor setStencilCompareFunction:MTLCompareFunctionAlways];
        [stencilDescriptor setReadMask:0];
        [stencilDescriptor setWriteMask:2];
        break;
    case ModVolMode::Or:
        [stencilDescriptor setStencilFailureOperation:MTLStencilOperationKeep];
        [stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationReplace];
        [stencilDescriptor setDepthFailureOperation:MTLStencilOperationKeep];
        [stencilDescriptor setStencilCompareFunction:MTLCompareFunctionAlways];
        [stencilDescriptor setReadMask:2];
        [stencilDescriptor setWriteMask:2];
        break;
    case ModVolMode::Inclusion:
        [stencilDescriptor setStencilFailureOperation:MTLStencilOperationZero];
        [stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationReplace];
        [stencilDescriptor setDepthFailureOperation:MTLStencilOperationZero];
        [stencilDescriptor setStencilCompareFunction:MTLCompareFunctionLessEqual];
        [stencilDescriptor setReadMask:3];
        [stencilDescriptor setWriteMask:3];
        break;
    case ModVolMode::Exclusion:
        [stencilDescriptor setStencilFailureOperation:MTLStencilOperationZero];
        [stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationKeep];
        [stencilDescriptor setDepthFailureOperation:MTLStencilOperationZero];
        [stencilDescriptor setStencilCompareFunction:MTLCompareFunctionEqual];
        [stencilDescriptor setReadMask:3];
        [stencilDescriptor setWriteMask:3];
        break;
    case ModVolMode::Final:
        [stencilDescriptor setStencilFailureOperation:MTLStencilOperationZero];
        [stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationZero];
        [stencilDescriptor setDepthFailureOperation:MTLStencilOperationZero];
        [stencilDescriptor setStencilCompareFunction:MTLCompareFunctionEqual];
        [stencilDescriptor setReadMask:0x81];
        [stencilDescriptor setWriteMask:3];
        break;
    }

    [descriptor setFrontFaceStencil:stencilDescriptor];
    [descriptor setBackFaceStencil:stencilDescriptor];

    auto state = [MetalContext::Instance()->GetDevice() newDepthStencilStateWithDescriptor:descriptor];

    modVolStencilStates[hash(mode, cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthPassDepthStencilState(int cullMode, bool naomi2) {
    MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
    [descriptor setLabel:@"Sorted Depth Pass"];
    [descriptor setDepthWriteEnabled:true];
    [descriptor setDepthCompareFunction:MTLCompareFunctionGreaterEqual];

    auto state = [MetalContext::Instance()->GetDevice() newDepthStencilStateWithDescriptor:descriptor];

    depthPassDepthStencilStates[hash(cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthStencilState(u32 listType, bool sortTriangles, bool shadowed, const PolyParam &pp) {
    MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
    if (shadowed)
        [descriptor setLabel:@"Main Shadowed Depth-Stencil State"];
    else
        [descriptor setLabel:@"Main Depth-Stencil State"];

    MTLCompareFunction compareFunction;
    if (listType == ListType_Punch_Through || sortTriangles) {
        compareFunction = MTLCompareFunctionGreaterEqual;
    } else {
        compareFunction = depthOps[pp.isp.DepthMode];
    }

    bool depthWriteEnabled;
    if (sortTriangles) {
        depthWriteEnabled = false;
    } else {
        // Z Write Disable seems to be ignored for punch-through.
        // Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
        if (listType == ListType_Punch_Through) {
            depthWriteEnabled = true;
        } else {
            depthWriteEnabled = !pp.isp.ZWriteDis;
        }
    }

    MTLStencilDescriptor *stencilDescriptor = [[MTLStencilDescriptor alloc] init];
    [stencilDescriptor setStencilFailureOperation:MTLStencilOperationKeep];
    [stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationKeep];

    if (shadowed) {
        [stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationReplace];
        [stencilDescriptor setStencilCompareFunction:MTLCompareFunctionAlways];
        [stencilDescriptor setReadMask:0];
        [stencilDescriptor setWriteMask:0x80];
    }

    [descriptor setDepthCompareFunction:compareFunction];
    [descriptor setDepthWriteEnabled:depthWriteEnabled];

    if (shadowed) {
        [descriptor setBackFaceStencil:stencilDescriptor];
        [descriptor setFrontFaceStencil:stencilDescriptor];
    }

    auto state = [MetalContext::Instance()->GetDevice() newDepthStencilStateWithDescriptor:descriptor];

    depthStencilStates[hash(listType, sortTriangles, shadowed, &pp)] = state;
}
