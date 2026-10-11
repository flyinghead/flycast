// SPDX-License-Identifier: GPL-2.0-or-later
#include "metal_context.h"
#include "metal_shaders.h"
#include "metal_shader_options.h"
#include "metal_pipeline_cache.h"
#include "metal_fx.h"
#include "metal_presentation_timing.h"
#include "ui/imgui_driver.h"
#include "ui/gui.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/spg.h"
#include "sdl/sdl.h"
#include "rend/transform_matrix.h"
#include "rend/osd.h"
#include "rend/TexCache.h"
#include <SDL.h>
#include <SDL_metal.h>
#include <unordered_map>
#include <array>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <exception>
#include <optional>
#include <chrono>
#ifdef METAL_RENDERER_TESTS
#include <functional>
#endif
#ifdef VIDEO_ROUTING
void os_VideoRoutingPublishMetalTexture(id<MTLTexture> texture, FCMetal4Commands * command);
void os_VideoRoutingTermMetal();
bool os_VideoRoutingMetalHasClients(id<MTLDevice> device);
#endif

static void checkMetal(NSError *error, const char *operation)
{
	if (error)
		throw RendererException(std::string(operation) + ": " + error.localizedDescription.UTF8String);
}

static void commitCommands(FCMetal4Commands * command, dispatch_group_t submissions)
{
	if (!command || !submissions || command.status >= FCMetal4StatusCommitted)
		throw RendererException("Invalid Metal command buffer submission");
	// Completion retains the group independently of the context/owner. An
	// unsubmitted render batch never enters it and still cancels its own slots.
	[command addCompletedHandler:^(FCMetal4Commands *) {
		dispatch_group_leave(submissions);
	}];
	dispatch_group_enter(submissions);
	try {
		[command commit];
	} catch (...) {
		// A validation/allocation failure before submission has no feedback.
		if (command.status == FCMetal4StatusEncoding) dispatch_group_leave(submissions);
		throw;
	}
}

#include "metal_display_link.h"

// Only the serial material queue accesses this cache. Jobs keep it and their
// library alive independently of the context across renderer/backend changes.
struct MetalMaterialFunctions
{
	struct Hash {
		size_t operator()(const MetalContext::MaterialConstants& key) const {
			size_t hash = key.flags;
			for (u32 value : {key.tsp0, key.tsp1, u32(key.sequence)})
				hash ^= size_t(value) + 0x9e3779b9u + (hash << 6) + (hash >> 2);
			return hash;
		}
	};
	std::unordered_map<std::string,
		std::unordered_map<MetalContext::MaterialConstants, MTL4SpecializedFunctionDescriptor *, Hash>> functions;
	MTL4FunctionDescriptor *get(id<MTLLibrary> library, NSString *name,
		const MetalContext::MaterialConstants& key)
	{
		auto& variants = functions[name.UTF8String];
		auto existing = variants.find(key);
		if (existing != variants.end()) return existing->second;
		MTLFunctionConstantValues *values = [[MTLFunctionConstantValues alloc] init];
		[values setConstantValue:&key.flags type:MTLDataTypeUInt atIndex:0];
		[values setConstantValue:&key.tsp0 type:MTLDataTypeUInt atIndex:1];
		[values setConstantValue:&key.tsp1 type:MTLDataTypeUInt atIndex:2];
		[values setConstantValue:&key.sequence type:MTLDataTypeBool atIndex:3];
		MTL4LibraryFunctionDescriptor *base = [[MTL4LibraryFunctionDescriptor alloc] init];
		base.library = library;
		base.name = name;
		MTL4SpecializedFunctionDescriptor *function = [[MTL4SpecializedFunctionDescriptor alloc] init];
		function.functionDescriptor = base;
		function.constantValues = values;
		variants.emplace(key, function);
		return function;
	}
};

struct MetalContext::Impl
{
	id<MTLDevice> device;
	id<MTL4Compiler> compiler;
	id<MTL4Compiler> materialCompiler;
	FCMetal4Queue * queue;
	dispatch_group_t submissions = dispatch_group_create();
	id<MTLLibrary> library;
	dispatch_queue_t shaderQueue;
	std::unordered_map<std::string, MTL4LibraryFunctionDescriptor *> functions;
	std::shared_ptr<MetalMaterialFunctions> materialFunctions = std::make_shared<MetalMaterialFunctions>();
	std::shared_ptr<MetalPipelineCache> pipelineCache;
	std::unique_ptr<MetalFXUpscaler> metalFX;
	bool packedTextures = false;
	FCMetal4Commands * textureUpdates;
	FCMetal4ComputeEncoder * textureBlit;
	unsigned textureUpdateDepth = 0;
	std::optional<RenderCommands> deferredRender;
	struct TextureUploadSlot {
		dispatch_semaphore_t available = dispatch_semaphore_create(1);
		std::vector<id<MTLBuffer>> pages;
		size_t pageCursor = 0;
		NSUInteger pageOffset = 0;
	};
	std::array<TextureUploadSlot, 3> textureUploadSlots;
	size_t nextTextureUploadSlot = 0, currentTextureUploadSlot = 0;
	u64 profilingTextureUpload = 0;
	struct TextureUploadTiming {
		u64 number = 0, sourceBytes = 0, stagingBytes = 0;
		size_t levels = 0, mipGenerations = 0, allocations = 0;
		double slotWaitMs = 0;
	} textureUploadTiming;
	id<MTLBuffer> readbackBuffer;
	std::mutex readbackMutex;
	CAMetalLayer *layer;
	SDL_MetalView view = nullptr;
	FCMetal4Commands * frame;
	id<CAMetalDrawable> drawable;
	bool drawableRequested = false;
	FCMetal4RenderEncoder * encoder;
	id<MTLTexture> lastFrame;
	float aspectRatio = 4.f / 3.f;
	int swapInterval = 1;
	double sourceVideoTime = 0, displayRefresh = 0;
	metal::PresentationTimeline presentationTimeline;
	std::unique_ptr<MetalDisplayLink> displayLink;
	std::shared_ptr<MetalDisplayImage> composition;
	struct CompositionSlot {
		id<MTLTexture> texture;
		id<MTLSharedEvent> readyEvent;
		u64 readyValue = 0;
		dispatch_semaphore_t available = dispatch_semaphore_create(1);
	};
	// Three queued images, one visible image, two display GPU readers and
	// one producer. A four-slot pool drops valid 60-FPS frames even when
	// neither GPU queue is overloaded.
	std::array<CompositionSlot, 7> compositions;
	size_t nextComposition = 0;
	bool frameTimings = false;
	bool presentationTimings = false;
	u64 profilingFrame = 0;
	u64 profilingSubmission = 0;
	u64 contentFrame = 0;
	u64 presentationPhase = 0;
	bool presentationGui = true, presentationSync = true, presentationFast = false;
#ifdef METAL_RENDERER_TESTS
	int vsyncTestWarmup = 0;
	std::function<void(id<MTLDrawable>)> testPresentationObserver;
#endif
	struct FrameSlot {
		dispatch_semaphore_t available = dispatch_semaphore_create(1);
		std::vector<std::array<id<MTLBuffer>, 2>> uiBuffers;
		size_t uiBufferCursor = 0;
		std::array<id<MTLTexture>, 8> vmu;
		std::array<u64, 8> vmuChanged{{~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull}};
		id<MTLTexture> routed;
	};
	std::array<FrameSlot, PresentationSlots> slots;
	size_t nextSlot = 0, currentSlot = 0;
	std::unordered_map<u64, id<MTLRenderPipelineState>> quads;
	id<MTLSamplerState> nearest, linear;
	id<MTLTexture> crosshair;
	id<MTLRenderPipelineState> crosshairPipeline;
	~Impl() {
		if (pipelineCache) {
			// Producers must finish before the writer's final flush. Pending
			// material jobs already own their inputs and skip canceled renders.
			dispatch_sync(shaderQueue, ^{});
			pipelineCache->finish();
		}
		if (view) SDL_Metal_DestroyView(view);
	}
};

class MetalDriver final : public ImGuiDriver
{
	struct UITexture { id<MTLTexture> texture; id<MTLSamplerState> sampler; };
	std::unordered_map<std::string, UITexture> textures;
	std::unordered_map<uintptr_t, id<MTLSamplerState>> textureSamplers;
	std::unordered_map<ImTextureData *, id<MTLTexture>> atlasTextures;
	id<MTLRenderPipelineState> pipeline;
	id<MTLDepthStencilState> disabledDepth;
	id<MTLSamplerState> linear, point;
	bool frameRendered = false;

public:
	MetalDriver()
	{
		auto context = MetalContext::Instance();
		MTLVertexDescriptor *layout = [MTLVertexDescriptor vertexDescriptor];
		layout.attributes[0].format = MTLVertexFormatFloat2;
		layout.attributes[0].offset = offsetof(ImDrawVert, pos);
		layout.attributes[1].format = MTLVertexFormatFloat2;
		layout.attributes[1].offset = offsetof(ImDrawVert, uv);
		layout.attributes[2].format = MTLVertexFormatUChar4Normalized;
		layout.attributes[2].offset = offsetof(ImDrawVert, col);
		layout.layouts[0].stride = sizeof(ImDrawVert);
		MTL4RenderPipelineColorAttachmentDescriptor *blend = [[MTL4RenderPipelineColorAttachmentDescriptor alloc] init];
		blend.blendingState = MTL4BlendStateEnabled;
		blend.sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
		blend.destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		blend.sourceAlphaBlendFactor = MTLBlendFactorOne;
		blend.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		pipeline = context->makePipeline(@"ui_vertex", @"ui_fragment", layout, MTLPixelFormatBGRA8Unorm, false, blend);
		MTLDepthStencilDescriptor *depth = [[MTLDepthStencilDescriptor alloc] init];
		depth.depthCompareFunction = MTLCompareFunctionAlways;
		disabledDepth = [context->device() newDepthStencilStateWithDescriptor:depth];
		if (!disabledDepth) throw FlycastException("Cannot create the Metal UI depth state");
		MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
		sd.supportArgumentBuffers = YES;
		sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
		sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToBorderColor;
		// Packed A1BGR5/ABGR4 textures cannot use an opaque-black border.
		sd.borderColor = MTLSamplerBorderColorTransparentBlack;
		linear = [context->device() newSamplerStateWithDescriptor:sd];
		sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
		point = [context->device() newSamplerStateWithDescriptor:sd];
		if (!linear || !point) throw FlycastException("Cannot create the Metal UI samplers");
		ImGui::GetIO().BackendRendererName = "flycast_metal4";
		ImGui::GetIO().BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
	}
	~MetalDriver() override
	{
		for (const auto& entry : atlasTextures) {
			entry.first->SetTexID(ImTextureID_Invalid);
			entry.first->SetStatus(ImTextureStatus_Destroyed);
		}
		ImGui::GetIO().BackendRendererName = nullptr;
		ImGui::GetIO().BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
	}
	void newFrame() override
	{
	}
	ImTextureID getTexture(const std::string& name) override
	{
		auto it = textures.find(name);
		return it == textures.end() ? ImTextureID{} : (ImTextureID)(__bridge void *)it->second.texture;
	}
	ImTextureID updateTexture(const std::string& name, const u8 *data, int width, int height, bool nearest) override
	{
		auto context = MetalContext::Instance();
		auto texture = context->makeTexture(width, height, MTLPixelFormatRGBA8Unorm, MTLTextureUsageShaderRead, false, true);
		[texture replaceRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0 withBytes:data bytesPerRow:width * 4];
		if (auto previous = textures.find(name); previous != textures.end())
			textureSamplers.erase((uintptr_t)(__bridge void *)previous->second.texture);
		id<MTLSamplerState> sampler = nearest ? point : linear;
		textures[name] = {texture, sampler};
		textureSamplers[(uintptr_t)(__bridge void *)texture] = sampler;
		return getTexture(name);
	}
	void deleteTexture(const std::string& name) override
	{
		auto texture = textures.find(name);
		if (texture == textures.end()) return;
		textureSamplers.erase((uintptr_t)(__bridge void *)texture->second.texture);
		textures.erase(texture);
	}
	void setFrameRendered() override { frameRendered = true; }
	void renderDrawData(ImDrawData *data, bool guiOpen) override
	{
		auto context = MetalContext::Instance();
		if (data->Textures)
			for (ImTextureData *tex : *data->Textures) {
				if (tex->Status == ImTextureStatus_WantDestroy) {
					atlasTextures.erase(tex);
					tex->SetTexID(ImTextureID_Invalid);
					tex->SetStatus(ImTextureStatus_Destroyed);
				} else if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates) {
					IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
					// Replace instead of mutating textures retained by in-flight frames.
					id<MTLTexture> image = context->makeTexture(tex->Width, tex->Height,
						MTLPixelFormatRGBA8Unorm, MTLTextureUsageShaderRead, false, true);
					[image replaceRegion:MTLRegionMake2D(0, 0, tex->Width, tex->Height)
						mipmapLevel:0 withBytes:tex->GetPixels() bytesPerRow:tex->GetPitch()];
					atlasTextures[tex] = image;
					tex->SetTexID((ImTextureID)(__bridge void *)image);
					tex->SetStatus(ImTextureStatus_OK);
				}
			}
		if (guiOpen && renderer != nullptr) renderer->RenderLastFrame();
		if (!frameRendered && !guiOpen) return;
		frameRendered = true;
		FCMetal4RenderEncoder * encoder = context->beginPresentation();
		if (!encoder || data->DisplaySize.x <= 0 || data->DisplaySize.y <= 0) return;
		// Presentation above still draws the retained game image. With no
		// lists there are neither UI draws nor callbacks needing UI state.
		if (data->CmdLists.empty()) return;
		const int width = data->DisplaySize.x * data->FramebufferScale.x;
		const int height = data->DisplaySize.y * data->FramebufferScale.y;
		if (width <= 0 || height <= 0) return;
		const auto [surfaceWidth, surfaceHeight] = context->presentationSize();
		const int clipWidth = std::min(width, surfaceWidth), clipHeight = std::min(height, surfaceHeight);
		if (clipWidth <= 0 || clipHeight <= 0) return;
		const auto buffers = context->uploadUIBuffers(*data);
		id<MTLBuffer> vertices = buffers.first, indices = buffers.second;
		float transform[4] = {2 / data->DisplaySize.x, -2 / data->DisplaySize.y,
			-1 - data->DisplayPos.x * 2 / data->DisplaySize.x, 1 + data->DisplayPos.y * 2 / data->DisplaySize.y};
		auto setup = [&]() {
			[encoder setRenderPipelineState:pipeline];
			[encoder setViewport:MTLViewport{0, 0, (double)width, (double)height, 0, 1}];
			[encoder setCullMode:MTLCullModeNone];
			[encoder setDepthStencilState:disabledDepth];
			[encoder setVertexBytes:transform length:sizeof(transform) atIndex:1];
		};
		setup();
		id<MTLTexture> boundTexture = nil;
		id<MTLSamplerState> boundSampler = nil;
		bool vertexBindingValid = false, textureBindingValid = false;
		MTLScissorRect boundScissor{};
		bool scissorBindingValid = false;
		NSUInteger globalVertexOffset = 0, globalIndexOffset = 0;
		for (const ImDrawList *list : data->CmdLists)
		{
			for (const ImDrawCmd& command : list->CmdBuffer)
			{
				if (command.UserCallback)
				{
					if (command.UserCallback == ImDrawCallback_ResetRenderState) setup();
					else command.UserCallback(list, &command);
					// Callbacks can replace these bindings on the same encoder.
					vertexBindingValid = textureBindingValid = scissorBindingValid = false;
					continue;
				}
				int x = std::max(0, int((command.ClipRect.x - data->DisplayPos.x) * data->FramebufferScale.x));
				int y = std::max(0, int((command.ClipRect.y - data->DisplayPos.y) * data->FramebufferScale.y));
				int right = std::min(clipWidth, int((command.ClipRect.z - data->DisplayPos.x) * data->FramebufferScale.x));
				int bottom = std::min(clipHeight, int((command.ClipRect.w - data->DisplayPos.y) * data->FramebufferScale.y));
				if (right <= x || bottom <= y || command.ElemCount == 0 || !vertices || !indices) continue;
				id<MTLTexture> texture = (__bridge id<MTLTexture>)(void *)command.GetTexID();
				const MTLScissorRect scissor{(NSUInteger)x, (NSUInteger)y, (NSUInteger)(right - x), (NSUInteger)(bottom - y)};
				if (!scissorBindingValid || boundScissor.x != scissor.x || boundScissor.y != scissor.y
						|| boundScissor.width != scissor.width || boundScissor.height != scissor.height) {
					[encoder setScissorRect:scissor];
					boundScissor = scissor;
					scissorBindingValid = true;
				}
				if (!vertexBindingValid) {
					[encoder setVertexBuffer:vertices offset:0 atIndex:0];
					vertexBindingValid = true;
				}
				if (!textureBindingValid || boundTexture != texture) {
					id<MTLSamplerState> sampler = linear;
					auto textureSampler = textureSamplers.find((uintptr_t)(__bridge void *)texture);
					if (textureSampler != textureSamplers.end()) sampler = textureSampler->second;
					[encoder setFragmentTexture:texture atIndex:0];
					if (!textureBindingValid || boundSampler != sampler)
						[encoder setFragmentSamplerState:sampler atIndex:0];
					boundTexture = texture; boundSampler = sampler;
					textureBindingValid = true;
				}
				// Indices stay local to their list; baseVertex selects its packed vertices.
				[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:command.ElemCount
					indexType:sizeof(ImDrawIdx) == 2 ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32
					indexBuffer:indices indexBufferOffset:(globalIndexOffset + command.IdxOffset) * sizeof(ImDrawIdx)
					instanceCount:1 baseVertex:NSInteger(globalVertexOffset + command.VtxOffset) baseInstance:0];
			}
			globalVertexOffset += list->VtxBuffer.Size;
			globalIndexOffset += list->IdxBuffer.Size;
		}
	}
	void present() override
	{
		if (frameRendered) MetalContext::Instance()->present();
		frameRendered = false;
	}
};

MetalContext::MetalContext(void *window, void *display)
	: GraphicsContext(window, display), impl(std::make_unique<Impl>())
{
	@autoreleasepool {
		if (@available(macOS 26.0, *)) {
			impl->device = MTLCreateSystemDefaultDevice();
			if (!impl->device || ![impl->device supportsFamily:MTLGPUFamilyApple7]
					|| ![impl->device supportsFamily:MTLGPUFamilyMetal4])
				throw FlycastException("Metal 4 on an Apple Silicon GPU is required");
			NOTICE_LOG(RENDERER, "Apple Silicon Metal device: %s", impl->device.name.UTF8String);
			// Every supported Apple Silicon GPU supports these packed formats.
			impl->packedTextures = true;
		} else throw FlycastException("Metal 4 requires macOS 26 or later");
		impl->queue = [[FCMetal4Queue alloc] initWithDevice:impl->device];
		if (!impl->queue) throw FlycastException("Cannot create the Metal command queue");
		if (!impl->submissions) throw FlycastException("Cannot create Metal submission tracking");
		impl->shaderQueue = dispatch_queue_create("org.flycast.metal.materials",
			dispatch_queue_attr_make_with_qos_class(DISPATCH_QUEUE_SERIAL, QOS_CLASS_UTILITY, 0));
		if (!impl->shaderQueue) throw FlycastException("Cannot create the Metal shader queue");
		MTLCompileOptions *options = metalShaderOptions(impl->device);
		NSError *error = nil;
		MTL4CompilerDescriptor *compilerDescriptor = [[MTL4CompilerDescriptor alloc] init];
		compilerDescriptor.label = @"Flycast Metal 4 compiler";
		impl->compiler = [impl->device newCompilerWithDescriptor:compilerDescriptor error:&error];
		checkMetal(error, "Metal 4 compiler");
		if (!impl->compiler) throw FlycastException("Cannot create the Metal 4 compiler");
		compilerDescriptor.label = @"Flycast Metal 4 material compiler";
		impl->materialCompiler = [impl->device newCompilerWithDescriptor:compilerDescriptor error:&error];
		checkMetal(error, "Metal 4 material compiler");
		if (!impl->materialCompiler) throw FlycastException("Cannot create the Metal 4 material compiler");
		MTL4LibraryDescriptor *libraryDescriptor = [[MTL4LibraryDescriptor alloc] init];
		libraryDescriptor.source = [NSString stringWithUTF8String:MetalShaderSource];
		libraryDescriptor.options = options;
		libraryDescriptor.name = @"Flycast PowerVR shaders";
		impl->library = [impl->compiler newLibraryWithDescriptor:libraryDescriptor error:&error];
		checkMetal(error, "Metal shader library");
		if (!impl->library) throw FlycastException("Cannot compile the Metal shader library");
		impl->pipelineCache = MetalPipelineCache::create(impl->compiler, impl->materialCompiler, MetalShaderSource,
			sizeof(MetalShaderSource) - 1, options);
		impl->metalFX = std::make_unique<MetalFXUpscaler>(impl->device, impl->compiler);
		if (!sdl_recreate_window(SDL_WINDOW_METAL)) throw FlycastException("Cannot create the SDL Metal window");
		impl->view = SDL_Metal_CreateView((SDL_Window *)this->window);
		if (!impl->view) throw FlycastException(SDL_GetError());
		impl->layer = (__bridge CAMetalLayer *)SDL_Metal_GetLayer(impl->view);
		if (!impl->layer) throw FlycastException("Cannot obtain the SDL Metal layer");
		impl->layer.device = impl->device;
		impl->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
		impl->layer.framebufferOnly = YES;
		impl->layer.maximumDrawableCount = 3;
		impl->layer.opaque = YES;
		MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
		sd.supportArgumentBuffers = YES;
		sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
		impl->nearest = [impl->device newSamplerStateWithDescriptor:sd];
		sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
		impl->linear = [impl->device newSamplerStateWithDescriptor:sd];
		if (!impl->nearest || !impl->linear) throw FlycastException("Cannot create the Metal presentation samplers");
		// These two fixed presentation pipelines must be ready before
		// acquiring a drawable for the first image or a VMU overlay.
		quadPipeline(true); quadPipeline(false);
		resize();
		impl->frameTimings = config::loadBool("log", "MetalFrameTiming", false);
		// Drawable feedback is independently selectable: collecting presented
		// times may affect the compositor's scheduling during GPU profiling.
		impl->presentationTimings = config::loadBool("log", "MetalPresentTiming", impl->frameTimings);
		resetUIDriver();
#ifdef METAL_RENDERER_TESTS
		if (config::loadBool("log", "MetalVSyncTests", false)) {
			NOTICE_LOG(RENDERER, "Metal VSync test ready: focus this window and press F9");
		} else {
		void metalValidateRenderer();
		metalValidateRenderer();
		validateSyntheticOverlays();
#ifdef VIDEO_ROUTING
		validateSyntheticSyphon();
#endif
		}
#endif
	}
}

void MetalContext::Create(void *window, void *display) { new MetalContext(window, display); }
#ifdef METAL_RENDERER_TESTS
void MetalContext::requestVSyncValidation() { impl->vsyncTestWarmup = 2; }
#endif
MetalContext::~MetalContext()
{
	impl->displayLink.reset();
	imguiDriver.reset();
	waitIdle();
#ifdef VIDEO_ROUTING
	os_VideoRoutingTermMetal();
#endif
}
id<MTLDevice> MetalContext::device() const { return impl->device; }
FCMetal4Queue * MetalContext::queue() const { return impl->queue; }
void MetalContext::submitCommandBuffer(FCMetal4Commands * command)
{
	if (!command || command.commandQueue != impl->queue)
		throw RendererException("Metal submission uses another command queue");
	commitCommands(command, impl->submissions);
}
id<MTLLibrary> MetalContext::library() const { return impl->library; }
void MetalContext::beginTextureUpdates()
{
	// A new independent batch must submit a previous Process whose Render
	// was skipped before acquiring another staging slot or changing its data.
	if (impl->textureUpdateDepth == 0) finishTextureUpdates();
	++impl->textureUpdateDepth;
}
void MetalContext::endTextureUpdates(bool deferForRender)
{
	if (impl->textureUpdateDepth == 0 || --impl->textureUpdateDepth != 0) return;
	if (deferForRender) sealTextureUpdates();
	else finishTextureUpdates();
}
void MetalContext::flushTextureUpdates() { flushRenderCommands(); finishTextureUpdates(); }
MetalContext::RenderCommands::RenderCommands(FCMetal4Commands * command, dispatch_group_t submissions,
	dispatch_semaphore_t uploadAvailable, std::function<void()> canceledBatch) noexcept
	: command(command), submissions(submissions), uploadAvailable(uploadAvailable), canceledBatch(std::move(canceledBatch)) {}
MetalContext::RenderCommands::RenderCommands(RenderCommands&& other) noexcept
	: command(other.command), submissions(other.submissions), rasterEncoder(other.rasterEncoder),
	  rasterColor(other.rasterColor), rasterDepth(other.rasterDepth), deferredDepthStore(other.deferredDepthStore),
	  uploadAvailable(other.uploadAvailable),
	  canceledBatch(std::move(other.canceledBatch)), renderAvailable(std::move(other.renderAvailable)),
	  renderCount(other.renderCount), timingSubmission(other.timingSubmission), submitted(other.submitted)
{
	other.command = nil;
	other.submissions = nil;
	other.rasterEncoder = nil;
	other.rasterColor = other.rasterDepth = nil;
	other.deferredDepthStore = false;
	other.uploadAvailable = nil;
}
MetalContext::RenderCommands::~RenderCommands()
{
	endRasterEncoder();
	if (!submitted && command) {
		// No GPU work was submitted, so completion cannot release staging or
		// render slots. Both uploaded and pending screen data need rebuilding.
		command = nil;
		if (uploadAvailable) dispatch_semaphore_signal(uploadAvailable);
		for (size_t i = 0; i < renderCount; ++i) dispatch_semaphore_signal(renderAvailable[i]);
		if (canceledBatch) canceledBatch();
	}
}
void MetalContext::RenderCommands::retainRenderSlot(dispatch_semaphore_t available)
{
	if (renderCount == renderAvailable.size()) throw RendererException("Metal render batch slot limit exceeded");
	renderAvailable[renderCount++] = available;
}
FCMetal4RenderEncoder * MetalContext::RenderCommands::continuingRasterEncoder(id<MTLTexture> color, id<MTLTexture> depth)
{
	if (rasterColor != color || rasterDepth != depth) endRasterEncoder();
	return rasterEncoder;
}
void MetalContext::RenderCommands::retainRasterEncoder(FCMetal4RenderEncoder * encoder,
	id<MTLTexture> color, id<MTLTexture> depth, bool deferredDepthStore)
{
	if (rasterEncoder) throw RendererException("Metal raster encoder already pending");
	rasterEncoder = encoder; rasterColor = color; rasterDepth = depth;
	this->deferredDepthStore = deferredDepthStore;
}
void MetalContext::RenderCommands::endRasterEncoder(bool preserveDepth)
{
	if (rasterEncoder) {
		if (deferredDepthStore) {
			const MTLStoreAction store = preserveDepth ? MTLStoreActionStore : MTLStoreActionDontCare;
			[rasterEncoder setDepthStoreAction:store];
			[rasterEncoder setStencilStoreAction:store];
		}
		[rasterEncoder endEncoding];
	}
	rasterEncoder = nil; rasterColor = rasterDepth = nil;
	deferredDepthStore = false;
}
void MetalContext::RenderCommands::commit(bool discardRasterDepth)
{
	endRasterEncoder(!discardRasterDepth);
	if (timingSubmission != 0) {
		// GPU timestamps describe this whole submission, including any Process
		// uploads and continued screen renders, rather than an individual render.
		const u64 number = timingSubmission;
		const size_t renders = renderCount;
		[command addCompletedHandler:^(FCMetal4Commands * done) {
			if (done.status == FCMetal4StatusCompleted && done.GPUStartTime > 0
					&& done.GPUEndTime >= done.GPUStartTime)
				NOTICE_LOG(RENDERER, "Metal timing submission %llu renders %zu GPU: %.3f ms",
					(unsigned long long)number, renders, (done.GPUEndTime - done.GPUStartTime) * 1000);
		}];
	}
	commitCommands(command, submissions);
	submitted = true;
}
void MetalContext::flushRenderCommands(bool discardRasterDepth)
{
	if (impl->deferredRender) {
		impl->deferredRender->commit(discardRasterDepth);
		impl->deferredRender.reset();
	}
}
void MetalContext::deferRenderCommands(RenderCommands&& commands)
{
	if (impl->deferredRender) throw RendererException("Metal render batch already pending");
	impl->deferredRender.emplace(std::move(commands));
}
MetalContext::RenderCommands MetalContext::beginRenderCommands(std::function<void()> canceledBatch, bool continueScreen)
{
	if (impl->textureUpdateDepth != 0)
		throw RendererException("Metal render started during texture updates");
	sealTextureUpdates();
	if (impl->deferredRender) {
		// Flush before the renderer can wrap to a slot held by this same
		// unsubmitted batch. RTT/readback also starts after the screen work.
		if (!continueScreen || impl->textureUpdates
				|| impl->deferredRender->renderCount == RenderBatchSlots) flushRenderCommands();
		else {
			RenderCommands commands(std::move(*impl->deferredRender));
			impl->deferredRender.reset();
			return commands;
		}
	}
	FCMetal4Commands * cb = impl->textureUpdates;
	dispatch_semaphore_t available = cb ? impl->textureUploadSlots[impl->currentTextureUploadSlot].available : nil;
	if (!cb) cb = [impl->queue commandBuffer];
	if (!cb) throw RendererException("Metal command buffer allocation failed");
	// Establish a movable scope owner before detaching the pending batch.
	// No additional per-render heap allocation is needed for this owner.
	RenderCommands commands(cb, impl->submissions, available, std::move(canceledBatch));
	if (impl->frameTimings && impl->profilingSubmission < 900)
		commands.timingSubmission = ++impl->profilingSubmission;
	impl->textureUpdates = nil;
	return commands;
}
bool MetalContext::canReuseTextureUpload(id<MTLTexture> texture) const
{
	// Process uploads precede their render submission on this one queue.
	// Explicit Metal 4 queue barriers order transfers after submitted reads.
	// An open presentation may still contain unsubmitted reads; retained
	// display images and RTT/render targets must also keep their contents.
	return canReuseTextureWrites(texture) && texture.usage == MTLTextureUsageShaderRead;
}
bool MetalContext::canReuseTextureWrites(id<MTLTexture> texture) const
{
	// Older submitted readers precede writes on the one queue. An open
	// presentation contains unsubmitted reads, and retained display images
	// must keep their contents. Same-encoder readers are checked by the caller.
	return texture && !impl->frame && texture != impl->lastFrame
		&& texture.device == impl->device && texture.storageMode == MTLStorageModePrivate;
}
void MetalContext::sealTextureUpdates()
{
	if (impl->textureBlit) [impl->textureBlit endEncoding];
	impl->textureBlit = nil;
	auto& timing = impl->textureUploadTiming;
	if (timing.number != 0) {
		NOTICE_LOG(RENDERER, "Metal timing upload batch %llu slot wait: %.3f ms levels: %zu mip generations: %zu source bytes: %llu staging span: %llu allocations: %zu",
			(unsigned long long)timing.number, timing.slotWaitMs, timing.levels, timing.mipGenerations,
			(unsigned long long)timing.sourceBytes, (unsigned long long)timing.stagingBytes, timing.allocations);
		// A Process end and beginRenderCommands may both seal the same batch.
		// Publish its counters once; sealing never changes submission rules.
		timing.number = 0;
	}
	if (impl->textureUpdates) {
		auto& slot = impl->textureUploadSlots[impl->currentTextureUploadSlot];
		// Keep a bounded reserve when upload sizes fluctuate, instead of
		// freeing and reallocating every unused page on alternating batches.
		// Used pages are never trimmed; completion still controls CPU reuse.
		size_t retained = slot.pageOffset == 0 ? 0 : slot.pageCursor + 1;
		NSUInteger spareBytes = 4 * 1024 * 1024;
		while (retained < slot.pages.size()) {
			const NSUInteger capacity = slot.pages[retained].length;
			if (capacity > spareBytes) break;
			spareBytes -= capacity;
			++retained;
		}
		slot.pages.resize(retained);
	}
}
void MetalContext::finishTextureUpdates()
{
	sealTextureUpdates();
	if (impl->textureUpdates) submitCommandBuffer(impl->textureUpdates);
	impl->textureUpdates = nil;
}
FCMetal4ComputeEncoder * MetalContext::textureBlitEncoder()
{
	if (impl->textureBlit) return impl->textureBlit;
	// A compatible texture may still be read by an earlier deferred render.
	// Submit those reads before this new upload command buffer can overwrite it.
	flushRenderCommands();
	finishTextureUpdates();
	FCMetal4Commands * cb = [impl->queue commandBuffer];
	if (!cb) throw RendererException("Metal texture command buffer allocation failed");
	FCMetal4ComputeEncoder * blit = [cb transferEncoder];
	if (!blit) throw RendererException("Metal texture encoder allocation failed");
	const size_t index = impl->nextTextureUploadSlot % impl->textureUploadSlots.size();
	auto& slot = impl->textureUploadSlots[index];
	dispatch_semaphore_t available = slot.available;
	auto& timing = impl->textureUploadTiming;
	if (impl->frameTimings && impl->profilingTextureUpload < 900) {
		timing = {};
		timing.number = ++impl->profilingTextureUpload;
	}
	const auto waitStarted = timing.number != 0 ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
	// Only this upload batch owns its CPU-writable pages. GPU completion,
	// rather than the next Process call, permits their reuse.
	dispatch_semaphore_wait(available, DISPATCH_TIME_FOREVER);
	if (timing.number != 0)
		timing.slotWaitMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - waitStarted).count();
	slot.pageCursor = 0; slot.pageOffset = 0;
	[cb addCompletedHandler:^(FCMetal4Commands * done) {
		dispatch_semaphore_signal(available);
		if (done.status == FCMetal4StatusError) ERROR_LOG(RENDERER, "Metal texture upload: %s", done.error.localizedDescription.UTF8String);
	}];
	impl->currentTextureUploadSlot = index;
	++impl->nextTextureUploadSlot;
	impl->textureUpdates = cb;
	impl->textureBlit = blit;
	return blit;
}
void MetalContext::uploadTextureLevel(id<MTLTexture> texture, NSUInteger level,
	const void *data, NSUInteger bytesPerRow)
{
	if (!texture || !data || level >= texture.mipmapLevelCount || bytesPerRow == 0)
		throw RendererException("Invalid Metal texture upload");
	const NSUInteger width = std::max(NSUInteger(1), texture.width >> level);
	const NSUInteger height = std::max(NSUInteger(1), texture.height >> level);
	// Uncompressed buffer-to-texture blits require pixel-aligned rows, not
	// linear-texture row padding. Keep the decoder's packed 1/2/4-byte pixels
	// contiguous, including small mip levels, and align only each upload's offset.
	const NSUInteger row = bytesPerRow;
	const NSUInteger maxBytes = impl->device.maxBufferLength;
	if (row > maxBytes / height)
		throw RendererException("Metal texture upload exceeds the device buffer limit");
	const NSUInteger bytes = row * height;
	// Nest within Process, or submit this level on scope exit when called
	// independently. Allocation failures also release the batch through it.
	struct Updates {
		MetalContext *context;
		explicit Updates(MetalContext *context) : context(context) { context->beginTextureUpdates(); }
		~Updates() { context->endTextureUpdates(); }
	} updates(this);
	FCMetal4ComputeEncoder * blit = textureBlitEncoder();
	auto& slot = impl->textureUploadSlots[impl->currentTextureUploadSlot];
	size_t page = slot.pageCursor;
	if (slot.pageOffset > std::numeric_limits<NSUInteger>::max() - 255)
		throw RendererException("Metal texture staging offset overflow");
	NSUInteger offset = (slot.pageOffset + 255) & ~NSUInteger(255);
	if (offset != 0 && page < slot.pages.size() && (offset > slot.pages[page].length
			|| bytes > slot.pages[page].length - offset)) {
		++page; offset = 0;
	}
	if (page == slot.pages.size()) slot.pages.push_back(nil);
	auto& buffer = slot.pages[page];
	if (!buffer || buffer.length < bytes) {
		NSUInteger capacity = std::min(maxBytes, NSUInteger(1024 * 1024));
		while (capacity < bytes) capacity = capacity > maxBytes / 2 ? maxBytes : capacity * 2;
		id<MTLBuffer> allocated = [impl->device newBufferWithLength:capacity
			options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
		if (!allocated) throw RendererException("Metal texture staging allocation failed");
		buffer = allocated;
		if (impl->textureUploadTiming.number != 0) ++impl->textureUploadTiming.allocations;
	}
	u8 *destination = static_cast<u8 *>(buffer.contents) + offset;
	memcpy(destination, data, bytes);
	[blit copyFromBuffer:buffer sourceOffset:offset sourceBytesPerRow:row sourceBytesPerImage:0
		sourceSize:MTLSizeMake(width, height, 1) toTexture:texture destinationSlice:0
		destinationLevel:level destinationOrigin:MTLOriginMake(0, 0, 0)];
	auto& timing = impl->textureUploadTiming;
	if (timing.number != 0) {
		++timing.levels;
		timing.sourceBytes += bytesPerRow * height;
		timing.stagingBytes += bytes + (page == slot.pageCursor ? offset - slot.pageOffset : 0);
	}
	slot.pageCursor = page;
	slot.pageOffset = offset + bytes;
}
void MetalContext::generateMipmaps(id<MTLTexture> texture)
{
	// Uploads and mip generation share one blit pass per Process call and
	// reach the render queue in order, without a CPU wait for each texture.
	[textureBlitEncoder() generateMipmapsForTexture:texture];
	if (impl->textureUploadTiming.number != 0) ++impl->textureUploadTiming.mipGenerations;
	if (impl->textureUpdateDepth == 0) finishTextureUpdates();
}
std::string MetalContext::getDriverName() { return impl->device.name.UTF8String; }
bool MetalContext::isAMD() { return getDriverName().find("AMD") != std::string::npos; }
bool MetalContext::supportsPackedTextures() const { return impl->packedTextures; }
bool MetalContext::supportsMetalFX() const { return impl->metalFX && impl->metalFX->supported(); }
void MetalContext::resetUIDriver()
{
	// The old driver's destructor clears ImGui's global backend fields.
	// Run it before the new constructor publishes its name/capabilities.
	imguiDriver.reset();
	imguiDriver = std::make_unique<MetalDriver>();
}
void MetalContext::resize()
{
	sdl_update_display_metrics((SDL_Window *)window, SDL_WINDOW_METAL);
	const CGSize size = CGSizeMake(settings.display.width, settings.display.height);
	if (impl->layer.drawableSize.width != size.width || impl->layer.drawableSize.height != size.height)
		impl->layer.drawableSize = size;
}
void MetalContext::setSwapInterval(int interval) {
	if (interval > 0) impl->swapInterval = std::min(2, interval);
}

id<MTLTexture> MetalContext::makeTexture(int width, int height, MTLPixelFormat format, MTLTextureUsage usage, bool mipmapped, bool cpuVisible)
{
	if (width <= 0 || height <= 0) throw RendererException("Invalid Metal texture dimensions");
	MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:mipmapped];
	desc.usage = usage;
	desc.hazardTrackingMode = MTLHazardTrackingModeUntracked;
	desc.storageMode = cpuVisible ? MTLStorageModeShared : MTLStorageModePrivate;
	id<MTLTexture> texture = [impl->device newTextureWithDescriptor:desc];
	if (!texture) throw RendererException("Metal texture allocation failed");
	return texture;
}
static MTL4RenderPipelineDescriptor *pipelineDescriptor(MTL4FunctionDescriptor *vertex, MTL4FunctionDescriptor *fragment,
	MTLVertexDescriptor *layout, MTLPixelFormat color, MTL4RenderPipelineColorAttachmentDescriptor *blend)
{
	MTL4RenderPipelineDescriptor *desc = [[MTL4RenderPipelineDescriptor alloc] init];
	desc.vertexFunctionDescriptor = vertex;
	desc.fragmentFunctionDescriptor = fragment;
	desc.vertexDescriptor = layout;
	desc.rasterSampleCount = 1;
	desc.colorAttachments[0].pixelFormat = color;
	if (blend) {
		desc.colorAttachments[0] = blend;
		desc.colorAttachments[0].pixelFormat = color;
	}
	// Metal 4 does not bake depth/stencil attachment formats into the PSO.
	// The actual textures and depth/stencil state remain render-pass state.
	return desc;
}
MTL4FunctionDescriptor *MetalContext::shaderFunction(NSString *name)
{
	auto& function = impl->functions[name.UTF8String];
	if (!function) {
		function = [[MTL4LibraryFunctionDescriptor alloc] init];
		function.library = impl->library;
		function.name = name;
	}
	return function;
}
id<MTLRenderPipelineState> MetalContext::makePipeline(NSString *vertex, NSString *fragment, MTLVertexDescriptor *layout, MTLPixelFormat color, bool /*depth*/, MTL4RenderPipelineColorAttachmentDescriptor *blend)
{
	MTL4RenderPipelineDescriptor *desc = pipelineDescriptor(shaderFunction(vertex),
		shaderFunction(fragment), layout, color, blend);
	NSError *error = nil;
	id<MTLRenderPipelineState> pipeline = impl->pipelineCache ? impl->pipelineCache->makeRenderPipeline(desc, &error)
		: [impl->compiler newRenderPipelineStateWithDescriptor:desc compilerTaskOptions:nil error:&error];
	checkMetal(error, "Metal render pipeline");
	if (!pipeline) throw RendererException("Metal pipeline creation failed");
	return pipeline;
}
id<MTLComputePipelineState> MetalContext::makeComputePipeline(NSString *function)
{
	NSError *error = nil;
	MTL4ComputePipelineDescriptor *desc = [[MTL4ComputePipelineDescriptor alloc] init];
	desc.computeFunctionDescriptor = shaderFunction(function);
	id<MTLComputePipelineState> pipeline = impl->pipelineCache ? impl->pipelineCache->makeComputePipeline(desc, &error)
		: [impl->compiler newComputePipelineStateWithDescriptor:desc compilerTaskOptions:nil error:&error];
	checkMetal(error, "Metal compute pipeline");
	if (!pipeline) throw RendererException("Metal compute pipeline creation failed");
	return pipeline;
}
void MetalContext::makePipelineAsync(NSString *vertex, NSString *fragment, MTLVertexDescriptor *layout,
	MTLPixelFormat color, bool /*depth*/, MTL4RenderPipelineColorAttachmentDescriptor *blend, MaterialConstants constants,
	std::function<void(id<MTLRenderPipelineState>, NSError *)> completion, std::function<bool()> wanted)
{
	// Capture owned inputs, never the context or renderer. A backend switch
	// may destroy them while Metal's compiler is still completing a variant.
	id<MTL4Compiler> compiler = impl->materialCompiler;
	id<MTLLibrary> library = impl->library;
	const auto functions = impl->materialFunctions;
	const auto pipelineCache = impl->pipelineCache;
	NSString *vertexName = [vertex copy], *fragmentName = [fragment copy];
	MTLVertexDescriptor *vertexLayout = [layout copy];
	MTL4RenderPipelineColorAttachmentDescriptor *colorBlend = [blend copy];
	dispatch_async(impl->shaderQueue, ^{
		@autoreleasepool {
			try {
				if (!wanted()) return;
				NSError *error = nil;
				MaterialConstants vertexConstants = constants;
				// Canonicalize vertex controls and the selected interpolation
				// layout without retaining unrelated fragment shading keys.
				if ([vertexName isEqualToString:@"pvr_material_vertex"]
						|| [vertexName isEqualToString:@"pvr_dc_material_vertex"]
						|| [vertexName isEqualToString:@"pvr_dc_bounded_vertex"]) {
					vertexConstants.flags = metalVertexFunctionFlags(constants.flags, constants.tsp0, constants.tsp1);
					vertexConstants.tsp0 = vertexConstants.tsp1 = 0;
				}
				MTL4FunctionDescriptor *v = functions->get(library, vertexName, vertexConstants);
				if (!wanted()) return;
				MaterialConstants fragmentConstants = constants;
				if ([fragmentName isEqualToString:@"pvr_material_fragment"]
						|| [fragmentName isEqualToString:@"pvr_material_depth_fragment"]
						|| [fragmentName isEqualToString:@"pvr_bounded_fragment"]
						|| [fragmentName isEqualToString:@"pvr_bounded_depth_fragment"]
						|| [fragmentName isEqualToString:@"oit_material_fragment"])
					fragmentConstants.flags &= MetalFragmentMaterialFlags;
				MTL4FunctionDescriptor *f = functions->get(library, fragmentName, fragmentConstants);
				if (!wanted()) return;
				MTL4RenderPipelineDescriptor *desc = pipelineDescriptor(v, f, vertexLayout, color, colorBlend);
				id<MTLRenderPipelineState> state = pipelineCache ? pipelineCache->makeRenderPipeline(desc, &error, true)
					: [compiler newRenderPipelineStateWithDescriptor:desc compilerTaskOptions:nil error:&error];
				completion(state, error);
			} catch (const std::exception& failure) {
				// A cache allocation failure must preserve the generic shader,
				// rather than escape a GCD block and terminate the app.
				if (!wanted()) return;
				NSString *message = [NSString stringWithUTF8String:failure.what()];
				NSError *error = [NSError errorWithDomain:@"org.flycast.metal" code:1
					userInfo:@{NSLocalizedDescriptionKey: message ?: @"Metal material creation failed"}];
				completion(nil, error);
			}
		}
	});
}
FCMetal4Commands * MetalContext::beginFrame()
{
	flushRenderCommands();
	finishTextureUpdates(); // Presentation may follow a Process without Render.
	if (!impl->frame) {
		impl->currentSlot = impl->nextSlot++ % impl->slots.size();
		dispatch_semaphore_t flight = impl->slots[impl->currentSlot].available;
		dispatch_semaphore_wait(flight, DISPATCH_TIME_FOREVER);
		impl->slots[impl->currentSlot].uiBufferCursor = 0;
		impl->drawableRequested = false;
		try {
			impl->frame = [impl->queue commandBuffer];
		} catch (...) {
			dispatch_semaphore_signal(flight);
			throw;
		}
		if (!impl->frame) {
			dispatch_semaphore_signal(flight);
			throw RendererException("Metal presentation command buffer allocation failed");
		}
		[impl->frame addCompletedHandler:^(FCMetal4Commands * cb) {
			if (cb.status == FCMetal4StatusError) ERROR_LOG(RENDERER, "Metal presentation: %s", cb.error.localizedDescription.UTF8String);
			dispatch_semaphore_signal(flight);
		}];
	}
	return impl->frame;
}
std::pair<id<MTLBuffer>, id<MTLBuffer>> MetalContext::uploadUIBuffers(const ImDrawData& data)
{
	if (data.TotalVtxCount < 0 || data.TotalIdxCount < 0)
		throw RendererException("Invalid Metal UI geometry counts");
	const size_t vertexBytes = size_t(data.TotalVtxCount) * sizeof(ImDrawVert);
	const size_t indexBytes = size_t(data.TotalIdxCount) * sizeof(ImDrawIdx);
	if (vertexBytes == 0 && indexBytes == 0) return {nil, nil};
	beginFrame(); // Acquires the presentation slot before any CPU write.
	auto& slot = impl->slots[impl->currentSlot];
	// A second UI draw in the same presentation must get distinct storage,
	// so its CPU upload cannot change a draw already encoded in this frame.
	const size_t draw = slot.uiBufferCursor++;
	if (slot.uiBuffers.size() <= draw) slot.uiBuffers.resize(draw + 1);
	auto reserve = [&](size_t size, id<MTLBuffer> __strong& buffer) {
		if (size == 0) return;
		if (!buffer || buffer.length < size) {
			const NSUInteger maximum = impl->device.maxBufferLength;
			if (size > maximum) throw RendererException("Metal UI geometry exceeds the device buffer limit");
			NSUInteger capacity = std::min<NSUInteger>(4096, maximum);
			while (capacity < size) capacity = capacity <= maximum / 2 ? capacity * 2 : maximum;
			buffer = [impl->device newBufferWithLength:capacity options:MTLResourceStorageModeShared];
			if (!buffer) throw RendererException("Metal UI buffer allocation failed");
		}
	};
	reserve(vertexBytes, slot.uiBuffers[draw][0]);
	reserve(indexBytes, slot.uiBuffers[draw][1]);
	id<MTLBuffer> vertices = vertexBytes ? slot.uiBuffers[draw][0] : nil;
	id<MTLBuffer> indices = indexBytes ? slot.uiBuffers[draw][1] : nil;
	size_t vertexOffset = 0, indexOffset = 0;
	// Pack directly into completion-protected Shared storage, without a CPU staging vector.
	for (const ImDrawList *list : data.CmdLists) {
		const size_t listVertexBytes = size_t(list->VtxBuffer.Size) * sizeof(ImDrawVert);
		const size_t listIndexBytes = size_t(list->IdxBuffer.Size) * sizeof(ImDrawIdx);
		if (listVertexBytes)
			memcpy(static_cast<u8 *>(vertices.contents) + vertexOffset, list->VtxBuffer.Data, listVertexBytes);
		if (listIndexBytes)
			memcpy(static_cast<u8 *>(indices.contents) + indexOffset, list->IdxBuffer.Data, listIndexBytes);
		vertexOffset += listVertexBytes;
		indexOffset += listIndexBytes;
	}
	return {vertices, indices};
}
void MetalContext::setLastFrame(id<MTLTexture> texture, float aspectRatio, double videoTime)
{
	if (texture) ++impl->contentFrame;
	impl->lastFrame = texture; impl->aspectRatio = aspectRatio;
	impl->sourceVideoTime = videoTime;
}
id<MTLTexture> MetalContext::lastFrame() const { return impl->lastFrame; }
float MetalContext::lastFrameAspectRatio() const { return impl->aspectRatio; }

id<MTLRenderPipelineState> MetalContext::quadPipeline(bool ignoreAlpha)
{
	const u64 key = ignoreAlpha;
	auto& pipeline = impl->quads[key];
	if (!pipeline) {
		MTL4RenderPipelineColorAttachmentDescriptor *blend = [[MTL4RenderPipelineColorAttachmentDescriptor alloc] init];
		blend.blendingState = ignoreAlpha ? MTL4BlendStateDisabled : MTL4BlendStateEnabled;
		blend.sourceRGBBlendFactor = blend.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
		blend.destinationRGBBlendFactor = blend.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
		pipeline = makePipeline(@"quad_vertex", @"quad_fragment", nil, MTLPixelFormatBGRA8Unorm, false, blend);
	}
	return pipeline;
}
void MetalContext::drawQuad(FCMetal4RenderEncoder * encoder, id<MTLTexture> texture, MTLViewport viewport, bool rotate, bool ignoreAlpha, const float *color, bool linear)
{
	struct alignas(16) Uniform { float color[4]; u32 rotate, ignoreAlpha; } u{{1, 1, 1, 1}, (u32)rotate, (u32)ignoreAlpha};
	if (color) memcpy(u.color, color, sizeof(u.color));
	[encoder setRenderPipelineState:quadPipeline(ignoreAlpha)];
	[encoder setCullMode:MTLCullModeNone];
	[encoder setViewport:viewport];
	[encoder setVertexBytes:&u length:sizeof(u) atIndex:0];
	[encoder setFragmentBytes:&u length:sizeof(u) atIndex:0];
	[encoder setFragmentTexture:texture atIndex:0];
	[encoder setFragmentSamplerState:linear ? impl->linear : impl->nearest atIndex:0];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
}
std::pair<int, int> MetalContext::presentationSize() const
{
	id<MTLTexture> target = impl->composition ? impl->composition->texture : impl->drawable.texture;
	return {int(target.width), int(target.height)};
}
FCMetal4RenderEncoder * MetalContext::beginPresentation()
{
	@autoreleasepool {
		if (impl->encoder) return impl->encoder;
		// SDL's size/DPI events update display metrics and call resize(). Writing
		// layer geometry on every frame needlessly changes presentation state.
		beginFrame();
		// A failed acquisition belongs to this command buffer. Overlay, GUI
		// and final-present calls must not each wait for the same missing image.
		if (impl->drawableRequested) return nil;
		const bool sync = config::VSync && !settings.input.fastForwardMode;
		const bool gui = gui_is_open(), fast = settings.input.fastForwardMode;
		if (gui != impl->presentationGui || sync != impl->presentationSync || fast != impl->presentationFast) {
			++impl->presentationPhase;
			impl->displayLink.reset();
			impl->presentationTimeline.reset();
		}
		impl->presentationGui = gui; impl->presentationSync = sync; impl->presentationFast = fast;
		if (impl->layer.displaySyncEnabled != sync) impl->layer.displaySyncEnabled = sync;
#ifdef VIDEO_ROUTING
		if (impl->layer.framebufferOnly == config::VideoRouting) impl->layer.framebufferOnly = !config::VideoRouting;
#endif
		const bool paced = impl->lastFrame && !gui && sync && config::FramePacing;
		if (paced) {
			if (impl->displayRefresh != settings.display.refreshRate) {
				impl->displayLink.reset(); impl->presentationTimeline.reset();
				impl->displayRefresh = settings.display.refreshRate;
			}
			if (!impl->displayLink) impl->displayLink = std::make_unique<MetalDisplayLink>(impl->layer, impl->queue,
				quadPipeline(true), impl->nearest, impl->submissions, impl->presentationTimings, settings.display.refreshRate);
			impl->displayLink->setActive(true, true);
		} else impl->displayLink.reset();
		impl->drawableRequested = true;
		if (paced) {
			// Never wait for a display copy on the emulation/render thread.
			// Slots remain leased through composition and every subsequent GPU read.
			for (size_t attempt = 0; attempt < impl->compositions.size(); ++attempt) {
				auto& slot = impl->compositions[impl->nextComposition++ % impl->compositions.size()];
				if (dispatch_semaphore_wait(slot.available, DISPATCH_TIME_NOW) != 0) continue;
				dispatch_semaphore_t available = slot.available;
				impl->composition = std::shared_ptr<MetalDisplayImage>(new MetalDisplayImage,
					[available](MetalDisplayImage *image) { delete image; dispatch_semaphore_signal(available); });
				const NSUInteger width = std::max<NSUInteger>(1, impl->layer.drawableSize.width);
				const NSUInteger height = std::max<NSUInteger>(1, impl->layer.drawableSize.height);
				if (!slot.texture || slot.texture.width != width || slot.texture.height != height)
					slot.texture = makeTexture(width, height, MTLPixelFormatBGRA8Unorm,
						MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead);
				if (!slot.readyEvent) slot.readyEvent = [impl->device newSharedEvent];
				if (!slot.readyEvent) throw RendererException("Cannot create Metal display event");
				impl->composition->readyEvent = slot.readyEvent;
				impl->composition->readyValue = ++slot.readyValue;
				impl->composition->texture = slot.texture;
				impl->composition->content = impl->contentFrame;
				break;
			}
			if (!impl->composition) return nil;
		} else {
			@try { impl->drawable = [impl->layer nextDrawable]; }
			@catch (NSException *exception) {
				throw RendererException(std::string("Metal drawable: ") + exception.reason.UTF8String);
			}
			if (!impl->drawable) return nil;
			[impl->frame useDrawable:impl->drawable];
		}
		const auto [width, height] = presentationSize();
		id<MTLTexture> image = impl->lastFrame;
		MTLViewport viewport{};
		MTLScissorRect imageScissor{};
		bool drawImage = false;
		bool usingMetalFX = false;
		const bool rotate = config::Rotate90;
		if (impl->lastFrame && width > 0 && height > 0
				&& std::isfinite(impl->aspectRatio) && impl->aspectRatio > 0) {
			int dx = 0, dy = 0;
			// The acquired surface can precede SDL's next size/DPI update.
			getWindowboxDimensions(width, height, impl->aspectRatio, dx, dy, rotate);
			dx = std::clamp(dx, 0, width / 2);
			dy = std::clamp(dy, 0, height / 2);
			const double viewWidth = width - 2 * dx, viewHeight = height - 2 * dy;
			if (viewWidth > 0 && viewHeight > 0) {
				float shiftX, shiftY;
				getVideoShift(shiftX, shiftY);
				if (rotate) {
					// Vulkan rotates the shifted clip-space vertices: x <- y, y <- -x.
					const float rotatedX = shiftY * viewWidth / impl->lastFrame.height;
					shiftY = -shiftX * viewHeight / impl->lastFrame.width;
					shiftX = rotatedX;
				} else {
					shiftX *= viewWidth / impl->lastFrame.width;
					shiftY *= viewHeight / impl->lastFrame.height;
				}
				viewport = MTLViewport{dx + shiftX, dy + shiftY, viewWidth, viewHeight, 0, 1};
				imageScissor = MTLScissorRect{(NSUInteger)dx, (NSUInteger)dy,
					(NSUInteger)viewWidth, (NSUInteger)viewHeight};
				drawImage = true;
			}
		}
		// MetalFX encodes its own passes, so it must run before the drawable
		// render encoder opens. Upscale only the unrotated game image; the
		// presentation quad retains aspect, shift, rotation and border rules.
		if (drawImage && config::MetalFXSpatial && !config::IntegerScale) {
			const NSUInteger outputWidth = rotate ? imageScissor.height : imageScissor.width;
			const NSUInteger outputHeight = rotate ? imageScissor.width : imageScissor.height;
			if (@available(macOS 26.0, *)) {
				id<MTLTexture> scaled = impl->metalFX->upscale(image, impl->frame,
					outputWidth, outputHeight, impl->currentSlot, impl->contentFrame);
				if (scaled) { image = scaled; usingMetalFX = true; }
			}
		} else impl->metalFX->releaseSlot(impl->currentSlot);
		MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
		pass.colorAttachments[0].texture = impl->composition ? impl->composition->texture : impl->drawable.texture;
		pass.colorAttachments[0].loadAction = MTLLoadActionClear;
		pass.colorAttachments[0].storeAction = MTLStoreActionStore;
		pass.colorAttachments[0].clearColor = MTLClearColorMake(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1);
		impl->encoder = [impl->frame renderCommandEncoderWithDescriptor:pass];
		if (!impl->encoder) throw RendererException("Metal presentation encoder allocation failed");
		if (drawImage) {
			// A video shift must not draw into the windowbox's cleared border.
			[impl->encoder setScissorRect:imageScissor];
			drawQuad(impl->encoder, image, viewport, rotate, true, nullptr, usingMetalFX || config::LinearInterpolation);
			// VMUs, crosshairs and UI use the full presentation surface.
			[impl->encoder setScissorRect:MTLScissorRect{0, 0, (NSUInteger)width, (NSUInteger)height}];
		}
		return impl->encoder;
	}
}
void MetalContext::drawOverlays()
{
	FCMetal4RenderEncoder * encoder = beginPresentation();
	if (!encoder) return;
	drawOverlays(encoder);
}
void MetalContext::drawOverlays(FCMetal4RenderEncoder * encoder)
{
	const auto [width, height] = presentationSize();
	if (width <= 0 || height <= 0) return;
	auto& slot = impl->slots[impl->currentSlot];
	const float scaling = settings.display.uiScale;
	if (settings.platform.isConsole() && config::FloatVMUs) {
		float padding = 8 * scaling, w = 96 * scaling, h = 64 * scaling;
		const float color[4] = {.75f, .75f, .75f, .75f};
		for (size_t i = 0; i < 8; ++i) {
			u32 pixels[48 * 32];
			u64 generation = slot.vmu[i] ? slot.vmuChanged[i] : ~0ull;
			if (!getVmuScreenSnapshot(i, generation, pixels)) { slot.vmu[i] = nil; continue; }
			if (!slot.vmu[i] || generation != slot.vmuChanged[i]) {
				// This frame owns its slot until GPU completion: CPU writes cannot
				// change a VMU sampled by an earlier queued presentation.
				if (!slot.vmu[i]) slot.vmu[i] = makeTexture(48, 32, MTLPixelFormatRGBA8Unorm, MTLTextureUsageShaderRead, false, true);
				// Decoded LCD rows are bottom-up, as used by GL/Vulkan and the
				// inverted UVs in the VMU menu. Metal texture row zero is the top.
				for (int y = 0; y < 16; ++y)
					std::swap_ranges(pixels + y * 48, pixels + (y + 1) * 48, pixels + (31 - y) * 48);
				[slot.vmu[i] replaceRegion:MTLRegionMake2D(0, 0, 48, 32) mipmapLevel:0 withBytes:pixels bytesPerRow:48 * 4];
				slot.vmuChanged[i] = generation;
			}
			float x = i & 2 ? width - padding - w : padding;
			float y = i & 4 ? height - padding - h - (i & 1 ? padding + h : 0) : padding + (i & 1 ? padding + h : 0);
			drawQuad(encoder, slot.vmu[i], MTLViewport{x, y, w, h, 0, 1}, false, false, color, false);
		}
	}
	struct alignas(16) OverlayQuad { float rectangle[4], color[4]; } guns[4];
	NSUInteger gunCount = 0;
	for (int i = 0; i < 4; ++i) {
		if (!crosshairNeeded(i)) continue;
		auto [x, y] = getCrosshairPosition(i);
		float w = config::CrosshairSize * scaling;
		if (w <= 0) continue;
		u32 c = config::CrosshairColor[i];
		guns[gunCount++] = {{x - w / 2, y - w / 2, w, w},
			{(c & 255) / 255.f, ((c >> 8) & 255) / 255.f, ((c >> 16) & 255) / 255.f, (c >> 24) / 255.f}};
	}
	if (gunCount) {
		if (!impl->crosshair) {
			impl->crosshair = makeTexture(16, 16, MTLPixelFormatRGBA8Unorm, MTLTextureUsageShaderRead, false, true);
			[impl->crosshair replaceRegion:MTLRegionMake2D(0, 0, 16, 16) mipmapLevel:0 withBytes:getCrosshairTextureData() bytesPerRow:16 * 4];
		}
		if (!impl->crosshairPipeline) {
			MTL4RenderPipelineColorAttachmentDescriptor *blend = [[MTL4RenderPipelineColorAttachmentDescriptor alloc] init];
			blend.blendingState = MTL4BlendStateEnabled;
			blend.sourceRGBBlendFactor = blend.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
			blend.destinationRGBBlendFactor = blend.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
			impl->crosshairPipeline = makePipeline(@"overlay_vertex", @"overlay_fragment", nil, MTLPixelFormatBGRA8Unorm, false, blend);
		}
		const float screen[2] = {(float)width, (float)height};
		[encoder setRenderPipelineState:impl->crosshairPipeline]; [encoder setCullMode:MTLCullModeNone];
		[encoder setViewport:MTLViewport{0, 0, screen[0], screen[1], 0, 1}];
		[encoder setVertexBytes:guns length:gunCount * sizeof(OverlayQuad) atIndex:0];
		[encoder setVertexBytes:screen length:sizeof(screen) atIndex:1];
		[encoder setFragmentTexture:impl->crosshair atIndex:0]; [encoder setFragmentSamplerState:impl->nearest atIndex:0];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4 instanceCount:gunCount];
	}
}
#ifdef VIDEO_ROUTING
void MetalContext::publishVideoRouting(id<MTLTexture> texture, FCMetal4Commands * command)
{
	// Keep the discoverable server alive, but avoid scaling/copying an
	// output nobody consumes. Cache scaled textures per GPU-owned slot.
	if (!os_VideoRoutingMetalHasClients(impl->device)) return;
	if (config::VideoRoutingScale) {
		const int h = config::VideoRoutingVRes;
		if (h <= 0 || texture.height == 0 || texture.width > std::numeric_limits<NSUInteger>::max() / NSUInteger(h))
			throw RendererException("Invalid Metal video routing dimensions");
		const NSUInteger scaledWidth = texture.width * NSUInteger(h) / texture.height;
		if (scaledWidth == 0 || scaledWidth > NSUInteger(std::numeric_limits<int>::max()))
			throw RendererException("Invalid Metal video routing dimensions");
		const int w = int(scaledWidth);
		auto& scaled = impl->slots[impl->currentSlot].routed;
		if (!scaled || scaled.width != w || scaled.height != h)
			scaled = makeTexture(w, h, MTLPixelFormatBGRA8Unorm);
		MTL4RenderPassDescriptor *p = [MTL4RenderPassDescriptor new];
		p.colorAttachments[0].texture = scaled; p.colorAttachments[0].loadAction = MTLLoadActionDontCare; p.colorAttachments[0].storeAction = MTLStoreActionStore;
		FCMetal4RenderEncoder * e = [command renderCommandEncoderWithDescriptor:p];
		if (!e) throw RendererException("Metal video routing encoder allocation failed");
		drawQuad(e, texture, MTLViewport{0, 0, (double)w, (double)h, 0, 1}); [e endEncoding]; texture = scaled;
	}
	os_VideoRoutingPublishMetalTexture(texture, command);
}
#endif
void MetalContext::present()
{
	@autoreleasepool {
		if (impl->composition) {
			if (impl->encoder) { [impl->encoder endEncoding]; impl->encoder = nil; }
#ifdef VIDEO_ROUTING
			if (config::VideoRouting) publishVideoRouting(impl->composition->texture, impl->frame);
			else os_VideoRoutingTermMetal();
#endif
			const double refresh = std::isfinite(settings.display.refreshRate) && settings.display.refreshRate > 0
				? settings.display.refreshRate : 60;
			impl->composition->target = impl->presentationTimeline.videoFrame(CACurrentMediaTime(),
				impl->sourceVideoTime, 2 * spg_GetFrameDuration() + 1 / refresh, impl->displayLink->schedulingLead());
			if (impl->presentationTimings && impl->contentFrame <= 3600)
				NOTICE_LOG(RENDERER, "Metal video %llu: source %.6f target %.6f submitted %.6f",
					(unsigned long long)impl->contentFrame, impl->sourceVideoTime,
					impl->composition->target, CACurrentMediaTime());
			impl->displayLink->prepare(impl->composition, impl->frame);
			submitCommandBuffer(impl->frame);
			impl->displayLink->enqueue(impl->composition);
			impl->composition.reset(); impl->frame = nil; impl->drawableRequested = false;
			return;
		}
		if (impl->displayLink && impl->displayLink->active()) {
			if (impl->frame) submitCommandBuffer(impl->frame);
			impl->frame = nil; impl->drawableRequested = false;
			return;
		}
		const int copies = impl->lastFrame ? metal::presentationCount(settings.display.refreshRate, impl->swapInterval,
			gui_is_open(), config::VSync, settings.input.fastForwardMode, config::DupeFrames) : 1;
		for (int copy = 0; copy < copies; ++copy) {
			@autoreleasepool {
				if (copy != 0) {
					// Redraw the complete retained image, including VMUs, guns and
					// the current OSD. Each copy owns a separate GPU resource slot.
					drawOverlays();
					if (ImDrawData *data = ImGui::GetDrawData()) imguiDriver->renderDrawData(data, false);
				}
				if (!impl->encoder) beginPresentation();
				if (impl->encoder) {
					[impl->encoder endEncoding];
					impl->encoder = nil;
				}
#ifdef VIDEO_ROUTING
				if (config::VideoRouting) {
					if (impl->drawable) publishVideoRouting(impl->drawable.texture, impl->frame);
				} else os_VideoRoutingTermMetal();
#endif
				const bool hasDrawable = impl->drawable != nil;
				if (impl->drawable) {
					if (@available(macOS 26.0, *)) {
#ifdef METAL_RENDERER_TESTS
						if (impl->testPresentationObserver) impl->testPresentationObserver(impl->drawable);
#endif
						if (impl->presentationTimings && impl->lastFrame && !impl->presentationGui && impl->profilingFrame < 1800) {
							const u64 frameNumber = ++impl->profilingFrame;
							const int interval = impl->swapInterval;
							const u64 content = impl->contentFrame;
							const u64 phase = impl->presentationPhase;
							const bool sync = impl->presentationSync, fast = impl->presentationFast;
							[impl->drawable addPresentedHandler:^(id<MTLDrawable> drawable) {
								NOTICE_LOG(RENDERER, "Metal timing presented %llu: %.6f s interval %d content %llu phase %llu vsync %d fast %d", (unsigned long long)frameNumber,
									drawable.presentedTime, interval, (unsigned long long)content, (unsigned long long)phase, sync, fast);
							}];
						}
						{
							// Only explicit duplicate copies need a duration rule.
							// Normal VSync presents follow the emulated frame cadence.
							const double duration = metal::minimumFrameDuration(settings.display.refreshRate, impl->swapInterval,
								impl->presentationGui, impl->presentationSync, impl->presentationFast, copies > 1);
							if (duration > 0)
								[impl->frame presentDrawable:impl->drawable afterMinimumDuration:duration];
							else [impl->frame presentDrawable:impl->drawable];
						}
					}
				}
				if (impl->frame) submitCommandBuffer(impl->frame);
				impl->encoder = nil; impl->drawable = nil; impl->frame = nil;
				impl->drawableRequested = false;
				// Submit even an empty frame so its slot is released. A missing
				// drawable also cancels duplicate copies until the next frame.
				if (!hasDrawable) break;
			}
		}
#ifdef METAL_RENDERER_TESTS
		if (@available(macOS 26.0, *))
			if (impl->vsyncTestWarmup > 0 && --impl->vsyncTestWarmup == 0)
				validateSyntheticVSync();
#endif
#ifdef TEST_AUTOMATION
		if (impl->lastFrame) {
			extern bool do_screenshot;
			static FILE *video = fopen(config::loadStr("record", "rawvid").c_str(), "wb");
			if (video == nullptr && !do_screenshot)
				return;
			std::vector<u8> rgba;
			readTexture(impl->lastFrame, rgba);
			if (video) {
				std::vector<u8> rgb(rgba.size() / 4 * 3);
				for (size_t i = 0; i < rgba.size() / 4; ++i)
					std::copy_n(rgba.data() + i * 4, 3, rgb.data() + i * 3);
				fwrite(rgb.data(), 1, rgb.size(), video); fflush(video);
			}
			if (do_screenshot) {
				dump_screenshot(rgba.data(), impl->lastFrame.width, impl->lastFrame.height, true);
				dc_exit(); flycast_term(); exit(0);
			}
		}
#endif
	}
}
void MetalContext::waitIdle()
{
	// Stop producing display submissions before draining either native queue.
	// This also retires retained compositions on renderer/backend changes.
	impl->displayLink.reset();
	impl->presentationTimeline.reset();
	flushRenderCommands();
	finishTextureUpdates();
	if (impl->frame) {
		if (impl->encoder) [impl->encoder endEncoding];
		submitCommandBuffer(impl->frame);
		impl->encoder = nil; impl->frame = nil; impl->drawable = nil;
		impl->drawableRequested = false;
	}
	// Drain the actual submissions without allocating an extra empty buffer.
	// This also waits when a new command-buffer allocation would have failed.
	dispatch_group_wait(impl->submissions, DISPATCH_TIME_FOREVER);
	for (auto& slot : impl->textureUploadSlots) {
		slot.pages.clear(); slot.pageCursor = 0; slot.pageOffset = 0;
	}
}
std::unique_ptr<MetalContext::TextureReadback> MetalContext::encodeReadTexture(id<MTLTexture> texture, FCMetal4Commands * command)
{
	// RTT-to-VRAM and full framebuffer emulation can read back every frame.
	// The buffer remains owned until GPU completion and CPU consumption finish.
	if (!texture || !command) throw RendererException("Invalid Metal readback resources");
	const NSUInteger width = texture.width, height = texture.height;
	if (!width || !height || width > (std::numeric_limits<NSUInteger>::max() - 255) / 4)
		throw RendererException("Invalid Metal readback dimensions");
	const NSUInteger row = (width * 4 + 255) & ~NSUInteger(255);
	const NSUInteger maximum = impl->device.maxBufferLength;
	if (row > maximum / height)
		throw RendererException("Metal readback exceeds the device buffer limit");
	const NSUInteger bytes = row * height;
	auto readback = std::unique_ptr<TextureReadback>(new TextureReadback(impl->readbackMutex));
	if (!impl->readbackBuffer || impl->readbackBuffer.length < bytes) {
		// Retain capacity across modest RTT/window-size changes, just like
		// the geometry pool. The lock also covers completion and CPU consumption.
		NSUInteger capacity = std::min<NSUInteger>(4096, maximum);
		while (capacity < bytes) capacity = capacity <= maximum / 2 ? capacity * 2 : maximum;
		id<MTLBuffer> allocated = [impl->device newBufferWithLength:capacity options:MTLResourceStorageModeShared];
		if (!allocated) throw RendererException("Metal readback allocation failed");
		impl->readbackBuffer = allocated;
	}
	id<MTLBuffer> buffer = impl->readbackBuffer;
	FCMetal4ComputeEncoder * blit = [command transferEncoder];
	if (!blit) throw RendererException("Metal readback encoder allocation failed");
	[blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
		sourceSize:MTLSizeMake(width, height, 1) toBuffer:buffer destinationOffset:0 destinationBytesPerRow:row destinationBytesPerImage:bytes];
	[blit endEncoding];
	readback->command = command; readback->buffer = buffer;
	readback->width = width; readback->height = height; readback->row = row;
	return readback;
}
void MetalContext::TextureReadback::finish(std::vector<u8>& pixels)
{
	completePixels(pixels, false);
}
const u8 *MetalContext::TextureReadback::finishForConversion(std::vector<u8>& scratch)
{
	return completePixels(scratch, true);
}
const u8 *MetalContext::TextureReadback::completePixels(std::vector<u8>& pixels, bool allowMapped)
{
	[command waitUntilCompleted];
	checkMetal(command.error, "Metal readback");
	const NSUInteger packedRow = width * 4;
	const NSUInteger packedBytes = packedRow * height; // Bounded by the checked padded allocation.
	const u8 *mapped = (const u8 *)buffer.contents;
	// VRAM writers consume packed RGBA synchronously. With no row padding,
	// the locked shared buffer already has that layout; no CPU copy is needed.
	if (allowMapped && row == packedRow) return mapped;
	pixels.resize(packedBytes);
	if (row == packedRow) {
		memcpy(pixels.data(), mapped, packedBytes);
		return pixels.data();
	}
	for (NSUInteger y = 0; y < height; ++y)
		memcpy(pixels.data() + y * packedRow, mapped + y * row, packedRow);
	return pixels.data();
}
void MetalContext::readTexture(id<MTLTexture> texture, std::vector<u8>& pixels)
{
	flushRenderCommands();
	finishTextureUpdates();
	FCMetal4Commands * command = [impl->queue commandBuffer];
	if (!command) throw RendererException("Metal readback command buffer allocation failed");
	auto readback = encodeReadTexture(texture, command);
	submitCommandBuffer(command);
	readback->finish(pixels);
}

#ifdef METAL_RENDERER_TESTS
#include "../../../tests/metal/metal_vsync_validation.inc"
#include "../../../tests/metal/metal_overlay_validation.inc"
#ifdef VIDEO_ROUTING
#include "../../../tests/metal/metal_syphon_validation.inc"
#endif
#endif
