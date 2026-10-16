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
#include "cfg/option.h"
#include "ui/gui.h"
#include "ui/imgui_driver.h"
#ifdef USE_SDL
#include "sdl/sdl.h"
#include <SDL_metal.h>
#endif

#include <algorithm>

MetalContext *MetalContext::contextInstance;

void MetalContext::CreateSwapChain()
{
	commandBuffers.clear();

	[layer setPixelFormat:MTLPixelFormatBGRA8Unorm];
	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	[layer setColorspace:colorSpace];
	CGColorSpaceRelease(colorSpace);
	[layer setFramebufferOnly:FALSE];
	[layer setMaximumDrawableCount:3];
	[layer setDisplaySyncEnabled:swapOnVSync];

	auto size = [layer drawableSize];
	// Validate up front, then let SetWindowSize() do the assignment and propagation.
	if (size.width <= 0 || size.height <= 0)
	{
		WARN_LOG(RENDERER, "Metal layer reported invalid drawable size %.0fx%.0f; skipping swap chain creation",
				 size.width, size.height);
		return;
	}
	SetWindowSize((u32)size.width, (u32)size.height);
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
	overlay->Init(quadPipelineWithAlpha.get());

	currentImage = 2;

	INFO_LOG(RENDERER, "Metal swap chain created: %d x %d, swap chain size %d", width, height, 3);
}

bool MetalContext::init()
{
	// MTLSharedEvent waitUntilSignaledValue:timeoutMS: (MetalCommandPool) requires macOS 12
	if (@available(macOS 12.0, *)) {
	} else {
		ERROR_LOG(RENDERER, "The Metal renderer requires macOS 12 or later");
		return false;
	}
#ifdef USE_SDL
	if (!sdl_recreate_window(SDL_WINDOW_METAL))
		return false;

	metalView = SDL_Metal_CreateView((SDL_Window *)window);
	if (metalView == nullptr) {
		ERROR_LOG(RENDERER, "Failed to create SDL Metal View: %s", SDL_GetError());
		term();
		return false;
	}
	layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(metalView);
#endif
	if (layer == nil) {
		ERROR_LOG(RENDERER, "No CAMetalLayer available");
		term();
		return false;
	}

	device = MTLCreateSystemDefaultDevice();
	if (!device) {
		ERROR_LOG(RENDERER, "Metal device is null.");
		term();
		return false;
	}
	// The packed 16-bit formats need macOS 11 and an Apple GPU
	if (@available(macOS 11.0, *))
		packed16BitFormats = [device supportsFamily:MTLGPUFamilyApple1];

	[layer setDevice:device];
	queue = [device newCommandQueue];

	shaderManager = std::make_unique<MetalShaders>();
	quadPipeline = std::make_unique<MetalQuadPipeline>(true, false);
	quadPipelineWithAlpha = std::make_unique<MetalQuadPipeline>(false, false);
	quadDrawer = std::make_unique<MetalQuadDrawer>();
	quadRotatePipeline = std::make_unique<MetalQuadPipeline>(true, true);
	quadRotateDrawer = std::make_unique<MetalQuadDrawer>();
	overlay = std::make_unique<MetalOverlay>();

#ifdef USE_SDL
	sdl_update_display_metrics((SDL_Window *)window, SDL_WINDOW_METAL);
#endif
	NOTICE_LOG(RENDERER, "Metal device: %s", [[device name] UTF8String]);

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

void MetalContext::BeginRenderPass()
{
	recreateSwapChainIfNeeded();
	if (!IsValid())
		return;

	// Reuse the drawable PresentFrame already acquired for this frame.
	if (currentDrawable == nil)
		currentDrawable = [layer nextDrawable];

	// nextDrawable can return nil when the pool is exhausted or it times out.
	// Skip this frame's pass. EndFrame and the encoder users below are nil-encoder safe.
	if (currentDrawable == nil) {
		DEBUG_LOG(RENDERER, "Skipping render pass: no drawable available");
		commandEncoder = nil;
		return;
	}

	if (!renderPassDescriptor)
		renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];

	auto colorAttachment = renderPassDescriptor.colorAttachments[0];
	[colorAttachment setTexture:currentDrawable.texture];
	[colorAttachment setLoadAction:MTLLoadActionClear];
	[colorAttachment setStoreAction:MTLStoreActionStore];
	[colorAttachment setClearColor:MTLClearColorMake(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.0f)];

	if (currentImage >= commandBuffers.size())
		commandBuffers.resize(currentImage + 1);

	if (!commandBuffers[currentImage]) {
		commandBuffers[currentImage] = [queue commandBuffer];
		[commandBuffers[currentImage] setLabel:@"Render Frame"];
	}

	commandEncoder = [commandBuffers[currentImage] renderCommandEncoderWithDescriptor:renderPassDescriptor];
}

void MetalContext::NewFrame()
{
	if (!IsValid())
		return;

	currentImage = (currentImage + 1) % 3;
	currentDrawable = nil;
	verify(!rendering);
	rendering = true;
}

void MetalContext::EndFrame()
{
	if (!IsValid())
		return;

	[commandEncoder endEncoding];
	commandEncoder = nil;

	if (currentDrawable != nil)
	{
		RenderVideoRouting();
		[commandBuffers[currentImage] presentDrawable:currentDrawable];
	}

	// Don't wait for completion. Triple buffering (currentImage % 3) keeps in-flight frames apart.
	[commandBuffers[currentImage] commit];
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
				PresentFrame(lastFrameTexture, lastFrameViewport, lastFrameAR);
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

void MetalContext::DrawFrame(id<MTLTexture> texture, MTLViewport viewport, float aspectRatio)
{
	// Fit the content aspect ratio into the drawable
	float drawableWidth = (float)width;
	float drawableHeight = (float)height;
	float screenAR = drawableWidth / drawableHeight;

	float viewportWidth, viewportHeight, viewportX, viewportY;
	if (screenAR >= aspectRatio) {
		// Screen is wider than content: pillarbox
		viewportHeight = drawableHeight;
		viewportWidth = drawableHeight * aspectRatio;
		viewportX = (drawableWidth - viewportWidth) / 2.0f;
		viewportY = 0;
	} else {
		// Screen is taller than content: letterbox
		viewportWidth = drawableWidth;
		viewportHeight = drawableWidth / aspectRatio;
		viewportX = 0;
		viewportY = (drawableHeight - viewportHeight) / 2.0f;
	}

	MetalQuadVertex vtx[4] {
			{ -1, -1, 0, 0, 1 },  // bottom-left
			{  1, -1, 0, 1, 1 },  // bottom-right
			{ -1,  1, 0, 0, 0 },  // top-left
			{  1,  1, 0, 1, 0 },  // top-right
	};

	[commandEncoder pushDebugGroup:@"DrawFrame"];

	const bool bound = config::Rotate90
		? quadRotatePipeline->BindPipeline(commandEncoder)
		: quadPipeline->BindPipeline(commandEncoder);
	if (!bound) {
		// The present pipeline failed to compile
		[commandEncoder popDebugGroup];
		return;
	}

	MTLViewport framePort = { viewportX, viewportY, viewportWidth, viewportHeight, 0, 1 };
	[commandEncoder setViewport:framePort];

	// Clamp in float space before casting to unsigned, then clamp to the render target.
	const float sx = std::max(0.f, viewportX);
	const float sy = std::max(0.f, viewportY);
	const float sw = std::max(0.f, viewportWidth);
	const float sh = std::max(0.f, viewportHeight);
	MTLScissorRect scissor = {
		(NSUInteger)sx,
		(NSUInteger)sy,
		(NSUInteger)sw,
		(NSUInteger)sh
	};
	if (scissor.x + scissor.width > width)
		scissor.width = width - scissor.x;
	if (scissor.y + scissor.height > height)
		scissor.height = height - scissor.y;
	[commandEncoder setScissorRect:scissor];

	const bool nearest = config::TextureFiltering == 1 || !config::LinearInterpolation;
	if (config::Rotate90)
		quadRotateDrawer->Draw(commandEncoder, texture, vtx, nearest);
	else
		quadDrawer->Draw(commandEncoder, texture, vtx, nearest);

	[commandEncoder popDebugGroup];
}

// Sends the presented image, UI included, to Syphon
void MetalContext::RenderVideoRouting()
{
#ifdef VIDEO_ROUTING
	if (!config::VideoRouting)
	{
		videoRoutingTexture = nil;
		os_VideoRoutingTermMetal();
		return;
	}
	id<MTLTexture> source = currentDrawable.texture;
	int targetWidth = config::VideoRoutingScale ? config::VideoRoutingVRes * width / height : width;
	int targetHeight = config::VideoRoutingScale ? config::VideoRoutingVRes : height;
	targetWidth = std::clamp(targetWidth, 1, 16384);
	targetHeight = std::clamp(targetHeight, 1, 16384);
	if ((u32)targetWidth == width && (u32)targetHeight == height)
	{
		videoRoutingTexture = nil;
		os_VideoRoutingPublishFrameTexture(source, commandBuffers[currentImage], 0, 0, width, height);
		return;
	}
	// Scale the image to the requested size
	if (videoRoutingTexture == nil || videoRoutingTexture.width != (NSUInteger)targetWidth
			|| videoRoutingTexture.height != (NSUInteger)targetHeight)
	{
		MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:source.pixelFormat
				width:targetWidth height:targetHeight mipmapped:NO];
		desc.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
		desc.storageMode = MTLStorageModePrivate;
		videoRoutingTexture = [device newTextureWithDescriptor:desc];
		[videoRoutingTexture setLabel:@"Video Routing"];
	}
	MTLRenderPassDescriptor *passDesc = [[MTLRenderPassDescriptor alloc] init];
	passDesc.colorAttachments[0].texture = videoRoutingTexture;
	passDesc.colorAttachments[0].loadAction = MTLLoadActionDontCare;
	passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
	id<MTLRenderCommandEncoder> encoder = [commandBuffers[currentImage] renderCommandEncoderWithDescriptor:passDesc];
	[encoder setLabel:@"Video Routing Scale"];
	if (quadPipeline->BindPipeline(encoder))
	{
		MetalQuadVertex vtx[4] {
				{ -1, -1, 0, 0, 1 },
				{  1, -1, 0, 1, 1 },
				{ -1,  1, 0, 0, 0 },
				{  1,  1, 0, 1, 0 },
		};
		[encoder setViewport:MTLViewport{ 0, 0, (double)targetWidth, (double)targetHeight, 0, 1 }];
		quadDrawer->Draw(encoder, source, vtx, false);
	}
	[encoder endEncoding];
	os_VideoRoutingPublishFrameTexture(videoRoutingTexture, commandBuffers[currentImage], 0, 0, targetWidth, targetHeight);
#endif
}

void MetalContext::PresentFrame(id<MTLTexture> texture, MTLViewport viewport, float aspectRatio)
{
	// nextDrawable returns an autoreleased object. Drain it every frame so the layer's
	// drawable pool isn't exhausted when presenting from a thread without a pool.
	@autoreleasepool {
	lastFrameTexture = texture;
	lastFrameViewport = viewport;
	lastFrameAR = aspectRatio;

	if (texture != nil && IsValid())
	{
		NewFrame();
		gui_draw_osd();
		BeginRenderPass();

		// BeginRenderPass leaves a nil encoder when no drawable was available
		if (commandEncoder != nil)
		{
			if (lastFrameTexture != nil) // Might have been nullified if swap chain recreated
				DrawFrame(texture, viewport, aspectRatio);
			overlay->Draw(commandEncoder, width, height, settings.display.uiScale, config::FloatVMUs, true);
			imguiDriver->renderDrawData(ImGui::GetDrawData(), false);
		}
		EndFrame();
	}
	} // @autoreleasepool
}

void MetalContext::PresentLastFrame()
{
	if (lastFrameTexture != nil && IsValid())
		DrawFrame(lastFrameTexture, lastFrameViewport, lastFrameAR);
}

void MetalContext::term()
{
	lastFrameTexture = nil;
	videoRoutingTexture = nil;
#ifdef VIDEO_ROUTING
	os_VideoRoutingTermMetal();
#endif
	currentDrawable = nil;
	imguiDriver.reset();
	if (overlay)
		overlay->Term();
	overlay.reset();
	quadDrawer.reset();
	quadPipeline.reset();
	quadPipelineWithAlpha.reset();
	quadRotateDrawer.reset();
	quadRotatePipeline.reset();
	shaderManager.reset();
	commandBuffers.clear();
	commandEncoder = nil;
	renderPassDescriptor = nil;
	queue = nil;
	device = nil;
	layer = nil;
#ifdef USE_SDL
	if (metalView != nullptr) {
		SDL_Metal_DestroyView(metalView);
		metalView = nullptr;
	}
#endif
}

bool MetalContext::HasSurfaceDimensionChanged() const
{
	auto size = [layer drawableSize];
	return width != size.width || height != size.height;
}

void MetalContext::SetWindowSize(u32 width, u32 height)
{
	if (this->width != width || this->height != height)
	{
		if (width == 0 || height == 0) {
			WARN_LOG(RENDERER, "Invalid window size: %dx%d, ignoring", width, height);
			return;
		}
		DEBUG_LOG(RENDERER, "Metal context size change: %dx%d -> %dx%d", this->width, this->height, width, height);

		this->width = width;
		this->height = height;
		settings.display.width = width;
		settings.display.height = height;

		resize();
	}
}

// Bounded replacement for [commandBuffer waitUntilCompleted]: a command buffer that never
// reports completion must not hang the caller. Commits the command buffer.
static bool commitAndWaitBounded(id<MTLCommandBuffer> commandBuffer, uint64_t timeoutMs, const char *what)
{
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);
	[commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> cb) {
		(void)cb;
		dispatch_semaphore_signal(sem);
	}];
	[commandBuffer commit];
	if (dispatch_semaphore_wait(sem,
			dispatch_time(DISPATCH_TIME_NOW, (int64_t)timeoutMs * NSEC_PER_MSEC)) != 0)
	{
		WARN_LOG(RENDERER, "%s: GPU did not signal completion within %llu ms - giving up on this frame",
				 what, (unsigned long long)timeoutMs);
		return false;
	}
	return true;
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

	// A zero/NaN aspect or an empty viewport (no frame presented yet) gives a 0 or
	// absurd size, and newTextureWithDescriptor would abort.
	if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
		return false;

	MTLTextureDescriptor *renderTargetDesc = [[MTLTextureDescriptor alloc] init];
	renderTargetDesc.width = width;
	renderTargetDesc.height = height;
	// Must match MetalQuadPipeline, which compiles its colour attachment as
	// BGRA8Unorm (metal_quad.mm). An RGBA8 target here trips the Metal API
	// validation assert in setRenderPipelineState: and, without validation,
	// renders with the channels swapped.
	renderTargetDesc.pixelFormat = MTLPixelFormatBGRA8Unorm;
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

	// Apply bounds checking to prevent Metal validation errors
	MTLScissorRect scissor = {
			0, 0,
			(NSUInteger)width, (NSUInteger)height
	};

	// Basic validation - ensure scissor doesn't exceed render target bounds
	scissor.width = std::min(scissor.width, (NSUInteger)width);
	scissor.height = std::min(scissor.height, (NSUInteger)height);
	scissor.width = std::max(scissor.width, (NSUInteger)1);
	scissor.height = std::max(scissor.height, (NSUInteger)1);

	[renderEncoder setScissorRect:scissor];

	MetalQuadVertex vtx[4] = {
			{ -1.f, -1.f, 0.f, 0.f, 1.f },
			{  1.f, -1.f, 0.f, 1.f, 1.f },
			{ -1.f,  1.f, 0.f, 0.f, 0.f },
			{  1.f,  1.f, 0.f, 1.f, 0.f },
	};

	// Match what the user sees, so the savestate thumbnail is upright. The frame is
	// not rotated by the projection; the 90° turn happens at present time in DrawFrame.
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

	if (!commitAndWaitBounded(commandBuffer, 1000, "MetalContext::GetLastFrame"))
		return false;

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
	// BGRA -> RGB conversion
	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			data.push_back(img[2]); // R
			data.push_back(img[1]); // G
			data.push_back(img[0]); // B
			img += 4;               // Skip A
		}
	}

	return true;
}

void MetalContext::Create(void *window, void *display) {
	new MetalContext(window, display);
}

MetalContext::MetalContext(void *window, void *display)
	: GraphicsContext(window, display)
{
	verify(contextInstance == nullptr);
	contextInstance = this;
	if (!init()) {
		// The MetalContext dtor doesn't run when the ctor throws
		term();
		contextInstance = nullptr;
		throw FlycastException("Metal initialization failed");
	}
}

MetalContext::~MetalContext() {
	verify(contextInstance == this);
	term();
	contextInstance = nullptr;
}
