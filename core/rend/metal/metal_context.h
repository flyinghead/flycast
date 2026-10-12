// Native Apple Silicon Metal backend. SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#if !defined(__aarch64__) && !defined(__arm64__)
#error "The native Metal backend requires Apple Silicon (arm64)"
#endif
#include "wsi/context.h"
#include <memory>
#include <utility>
#include <os/availability.h>

#ifdef __OBJC__
#include <array>
#include <functional>
#include <mutex>
#pragma push_macro("PAGE_SIZE")
#pragma push_macro("PAGE_MASK")
// The macOS SDK exposes QuickDraw names also used by the PVR core.
#define Rect MacOSRect
#define RGBColor MacOSRGBColor
#import <Metal/Metal.h>
#include "metal_commands.h"
#import <QuartzCore/CAMetalLayer.h>
#include <dispatch/dispatch.h>
struct ImDrawData;
#undef RGBColor
#undef Rect
#pragma pop_macro("PAGE_MASK")
#pragma pop_macro("PAGE_SIZE")
#endif

class API_AVAILABLE(macos(26.0)) MetalContext final : public GraphicsContext
{
public:
	static void Create(void *window, void *display);
	static MetalContext *Instance() { return static_cast<MetalContext *>(GraphicsContext::Instance()); }
	~MetalContext() override;
	void resize() override;
	std::string getDriverName() override;
	std::string getDriverVersion() override { return "Metal 4 / MSL 4.0"; }
	bool isAMD() override;
	bool supportsPackedTextures() const;
	bool supportsMetalFX() const;
	bool hasPerPixel() override { return true; }
	void setSwapInterval(int interval) override;
	void waitIdle();
	void resetUIDriver();
	void present();
	void drawOverlays();
#ifdef METAL_RENDERER_TESTS
	void requestVSyncValidation();
#endif

#ifdef __OBJC__
	id<MTLDevice> device() const;
	FCMetal4Queue * queue() const;
	// Include independently encoded work in waitIdle's completion tracking.
	void submitCommandBuffer(FCMetal4Commands * command);
	id<MTLLibrary> library() const;
	void beginTextureUpdates();
	void endTextureUpdates(bool deferForRender = false);
	void flushTextureUpdates();
	static constexpr size_t RenderBatchSlots = 3;
	static constexpr size_t PresentationSlots = 3;
	void flushRenderCommands(bool discardRasterDepth = false);
	// Owns one submission, including preceding Process uploads and up to
	// RenderBatchSlots screen renders with distinct CPU-writable resources. The
	// cancellation callback runs synchronously on destruction, never on the GPU.
	class RenderCommands {
	public:
		~RenderCommands();
		RenderCommands(RenderCommands&& other) noexcept;
		RenderCommands(const RenderCommands&) = delete;
		RenderCommands& operator=(const RenderCommands&) = delete;
		FCMetal4Commands * buffer() const { return command; }
		void retainRenderSlot(dispatch_semaphore_t available);
		FCMetal4RenderEncoder * continuingRasterEncoder(id<MTLTexture> color, id<MTLTexture> depth);
		void retainRasterEncoder(FCMetal4RenderEncoder * encoder, id<MTLTexture> color,
			id<MTLTexture> depth, bool deferredDepthStore = false);
		void endRasterEncoder(bool preserveDepth = true);
		void commit(bool discardRasterDepth = false);
	private:
		friend class MetalContext;
		RenderCommands(FCMetal4Commands * command, dispatch_group_t submissions, dispatch_semaphore_t uploadAvailable,
			std::function<void()> canceledBatch) noexcept;
		FCMetal4Commands * command;
		dispatch_group_t submissions;
		FCMetal4RenderEncoder * rasterEncoder = nil;
		id<MTLTexture> rasterColor = nil, rasterDepth = nil;
		bool deferredDepthStore = false;
		dispatch_semaphore_t uploadAvailable;
		std::function<void()> canceledBatch;
		std::array<dispatch_semaphore_t, RenderBatchSlots> renderAvailable{};
		size_t renderCount = 0;
		u64 timingSubmission = 0;
		bool submitted = false;
	};
	RenderCommands beginRenderCommands(std::function<void()> canceledBatch, bool continueScreen = false);
	void deferRenderCommands(RenderCommands&& commands);
	bool canReuseTextureUpload(id<MTLTexture> texture) const;
	// Caller must also exclude its current encoder's texture readers.
	bool canReuseTextureWrites(id<MTLTexture> texture) const;
	void uploadTextureLevel(id<MTLTexture> texture, NSUInteger level,
		const void *data, NSUInteger bytesPerRow);
	void generateMipmaps(id<MTLTexture> texture);
	FCMetal4Commands * beginFrame();
	std::pair<id<MTLBuffer>, id<MTLBuffer>> uploadUIBuffers(const ImDrawData& data);
	FCMetal4RenderEncoder * beginPresentation();
	// Pixel dimensions of the acquired drawable; zero when none is acquired.
	std::pair<int, int> presentationSize() const;
	void setLastFrame(id<MTLTexture> texture, float aspectRatio, double videoTime = 0);
	id<MTLTexture> lastFrame() const;
	float lastFrameAspectRatio() const;
	id<MTLTexture> makeTexture(int width, int height, MTLPixelFormat format,
		MTLTextureUsage usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget,
		bool mipmapped = false, bool cpuVisible = false);
	id<MTLRenderPipelineState> makePipeline(NSString *vertex, NSString *fragment,
		MTLVertexDescriptor *layout, MTLPixelFormat color, bool depth,
		MTL4RenderPipelineColorAttachmentDescriptor *blend = nil);
	id<MTLComputePipelineState> makeComputePipeline(NSString *function);
	struct MaterialConstants {
		u32 flags, tsp0, tsp1;
		bool sequence;
		bool operator==(const MaterialConstants& other) const {
			return flags == other.flags && tsp0 == other.tsp0
				&& tsp1 == other.tsp1 && sequence == other.sequence;
		}
	};
	void makePipelineAsync(NSString *vertex, NSString *fragment,
		MTLVertexDescriptor *layout, MTLPixelFormat color, bool depth,
		MTL4RenderPipelineColorAttachmentDescriptor *blend, MaterialConstants constants,
		std::function<void(id<MTLRenderPipelineState>, NSError *)> completion,
		std::function<bool()> wanted);
	void drawQuad(FCMetal4RenderEncoder * encoder, id<MTLTexture> texture,
		MTLViewport viewport, bool rotate = false, bool ignoreAlpha = true,
		const float *color = nullptr, bool linear = true);
	void readTexture(id<MTLTexture> texture, std::vector<u8>& pixels);
	class TextureReadback {
	public:
		~TextureReadback() = default;
		// Call after the owning command buffer has been committed. The lock
		// prevents another readback from reusing storage until this object dies.
		void finish(std::vector<u8>& pixels);
		// Returns packed RGBA rows for immediate CPU conversion. Contiguous
		// shared storage is read directly; padded rows are copied into scratch.
		// Keep this object and scratch alive until the returned data is consumed.
		const u8 *finishForConversion(std::vector<u8>& scratch);
	private:
		friend class MetalContext;
		explicit TextureReadback(std::mutex& mutex) : lock(mutex) {}
		const u8 *completePixels(std::vector<u8>& pixels, bool allowMapped);
		std::unique_lock<std::mutex> lock;
		FCMetal4Commands * command;
		id<MTLBuffer> buffer;
		NSUInteger width = 0, height = 0, row = 0;
	};
	std::unique_ptr<TextureReadback> encodeReadTexture(id<MTLTexture> texture, FCMetal4Commands * command);
#endif
private:
#ifdef __OBJC__
	FCMetal4ComputeEncoder * textureBlitEncoder();
	void sealTextureUpdates();
	void finishTextureUpdates();
	MTL4FunctionDescriptor *shaderFunction(NSString *name);
	id<MTLRenderPipelineState> quadPipeline(bool ignoreAlpha);
	void drawOverlays(FCMetal4RenderEncoder * encoder);
#ifdef VIDEO_ROUTING
	void publishVideoRouting(id<MTLTexture> texture, FCMetal4Commands * command);
#endif
#ifdef METAL_RENDERER_TESTS
	void validateSyntheticVSync() API_AVAILABLE(macos(26.0));
	void validateSyntheticOverlays();
#ifdef VIDEO_ROUTING
	void validateSyntheticSyphon();
#endif
#endif
#endif
	MetalContext(void *window, void *display);
	struct Impl;
	std::unique_ptr<Impl> impl;
};
