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
    descriptor->release();

    pipelines[hash(listType, sortTriangles, &pp, gpuPalette, dithering)] = state;
}
