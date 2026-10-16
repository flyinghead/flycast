/*
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
// Metal OIT drawer. Port of core/rend/vulkan/oit/oit_drawer.h.
//
// Class hierarchy:
//   MetalBaseDrawer
//     └── MetalOITDrawer
//           ├── MetalOITScreenDrawer
//           └── MetalOITTextureDrawer
#pragma once
#include "../metal_apple.h"

#include "types.h"
#include "../metal.h"
#include "../metal_drawer.h"
#include "../metal_quad.h"
#include "../metal_texture.h"
#include "metal_oit_buffer.h"
#include "metal_oit_pipeline.h"

#include <array>
#include <functional>
#include <memory>

class MetalShaders;

class MetalOITDrawer : public MetalBaseDrawer
{
public:
	virtual ~MetalOITDrawer() = default;

	/// Top-level frame draw entry. Mirrors Vulkan OITDrawer::Draw().
	/// Will issue: opaque pass, OIT collect pass (translucent), OIT
	/// resolve pass, present.
	bool Draw(const MetalTexture *fogTexture, const MetalTexture *paletteTexture);

	/// Frame lifecycle. NewFrame returns the active command buffer for
	/// the current frame so screen/texture drawers can layer their own
	/// pre/post work. EndFrame closes the buffer and commits.
	virtual id<MTLCommandBuffer> NewFrame() = 0;
	virtual void EndFrame() = 0;

protected:
	void Init(MetalOITPipelineManager *pipelineManager, MetalOITBuffers *oitBuffers,
			  MetalSamplers *samplers = nullptr,
			  MetalPipelineManager *regularPipelineManager = nullptr)
	{
		this->pipelineManager        = pipelineManager;
		this->oitBuffers             = oitBuffers;
		this->samplers               = samplers;
		this->regularPipelineManager = regularPipelineManager;
		this->tileDepth = pipelineManager != nullptr && pipelineManager->UseTileDepth();
	}

	void Term()
	{
		for (auto &tex : colorAttachments) tex = nil;
		for (auto &tex : depthAttachments) tex = nil;
		for (auto &fb  : tempPassDescriptors) fb = nil;
		resolveBackground = nil;
		opaqueDepthCopy = nil;
		opaqueDepthTile = nil;
		resolveDepthStencil = nil;
		maxWidth  = 0;
		maxHeight = 0;
	}

	/// Allocate / resize the ping-pong color + depth attachments to
	/// (width,height). Re-creates only if the requested size exceeds
	/// the cached max dimensions.
	void MakeBuffers(int width, int height);

	/// Per-render-pass framebuffer (Vulkan OITScreenDrawer toggles ping-pong
	/// each pass). Legacy 3-encoder path used getRenderPassDescriptor(0..2).
	virtual MTLRenderPassDescriptor *getOitRenderPassDescriptor(int renderPassIndex,
																  int renderPassCount,
																  bool initialPass) = 0;

	void toggleFramebufferIndex() { framebufferIndex = (framebufferIndex + 1) & 1; }
	void resetOitPixelCounter();
	void bindCommonFragmentState(id<MTLRenderCommandEncoder> encoder,
								 const MetalTexture *fogTexture,
								 const MetalTexture *paletteTexture);
	void bindOitCollectBuffers(id<MTLRenderCommandEncoder> encoder, int w, int h);
	void drawResolve(id<MTLRenderCommandEncoder> encoder, bool finalPass, int w, int h);
	/// Blit `depth` into opaqueDepthCopy (growing it as needed) for the
	/// collect shader's manual depth test. Must run outside any render encoder.
	void copyOpaqueDepth(id<MTLTexture> depth, int w, int h);
	/// Tile depth: add the memoryless opaque-depth attachment (colour 1) to the
	/// OP/PT/modvol/collect encoder's pass descriptor. Returns false if the
	/// texture couldn't be created.
	bool attachOpaqueDepthTile(MTLRenderPassDescriptor *passDesc);
	/// Tile depth, render passes after the first: write opaqueDepthCopy (the
	/// depth the pass loaded) into colour 1 with a full-screen draw.
	void seedOpaqueDepthTile(id<MTLRenderCommandEncoder> encoder);
	id<MTLDepthStencilState> getAlwaysDepthStencilState();
	/// Marks the opaque modifier volumes of this render pass in the stencil, draws
	/// the two-volume polys' second parameter set inside the marks (and the
	/// translucent ones' first set outside), then applies the shadow-scale Final.
	void drawOpaqueModVols(id<MTLRenderCommandEncoder> encoder, int first, int count,
						   const RenderPass &prev, const RenderPass &cur, int w, int h);
	/// Two-volume polys (pcw.Volume: a second colour/offset/uv + tsp/tcw/texture set
	/// selected per pixel by the modifier volumes). Metal cannot read the stencil in
	/// the fragment shader, so each such poly is drawn once per set with a stencil
	/// test instead; `area` 1 = second set inside the mark, 2 = first set outside.
	/// The translucent collect draws use the [[early_fragment_tests]] fs_main_early
	/// so that test runs before the a-buffer append.
	static bool isTwoVolumePoly(const PolyParam &pp) {
		return pp.tsp1.full != (u32)-1 || pp.tcw1.full != (u32)-1;
	}
	static PolyParam area1Params(const PolyParam &pp) {
		PolyParam p = pp;
		p.tsp = pp.tsp1;
		p.tcw = pp.tcw1;
		p.texture = pp.texture1;
		return p;
	}
	void drawTwoVolumePolys(id<MTLRenderCommandEncoder> encoder, const RenderPass &prev,
							const RenderPass &cur, int w, int h);
	void drawRegularPoly(id<MTLRenderCommandEncoder> encoder, u32 listType,
						 bool sortTriangles, const PolyParam &poly,
						 u32 first, u32 count, int area = 0);
	void drawTranslucentDepthOnly(id<MTLRenderCommandEncoder> encoder,
								  bool autosort, u32 first, u32 last);

	int getFramebufferIndex() const { return framebufferIndex; }

	// Per-frame state. Names mirror Vulkan OITDrawer for easy cross-reference.
	MTLViewport viewport {};
	std::array<id<MTLTexture>, 2>                 colorAttachments {};
	std::array<id<MTLTexture>, 2>                 depthAttachments {};
	std::array<MTLRenderPassDescriptor *, 2>      tempPassDescriptors {};
	/// Scratch copy of the opaque color attachment, blitted between the
	/// collect and resolve encoders and sampled by oit_resolve_fragment.
	/// Replaces the [[color(0)]] framebuffer fetch the resolve shader used
	/// to do, which isn't supported by every Metal implementation.
	id<MTLTexture>                                resolveBackground = nil;
	/// Scratch copy of the opaque + punch-through depth, blitted before the
	/// collect draws and read by fs_main's OIT path for its manual depth test
	/// (Vulkan reads the same depth as a subpass input). Sized lazily from
	/// the pass's depth attachment so the screen and RTT drawers share it.
	id<MTLTexture>                                opaqueDepthCopy = nil;
	/// Tile-memory opaque depth (colour attachment 1, R32Float, memoryless,
	/// cleared to 0 = the depth clear value, never stored). OP/PT draws write
	/// their depth to it and the collect reads it by framebuffer fetch, so the
	/// manual depth test needs no encoder break. Used when tileDepth.
	id<MTLTexture>                                opaqueDepthTile = nil;
	/// MetalOITTileDepthSupported() for this device; fixed at Init.
	bool                                          tileDepth = false;
	/// Depth/stencil state for the resolve quad: always pass, no depth write,
	/// no stencil test. Vulkan bakes the equivalent into its final pipeline as
	/// a default-constructed PipelineDepthStencilStateCreateInfo; on Metal
	/// depth/stencil state is encoder state, so it must be set explicitly or
	/// the resolve inherits whatever DrawModifierVolumes left bound.
	id<MTLDepthStencilState>                      resolveDepthStencil = nil;
	id<MTLCommandBuffer>                          currentCommandBuffer = nil;
	std::vector<bool>                             clearNeeded;
	int  maxWidth        = 0;
	int  maxHeight       = 0;
	int  framebufferIndex = 0;
	bool needAttachmentTransition = false;

protected:
	// Drawer-side orchestration. Iterates rendContext->global_param_tr
	// and emits per-poly drawIndexedPrimitives through the collect
	// pipeline. Each poly gets the same per-poly setup the regular
	// MetalDrawer does (pipeline state, depth/stencil, cull, scissor,
	// PushBlock setBytes) — the only difference is the pipeline comes
	// from MetalOITPipelineManager::GetCollectPipeline so fs_main runs
	// with oitMode=true.
	void DrawList(id<MTLRenderCommandEncoder> encoder, u32 listType,
				  bool sortTriangles, const std::vector<PolyParam>& polys,
				  u32 first, u32 last, bool oitCollect = true);
	/// triangleList: the index range is an unstripped triangle list (only
	/// DrawSorted, which walks rendContext->sortedTriangles). Everything else indexes
	/// the triangle strips makeIndex() builds for global_param_tr.
	void DrawPoly(id<MTLRenderCommandEncoder> encoder, u32 listType,
				  bool sortTriangles, const PolyParam& poly,
				  u32 first, u32 count, bool oitCollect = true,
				  bool depthOnly = false, bool triangleList = false, int area = 0);
	void DrawSorted(id<MTLRenderCommandEncoder> encoder,
					const std::vector<SortedTriangle>& polys,
					u32 first, u32 last);
	/// TR modifier volumes, in the resolve encoder. With
	/// MetalOITFragmentOrdering::EncoderSplit the encoder is ended and reopened
	/// from passDesc (set up again by setupEncoder) between a volume's marks and
	/// its sum, so `encoder` is updated in place.
	void DrawModifierVolumes(__strong id<MTLRenderCommandEncoder> &encoder,
							 MTLRenderPassDescriptor *passDesc,
							 const std::function<void(id<MTLRenderCommandEncoder>)> &setupEncoder,
							 int first, int count, bool useSharedMvo = false);

	// The autosort path deliberately calls DrawList on global_param_tr instead of DrawSorted --
	// matching Vulkan, because ta_vtx.cpp only builds sortedTriangles when
	// !perPixel and OIT counts as perPixel, so sortedTriangles is always
	// empty here. See the comment at the DrawList call site.

	struct Offsets {
		u64 indexOffset           = 0;
		u64 modVolOffset          = 0;
		u64 vertexUniformOffset   = 0;
		u64 fragmentUniformOffset = 0;
		u64 polyParamsOffset      = 0;
		u64 polyParamsSize        = 0;
		u64 naomi2OpaqueOffset    = 0;
		u64 naomi2PunchThroughOffset = 0;
		u64 naomi2TranslucentOffset  = 0;
		u64 naomi2ModVolOffset    = 0;
		u64 naomi2TrModVolOffset  = 0;
		u64 lightsOffset          = (u64)-1;
	} offsets;

	void UploadMainBuffer(const MetalVertexShaderUniforms &vertexUniforms,
						  const MetalFragmentShaderUniforms &fragmentUniforms);

	// Regular (non-OIT) pipeline manager for opaque + punch-through
	// draws that render into the OIT ping-pong colour attachment before
	// the collect pass. Owned by the screen drawer subclass; passed in
	// via Init. Required for full visual output — without it the OIT
	// path renders only translucent geometry on a black background.
	MetalPipelineManager    *regularPipelineManager = nullptr;
	MetalOITPipelineManager *pipelineManager = nullptr;
	MetalOITBuffers         *oitBuffers      = nullptr;
	MetalSamplers           *samplers        = nullptr;
	id<MTLBuffer>            curMainBuffer   = nil;
	std::unique_ptr<MetalRingBuffer> ring;
	u32                      currentBaseOffset = 0;
	bool                     dithering       = false;
	/// Per-channel dither divisor derived from fb_W_CTRL.fb_packmode in Draw()
	/// (Vulkan: oit_drawer.cpp). Consumed by the resolve pass.
	float                    ditherDivisor[4] = { 0.f, 0.f, 0.f, 1.f };
};

class MetalOITScreenDrawer : public MetalOITDrawer
{
public:
	/// Initialise for screen-bound output.
	void Init(MetalShaders *shaderManager, MetalOITBuffers *oitBuffers,
			  const MTLViewport &vp, MetalSamplers *samplers = nullptr)
	{
		emulateFramebuffer = config::EmulateFramebuffer;
		if (!screenPipelineManager) {
			screenPipelineManager = std::make_unique<MetalOITPipelineManager>(shaderManager, oitBuffers);
			screenPipelineManager->Init();
		}
		if (!regularPipelineManager_) {
			regularPipelineManager_ = std::make_unique<MetalPipelineManager>(
				shaderManager, screenPipelineManager->UseTileDepth());
		}
		MetalOITDrawer::Init(screenPipelineManager.get(), oitBuffers, samplers,
							 regularPipelineManager_.get());
		viewport = vp;
	}

	void Term()
	{
		releaseReadbackTextures();
		regularPipelineManager_.reset();
		screenPipelineManager.reset();
		MetalOITDrawer::Term();
	}

	id<MTLCommandBuffer> NewFrame() override;
	void EndFrame() override;

	/// End the frame, commit the OIT command buffer, and hand the resolved
	/// color attachment to MetalContext::PresentFrame for surface
	/// presentation. Returns false if there's nothing to present (e.g.
	/// the current frame produced no encoders). Defined in
	/// metal_oit_drawer.mm.
	bool PresentFrame();

protected:
	MTLRenderPassDescriptor *getOitRenderPassDescriptor(int renderPassIndex,
														  int renderPassCount,
														  bool initialPass) override;

private:
	std::unique_ptr<MetalOITPipelineManager> screenPipelineManager;
	std::unique_ptr<MetalPipelineManager>    regularPipelineManager_;
	bool  frameRendered      = false;
	float aspectRatio        = 0.f;
	bool  frameStarted       = false;
	bool  emulateFramebuffer = false;
};

class MetalOITTextureDrawer : public MetalOITDrawer
{
public:
	void Init(MetalShaders *shaderManager, MetalOITBuffers *oitBuffers,
			  MetalTextureCache *textureCache, MetalSamplers *samplers = nullptr)
	{
		if (!rttPipelineManager) {
			rttPipelineManager = std::make_unique<MetalOITPipelineManager>(shaderManager, oitBuffers);
			rttPipelineManager->Init();
		}
		if (!regularPipelineManager_) {
			regularPipelineManager_ = std::make_unique<MetalPipelineManager>(
				shaderManager, rttPipelineManager->UseTileDepth());
		}
		MetalOITDrawer::Init(rttPipelineManager.get(), oitBuffers, samplers,
							 regularPipelineManager_.get());
		this->textureCache = textureCache;
		if (rttPassDescriptor == nil)
			rttPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
	}

	void Term()
	{
		texture           = nullptr;
		rttColorImage     = nil;
		rttDepthAttachment = nil;
		colorAttachment   = nil;
		rttPassDescriptor = nil;
		regularPipelineManager_.reset();
		rttPipelineManager.reset();
		MetalOITDrawer::Term();
	}

	id<MTLCommandBuffer> NewFrame() override;
	void EndFrame() override;

protected:
	MTLRenderPassDescriptor *getOitRenderPassDescriptor(int renderPassIndex,
														  int renderPassCount,
														  bool initialPass) override;


private:
	u32 textureAddr = 0;
	// RTT readback scratch; see MetalTextureDrawer::rttReadback.
	PixelBuffer<u32> rttReadback;

	MetalTexture *texture = nullptr;
	MetalTextureCache *textureCache = nullptr;
	id<MTLTexture> colorAttachment = nil;
	id<MTLTexture> rttColorImage = nil;
	id<MTLTexture> rttDepthAttachment = nil;
	MTLRenderPassDescriptor *rttPassDescriptor = nil;
	std::unique_ptr<MetalOITPipelineManager> rttPipelineManager;
	std::unique_ptr<MetalPipelineManager>    regularPipelineManager_;
};
