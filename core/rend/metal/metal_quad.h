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

#include <Metal/Metal.h>
#include "metal_shaders.h"
#include "metal_buffer.h"

struct MetalQuadVertex
{
    float x, y, z;
    float u, v;
};

class MetalQuadBuffer
{
public:
    MetalQuadBuffer()
    {
        buffer = std::make_unique<MetalBufferData>(sizeof(MetalQuadVertex) * 4);
    }

    void Bind(id<MTLRenderCommandEncoder> commandEncoder)
    {
        [commandEncoder setVertexBuffer:buffer->buffer offset:0 atIndex:0];
    }

    void Draw(id<MTLRenderCommandEncoder> commandEncoder)
    {
        [commandEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
    }

    void Update(MetalQuadVertex vertices[4] = nullptr)
    {
        if (vertices == nullptr)
        {
            static MetalQuadVertex defaultVtx[4]
            {
                { -1.f, -1.f, 0.f, 0.f, 1.f },
                {  1.f, -1.f, 0.f, 1.f, 1.f },
                { -1.f,  1.f, 0.f, 0.f, 0.f },
                {  1.f,  1.f, 0.f, 1.f, 0.f },
            };
            vertices = defaultVtx;
        };

        memcpy([buffer->buffer contents], vertices, sizeof(MetalQuadVertex) * 4);
    }
private:
    std::unique_ptr<MetalBufferData> buffer;
};

class MetalQuadPipeline
{
public:
    MetalQuadPipeline(bool ignoreTexAlpha, bool rotate = false)
            : rotate(rotate), ignoreTexAlpha(ignoreTexAlpha) {}
    void Init(MetalShaders *shaderManager);
    void Term() {
        linearSampler = nil;
        nearestSampler = nil;
    }
    void BindPipeline(id<MTLRenderCommandEncoder> commandEncoder) { [commandEncoder setRenderPipelineState:GetPipeline()]; }

    id<MTLSamplerState> GetLinearSampler() { return linearSampler; }
    id<MTLSamplerState> GetNearestSampler() { return nearestSampler; }
private:
    id<MTLRenderPipelineState> GetPipeline() {
        if (!pipeline)
            CreatePipeline();
        return pipeline;
    }
    void CreatePipeline();


    id<MTLRenderPipelineState> pipeline;
    id<MTLSamplerState> linearSampler;
    id<MTLSamplerState> nearestSampler;
    MetalShaders *shaderManager = nullptr;
    bool rotate;
    bool ignoreTexAlpha;
};

class MetalQuadDrawer
{
public:
    MetalQuadDrawer() = default;
    MetalQuadDrawer(MetalQuadDrawer &&) = default;
    MetalQuadDrawer(const MetalQuadDrawer &) = delete;
    MetalQuadDrawer& operator=(MetalQuadDrawer &&) = default;
    MetalQuadDrawer& operator=(const MetalQuadDrawer &) = delete;

    void Init(MetalQuadPipeline *pipeline);
    void Draw(id<MTLRenderCommandEncoder> commandEncoder, id<MTLTexture> texture, MetalQuadVertex vertices[4] = nullptr, bool nearestFilter = false, const float *color = nullptr);
private:
    MetalQuadPipeline *pipeline = nullptr;
    std::unique_ptr<MetalQuadBuffer> buffer;
};
