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
    MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(NS::String::string("Blit Pass", NS::UTF8StringEncoding));

    auto attachment = descriptor->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    descriptor->setVertexFunction(renderer->GetShaders()->GetBlitVertexShader());
    descriptor->setFragmentFunction(renderer->GetShaders()->GetBlitFragmentShader());

    NS::Error *error = nullptr;
    auto state = MetalContext::Instance()->GetDevice()->newRenderPipelineState(descriptor, &error);

    if (state == nullptr) {
        ERROR_LOG(RENDERER, "Failed to create Blit Pipeline State: %s", error->localizedDescription()->utf8String());
    }

    descriptor->release();

    blitPassPipeline = state;
}

void MetalPipelineManager::CreateModVolPipeline(ModVolMode mode, int cullMode, bool naomi2) {
    MTL::VertexDescriptor *vertexDesc = nullptr;
    MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();

    descriptor->setLabel(NS::String::string("Mod Vol Pass", NS::UTF8StringEncoding));

    if (mode == ModVolMode::Final) {
        descriptor->setVertexDescriptor(GetMainVertexInputDescriptor(false, naomi2));
    }
    else {
        vertexDesc = MTL::VertexDescriptor::alloc()->init();

        auto layout = vertexDesc->layouts()->object(0);
        layout->setStride(sizeof(float) * 3);

        auto attribute = vertexDesc->attributes()->object(0);
        attribute->setOffset(0);
        attribute->setBufferIndex(0);
        attribute->setFormat(MTL::VertexFormatFloat3);

        descriptor->setVertexDescriptor(vertexDesc);
    }

    auto attachment = descriptor->colorAttachments()->object(0);
    attachment->setBlendingEnabled(mode == ModVolMode::Final);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorSourceAlpha);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);
    attachment->setWriteMask(mode != ModVolMode::Final ? MTL::ColorWriteMaskNone : MTL::ColorWriteMaskAll);

    ModVolShaderParams shaderParams { naomi2, !settings.platform.isNaomi2() && config::NativeDepthInterpolation };
    descriptor->setVertexFunction(renderer->GetShaders()->GetModVolVertexShader(shaderParams));
    descriptor->setFragmentFunction(renderer->GetShaders()->GetModVolFragmentShader(!settings.platform.isNaomi2() && config::NativeDepthInterpolation));

    NS::Error *error = nullptr;
    auto state = MetalContext::Instance()->GetDevice()->newRenderPipelineState(descriptor, &error);

    if (state == nullptr) {
        ERROR_LOG(RENDERER, "Failed to create Depth Render Pipeline State: %s", error->localizedDescription()->utf8String());
    }

    descriptor->release();
    if (vertexDesc) {
        vertexDesc->release();
    }

    modVolPipelines[hash(mode, cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthPassPipeline(int cullMode, bool naomi2)
{
    MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(NS::String::string("Depth Pass", NS::UTF8StringEncoding));
    descriptor->setVertexDescriptor(GetMainVertexInputDescriptor(false, false));

    auto attachment = descriptor->colorAttachments()->object(0);
    attachment->setBlendingEnabled(false);
    attachment->setSourceRGBBlendFactor(MTL::BlendFactorZero);
    attachment->setDestinationRGBBlendFactor(MTL::BlendFactorZero);
    attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment->setSourceAlphaBlendFactor(MTL::BlendFactorZero);
    attachment->setDestinationAlphaBlendFactor(MTL::BlendFactorZero);
    attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);
    attachment->setWriteMask(MTL::ColorWriteMaskNone);

    // TODO: Need functions here
    // descriptor->setVertexFunction();
    // descriptor->setFragmentFunction();

    NS::Error *error = nullptr;
    auto state = MetalContext::Instance()->GetDevice()->newRenderPipelineState(descriptor, &error);

    if (state == nullptr) {
        ERROR_LOG(RENDERER, "Failed to create Depth Render Pipeline State: %s", error->localizedDescription()->utf8String());
    }

    descriptor->release();

    depthPassPipelines[hash(cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreatePipeline(u32 listType, bool sortTriangles, const PolyParam &pp, int gpuPalette, bool dithering) {
    MTL::RenderPipelineDescriptor *descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setLabel(NS::String::string("Main Draw", NS::UTF8StringEncoding));
    descriptor->setVertexDescriptor(GetMainVertexInputDescriptor(true, pp.isNaomi2()));

    u32 src = pp.tsp.SrcInstr;
    u32 dst = pp.tsp.DstInstr;

    auto attachment = descriptor->colorAttachments()->object(0);
    attachment->setBlendingEnabled(true);
    attachment->setSourceRGBBlendFactor(GetBlendFactor(src, true));
    attachment->setDestinationRGBBlendFactor(GetBlendFactor(dst, false));
    attachment->setRgbBlendOperation(MTL::BlendOperationAdd);
    attachment->setSourceAlphaBlendFactor(GetBlendFactor(src, true));
    attachment->setDestinationAlphaBlendFactor(GetBlendFactor(dst, false));
    attachment->setAlphaBlendOperation(MTL::BlendOperationAdd);
    attachment->setWriteMask(MTL::ColorWriteMaskAll);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);
    descriptor->setStencilAttachmentPixelFormat(MTL::PixelFormatDepth32Float_Stencil8);

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

    descriptor->setVertexFunction(renderer->GetShaders()->GetVertexShader(vertParams));
    descriptor->setFragmentFunction(renderer->GetShaders()->GetFragmentShader(fragParams));

    NS::Error *error = nullptr;
    auto state = MetalContext::Instance()->GetDevice()->newRenderPipelineState(descriptor, &error);

    if (state == nullptr) {
        ERROR_LOG(RENDERER, "Failed to create Render Pipeline State: %s", error->localizedDescription()->utf8String());
    }

    descriptor->release();

    pipelines[hash(listType, sortTriangles, &pp, gpuPalette, dithering)] = state;
}

void MetalPipelineManager::CreateModVolDepthStencilState(ModVolMode mode, int cullMode, bool naomi2) {
    MTL::DepthStencilDescriptor *descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    descriptor->setDepthWriteEnabled(false);
    descriptor->setDepthCompareFunction(mode == ModVolMode::Xor || mode == ModVolMode::Or ? MTL::CompareFunctionGreater : MTL::CompareFunctionAlways);

    MTL::StencilDescriptor *stencilDescriptor = MTL::StencilDescriptor::alloc()->init();
    switch (mode)
    {
    case ModVolMode::Xor:
        stencilDescriptor->setStencilFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationInvert);
        stencilDescriptor->setDepthFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionAlways);
        stencilDescriptor->setReadMask(0);
        stencilDescriptor->setWriteMask(2);
        break;
    case ModVolMode::Or:
        stencilDescriptor->setStencilFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationReplace);
        stencilDescriptor->setDepthFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionAlways);
        stencilDescriptor->setReadMask(2);
        stencilDescriptor->setWriteMask(2);
        break;
    case ModVolMode::Inclusion:
        stencilDescriptor->setStencilFailureOperation(MTL::StencilOperationZero);
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationReplace);
        stencilDescriptor->setDepthFailureOperation(MTL::StencilOperationZero);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionLessEqual);
        stencilDescriptor->setReadMask(3);
        stencilDescriptor->setWriteMask(3);
        break;
    case ModVolMode::Exclusion:
        stencilDescriptor->setStencilFailureOperation(MTL::StencilOperationZero);
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setDepthFailureOperation(MTL::StencilOperationZero);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionEqual);
        stencilDescriptor->setReadMask(3);
        stencilDescriptor->setWriteMask(3);
        break;
    case ModVolMode::Final:
        stencilDescriptor->setStencilFailureOperation(MTL::StencilOperationZero);
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationZero);
        stencilDescriptor->setDepthFailureOperation(MTL::StencilOperationZero);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionEqual);
        stencilDescriptor->setReadMask(0x81);
        stencilDescriptor->setWriteMask(3);
        break;
    }

    descriptor->setFrontFaceStencil(stencilDescriptor);
    descriptor->setBackFaceStencil(stencilDescriptor);

    auto state = MetalContext::Instance()->GetDevice()->newDepthStencilState(descriptor);

    descriptor->release();

    modVolStencilStates[hash(mode, cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthPassDepthStencilState(int cullMode, bool naomi2) {
    MTL::DepthStencilDescriptor *descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    descriptor->setLabel(NS::String::string("Sorted Depth Pass", NS::UTF8StringEncoding));
    descriptor->setDepthWriteEnabled(true);
    descriptor->setDepthCompareFunction(MTL::CompareFunctionGreaterEqual);

    auto state = MetalContext::Instance()->GetDevice()->newDepthStencilState(descriptor);

    descriptor->release();

    depthPassDepthStencilStates[hash(cullMode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthStencilState(u32 listType, bool sortTriangles, const PolyParam &pp, int gpuPalette, bool dithering) {
    MTL::DepthStencilDescriptor *descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    descriptor->setLabel(NS::String::string("Main Depth Pass", NS::UTF8StringEncoding));

    MTL::CompareFunction compareFunction;
    if (listType == ListType_Punch_Through || sortTriangles) {
        compareFunction = MTL::CompareFunctionGreaterEqual;
    } else {
        compareFunction = depthOps[pp.isp.DepthMode];
    }

    bool depthWriteEnabled;
    // Z Write Disable seems to be ignored for punch-through.
    // Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
    if (listType == ListType_Punch_Through) {
        depthWriteEnabled = true;
    } else {
        depthWriteEnabled = !pp.isp.ZWriteDis;
    }

    bool shadowed = listType == ListType_Opaque || listType == ListType_Punch_Through;
    MTL::StencilDescriptor *stencilDescriptor = MTL::StencilDescriptor::alloc()->init();
    if (shadowed) {
        stencilDescriptor->setStencilFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setDepthFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationReplace);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionAlways);
        stencilDescriptor->setReadMask(0);
        stencilDescriptor->setWriteMask(0x80);
    } else {
        stencilDescriptor->setStencilFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setDepthFailureOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setDepthStencilPassOperation(MTL::StencilOperationKeep);
        stencilDescriptor->setStencilCompareFunction(MTL::CompareFunctionNever);
    }

    descriptor->setDepthCompareFunction(compareFunction);
    descriptor->setDepthWriteEnabled(depthWriteEnabled);
    descriptor->setBackFaceStencil(stencilDescriptor);
    descriptor->setFrontFaceStencil(stencilDescriptor);

    auto state = MetalContext::Instance()->GetDevice()->newDepthStencilState(descriptor);

    descriptor->release();
    stencilDescriptor->release();

    depthStencilStates[hash(listType, sortTriangles, &pp, gpuPalette, dithering)] = state;
}
