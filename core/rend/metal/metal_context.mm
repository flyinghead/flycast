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

#include "metal_context.h"
#include "metal_driver.h"
#ifdef USE_SDL
#include "sdl/sdl.h"
#endif
#include "ui/imgui_driver.h"
#import "metal_buffer.h"

MetalContext *MetalContext::contextInstance;

void MetalContext::CreateSwapChain()
{
    // WAIT IDLE

    commandBuffers.clear();

    [layer setPixelFormat:MTLPixelFormatBGRA8Unorm];
    [layer setFramebufferOnly:TRUE];
    [layer setColorspace:CGColorSpaceCreateWithName(kCGColorSpaceSRGB)];
    [layer setMaximumDrawableCount:3];
#if TARGET_OS_MAC || TARGET_OS_MACCATALYST
    [layer setDisplaySyncEnabled:TRUE];
#endif

    auto size = [layer drawableSize];
    width = size.width;
    height = size.height;
    SetWindowSize(width, height);
    resized = false;

    if (swapOnVSync && config::DupeFrames && settings.display.refreshRate > 60.f)
        swapInterval = settings.display.refreshRate / 60.f;
    else
        swapInterval = 1;

    commandBuffers.resize(3);

    quadPipeline->Init(shaderManager.get());
    quadPipelineWithAlpha->Init(shaderManager.get());
    quadDrawer->Init(quadPipeline.get());
    quadRotatePipeline->Init(shaderManager.get());
    quadRotateDrawer->Init(quadRotatePipeline.get());

    currentImage = 2;

    ERROR_LOG(RENDERER, "Metal swap chain created: %d x %d, swap chain size %d", width, height, 3);
}

bool MetalContext::init()
{
    GraphicsContext::instance = this;

#ifdef USE_SDL
    if (!sdl_recreate_window(SDL_WINDOW_METAL))
        return false;

    auto view = SDL_Metal_CreateView((SDL_Window *)window);

    if (view == nullptr) {
        term();
        ERROR_LOG(RENDERER, "Failed to create SDL Metal View");
        return false;
    }

    layer = static_cast<CAMetalLayer*>(SDL_Metal_GetLayer(view));
#endif

    device = MTLCreateSystemDefaultDevice();

    if (!device) {
        term();
        NOTICE_LOG(RENDERER, "Metal Device is null.");
        return false;
    }

    [layer setDevice:device];
    queue = [device newCommandQueue];

    shaderManager = std::make_unique<MetalShaders>();
    quadPipeline = std::make_unique<MetalQuadPipeline>(true, false);
    quadPipelineWithAlpha = std::make_unique<MetalQuadPipeline>(false, false);
    quadDrawer = std::make_unique<MetalQuadDrawer>();
    quadRotatePipeline = std::make_unique<MetalQuadPipeline>(true, true);
    quadRotateDrawer = std::make_unique<MetalQuadDrawer>();

    NOTICE_LOG(RENDERER, "Created Metal view.");

    imguiDriver = std::unique_ptr<ImGuiDriver>(new MetalDriver());

    CreateSwapChain();

    return true;
}

std::string MetalContext::getDriverName() {
    return [[device name] UTF8String];
}

bool MetalContext::recreateSwapChainIfNeeded()
{
    if (resized || HasSurfaceDimensionChanged())
    {
        CreateSwapChain();
        lastFrameTexture = nil;
        return true;
    }
    else
        return false;
}

void MetalContext::BeginRenderPass() {
    recreateSwapChainIfNeeded();
    if (!IsValid())
        return;

    currentDrawable = [layer nextDrawable];

    if (!renderPassDescriptor) {
        renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
    }

    auto colorAttachment = renderPassDescriptor.colorAttachments[0];
    [colorAttachment setTexture:currentDrawable.texture];
    [colorAttachment setLoadAction:MTLLoadActionClear];
    [colorAttachment setStoreAction:MTLStoreActionStore];
    [colorAttachment setClearColor:MTLClearColorMake(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.0f)];

    if (currentImage >= commandBuffers.size()) {
        commandBuffers.resize(currentImage + 1);
    }

    if (!commandBuffers[currentImage]) {
        commandBuffers[currentImage] = [queue commandBuffer];
        [commandBuffers[currentImage] setLabel:@"Render Frame"];
    }

    commandEncoder = [commandBuffers[currentImage] renderCommandEncoderWithDescriptor: renderPassDescriptor];
    [commandBuffers[currentImage] presentDrawable:currentDrawable];
};

void MetalContext::NewFrame() {
    if (!IsValid())
        return;

    currentImage = (currentImage + 1) % 3;
    currentDrawable = nil;
    verify(!rendering);
    rendering = true;
}

void MetalContext::EndFrame() {
    if (!IsValid())
        return;

    [commandEncoder endEncoding];
    [commandBuffers[currentImage] commit];
    [commandBuffers[currentImage] waitUntilCompleted];
    commandBuffers[currentImage] = nil;

    verify(rendering);
    rendering = false;
    renderDone = true;
}

void MetalContext::Present()
{
    if (renderDone)
    {
        if (lastFrameTexture != nil && IsValid() && !gui_is_open())
            for (int i = 1; i < swapInterval; i++)
            {
                PresentFrame(lastFrameTexture, lastFrameViewport, lastFrameAR);
            }
        renderDone = false;
    }
    if (swapOnVSync == (settings.input.fastForwardMode || !config::VSync))
    {
        swapOnVSync = (!settings.input.fastForwardMode && config::VSync);
        resized = true;
    }
    if (resized) {
        CreateSwapChain();
        lastFrameTexture = nil;
    }
}

void MetalContext::DrawFrame(id<MTLTexture> texture, MTLViewport viewport, float aspectRatio) {
    MetalQuadVertex vtx[4] {
            { -1, -1, 0, 0, 1 },
            {  1, -1, 0, 1, 1 },
            { -1,  1, 0, 0, 0 },
            {  1,  1, 0, 1, 0 },
    };
    float shiftX, shiftY;
    getVideoShift(shiftX, shiftY);
    vtx[0].x = vtx[2].x = -1.f + shiftX * 2.f / viewport.width;
    vtx[1].x = vtx[3].x = vtx[0].x + 2;
    vtx[0].y = vtx[1].y = -1.f + shiftY * 2.f / viewport.height;
    vtx[2].y = vtx[3].y = vtx[0].y + 2;

    [commandEncoder pushDebugGroup:@"DrawFrame"];

    if (config::Rotate90)
        quadRotatePipeline->BindPipeline(commandEncoder);
    else
        quadPipeline->BindPipeline(commandEncoder);

    float screenAR = (float)width / height;
    float dx = 0;
    float dy = 0;
    if (aspectRatio > screenAR)
        dy = height * (1 - screenAR / aspectRatio) / 2;
    else
        dx = width * (1 - aspectRatio / screenAR) / 2;

    MTLViewport framePort = { dx, dy, width - dx * 2, height - dy * 2, 0, 1 };
    [commandEncoder setViewport:framePort];
    [commandEncoder setScissorRect:MTLScissorRect { (uint)dx, (uint)dy, (uint)(width - dx * 2), (uint)(height - dy * 2) }];
    if (config::Rotate90)
        quadRotateDrawer->Draw(commandEncoder, texture, vtx, config::TextureFiltering == 1);
    else
        quadDrawer->Draw(commandEncoder, texture, vtx, config::TextureFiltering == 1);

    [commandEncoder popDebugGroup];
}

void MetalContext::PresentFrame(id<MTLTexture> texture, MTLViewport viewport, float aspectRatio)
{
    lastFrameTexture = texture;
    lastFrameViewport = viewport;
    lastFrameAR = aspectRatio;

    if (texture != nil && IsValid())
    {
        NewFrame();

        BeginRenderPass();

        gui_draw_osd();

        if (lastFrameTexture != nil) // Might have been nullified if swap chain recreated
            DrawFrame(texture, viewport, aspectRatio);

        imguiDriver->renderDrawData(ImGui::GetDrawData(), false);
        EndFrame();
    }
    else {
        if (!IsValid())
        {
            ERROR_LOG(RENDERER, "NOT PRESENTING INVALID SIZE!");
        }
    }
}

void MetalContext::PresentLastFrame()
{
    if (lastFrameTexture != nil && IsValid())
        DrawFrame(lastFrameTexture, lastFrameViewport, lastFrameAR);
}

void MetalContext::term() {
    GraphicsContext::instance = nullptr;
    lastFrameTexture = nil;
    imguiDriver.reset();
    quadDrawer.reset();
    quadPipeline.reset();
    quadPipelineWithAlpha.reset();
    quadRotateDrawer.reset();
    quadRotatePipeline.reset();
    shaderManager.reset();
    commandBuffers.clear();
}

bool MetalContext::HasSurfaceDimensionChanged() const
{
    auto size = [layer drawableSize];
    return width != size.width || height != size.height;
}

void MetalContext::SetWindowSize(u32 width, u32 height)
{
    if (this->width != width && this->height != height)
    {
        this->width = width;
        this->height = height;

        if (width != 0)
            settings.display.width = width;

        if (height != 0)
            settings.display.height = height;

        resize();
    }
}

MetalContext::MetalContext() {
    verify(contextInstance == nullptr);
    contextInstance = this;
}

MetalContext::~MetalContext() {
    verify(contextInstance == this);
    contextInstance = nullptr;
}

bool MetalContext::GetLastFrame(std::vector<u8> &data, int &width, int &height)
{
    if (lastFrameTexture == nil)
        return false;

    if (width != 0) {
        height = width / lastFrameAR;
    }
    else if (height != 0) {
        width = lastFrameAR * height;
    }
    else
    {
        width = lastFrameViewport.width;
        height = lastFrameViewport.height;
        if (config::Rotate90)
            std::swap(width, height);
        // We need square pixels for PNG
        int w = lastFrameAR * height;
        if (width > w)
            height = width / lastFrameAR;
        else
            width = w;
    }

    MTLTextureDescriptor *renderTargetDesc = [[MTLTextureDescriptor alloc] init];
    renderTargetDesc.width = width;
    renderTargetDesc.height = height;
    renderTargetDesc.pixelFormat = MTLPixelFormatRGBA8Unorm;
    renderTargetDesc.usage = MTLTextureUsageRenderTarget;
    renderTargetDesc.storageMode = MTLStorageModePrivate;

    id<MTLTexture> renderTarget = [device newTextureWithDescriptor:renderTargetDesc];
    [renderTarget setLabel:@"Screenshot Render Target"];

    NSUInteger bytesPerPixel = 4;
    NSUInteger bytesPerRow = width * bytesPerPixel;
    NSUInteger bufferSize = bytesPerRow * height;

    id<MTLBuffer> readbackBuffer = [device newBufferWithLength:bufferSize
                                                       options:MTLResourceStorageModeShared];
    [readbackBuffer setLabel:@"Screenshot Readback Buffer"];

    id<MTLCommandBuffer> commandBuffer = [queue commandBuffer];
    [commandBuffer setLabel:@"GetLastFrame"];

    MTLRenderPassDescriptor *renderPassDesc = [[MTLRenderPassDescriptor alloc] init];
    renderPassDesc.colorAttachments[0].texture = renderTarget;
    renderPassDesc.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
    renderPassDesc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);

    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDesc];
    [renderEncoder setLabel:@"GetLastFrame Render"];

    MTLViewport viewport = {
            0.0, 0.0,
            (double)width, (double)height,
            0.0, 1.0
    };
    [renderEncoder setViewport:viewport];

    MTLScissorRect scissor = {
            0, 0,
            (NSUInteger)width, (NSUInteger)height
    };
    [renderEncoder setScissorRect:scissor];

    MetalQuadVertex vtx[4] = {
            { -1.f, -1.f, 0.f, 0.f, 1.f },
            {  1.f, -1.f, 0.f, 1.f, 1.f },
            { -1.f,  1.f, 0.f, 0.f, 0.f },
            {  1.f,  1.f, 0.f, 1.f, 0.f },
    };

    if (config::Rotate90) {
        quadRotatePipeline->BindPipeline(renderEncoder);
        quadRotateDrawer->Draw(renderEncoder, lastFrameTexture, vtx, false);
    } else {
        quadPipeline->BindPipeline(renderEncoder);
        quadDrawer->Draw(renderEncoder, lastFrameTexture, vtx, false);
    }

    [renderEncoder endEncoding];

    // Copy from render target to buffer
    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    [blitEncoder setLabel:@"GetLastFrame Blit"];

    MTLOrigin sourceOrigin = MTLOriginMake(0, 0, 0);
    MTLSize sourceSize = MTLSizeMake(width, height, 1);

    [blitEncoder copyFromTexture:renderTarget
                     sourceSlice:0
                     sourceLevel:0
                    sourceOrigin:sourceOrigin
                      sourceSize:sourceSize
                        toBuffer:readbackBuffer
               destinationOffset:0
          destinationBytesPerRow:bytesPerRow
        destinationBytesPerImage:bufferSize];

    [blitEncoder endEncoding];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];

    if (commandBuffer.status != MTLCommandBufferStatusCompleted) {
        NSError *error = commandBuffer.error;
        WARN_LOG(RENDERER, "MetalContext::GetLastFrame: Command buffer failed: %s",
                 error ? error.localizedDescription.UTF8String : "Unknown error");
        return false;
    }

    // Read back the data
    const u8 *img = (const u8 *)[readbackBuffer contents];
    data.clear();

    data.reserve(width * height * 3);
    // RGBA -> RGB conversion
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data.push_back(*img++); // R
            data.push_back(*img++); // G
            data.push_back(*img++); // B
            img++; // Skip A
        }
    }

    return true;
}
