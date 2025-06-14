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
#include <QuartzCore/QuartzCore.h>

#include "rend/transform_matrix.h"
#include "wsi/context.h"
#include "metal_quad.h"

class MetalContext : public GraphicsContext
{
public:
    MetalContext();
    ~MetalContext() override;

    bool init();
    void term() override;

    void resize() override { resized = true; }
    bool IsValid() { return width != 0 && height != 0; }
    void NewFrame();
    void BeginRenderPass();
    void EndFrame();
    void Present();
    void PresentFrame(id<MTLTexture> texture, MTLViewport viewport, float aspectRatio);
    void PresentLastFrame();
    bool GetLastFrame(std::vector<u8>& data, int& width, int& height);

    id<MTLDevice> GetDevice() const { return device; }
    CAMetalLayer* GetLayer() const { return layer; }
    id<MTLCommandQueue> GetQueue() const { return queue; }
    MTLRenderPassDescriptor* GetDescriptor() const { return renderPassDescriptor; }
    id<MTLRenderCommandEncoder> GetEncoder() const { return commandEncoder; }
    id<MTLCommandBuffer> GetCommandBuffer() const { return commandBuffers[currentImage]; }
    bool IsRendering() const { return rendering; }

    std::string getDriverName() override;

    std::string getDriverVersion() override {
        return "";
    }

    bool isAMD() override {
        return false;
    }

    bool hasPerPixel() override {
        return false;
    }
    bool recreateSwapChainIfNeeded();

    static MetalContext* Instance() { return contextInstance; }
private:
    void CreateSwapChain();
    void DrawFrame(id<MTLTexture> texture, MTLViewport viewport, float aspectRatio);

    bool HasSurfaceDimensionChanged() const;
    void SetWindowSize(u32 width, u32 height);

    bool rendering = false;
    bool renderDone = false;
    u32 width = 0;
    u32 height = 0;
    bool resized = false;
    bool swapOnVSync = true;
    int swapInterval = 1;

    u32 currentImage = 0;

    id<CAMetalDrawable> currentDrawable = nil;
    MTLRenderPassDescriptor *renderPassDescriptor = nil;

    std::vector<id<MTLCommandBuffer>> commandBuffers;
    id<MTLRenderCommandEncoder> commandEncoder;

    std::unique_ptr<MetalQuadPipeline> quadPipelineWithAlpha;
    std::unique_ptr<MetalQuadPipeline> quadPipeline;
    std::unique_ptr<MetalQuadPipeline> quadRotatePipeline;
    std::unique_ptr<MetalQuadDrawer> quadDrawer;
    std::unique_ptr<MetalQuadDrawer> quadRotateDrawer;
    std::unique_ptr<MetalShaders> shaderManager;

    id<MTLTexture> lastFrameTexture = nil;
    MTLViewport lastFrameViewport;
    float lastFrameAR = 0.f;

    id<MTLDevice> device = nil;
    id<MTLCommandQueue> queue = nil;
    CAMetalLayer* layer;

    static MetalContext* contextInstance;
};