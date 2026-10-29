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
#include "metal_apple.h"

#include "types.h"
#include "rend/transform_matrix.h"
#include "wsi/context.h"
#include "metal_quad.h"
#include "metal_overlay.h"
#ifdef USE_METALFX
#include "metal_upscaler.h"
#endif

#ifdef VIDEO_ROUTING
// Syphon, implemented by the macOS shell
void os_VideoRoutingPublishFrameTexture(id<MTLTexture> texture, id<MTLCommandBuffer> commandBuffer, float x, float y, float w, float h);
void os_VideoRoutingTermMetal();
#endif

class MetalContext : public GraphicsContext
{
public:
	~MetalContext() override;
	// Throws FlycastException if Metal initialization fails.
	static void Create(void *window, void *display);

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
		return true;
	}
	bool recreateSwapChainIfNeeded();

	// True if the device supports the packed 16-bit pixel formats (A1BGR5, B5G6R5, ABGR4).
	// Only Apple GPUs do.
	bool SupportsPacked16BitFormats() const { return packed16BitFormats; }

	static MetalContext* Instance() { return contextInstance; }
private:
	MetalContext(void *window, void *display);
	bool init();
	void term();
	void CreateSwapChain();
	id<MTLCommandBuffer> currentCommandBuffer();
	void getFrameRect(float aspectRatio, float& x, float& y, float& w, float& h) const;
#ifdef USE_METALFX
	void getUpscaleRect(float aspectRatio, u32& x, u32& y, u32& w, u32& h) const;
	void UpscaleFrame(id<MTLTexture> texture, float aspectRatio);
#endif
	void DrawFrame(id<MTLTexture> texture, MTLViewport viewport, float aspectRatio);
	void RenderVideoRouting();

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
	std::unique_ptr<MetalOverlay> overlay;
	std::unique_ptr<MetalShaders> shaderManager;

	id<MTLTexture> lastFrameTexture = nil;
	MTLViewport lastFrameViewport;
	float lastFrameAR = 0.f;

	// Scaled copy of the frame sent by video routing
	id<MTLTexture> videoRoutingTexture = nil;
#ifdef USE_METALFX
	MetalUpscaler upscaler;
	// lastFrameTexture upscaled to its on-screen size, or nil
	id<MTLTexture> upscaledTexture = nil;
	// Set while Present() repeats the last frame
	bool presentingDupeFrame = false;
#endif

	id<MTLDevice> device = nil;
	id<MTLCommandQueue> queue = nil;
	CAMetalLayer* layer = nil;
	void *metalView = nullptr;
	bool packed16BitFormats = false;

	static MetalContext* contextInstance;
};
