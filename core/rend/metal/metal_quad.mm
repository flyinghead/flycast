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

#include "metal_quad.h"
#import "metal_context.h"

void MetalQuadPipeline::CreatePipeline()
{
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    [pipelineDescriptor setVertexFunction:shaderManager->GetQuadVertexShader(rotate)];
    [pipelineDescriptor setFragmentFunction:shaderManager->GetQuadFragmentShader(ignoreTexAlpha)];

    [pipelineDescriptor setInputPrimitiveTopology:MTLPrimitiveTopologyClassTriangle];

    auto color = pipelineDescriptor.colorAttachments[0];
    [color setBlendingEnabled:TRUE];
    [color setSourceRGBBlendFactor:MTLBlendFactorSourceAlpha];
    [color setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
    [color setRgbBlendOperation:MTLBlendOperationAdd];
    [color setSourceAlphaBlendFactor:MTLBlendFactorSourceAlpha];
    [color setDestinationAlphaBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
    [color setAlphaBlendOperation:MTLBlendOperationAdd];
    [color setWriteMask:MTLColorWriteMaskAll];
    [color setPixelFormat:MTLPixelFormatRGBA8Unorm];

    MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    auto pos = vertexDescriptor.attributes[0];
    [pos setFormat:MTLVertexFormatFloat3];
    [pos setBufferIndex:0];
    [pos setOffset:offsetof(MetalQuadVertex, x)];

    auto uv = vertexDescriptor.attributes[1];
    [uv setFormat:MTLVertexFormatFloat2];
    [uv setBufferIndex:0];
    [uv setOffset:offsetof(MetalQuadVertex, u)];

    auto layout = vertexDescriptor.layouts[0];
    [layout setStride:sizeof(MetalQuadVertex)];

    [pipelineDescriptor setVertexDescriptor:vertexDescriptor];

    NSError *error = nil;
    pipeline = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];

    if (pipeline == nil)
    {
        ERROR_LOG(RENDERER, "Failed to create quad pipeline: %s", [[error localizedDescription] UTF8String]);
    }
}

void MetalQuadPipeline::Init(MetalShaders *shaderManager)
{
    this->shaderManager = shaderManager;
    if (linearSampler == nil)
    {
        MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        [samplerDescriptor setMinFilter:MTLSamplerMinMagFilterLinear];
        [samplerDescriptor setMagFilter:MTLSamplerMinMagFilterLinear];
        [samplerDescriptor setMipFilter:MTLSamplerMipFilterLinear];
        [samplerDescriptor setSAddressMode:MTLSamplerAddressModeClampToEdge];
        [samplerDescriptor setTAddressMode:MTLSamplerAddressModeClampToEdge];
        [samplerDescriptor setRAddressMode:MTLSamplerAddressModeClampToEdge];
        linearSampler = [MetalContext::Instance()->GetDevice() newSamplerStateWithDescriptor:samplerDescriptor];
    }
    if (nearestSampler == nil)
    {
        MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        [samplerDescriptor setMinFilter:MTLSamplerMinMagFilterNearest];
        [samplerDescriptor setMagFilter:MTLSamplerMinMagFilterNearest];
        [samplerDescriptor setMipFilter:MTLSamplerMipFilterNearest];
        [samplerDescriptor setSAddressMode:MTLSamplerAddressModeClampToEdge];
        [samplerDescriptor setTAddressMode:MTLSamplerAddressModeClampToEdge];
        [samplerDescriptor setRAddressMode:MTLSamplerAddressModeClampToEdge];
        nearestSampler = [MetalContext::Instance()->GetDevice() newSamplerStateWithDescriptor:samplerDescriptor];
    }
}

void MetalQuadDrawer::Init(MetalQuadPipeline *pipeline) {
    this->pipeline = pipeline;
    buffer = std::make_unique<MetalQuadBuffer>();
}

void MetalQuadDrawer::Draw(id<MTLRenderCommandEncoder> commandEncoder, id<MTLTexture> texture, MetalQuadVertex *vertices, bool nearestFilter, const float *color)
{
    pipeline->BindPipeline(commandEncoder);
    buffer->Update(vertices);
    buffer->Bind(commandEncoder);

    if (texture != nil)
    {
        [commandEncoder setFragmentTexture:texture atIndex:0];
        [commandEncoder setFragmentSamplerState:nearestFilter ? pipeline->GetNearestSampler() : pipeline->GetLinearSampler() atIndex:0];
    }

    if (color == nullptr)
    {
        static float fullWhite[] { 1.f, 1.f, 1.f, 1.f };
        color = fullWhite;
    }

    [commandEncoder setFragmentBytes:color length:sizeof(float) * 4 atIndex:0];
    buffer->Draw(commandEncoder);
}