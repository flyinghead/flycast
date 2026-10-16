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
// Metal OIT drawer. Port of core/rend/vulkan/oit/oit_drawer.cpp.
//
// Per Draw():
//   1. blit encoder: reset the pixel counter and set every a-buffer head pointer to EOL
//   2. collect render encoder: opaque and punch-through polys, then translucent
//      fragments appended to the per-pixel linked lists (color writes masked off)
//   3. resolve render encoder: translucent modifier volumes, then the resolve pass
//      sorts and blends each pixel's list over the opaque background
#include "metal_oit_drawer.h"
#include "../metal_context.h"
#include "hw/pvr/ta.h"

#include <algorithm>
#include <cstddef>

namespace {

// Build (or reuse) a ResolveUniforms upload buffer. Three uints, tiny —
// passed via setFragmentBytes so no MTLBuffer allocation is needed at
// the call site. Centralised here so collect and resolve see the same
// viewport sizing.
// Must match ResolveUniforms in metal_oit_shaders.h: MSL float4 is 16-byte
// aligned, so the struct is 48 bytes (ditherDivisor at 16, applyDither at 32).
struct alignas(16) MetalOITResolveUniformsCPU {
	uint32_t viewportWidth;
	uint32_t viewportHeight;
	uint32_t pixelBufferSize;
	float shadeScale;
	float ditherDivisor[4];
	uint32_t applyDither;
};
static_assert(offsetof(MetalOITResolveUniformsCPU, ditherDivisor) == 16, "ResolveUniforms layout");
static_assert(offsetof(MetalOITResolveUniformsCPU, applyDither) == 32, "ResolveUniforms layout");
static_assert(sizeof(MetalOITResolveUniformsCPU) == 48, "ResolveUniforms layout");

} // namespace

bool MetalOITDrawer::Draw(const MetalTexture *fogTexture,
						  const MetalTexture *paletteTexture)
{
	// Runs on the emu/render thread (no pool of its own) — same rationale as
	// the @autoreleasepool in the non-OIT MetalDrawer::Draw.
	@autoreleasepool {
	if (commandPool == nullptr) {
		WARN_LOG(RENDERER, "MetalOITDrawer::Draw: no command pool");
		return false;
	}
	if (oitBuffers == nullptr || pipelineManager == nullptr) {
		WARN_LOG(RENDERER, "MetalOITDrawer::Draw: pipeline manager or OIT buffers missing");
		return false;
	}
	const int w = (int)viewport.width;
	const int h = (int)viewport.height;
	if (w <= 0 || h <= 0)
		return false;
	MakeBuffers(w, h);
	if (colorAttachments[0] == nil)
		return false;

	if (currentCommandBuffer == nil)
		currentCommandBuffer = commandPool->Allocate();
	if (currentCommandBuffer == nil)
		return false;
	[currentCommandBuffer pushDebugGroup:@"OIT Frame"];

	dithering = config::EmulateFramebuffer && rendContext->fb_W_CTRL.fb_dither
				&& rendContext->fb_W_CTRL.fb_packmode <= 3;
	ditherDivisor[0] = ditherDivisor[1] = ditherDivisor[2] = 0.f;
	ditherDivisor[3] = 1.f;
	if (dithering) {
		switch (rendContext->fb_W_CTRL.fb_packmode)
		{
			case 0: // 0555 KRGB 16 bit
			case 3: // 1555 ARGB 16 bit
				ditherDivisor[0] = ditherDivisor[1] = ditherDivisor[2] = 2.f;
				break;
			case 1: // 565 RGB 16 bit
				ditherDivisor[0] = ditherDivisor[2] = 2.f;
				ditherDivisor[1] = 4.f;
				break;
			case 2: // 4444 ARGB 16 bit
				ditherDivisor[0] = ditherDivisor[1] = ditherDivisor[2] = 1.f;
				break;
			default:
				break;
		}
	}
	bool firstFrameAfterInit = oitBuffers->isFirstFrameAfterInit();
	oitBuffers->OnNewFrame();

	if (firstFrameAfterInit) {
		id<MTLBlitCommandEncoder> blit = [currentCommandBuffer blitCommandEncoder];
		id<MTLBuffer> head = oitBuffers->getAbufferPointer();
		if (head != nil) {
			NSUInteger fillBytes = (NSUInteger)w * (NSUInteger)h * sizeof(uint32_t);
			if (fillBytes > [head length])
				fillBytes = [head length];
			[blit fillBuffer:head range:NSMakeRange(0, fillBytes) value:0xFF];
		}
		[blit endEncoding];
	}

	// Metal reads [[flat]] varyings from the FIRST vertex; Dreamcast flat
	// shading uses the LAST. The Vulkan OIT drawer does this fix-up too
	// (oit_drawer.cpp) when VK_EXT_provoking_vertex is unavailable. Without
	// it every flat-shaded poly takes its color/alpha from the wrong vertex —
	// most visibly the THPS2 / Mat Hoffman player-shadow decals, which carry
	// their real color only on the provoking vertex and rendered invisible.
	// Must run before UploadMainBuffer so the reordered verts/idx get uploaded.
	setFirstProvokingVertex(*rendContext);

	// Matrices must be computed BEFORE the uniforms snapshot them — the
	// non-OIT drawers do this in BeginRenderPass, and Vulkan's OIT drawer
	// calls CalcMatrices at the top of Draw. Reading GetNormalMatrix first
	// uploaded the previous frame's ndcMat (uninitialized on the first
	// frame after init/resize).
	matrices.CalcMatrices(rendContext, w, h);
	SetBaseScissor(viewport);

	MetalVertexShaderUniforms vtxUniforms {};
	vtxUniforms.ndcMat = matrices.GetNormalMatrix();
	MetalFragmentShaderUniforms fragUniforms =
		MakeFragmentUniforms<MetalFragmentShaderUniforms>();
	std::copy(std::begin(ditherDivisor), std::end(ditherDivisor), fragUniforms.ditherDivisor);
	UploadMainBuffer(vtxUniforms, fragUniforms);

	const int renderPassCount = (int)rendContext->render_passes.size();
	RenderPass previous_pass {};
	for (int rp = 0; rp < renderPassCount; ++rp) {
		const RenderPass &cur = rendContext->render_passes[rp];
		const bool initialPass = rp == 0;
		const bool finalPass = rp == renderPassCount - 1;

		resetOitPixelCounter();

		MTLRenderPassDescriptor *passDesc =
			getOitRenderPassDescriptor(rp, renderPassCount, initialPass);
		if (passDesc == nil)
			continue;

		auto setupEncoder = [&](id<MTLRenderCommandEncoder> e) {
			[e setViewport:viewport];
			[e setFrontFacingWinding:MTLWindingCounterClockwise];
			[e setVertexBuffer:curMainBuffer offset:currentBaseOffset + 0 atIndex:30];
			[e setVertexBuffer:curMainBuffer
						offset:currentBaseOffset + offsets.vertexUniformOffset
					   atIndex:0];
			[e setFragmentBuffer:curMainBuffer
						  offset:currentBaseOffset + offsets.fragmentUniformOffset
						 atIndex:0];
			bindCommonFragmentState(e, fogTexture, paletteTexture);
		};

		// The collect fragments must be depth-tested against the opaque +
		// punch-through depth in the shader. fs_main writes [[depth(any)]], so
		// the hardware depth test runs after the fragment function, and the
		// a-buffer append (device atomics) has already happened by then: the
		// collect pass's depth-stencil state only gates the masked-out colour
		// write and rejects nothing. Vulkan tests manually against the depth
		// subpass input (oit_shaders.cpp, "Manual depth testing").
		//
		// Apple GPUs (tileDepth): the OP/PT draws also write their depth to a
		// memoryless colour attachment and the collect reads it back with
		// framebuffer fetch — the depth never leaves tile memory. Elsewhere the
		// encoder is ended after OP/PT and the depth attachment blitted to a
		// sampled copy (below), which on a TBDR GPU would store and reload the
		// colour and depth attachments.
		const bool collectTr = cur.autosort && cur.tr_count > previous_pass.tr_count;
		bool useTileDepth = tileDepth && attachOpaqueDepthTile(passDesc);
		if (tileDepth && !useTileDepth)
			continue;   // every pipeline here declares the tile attachment
		// Passes after the first load the depth of the earlier ones (and their
		// translucent depth-only writes); the tile copy starts cleared, so seed
		// it from a copy taken before the encoder opens (no extra encoder break:
		// the previous pass's resolve encoder already stored the depth).
		const bool seedTile = useTileDepth && !initialPass && collectTr;
		if (seedTile)
			copyOpaqueDepth(passDesc.depthAttachment.texture, w, h);

		id<MTLRenderCommandEncoder> enc =
			[currentCommandBuffer renderCommandEncoderWithDescriptor:passDesc];
		[enc pushDebugGroup:@"OIT Render Pass"];
		setupEncoder(enc);
		if (seedTile)
			seedOpaqueDepthTile(enc);

		// OP + PT (Metal merges Vulkan depth+color subpasses)
		if (regularPipelineManager != nullptr) {
			for (u32 i = previous_pass.op_count; i < cur.op_count; ++i) {
				const PolyParam &pp = rendContext->global_param_op[i];
				if (pp.count > 2)
					drawRegularPoly(enc, ListType_Opaque, false, pp, pp.first, pp.count);
			}
			for (u32 i = previous_pass.pt_count; i < cur.pt_count; ++i) {
				const PolyParam &pp = rendContext->global_param_pt[i];
				if (pp.count > 2)
					drawRegularPoly(enc, ListType_Punch_Through, false, pp, pp.first, pp.count);
			}
		}

		// Copy fallback (no tile depth): end the encoder, copy the depth to a
		// sampled texture and reopen; fs_main reads the copy at texture 4.
		if (!useTileDepth && collectTr) {
			[enc popDebugGroup];
			[enc endEncoding];
			copyOpaqueDepth(passDesc.depthAttachment.texture, w, h);
			passDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
			passDesc.depthAttachment.loadAction    = MTLLoadActionLoad;
			passDesc.stencilAttachment.loadAction  = MTLLoadActionLoad;
			enc = [currentCommandBuffer renderCommandEncoderWithDescriptor:passDesc];
			[enc pushDebugGroup:@"OIT Collect Pass"];
			setupEncoder(enc);
			[enc setFragmentTexture:opaqueDepthCopy atIndex:4];
		}

		if (regularPipelineManager != nullptr) {
			drawOpaqueModVols(enc, (int)previous_pass.mvo_count,
							  (int)cur.mvo_count - (int)previous_pass.mvo_count,
							  previous_pass, cur, w, h);
		}

		// Translucent collect or regular blended TR
		if (cur.autosort) {
			bindOitCollectBuffers(enc, w, h);
			// Always draw the raw translucent list, never rendContext->sortedTriangles.
			//
			// ta_vtx.cpp:1194 only builds sortedTriangles when `!perPixel`, and
			// parseRenderPass counts Metal_OIT as perPixel — OIT sorts per pixel
			// in the a-buffer, so the core deliberately skips the CPU sort and
			// leaves sortedTriangles empty with sorted_tr_count == 0. The old
			// code called DrawSorted() on that empty vector whenever
			// !PerStripSorting, and since reconcileStaleOITRenderer refuses to
			// enable OIT unless PerStripSorting is off, that dead branch was the
			// one taken 100% of the time: DrawSorted returned immediately on
			// first == last, the collect pass emitted zero fragments (verified:
			// OIT pixel counter read back 0 with 11 translucent polys queued),
			// and the resolve composited an empty a-buffer — leaving only the
			// flat-shaded opaque layer on screen. That is the blank Dreamcast
			// BIOS boot swirl and BIOS menu.
			//
			// Vulkan's OIT drawer (oit_drawer.cpp:434) has no DrawSorted path at
			// all — both autosort branches walk global_param_tr with tr_count.
			DrawList(enc, ListType_Translucent, true, rendContext->global_param_tr,
					 previous_pass.tr_count, cur.tr_count, true);
		} else {
			DrawList(enc, ListType_Translucent, false, rendContext->global_param_tr,
					 previous_pass.tr_count, cur.tr_count, false);
		}

		// The collect fragments write the a-buffer (pixel records + head
		// pointers) through device-memory atomics, and Metal does NOT order
		// those writes against later draws within the same render pass —
		// a fragment->fragment render-encoder memoryBarrier is invalid on
		// Apple GPUs (see MetalOITFragmentOrdering). The
		// TR-modvol and resolve draws below READ those lists, so they must
		// run in a second encoder: Metal's automatic hazard tracking orders
		// buffer access between passes. The single merged encoder read the
		// a-buffer racily — intermittent gray/ghost frames in the simulator,
		// consistently missing HUD-under-translucency on device TBDR. (The
		// file header's "two-encoder dance" described exactly this split.)
		[enc popDebugGroup];
		[enc endEncoding];

		// Snapshot the opaque result for the resolve shader. The resolve used
		// to read the attachment in place via [[color(0)]] framebuffer fetch,
		// but that construct is unimplemented in the iOS Simulator's Metal:
		// CreateResolvePipeline failed with "reading from a rendertarget is
		// not supported" on every frame, drawResolve returned early, and the
		// whole translucent list was collected into the a-buffer and then
		// thrown away. Copying to a plain texture composites identically and
		// builds everywhere. Metal's automatic hazard tracking orders the
		// blit after the collect encoder and before the resolve encoder.
		id<MTLTexture> opaqueTex = passDesc.colorAttachments[0].texture;
		if (resolveBackground != nil && opaqueTex != nil) {
			const NSUInteger copyW = std::min((NSUInteger)w,
											  std::min(opaqueTex.width, resolveBackground.width));
			const NSUInteger copyH = std::min((NSUInteger)h,
											  std::min(opaqueTex.height, resolveBackground.height));
			if (copyW > 0 && copyH > 0) {
				id<MTLBlitCommandEncoder> bgBlit = [currentCommandBuffer blitCommandEncoder];
				[bgBlit copyFromTexture:opaqueTex
							sourceSlice:0
							sourceLevel:0
						   sourceOrigin:MTLOriginMake(0, 0, 0)
							 sourceSize:MTLSizeMake(copyW, copyH, 1)
							  toTexture:resolveBackground
					   destinationSlice:0
					   destinationLevel:0
					  destinationOrigin:MTLOriginMake(0, 0, 0)];
				[bgBlit endEncoding];
			}
		}

		passDesc.colorAttachments[0].loadAction = MTLLoadActionLoad;
		passDesc.depthAttachment.loadAction    = MTLLoadActionLoad;
		passDesc.stencilAttachment.loadAction  = MTLLoadActionLoad;
		// The tile depth is dead after the collect, and a memoryless attachment
		// can't be loaded: the resolve encoder's pipelines don't declare it.
		passDesc.colorAttachments[1].texture = nil;
		enc = [currentCommandBuffer renderCommandEncoderWithDescriptor:passDesc];
		[enc pushDebugGroup:@"OIT Resolve Pass"];
		setupEncoder(enc);

		// TR modvol after collect (Vulkan final subpass)
		if (cur.mv_op_tr_shared) {
			DrawModifierVolumes(enc, passDesc, setupEncoder, (int)previous_pass.mvo_count,
								(int)cur.mvo_count - (int)previous_pass.mvo_count,
								true);
		} else {
			DrawModifierVolumes(enc, passDesc, setupEncoder, (int)previous_pass.mvo_tr_count,
								(int)cur.mvo_tr_count - (int)previous_pass.mvo_tr_count,
								false);
		}

		drawResolve(enc, finalPass, w, h);

		if (!finalPass)
			drawTranslucentDepthOnly(enc, cur.autosort, previous_pass.tr_count, cur.tr_count);

		[enc popDebugGroup];
		[enc endEncoding];
		previous_pass = cur;
	}

	[currentCommandBuffer popDebugGroup];
	return true;
	} // @autoreleasepool
}

void MetalOITDrawer::copyOpaqueDepth(id<MTLTexture> depth, int w, int h)
{
	if (depth == nil)
		return;
	if (opaqueDepthCopy == nil || opaqueDepthCopy.width < depth.width
		|| opaqueDepthCopy.height < depth.height
		|| opaqueDepthCopy.pixelFormat != depth.pixelFormat)
	{
		MTLTextureDescriptor *desc =
			[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:depth.pixelFormat
																width:depth.width
															   height:depth.height
															mipmapped:NO];
		desc.usage       = MTLTextureUsageShaderRead;
		desc.storageMode = MTLStorageModePrivate;
		opaqueDepthCopy = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:desc];
		opaqueDepthCopy.label = @"OIT opaque depth copy";
	}
	const NSUInteger copyW = std::min((NSUInteger)w, depth.width);
	const NSUInteger copyH = std::min((NSUInteger)h, depth.height);
	if (opaqueDepthCopy == nil || copyW == 0 || copyH == 0)
		return;
	id<MTLBlitCommandEncoder> blit = [currentCommandBuffer blitCommandEncoder];
	[blit copyFromTexture:depth
			  sourceSlice:0
			  sourceLevel:0
			 sourceOrigin:MTLOriginMake(0, 0, 0)
			   sourceSize:MTLSizeMake(copyW, copyH, 1)
				toTexture:opaqueDepthCopy
		 destinationSlice:0
		 destinationLevel:0
		destinationOrigin:MTLOriginMake(0, 0, 0)];
	[blit endEncoding];
}

bool MetalOITDrawer::attachOpaqueDepthTile(MTLRenderPassDescriptor *passDesc)
{
	id<MTLTexture> color = passDesc.colorAttachments[0].texture;
	if (color == nil)
		return false;
	if (opaqueDepthTile == nil || opaqueDepthTile.width != color.width
		|| opaqueDepthTile.height != color.height)
	{
		MTLTextureDescriptor *desc =
			[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
																width:color.width
															   height:color.height
															mipmapped:NO];
		desc.usage       = MTLTextureUsageRenderTarget;
		desc.storageMode = MTLStorageModePrivate;
		if (@available(macOS 11.0, iOS 10.0, tvOS 10.0, *))
			desc.storageMode = MTLStorageModeMemoryless;
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();
		opaqueDepthTile = [device newTextureWithDescriptor:desc];
		if (opaqueDepthTile == nil && desc.storageMode != MTLStorageModePrivate) {
			desc.storageMode = MTLStorageModePrivate;
			opaqueDepthTile = [device newTextureWithDescriptor:desc];
		}
		if (opaqueDepthTile == nil) {
			WARN_LOG(RENDERER, "MetalOITDrawer: can't create the opaque depth tile attachment");
			return false;
		}
		opaqueDepthTile.label = @"OIT opaque depth (tile)";
	}
	passDesc.colorAttachments[1].texture     = opaqueDepthTile;
	passDesc.colorAttachments[1].loadAction  = MTLLoadActionClear;
	passDesc.colorAttachments[1].storeAction = MTLStoreActionDontCare;
	// 0 = the depth attachment's clear value: nothing opaque, every
	// translucent fragment passes the GEqual test.
	passDesc.colorAttachments[1].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 0.0);
	return true;
}

id<MTLDepthStencilState> MetalOITDrawer::getAlwaysDepthStencilState()
{
	if (resolveDepthStencil == nil) {
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();
		if (device != nil) {
			MTLDepthStencilDescriptor *dsDesc = [[MTLDepthStencilDescriptor alloc] init];
			dsDesc.depthCompareFunction = MTLCompareFunctionAlways;
			dsDesc.depthWriteEnabled    = NO;
			dsDesc.label                = @"OIT Resolve DS";
			resolveDepthStencil = [device newDepthStencilStateWithDescriptor:dsDesc];
		}
	}
	return resolveDepthStencil;
}

void MetalOITDrawer::seedOpaqueDepthTile(id<MTLRenderCommandEncoder> encoder)
{
	id<MTLRenderPipelineState> pso = pipelineManager->GetSeedOpaqueDepthPipeline();
	id<MTLDepthStencilState> ds = getAlwaysDepthStencilState();
	if (pso == nil || ds == nil || opaqueDepthCopy == nil)
		return;
	[encoder pushDebugGroup:@"OIT seed opaque depth"];
	[encoder setRenderPipelineState:pso];
	[encoder setDepthStencilState:ds];
	[encoder setCullMode:MTLCullModeNone];
	SetScissor(encoder, baseScissor);
	[encoder setFragmentTexture:opaqueDepthCopy atIndex:4];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
	[encoder popDebugGroup];
}

void MetalOITDrawer::resetOitPixelCounter()
{
	id<MTLBlitCommandEncoder> blit = [currentCommandBuffer blitCommandEncoder];
	oitBuffers->ResetPixelCounter(blit);
	[blit endEncoding];
}

void MetalOITDrawer::bindCommonFragmentState(id<MTLRenderCommandEncoder> encoder,
											 const MetalTexture *fogTexture,
											 const MetalTexture *paletteTexture)
{
	if (fogTexture != nullptr && samplers != nullptr) {
		[encoder setFragmentTexture:fogTexture->GetTexture() atIndex:2];
		TSP fogTsp = {};
		fogTsp.FilterMode = 1;
		fogTsp.ClampU = 1;
		fogTsp.ClampV = 1;
		id<MTLSamplerState> fogSampler = samplers->GetSampler(fogTsp);
		if (fogSampler != nil)
			[encoder setFragmentSamplerState:fogSampler atIndex:2];
	}
	if (paletteTexture != nullptr && samplers != nullptr) {
		[encoder setFragmentTexture:paletteTexture->GetTexture() atIndex:3];
		TSP palTsp = {};
		palTsp.FilterMode = 0;
		palTsp.ClampU = 1;
		palTsp.ClampV = 1;
		id<MTLSamplerState> palSampler = samplers->GetSampler(palTsp);
		if (palSampler != nil)
			[encoder setFragmentSamplerState:palSampler atIndex:3];
	}
}

void MetalOITDrawer::bindOitCollectBuffers(id<MTLRenderCommandEncoder> encoder, int w, int h)
{
	[encoder setFragmentBuffer:oitBuffers->getPixelCounter()   offset:0 atIndex:2];
	[encoder setFragmentBuffer:oitBuffers->getPixelBuffer()    offset:0 atIndex:3];
	[encoder setFragmentBuffer:oitBuffers->getAbufferPointer() offset:0 atIndex:4];
	MetalOITResolveUniformsCPU collectVpUniforms = {
		(uint32_t)w,
		(uint32_t)h,
		(uint32_t)([oitBuffers->getPixelBuffer() length] / 16u),
		0.f,
		{ 0.f, 0.f, 0.f, 1.f },
		0u,
	};
	[encoder setFragmentBytes:&collectVpUniforms length:sizeof(collectVpUniforms) atIndex:5];
}

void MetalOITDrawer::drawResolve(id<MTLRenderCommandEncoder> encoder, bool finalPass, int w, int h)
{
	id<MTLRenderPipelineState> pso = pipelineManager->GetResolvePipeline();
	if (pso == nil)
		return;
	[encoder setRenderPipelineState:pso];

	// The resolve quad must not be depth- or stencil-tested. Vulkan bakes that
	// into the final pipeline (CreateFinalPipeline uses a default-constructed
	// PipelineDepthStencilStateCreateInfo — all tests disabled), but on Metal
	// depth/stencil state lives on the encoder, and DrawModifierVolumes runs
	// immediately before this in the same encoder and leaves its TR-modvol
	// state bound. Any scene with translucent modifier volumes would then
	// stencil-test the full-screen resolve and drop the composite. Bind an
	// explicit always/no-write/no-stencil state instead of inheriting.
	if (getAlwaysDepthStencilState() != nil)
		[encoder setDepthStencilState:resolveDepthStencil];

	// Same inheritance trap as the depth/stencil state above, and just as
	// damaging: DrawModifierVolumes calls SetTileClip per volume, which sets
	// the encoder scissor, and the resolve's full-screen triangle would run
	// under whatever tile rect the last modvol left behind. Pixels outside it
	// are never resolved — and because the resolve is what atomically swaps
	// each pixel's a-buffer head back to EOL, those pixels also keep last
	// frame's list head, so the next frame chains onto stale entries. Restore
	// the base scissor (Vulkan's final pipeline uses a dynamic scissor set to
	// the full viewport).
	SetScissor(encoder, baseScissor);
	[encoder setFragmentBuffer:oitBuffers->getPixelBuffer()    offset:0 atIndex:1];
	[encoder setFragmentBuffer:oitBuffers->getAbufferPointer() offset:0 atIndex:2];
	[encoder setFragmentBuffer:curMainBuffer
						  offset:currentBaseOffset + offsets.polyParamsOffset
						 atIndex:4];
	// Opaque background: the scratch copy blitted in Draw() after the collect
	// encoder. Vulkan reads its equivalent as a resolve-subpass input
	// attachment; the Metal port used [[color(0)]] framebuffer fetch, which
	// the iOS Simulator cannot create a pipeline for.
	if (resolveBackground != nil)
		[encoder setFragmentTexture:resolveBackground atIndex:0];

	const bool applyDither = dithering && finalPass;
	MetalOITResolveUniformsCPU resolveUniforms = {
		(uint32_t)w,
		(uint32_t)h,
		(uint32_t)([oitBuffers->getPixelBuffer() length] / 16u),
		// The resolve shader multiplies shadowed TR fragments by this value
		// directly (srcColor.rgb *= shadeScale), same as Vulkan's resolve
		// (oit_shaders.cpp srcColor.rgb *= shade_scale_factor with
		// scale_factor/256). Passing 1-s here inverted shadow darkness around
		// 0.5. Note drawOpaqueModVols below correctly keeps 1-s: its final
		// quad goes through the non-OIT alpha-blend pattern (SrcAlpha /
		// OneMinusSrcAlpha), where alpha 1-s nets dst*s.
		FPU_SHAD_SCALE.scale_factor / 256.f,
		{ ditherDivisor[0], ditherDivisor[1], ditherDivisor[2], ditherDivisor[3] },
		applyDither ? 1u : 0u,
	};
	[encoder setFragmentBytes:&resolveUniforms length:sizeof(resolveUniforms) atIndex:3];
	[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
}

void MetalOITDrawer::drawRegularPoly(id<MTLRenderCommandEncoder> encoder, u32 listType,
									 bool sortTriangles, const PolyParam &polyIn,
									 u32 first, u32 count, int area)
{
	if (regularPipelineManager == nullptr || curMainBuffer == nil)
		return;
	const PolyParam polyArea1 = area == 1 ? area1Params(polyIn) : PolyParam();
	const PolyParam &poly = area == 1 ? polyArea1 : polyIn;

	MTLScissorRect scissorRect {};
	TileClipping tileClip = SetTileClip(encoder, poly.tileclip, scissorRect);

	float trilinearAlpha = 1.0f;
	if (poly.tsp.FilterMode > 1 && poly.pcw.Texture
		&& listType != ListType_Punch_Through && poly.tcw.MipMapped == 1) {
		trilinearAlpha = 0.25f * (poly.tsp.MipMapD & 0x3);
		if (poly.tsp.FilterMode == 2)
			trilinearAlpha = 1.0f - trilinearAlpha;
	}
	int gpuPalette = poly.texture == nullptr || !((MetalTexture *)poly.texture)->gpuPalette
					 ? 0 : poly.tsp.FilterMode + 1;
	float palette_index = 0.0f;
	if (gpuPalette != 0) {
		if (config::TextureFiltering == 1) gpuPalette = 1;
		else if (config::TextureFiltering == 2) gpuPalette = 2;
		if (poly.tcw.PixelFmt == PixelPal4)
			palette_index = float(poly.tcw.PalSelect << 4) / 1023.0f;
		else
			palette_index = float(poly.tcw.PalSelect >> 4 << 8) / 1023.0f;
	}

	std::array<float, 8> pushConstants {};
	if (tileClip == TileClipping::Inside || trilinearAlpha != 1.0f || gpuPalette != 0) {
		pushConstants = {
			(float)scissorRect.x,
			(float)scissorRect.y,
			(float)scissorRect.x + (float)scissorRect.width,
			(float)scissorRect.y + (float)scissorRect.height,
			trilinearAlpha,
			palette_index,
		};
	} else {
		pushConstants = { 0, 0, 0, 0, 0, 0 };
	}
	[encoder setFragmentBytes:pushConstants.data()
					   length:sizeof(pushConstants) + MetalBufferPacker::align(sizeof(pushConstants), 16)
					  atIndex:1];

	const bool shadowed = listType == ListType_Opaque || listType == ListType_Punch_Through;

	// Dithering is applied once, by the resolve pass (as in Vulkan OIT, where
	// only the final shader dithers); dithering here would be applied twice.
	auto pso = regularPipelineManager->GetPipeline(listType, sortTriangles, poly, gpuPalette, false, false, area == 1);
	if (pso == nil)
		return;
	[encoder setRenderPipelineState:pso];
	[encoder setDepthStencilState:regularPipelineManager->GetDepthStencilStates(
		listType, sortTriangles, shadowed, poly, false, area)];
	[encoder setCullMode:toMetalCullMode(poly.isp.CullMode)];
	if (area != 0)
		[encoder setStencilReferenceValue:0x81];
	else if (shadowed)
		[encoder setStencilReferenceValue:poly.pcw.Shadow != 0 ? 0x80 : 0];

	if (poly.texture != nullptr && samplers != nullptr) {
		[encoder setFragmentTexture:((MetalTexture *)poly.texture)->GetReadOnlyTexture() atIndex:0];
		id<MTLSamplerState> samplerState =
			samplers->GetSampler(poly, listType == ListType_Punch_Through);
		if (samplerState == nil)
			return;
		[encoder setFragmentSamplerState:samplerState atIndex:0];
	} else if (poly.pcw.Texture && samplers != nullptr) {
		static id<MTLTexture> defaultTexture = nil;
		if (defaultTexture == nil) {
			MTLTextureDescriptor *texDesc = [MTLTextureDescriptor
				texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
				width:1 height:1 mipmapped:NO];
			defaultTexture = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:texDesc];
			uint32_t whitePixel = 0xFFFFFFFF;
			[defaultTexture replaceRegion:MTLRegionMake2D(0, 0, 1, 1)
							  mipmapLevel:0
								withBytes:&whitePixel
							  bytesPerRow:4];
		}
		[encoder setFragmentTexture:defaultTexture atIndex:0];
		TSP defaultTsp = {};
		id<MTLSamplerState> defaultSampler = samplers->GetSampler(defaultTsp);
		if (defaultSampler == nil)
			return;
		[encoder setFragmentSamplerState:defaultSampler atIndex:0];
	}

	// Naomi2 per-poly vertex uniforms + lights (mirrors MetalDrawer::DrawPoly).
	if (poly.pcw.Texture || poly.isNaomi2()) {
		u64 offset = 0;
		u32 index = 0;
		if (poly.isNaomi2()) {
			switch (listType) {
			case ListType_Opaque:
				offset = offsets.naomi2OpaqueOffset;
				index = (u32)(&polyIn - &rendContext->global_param_op[0]);
				if (index >= rendContext->global_param_op.size())
					return;
				break;
			case ListType_Punch_Through:
				offset = offsets.naomi2PunchThroughOffset;
				index = (u32)(&polyIn - &rendContext->global_param_pt[0]);
				if (index >= rendContext->global_param_pt.size())
					return;
				break;
			default:
				break;
			}
		}

		size_t size = sizeof(MetalN2VertexShaderUniforms)
					  + MetalBufferPacker::align(sizeof(MetalN2VertexShaderUniforms), 16);
		u64 finalOffset = currentBaseOffset + offset + index * size;
		if (finalOffset >= [curMainBuffer length])
			return;
		[encoder setVertexBuffer:curMainBuffer offset:finalOffset atIndex:1];

		if (offsets.lightsOffset != (u64)-1) {
			size = sizeof(N2LightModel) + MetalBufferPacker::align(sizeof(N2LightModel), 16);
			if (poly.lightModel >= 256)
				return;
			u64 lightsOffset = currentBaseOffset + offsets.lightsOffset + poly.lightModel * size;
			if (lightsOffset >= [curMainBuffer length])
				return;
			[encoder setVertexBuffer:curMainBuffer offset:lightsOffset atIndex:2];
		}
	}

	const u64 indexOffset = currentBaseOffset + offsets.indexOffset + first * sizeof(u32);
	if (indexOffset + count * sizeof(u32) > [curMainBuffer length])
		return;

	[encoder setVertexBuffer:curMainBuffer offset:currentBaseOffset atIndex:30];

	MTLPrimitiveType primitive = sortTriangles && !config::PerStripSorting
								 ? MTLPrimitiveTypeTriangle
								 : MTLPrimitiveTypeTriangleStrip;
	[encoder drawIndexedPrimitives:primitive
						indexCount:count
						 indexType:MTLIndexTypeUInt32
					   indexBuffer:curMainBuffer
				 indexBufferOffset:indexOffset];
}

void MetalOITDrawer::drawOpaqueModVols(id<MTLRenderCommandEncoder> encoder, int first, int count,
									   const RenderPass &prev, const RenderPass &cur, int w, int h)
{
	if (count <= 0 || rendContext->modtrig.empty() || !config::ModifierVolumes
		|| regularPipelineManager == nullptr || curMainBuffer == nil) {
		// No marks this pass: the two-volume polys still need their first-set draw.
		drawTwoVolumePolys(encoder, prev, cur, w, h);
		return;
	}

	[encoder setVertexBufferOffset:currentBaseOffset + offsets.modVolOffset atIndex:30];
	// Tile clip fields refreshed per volume below (Inside mode = shader discard).
	MetalModVolPushBlock mvPush;
	mvPush.shaderColor = 1.0f - FPU_SHAD_SCALE.scale_factor / 256.f;
	[encoder setFragmentBytes:&mvPush length:sizeof(mvPush) atIndex:1];

	ModifierVolumeParam *mvParams = &rendContext->global_param_mvo[first];
	int mod_base = -1;
	for (int cmv = 0; cmv < count; ++cmv) {
		ModifierVolumeParam &param = mvParams[cmv];
		if (param.count == 0) continue;
		const u32 mv_mode = param.isp.DepthMode;
		if (mod_base == -1) mod_base = param.first;
		ModVolMode pipeMode = (!param.isp.VolumeLast && mv_mode > 0)
							  ? ModVolMode::Or : ModVolMode::Xor;
		auto pso = regularPipelineManager->GetModifierVolumePipeline(pipeMode, param.isNaomi2());
		auto ds = regularPipelineManager->GetModVolDepthStencilStates(pipeMode, param.isNaomi2());
		if (pso == nil) continue;
		[encoder setRenderPipelineState:pso];
		[encoder setDepthStencilState:ds];
		[encoder setCullMode:toMetalCullMode(param.isp.CullMode)];
		[encoder setStencilReferenceValue:2];
		MTLScissorRect sr {};
		TileClipping clipMode = SetTileClip(encoder, param.tileclip, sr);
		if (mvPush.setTileClip(clipMode, sr))
			[encoder setFragmentBytes:&mvPush length:sizeof(mvPush) atIndex:1];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle
					vertexStart:param.first * 3
					vertexCount:param.count * 3];
		if (mv_mode == 1 || mv_mode == 2) {
			ModVolMode sumMode = mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion;
			auto sumPso = regularPipelineManager->GetModifierVolumePipeline(sumMode, param.isNaomi2());
			auto sumDs = regularPipelineManager->GetModVolDepthStencilStates(sumMode, param.isNaomi2());
			if (sumPso != nil) {
				[encoder setRenderPipelineState:sumPso];
				[encoder setDepthStencilState:sumDs];
				[encoder setCullMode:toMetalCullMode(param.isp.CullMode)];
				[encoder setStencilReferenceValue:1];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangle
							vertexStart:mod_base * 3
							vertexCount:(param.first + param.count - mod_base) * 3];
			}
			mod_base = -1;
		}
	}
	[encoder setVertexBufferOffset:currentBaseOffset atIndex:30];

	// Reset the tile clip before the screen-covering Final quad, exactly as
	// MetalDrawer::DrawModifierVolumes does (Vulkan calls SetTileClip(0) here).
	// The per-volume SetTileClip above leaves the last volume's rect on the
	// encoder, and without this the darkening quad is cropped to it — the
	// regular drawer already carried this fix; the OIT port did not.
	// Two-volume polys read the marks before the Final pass clears them.
	drawTwoVolumePolys(encoder, prev, cur, w, h);

	MTLScissorRect fullScissor {};
	SetTileClip(encoder, 0, fullScissor);
	if (mvPush.setTileClip(TileClipping::Off, fullScissor))
		[encoder setFragmentBytes:&mvPush length:sizeof(mvPush) atIndex:1];

	auto finalPso = regularPipelineManager->GetModifierVolumePipeline(ModVolMode::Final, false);
	auto finalDs = regularPipelineManager->GetModVolDepthStencilStates(ModVolMode::Final, false);
	if (finalPso != nil) {
		[encoder setRenderPipelineState:finalPso];
		[encoder setDepthStencilState:finalDs];
		[encoder setCullMode:toMetalCullMode(0)];
		[encoder setStencilReferenceValue:0x81];
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip
							indexCount:4
							 indexType:MTLIndexTypeUInt32
						   indexBuffer:curMainBuffer
					 indexBufferOffset:currentBaseOffset + offsets.indexOffset];
	}
}

void MetalOITDrawer::drawTwoVolumePolys(id<MTLRenderCommandEncoder> encoder, const RenderPass &prev,
										const RenderPass &cur, int w, int h)
{
	if (regularPipelineManager == nullptr || curMainBuffer == nil)
		return;
	bool any = false;
	for (u32 i = prev.tr_count; i < cur.tr_count && !any; ++i)
		any = isTwoVolumePoly(rendContext->global_param_tr[i]);
	for (u32 i = prev.op_count; i < cur.op_count && !any; ++i)
		any = isTwoVolumePoly(rendContext->global_param_op[i]);
	for (u32 i = prev.pt_count; i < cur.pt_count && !any; ++i)
		any = isTwoVolumePoly(rendContext->global_param_pt[i]);
	if (!any)
		return;
	[encoder pushDebugGroup:@"Two-volume polys"];
	// Translucent: both sets, each gated on the stencil, into the A-buffer (autosort)
	// or straight to the colour attachment (per-strip). Naomi2 keeps its own vertex
	// shader without the second-set selector, so it stays first-set only.
	const bool collect = cur.autosort;
	if (collect)
		bindOitCollectBuffers(encoder, w, h);
	for (u32 i = prev.tr_count; i < cur.tr_count; ++i) {
		const PolyParam &pp = rendContext->global_param_tr[i];
		if (pp.count <= 2 || !isTwoVolumePoly(pp))
			continue;
		DrawPoly(encoder, ListType_Translucent, cur.autosort, pp, pp.first, pp.count, collect, false, false, 2);
		if (!pp.isNaomi2())
			DrawPoly(encoder, ListType_Translucent, cur.autosort, pp, pp.first, pp.count, collect, false, false, 1);
	}
	// Opaque / punch-through: the first set is already on screen; repaint the second
	// set inside the marks (and clear those marks so the Final pass skips them).
	for (u32 i = prev.op_count; i < cur.op_count; ++i) {
		const PolyParam &pp = rendContext->global_param_op[i];
		if (pp.count > 2 && isTwoVolumePoly(pp) && !pp.isNaomi2())
			drawRegularPoly(encoder, ListType_Opaque, false, pp, pp.first, pp.count, 1);
	}
	for (u32 i = prev.pt_count; i < cur.pt_count; ++i) {
		const PolyParam &pp = rendContext->global_param_pt[i];
		if (pp.count > 2 && isTwoVolumePoly(pp) && !pp.isNaomi2())
			drawRegularPoly(encoder, ListType_Punch_Through, false, pp, pp.first, pp.count, 1);
	}
	[encoder popDebugGroup];
}

void MetalOITDrawer::drawTranslucentDepthOnly(id<MTLRenderCommandEncoder> encoder,
											  bool autosort, u32 first, u32 last)
{
	if (first == last)
		return;
	const PolyParam *pp_end = rendContext->global_param_tr.data() + last;
	for (const PolyParam *pp = &rendContext->global_param_tr[first]; pp != pp_end; ++pp) {
		if (pp->count <= 2)
			continue;
		DrawPoly(encoder, ListType_Translucent, autosort, *pp, pp->first, pp->count,
				 false, true);
	}
}

void MetalOITDrawer::MakeBuffers(int width, int height)
{
	if (width <= maxWidth && height <= maxHeight)
		return;
	id<MTLDevice> device = MetalContext::Instance()->GetDevice();
	if (device == nil) {
		WARN_LOG(RENDERER, "MetalOITDrawer::MakeBuffers: no Metal device");
		return;
	}

	// Two ping-pong color attachments — BGRA8Unorm matches the swapchain
	// format used by MetalContext for screen presentation, and matches
	// the colorAttachments[0].pixelFormat baked into the OIT pipeline
	// states in MetalOITPipelineManager (see metal_oit_pipeline.h).
	MTLTextureDescriptor *colorDesc =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
															width:width
														   height:height
														mipmapped:NO];
	colorDesc.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	colorDesc.storageMode = MTLStorageModePrivate;

	// Depth32Float_Stencil8 matches the pipeline's depth/stencil pixel
	// formats. Same descriptor for both ping-pong slots.
	MTLTextureDescriptor *depthDesc =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float_Stencil8
															width:width
														   height:height
														mipmapped:NO];
	depthDesc.usage       = MTLTextureUsageRenderTarget;
	depthDesc.storageMode = MTLStorageModePrivate;

	for (int i = 0; i < 2; ++i) {
		colorAttachments[i] = [device newTextureWithDescriptor:colorDesc];
		colorAttachments[i].label = [NSString stringWithFormat:@"OIT color %d", i];
		depthAttachments[i] = [device newTextureWithDescriptor:depthDesc];
		depthAttachments[i].label = [NSString stringWithFormat:@"OIT depth %d", i];
	}

	// Scratch background for the resolve pass. Same format/size as the color
	// attachments; it only ever receives a blit and is sampled by
	// oit_resolve_fragment, so ShaderRead is the only usage it needs.
	MTLTextureDescriptor *bgDesc =
		[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
															width:width
														   height:height
														mipmapped:NO];
	bgDesc.usage       = MTLTextureUsageShaderRead;
	bgDesc.storageMode = MTLStorageModePrivate;
	resolveBackground  = [device newTextureWithDescriptor:bgDesc];
	resolveBackground.label = @"OIT resolve background";

	maxWidth  = width;
	maxHeight = height;
	clearNeeded.assign(2, true);

	// Ensure the OIT per-pixel storage grew along with the framebuffer.
	if (oitBuffers != nullptr)
		oitBuffers->Init(width, height);
}

// ----- MetalOITDrawer: main buffer upload ---------------------------------

void MetalOITDrawer::UploadMainBuffer(const MetalVertexShaderUniforms &vertexUniforms,
									  const MetalFragmentShaderUniforms &fragmentUniforms)
{
	MetalBufferPacker packer;
	// Vertex data (same buffer layout the regular MetalDrawer uses;
	// bound at vertex buffer index 30 via the OIT collect pipeline's
	// vertex descriptor in metal_oit_pipeline.h).
	packer.add(rendContext->verts.data(),
			   rendContext->verts.size() * sizeof(decltype(*rendContext->verts.data())));
	offsets.modVolOffset = packer.add(rendContext->modtrig.data(),
									   rendContext->modtrig.size() * sizeof(decltype(*rendContext->modtrig.data())));
	offsets.indexOffset = packer.add(rendContext->idx.data(),
									  rendContext->idx.size() * sizeof(decltype(*rendContext->idx.data())));
	offsets.vertexUniformOffset   = packer.addUniform(&vertexUniforms,   sizeof(vertexUniforms));
	offsets.fragmentUniformOffset = packer.addUniform(&fragmentUniforms, sizeof(fragmentUniforms));

	std::vector<u32> trPolyParams;
	if (rendContext->global_param_tr.empty())
		trPolyParams.push_back(0);
	else {
		trPolyParams.resize(rendContext->global_param_tr.size() * 2);
		size_t i = 0;
		for (const PolyParam &pp : rendContext->global_param_tr) {
			trPolyParams[i++] = (pp.tsp.full & 0xffff00c0) | ((pp.isp.full >> 16) & 0xe400) | ((pp.pcw.full >> 7) & 1);
			trPolyParams[i++] = pp.tsp1.full;
		}
	}
	offsets.polyParamsSize = trPolyParams.size() * sizeof(u32);
	offsets.polyParamsOffset = packer.addStorage(trPolyParams.data(), offsets.polyParamsSize);

	// Naomi2 per-poly vertex uniforms + lights.
	// specialises the vertex shader on pp.isNaomi2() (metal_oit_pipeline.h:
	// 210/233) exactly like the regular MetalPipelineManager, so the
	// naomi2 vertex shader expects MetalN2VertexShaderUniforms at vertex
	// buffer index 1 and N2LightModel at index 2. Pack them here (mirrors
	// MetalDrawer::UploadMainBuffer metal_drawer.mm:466-471); DrawPoly
	// binds them per-poly. Without this, Naomi2 translucent geometry under
	// OIT renders with stale/default model matrices.
	//
	// trModVolIncluded=true: unlike the regular drawer, the OIT path runs
	// TR modvols through this drawer (DrawModifierVolumes uses
	// rendContext->global_param_mvo_tr) so the packer must reserve their N2 slots
	// for naomi2TrModVolOffset to be valid.
	std::vector<u8> n2uniforms;
	if (settings.platform.isNaomi2())
	{
		packNaomi2Uniforms(packer, offsets, n2uniforms, /*trModVolIncluded*/ true);
		offsets.lightsOffset = packNaomi2Lights(packer);
	}

	u64 totalSize = packer.size();
	if (!ring)
		ring.reset(new MetalRingBuffer(std::max<u64>(512 * 1024, totalSize), 3));
	else if (totalSize > ring->getSliceSize())
		ring.reset(new MetalRingBuffer(totalSize, 3));

	currentBaseOffset = (u32)ring->baseOffset();
	packer.uploadToBuffer(ring->getBuffer(), currentBaseOffset);
	curMainBuffer = ring->getBuffer();
}

// ----- MetalOITDrawer: per-poly orchestration -----------------------------
//
// These are real implementations, not stubs -- per-poly texture/sampler
// binding, Naomi2 uniforms and the polyNumber constant are all wired, and
// UploadMainBuffer above packs the vertex/index/uniform/tr_poly_params data
// they index into.

void MetalOITDrawer::DrawModifierVolumes(__strong id<MTLRenderCommandEncoder> &encoder,
										 MTLRenderPassDescriptor *passDesc,
										 const std::function<void(id<MTLRenderCommandEncoder>)> &setupEncoder,
										 int first, int count, bool useSharedMvo)
{
	if (count == 0 || rendContext->modtrig.empty() || !config::ModifierVolumes)
		return;
	if (pipelineManager == nullptr || curMainBuffer == nil || oitBuffers == nullptr)
		return;
	[encoder pushDebugGroup:@"OIT TR ModVols"];

	MetalOITResolveUniformsCPU modVolUniforms = {
		(uint32_t)viewport.width,
		(uint32_t)viewport.height,
		(uint32_t)([oitBuffers->getPixelBuffer() length] / 16u),
		0.f,
		{ 0.f, 0.f, 0.f, 1.f },
		0u,
	};
	// Per-volume tile clip for oit_tr_modvol_fragment (Inside mode = shader skip).
	MetalModVolPushBlock trClip;
	auto bindModVolState = [&]() {
		[encoder setVertexBufferOffset:currentBaseOffset + offsets.modVolOffset atIndex:30];
		id<MTLDepthStencilState> trModVolDs = pipelineManager->GetTrModVolDepthStencilState();
		if (trModVolDs != nil)
			[encoder setDepthStencilState:trModVolDs];
		[encoder setFragmentBuffer:oitBuffers->getPixelBuffer()    offset:0 atIndex:1];
		[encoder setFragmentBuffer:oitBuffers->getAbufferPointer() offset:0 atIndex:2];
		[encoder setFragmentBytes:&modVolUniforms length:sizeof(modVolUniforms) atIndex:3];
		[encoder setFragmentBytes:&trClip length:sizeof(trClip) atIndex:5];
		[encoder setFragmentBuffer:curMainBuffer
							  offset:currentBaseOffset + offsets.polyParamsOffset
							 atIndex:4];
	};
	bindModVolState();

	// Order a volume's marks before its sum, and the sum before the next
	// volume and the resolve: Vulkan's two by-region fragment->fragment
	// barriers. See MetalOITFragmentOrdering.
	const MetalOITFragmentOrdering ordering = pipelineManager->FragmentOrdering();
	auto orderFragments = [&](u32 tileclip) {
		switch (ordering) {
		case MetalOITFragmentOrdering::RasterOrderGroups:
			break;	// the shaders' a-buffer accesses are in raster order group 0
		case MetalOITFragmentOrdering::MemoryBarrier:
#if TARGET_OS_OSX
			[encoder memoryBarrierWithScope:MTLBarrierScopeBuffers
								afterStages:MTLRenderStageFragment
							   beforeStages:MTLRenderStageFragment];
#endif
			break;
		case MetalOITFragmentOrdering::EncoderSplit:
		{
			[encoder popDebugGroup];
			[encoder popDebugGroup];
			[encoder endEncoding];
			encoder = [currentCommandBuffer renderCommandEncoderWithDescriptor:passDesc];
			[encoder pushDebugGroup:@"OIT Resolve Pass"];
			setupEncoder(encoder);
			[encoder pushDebugGroup:@"OIT TR ModVols"];
			bindModVolState();
			// A new encoder starts with a full-target scissor: drop the cached
			// one so SetTileClip sets the volume's rect again.
			currentScissor = MTLScissorRect {};
			MTLScissorRect scissorRect {};
			SetTileClip(encoder, tileclip, scissorRect);
			break;
		}
		}
	};

	ModifierVolumeParam *params = useSharedMvo
								  ? &rendContext->global_param_mvo[first]
								  : &rendContext->global_param_mvo_tr[first];
	int mod_base = -1;
	for (int cmv = 0; cmv < count; ++cmv) {
		ModifierVolumeParam &param = params[cmv];
		if (param.count == 0)
			continue;
		const u32 mv_mode = param.isp.DepthMode;
		if (mod_base == -1)
			mod_base = param.first;

		ModVolMode pipeMode = (!param.isp.VolumeLast && mv_mode > 0)
							  ? ModVolMode::Or : ModVolMode::Xor;
		auto pso = pipelineManager->GetTrModVolPipeline(pipeMode,
														param.isp.CullMode,
														param.isNaomi2());
		if (pso == nil)
			continue;
		[encoder setRenderPipelineState:pso];
		[encoder setCullMode:toMetalCullMode(param.isp.CullMode)];
		MTLScissorRect scissorRect {};
		TileClipping clipMode = SetTileClip(encoder, param.tileclip, scissorRect);
		if (trClip.setTileClip(clipMode, scissorRect))
			[encoder setFragmentBytes:&trClip length:sizeof(trClip) atIndex:5];
		[encoder drawPrimitives:MTLPrimitiveTypeTriangle
					vertexStart:param.first * 3
					vertexCount:param.count * 3];

		if (mv_mode == 1 || mv_mode == 2) {
			orderFragments(param.tileclip);

			ModVolMode sumMode = (mv_mode == 1) ? ModVolMode::Inclusion
												: ModVolMode::Exclusion;
			auto sumPso = pipelineManager->GetTrModVolPipeline(sumMode,
																param.isp.CullMode,
																param.isNaomi2());
			if (sumPso != nil) {
				[encoder setRenderPipelineState:sumPso];
				[encoder setCullMode:toMetalCullMode(param.isp.CullMode)];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangle
							vertexStart:mod_base * 3
							vertexCount:(param.first + param.count - mod_base) * 3];
			}
			mod_base = -1;

			orderFragments(param.tileclip);
		}
	}

	[encoder setVertexBufferOffset:currentBaseOffset atIndex:30];
	[encoder popDebugGroup];
}

void MetalOITDrawer::DrawSorted(id<MTLRenderCommandEncoder> encoder,
								const std::vector<SortedTriangle>& polys,
								u32 first, u32 last)
{
	if (first == last)
		return;
	[encoder pushDebugGroup:@"OIT DrawSorted"];
	for (u32 idx = first; idx < last; ++idx) {
		const SortedTriangle &t = polys[idx];
		DrawPoly(encoder, ListType_Translucent, /*sortTriangles*/ true,
				 rendContext->global_param_tr[t.polyIndex], t.first, t.count,
				 /*oitCollect*/ true, /*depthOnly*/ false, /*triangleList*/ true);
	}
	[encoder popDebugGroup];
}

void MetalOITDrawer::DrawList(id<MTLRenderCommandEncoder> encoder, u32 listType,
							  bool sortTriangles, const std::vector<PolyParam>& polys,
							  u32 first, u32 last, bool oitCollect)
{
	if (first == last)
		return;
	[encoder pushDebugGroup:@"OIT DrawList"];
	const PolyParam *pp_end = polys.data() + last;
	for (const PolyParam *pp = &polys[first]; pp != pp_end; pp++) {
		// Two-volume translucent polys were drawn by drawTwoVolumePolys.
		if (pp->count > 2 && !(listType == ListType_Translucent && isTwoVolumePoly(*pp)))
			DrawPoly(encoder, listType, sortTriangles, *pp, pp->first, pp->count, oitCollect);
	}
	[encoder popDebugGroup];
}

void MetalOITDrawer::DrawPoly(id<MTLRenderCommandEncoder> encoder, u32 listType,
							  bool sortTriangles, const PolyParam& polyIn,
							  u32 first, u32 count, bool oitCollect, bool depthOnly,
							  bool triangleList, int area)
{
	const PolyParam polyArea1 = area == 1 ? area1Params(polyIn) : PolyParam();
	const PolyParam &poly = area == 1 ? polyArea1 : polyIn;
	if (pipelineManager == nullptr)
		return;
	if (depthOnly) {
		if (regularPipelineManager == nullptr)
			return;
	} else if (oitCollect) {
		// collect path
	} else if (regularPipelineManager == nullptr) {
		return;
	}

	// ---- 1. Scissor / tile clip ----
	MTLScissorRect scissorRect {};
	TileClipping tileClip = SetTileClip(encoder, poly.tileclip, scissorRect);

	// ---- 2. Trilinear alpha + GPU palette ----
	float trilinearAlpha = 1.0f;
	if (poly.tsp.FilterMode > 1 && poly.pcw.Texture
		&& listType != ListType_Punch_Through && poly.tcw.MipMapped == 1) {
		trilinearAlpha = 0.25f * (poly.tsp.MipMapD & 0x3);
		if (poly.tsp.FilterMode == 2)
			trilinearAlpha = 1.0f - trilinearAlpha;
	}
	int gpuPalette = poly.texture == nullptr || !((MetalTexture *)poly.texture)->gpuPalette
					 ? 0 : poly.tsp.FilterMode + 1;
	float palette_index = 0.0f;
	if (gpuPalette != 0) {
		if (config::TextureFiltering == 1) gpuPalette = 1;
		else if (config::TextureFiltering == 2) gpuPalette = 2;
		if (poly.tcw.PixelFmt == PixelPal4)
			palette_index = float(poly.tcw.PalSelect << 4) / 1023.0f;
		else
			palette_index = float(poly.tcw.PalSelect >> 4 << 8) / 1023.0f;
	}

	// ---- 3. PushBlock at fragment buffer 1 ----
	std::array<float, 8> pushConstants {};
	if (tileClip == TileClipping::Inside || trilinearAlpha != 1.0f || gpuPalette != 0) {
		pushConstants = {
			(float)scissorRect.x,
			(float)scissorRect.y,
			(float)scissorRect.x + (float)scissorRect.width,
			(float)scissorRect.y + (float)scissorRect.height,
			trilinearAlpha,
			palette_index,
		};
	} else {
		pushConstants = { 0, 0, 0, 0, 0, 0 };
	}
	[encoder setFragmentBytes:pushConstants.data()
					   length:sizeof(pushConstants) + MetalBufferPacker::align(sizeof(pushConstants), 16)
					  atIndex:1];

	// ---- 4. Pipeline state ----

	id<MTLRenderPipelineState> pso = nil;
	if (depthOnly) {
		pso = pipelineManager->GetTranslucentDepthPipeline(listType, sortTriangles,
														   poly, gpuPalette, false);
	} else if (oitCollect) {
		pso = pipelineManager->GetCollectPipeline(listType, sortTriangles,
												  poly, gpuPalette, false, area);
	} else {
		pso = regularPipelineManager->GetPipeline(listType, sortTriangles,
												  poly, gpuPalette, false, false, area == 1);
	}
	if (pso == nil)
		return;
	[encoder setRenderPipelineState:pso];
	[encoder setCullMode:toMetalCullMode(poly.isp.CullMode)];

	if (oitCollect && !depthOnly && area != 0) {
		// fs_main_early: stencil area test before the a-buffer append.
		id<MTLDepthStencilState> ds = pipelineManager->GetTwoVolumeCollectDepthStencilState(area);
		if (ds == nil)
			return;
		[encoder setDepthStencilState:ds];
		[encoder setStencilReferenceValue:0x81];
	} else if (regularPipelineManager != nullptr) {
		[encoder setDepthStencilState:regularPipelineManager->GetDepthStencilStates(
			listType, sortTriangles, false, poly, false, area)];
		if (area != 0)
			[encoder setStencilReferenceValue:0x81];
	}


	// ---- 5. Texture + sampler binding ----
	if (poly.texture != nullptr && samplers != nullptr) {
		auto texture = ((MetalTexture *)poly.texture)->GetReadOnlyTexture();
		[encoder setFragmentTexture:texture atIndex:0];
		id<MTLSamplerState> samplerState =
			samplers->GetSampler(poly, listType == ListType_Punch_Through);
		if (samplerState != nil)
			[encoder setFragmentSamplerState:samplerState atIndex:0];
	}

	// ---- 5b. Naomi2 per-poly vertex uniforms + lights ----
	//
	// Mirrors MetalDrawer::DrawPoly (metal_drawer.mm:241-303). The collect
	// pipeline's naomi2-specialised vertex shader reads the per-poly model
	// matrices from vertex buffer index 1 and the light model from index 2.
	// Same guard condition (poly.pcw.Texture || poly.isNaomi2()) keeps this
	// harmless for textured non-naomi2 polys. Only ListType_Translucent
	// flows through the OIT collect DrawPoly, but the switch is kept
	// complete for parity / future RTT-OIT reuse. Bounds checks ported
	// verbatim — an out-of-range offset returns early rather than binding
	// garbage (matches the regular drawer's defensive posture).
	if (poly.pcw.Texture || poly.isNaomi2())
	{
		u64 offset = 0;
		u32 index = 0;
		if (poly.isNaomi2())
		{
			switch (listType)
			{
				case ListType_Opaque:
					offset = offsets.naomi2OpaqueOffset;
					index = (u32)(&polyIn - &rendContext->global_param_op[0]);
					if (index >= rendContext->global_param_op.size())
						return;
					break;
				case ListType_Punch_Through:
					offset = offsets.naomi2PunchThroughOffset;
					index = (u32)(&polyIn - &rendContext->global_param_pt[0]);
					if (index >= rendContext->global_param_pt.size())
						return;
					break;
				case ListType_Translucent:
					offset = offsets.naomi2TranslucentOffset;
					index = (u32)(&polyIn - &rendContext->global_param_tr[0]);
					if (index >= rendContext->global_param_tr.size())
						return;
					break;
				default:
					break;
			}
		}

		size_t size = sizeof(MetalN2VertexShaderUniforms)
					  + MetalBufferPacker::align(sizeof(MetalN2VertexShaderUniforms), 16);
		u64 finalOffset = currentBaseOffset + offset + index * size;
		if (finalOffset >= [curMainBuffer length])
			return;
		[encoder setVertexBuffer:curMainBuffer offset:finalOffset atIndex:1];

		if (offsets.lightsOffset != (u64)-1)
		{
			size = sizeof(N2LightModel) + MetalBufferPacker::align(sizeof(N2LightModel), 16);
			if (poly.lightModel >= 256)
				return;
			u64 lightsOffset = currentBaseOffset + offsets.lightsOffset + poly.lightModel * size;
			if (lightsOffset >= [curMainBuffer length])
				return;
			[encoder setVertexBuffer:curMainBuffer offset:lightsOffset atIndex:2];
		}
	}

	// ---- 6. Draw ----
	//
	// curMainBuffer is populated by MetalOITDrawer::UploadMainBuffer, which
	// Draw() calls before the collect pass. This guard is a safety net for the
	// pre-upload window, not the dormant state an older comment here claimed:
	// it stopped being nullptr when UploadMainBuffer landed.
	if (curMainBuffer == nil)
		return;

	// polyNumber MUST be bound after the Naomi2 block above, not before it.
	// Both want vertex buffer index 1, and that block rebinds index 1 whenever
	// `poly.pcw.Texture || poly.isNaomi2()` — so for a textured non-Naomi2
	// translucent poly (most of the Dreamcast BIOS menu) it clobbered the
	// constant with the packed vertex data. The collect vertex shader then
	// computed vtx_index from garbage, and the resolve's
	// getPolyNumber(pixel) indexed the wrong entry in tr_poly_params, giving
	// correct geometry with wrong/black shading. Vulkan has no such conflict:
	// polyNumber is a push constant, not a bound buffer (oit_drawer.cpp:92).
	if (oitCollect && listType == ListType_Translucent && !poly.isNaomi2()) {
		int polyNumber = 0;
		if (!rendContext->global_param_tr.empty()) {
			const u32 firstVertexIdx = rendContext->idx[rendContext->global_param_tr[0].first];
			// polyIn, not poly: for a two-volume area-1 draw `poly` is a stack
			// copy (area1Params), and its address gave the resolve a garbage
			// tr_poly_params index for every second-set fragment.
			polyNumber = (int)((&polyIn - &rendContext->global_param_tr[0]) << 17) - (int)firstVertexIdx;
		}
		[encoder setVertexBytes:&polyNumber length:sizeof(polyNumber) atIndex:1];
	}

	// Strips unless the caller is walking rendContext->sortedTriangles, whose indices
	// are an unstripped triangle list. The old rule keyed off
	// `sortTriangles && !PerStripSorting`, but every OIT translucent draw sets
	// sortTriangles=true with PerStripSorting off while indexing the strip data
	// makeIndex() built for global_param_tr — so it told Metal to read strip
	// indices as independent triangles.
	MTLPrimitiveType primitive = triangleList ? MTLPrimitiveTypeTriangle
											  : MTLPrimitiveTypeTriangleStrip;
	[encoder drawIndexedPrimitives:primitive
						indexCount:count
						 indexType:MTLIndexTypeUInt32
					   indexBuffer:curMainBuffer
				 indexBufferOffset:currentBaseOffset + offsets.indexOffset + first * sizeof(u32)];
}

// ----- MetalOITScreenDrawer -----------------------------------------------

id<MTLCommandBuffer> MetalOITScreenDrawer::NewFrame()
{
	if (frameStarted)
		return currentCommandBuffer;
	if (commandPool == nullptr) {
		WARN_LOG(RENDERER, "MetalOITScreenDrawer::NewFrame: no command pool");
		return nil;
	}
	// The regular Metal drawer lazily allocates the command
	// buffer when the first draw call lands in BeginRenderPass. For OIT
	// the lifecycle is inverted: Draw() owns the command buffer end-to-
	// end (allocate -> blit -> two encoders -> hand back), so NewFrame
	// is just the "ready for a frame" toggle.
	frameStarted  = true;
	frameRendered = false;
	return currentCommandBuffer;
}

void MetalOITScreenDrawer::EndFrame()
{
	if (!frameStarted)
		return;
	frameStarted = false;
	if (currentCommandBuffer != nil) {
		// Hand the command buffer to the pool — it commits it as part of
		// EndFrame and rotates the ring index. Mirrors what MetalScreen
		// Drawer::EndRenderPass does after [encoder endEncoding].
		commandPool->EndFrame();
		aspectRatio = getOutputFramebufferAspectRatio();
		frameRendered = true;
	}
	currentCommandBuffer = nil;
	// Swap ping-pong so the next frame writes to the other color slot.
	framebufferIndex = (framebufferIndex + 1) & 1;
}

MTLRenderPassDescriptor *MetalOITScreenDrawer::getOitRenderPassDescriptor(
	int renderPassIndex, int renderPassCount, bool initialPass)
{
	(void)renderPassIndex;
	(void)renderPassCount;
	// Every pass of the frame targets the SAME attachment (non-initial passes
	// load it), and the resolve reads/writes it in place via framebuffer
	// fetch. The old per-pass toggle here, combined with EndFrame's per-frame
	// toggle, made single-pass frames render into the same slot every frame —
	// so the sibling the resolve sampled as "background" was never written.
	const int idx = framebufferIndex;
	MTLRenderPassDescriptor *desc = tempPassDescriptors[idx];
	if (desc == nil) {
		desc = [MTLRenderPassDescriptor renderPassDescriptor];
		tempPassDescriptors[idx] = desc;
	}
	if (colorAttachments[idx] == nil || depthAttachments[idx] == nil)
		return nil;

	desc.colorAttachments[0].texture     = colorAttachments[idx];
	desc.colorAttachments[0].loadAction  = initialPass ? MTLLoadActionClear : MTLLoadActionLoad;
	desc.colorAttachments[0].storeAction = MTLStoreActionStore;
	desc.colorAttachments[0].clearColor  = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

	desc.depthAttachment.texture         = depthAttachments[idx];
	desc.depthAttachment.loadAction      = initialPass ? MTLLoadActionClear : MTLLoadActionLoad;
	desc.depthAttachment.storeAction     = MTLStoreActionStore;
	// Reversed log depth: 0.0 = farthest. Vulkan OIT clears to 0
	// (oit_drawer.cpp ClearDepthStencilValue{0,0}); 1.0 (= nearest) makes
	// every Greater/GEqual test fail on pixels the background doesn't cover.
	desc.depthAttachment.clearDepth      = 0.0;

	desc.stencilAttachment.texture       = depthAttachments[idx];
	desc.stencilAttachment.loadAction    = initialPass ? MTLLoadActionClear : MTLLoadActionLoad;
	desc.stencilAttachment.storeAction   = MTLStoreActionStore;
	desc.stencilAttachment.clearStencil  = 0;

	clearNeeded[idx] = false;
	return desc;
}

bool MetalOITScreenDrawer::PresentFrame()
{
	EndFrame();
	if (!frameRendered)
		return false;
	frameRendered = false;
	// After EndFrame() the framebufferIndex has already advanced; present
	// the slot we just finished rendering to, which is the PREVIOUS index.
	int presentIdx = (framebufferIndex + 1) & 1;
	if (colorAttachments[presentIdx] == nil)
		return false;
	MetalContext::Instance()->PresentFrame(colorAttachments[presentIdx], viewport, aspectRatio);
	return true;
}

// ----- MetalOITTextureDrawer ----------------------------------------------

id<MTLCommandBuffer> MetalOITTextureDrawer::NewFrame()
{
	DEBUG_LOG(RENDERER, "OIT RTT packmode=%d stride=%d - %d x %d @ %06x",
			 rendContext->fb_W_CTRL.fb_packmode, rendContext->fb_W_LINESTRIDE * 8,
			 rendContext->fbClip.size.x, rendContext->fbClip.size.y, rendContext->fb_W_SOF1 & VRAM_MASK);

	textureAddr = rendContext->fb_W_SOF1 & VRAM_MASK;
	// Since 31d5ddb92: render target = framebufferWidth/Height computed in
	// Renderer_if (see MetalTextureDrawer::BeginRenderPass). Mirrors Vulkan
	// OITTextureDrawer::NewFrame.
	u32 width = rendContext->framebufferWidth;
	u32 height = rendContext->framebufferHeight;

	if (width == 0 || height == 0 || width > 16384 || height > 16384) {
		ERROR_LOG(RENDERER, "OIT RTT invalid dimensions: %ux%u", width, height);
		width = std::max(8u, std::min(width, 1024u));
		height = std::max(8u, std::min(height, 1024u));
	}
	matrices.CalcMatrices(rendContext, width, height);
	// Texture-cache key: the unscaled pow2 PVR framebuffer size.
	int wpo2, hpo2;
	getPvrFramebufferSize(*rendContext, wpo2, hpo2);

	id<MTLDevice> device = MetalContext::Instance()->GetDevice();
	if (device == nil)
		return nil;

	if (commandPool != nullptr && currentCommandBuffer == nil)
		currentCommandBuffer = commandPool->Allocate();

	if (!rttDepthAttachment || width > rttDepthAttachment.width
		|| height > rttDepthAttachment.height)
	{
		MTLTextureDescriptor *depthDescriptor = [[MTLTextureDescriptor alloc] init];
		depthDescriptor.width = width;
		depthDescriptor.height = height;
		depthDescriptor.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
		depthDescriptor.usage = MTLTextureUsageRenderTarget;
		depthDescriptor.storageMode = MTLStorageModePrivate;
		rttDepthAttachment = [device newTextureWithDescriptor:depthDescriptor];
		[rttDepthAttachment setLabel:@"OIT RTT Depth"];
	}

	if (config::RenderToTextureBuffer)
	{
		texture = textureCache->getRTTexture(textureAddr, rendContext->fb_W_CTRL.fb_packmode,
											 wpo2, hpo2);
		if (textureCache->IsInFlight(texture, false)) {
			texture->CreateReadOnlyCopy(currentCommandBuffer);
			texture->deferDeleteResource(commandPool);
		}
		textureCache->SetInFlight(texture);

		const bool needsRecreation = texture->GetTexture() == nil
			|| texture->GetTexture().width != width
			|| texture->GetTexture().height != height
			|| !(texture->GetTexture().usage & MTLTextureUsageRenderTarget);
		if (needsRecreation) {
			MTLTextureDescriptor *colorDescriptor = [[MTLTextureDescriptor alloc] init];
			colorDescriptor.width = width;
			colorDescriptor.height = height;
			colorDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
			colorDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			colorDescriptor.storageMode = MTLStorageModeShared;
			id<MTLTexture> newTexture = [device newTextureWithDescriptor:colorDescriptor];
			[newTexture setLabel:@"OIT RTT Color"];
			texture->SetTexture(newTexture, width, height);
		}
		rttColorImage = texture->GetTexture();
	}
	else
	{
		texture = textureCache->getRTTexture(textureAddr, rendContext->fb_W_CTRL.fb_packmode,
											 wpo2, hpo2);
		if (textureCache->IsInFlight(texture, false)) {
			texture->CreateReadOnlyCopy(currentCommandBuffer);
			texture->deferDeleteResource(commandPool);
		}
		textureCache->SetInFlight(texture);

		// Default (no VRAM readback): render directly into the texture-cache
		// texture the game will sample, like Vulkan's OIT RTT. This branch
		// used to fetch the cache texture, mark it in-flight — and then
		// render into a private scratch attachment nothing ever read, so the
		// decal sampled the texture object's stale contents (THPS2 blob
		// shadow showed the font atlas).
		const bool needsRecreation = texture->GetTexture() == nil
			|| texture->GetTexture().width != width
			|| texture->GetTexture().height != height
			|| !(texture->GetTexture().usage & MTLTextureUsageRenderTarget);
		if (needsRecreation) {
			MTLTextureDescriptor *colorDescriptor = [[MTLTextureDescriptor alloc] init];
			colorDescriptor.width = width;
			colorDescriptor.height = height;
			colorDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
			colorDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			colorDescriptor.storageMode = MTLStorageModeShared;
			id<MTLTexture> newTexture = [device newTextureWithDescriptor:colorDescriptor];
			[newTexture setLabel:@"OIT RTT Color"];
			texture->SetTexture(newTexture, width, height);
		}
		rttColorImage = texture->GetTexture();
	}

	viewport.originX = 0.0;
	viewport.originY = 0.0;
	viewport.width = (double)width;
	viewport.height = (double)height;
	viewport.znear = 0.0;
	viewport.zfar = 1.0;

	MakeBuffers((int)width, (int)height);

	// Base scissor from TransformMatrix::getBaseScissor(), clamped to the target.
	SetBaseScissor(viewport);

	return currentCommandBuffer;
}

void MetalOITTextureDrawer::EndFrame()
{
	if (currentCommandBuffer == nil)
		return;

	// Whole (scaler-adjusted) framebuffer; WriteTextureToVRam applies fbClip.
	u32 fbw = rendContext->framebufferWidth;
	u32 fbh = rendContext->framebufferHeight;

	if (config::RenderToTextureBuffer)
	{
		commandPool->EndFrameAndWait();

		id<MTLTexture> srcTexture = texture ? texture->GetTexture() : rttColorImage;
		if (srcTexture != nil) {
			fbw = std::min<u32>(fbw, (u32)srcTexture.width);
			fbh = std::min<u32>(fbh, (u32)srcTexture.height);
			rttReadback.init(fbw, fbh);
			[srcTexture getBytes:rttReadback.data()
					 bytesPerRow:fbw * 4
					  fromRegion:MTLRegionMake2D(0, 0, fbw, fbh)
					 mipmapLevel:0];
			u16 *dst = (u16 *)&vram[textureAddr];
			// Metal render targets are BGRA8; WriteTextureToVRam packs from RGBA byte order
			// (what the Vulkan/GL readbacks hand it). Without the swap red and blue trade
			// places in VRAM — a tint for ordinary RTT, but fatal for games that re-read the
			// 565 target as 4444, where the alpha nibble IS red's top bits (THPS2's skater
			// shadow: cyan background 08ffff -> 0x0FFF -> alpha 0; swapped -> 0xFFE1 ->
			// opaque yellow, the whole floor darkened and no blob).
			{
				u8 *px = (u8 *)rttReadback.data();
				for (u32 i = 0, n = fbw * fbh; i < n; i++, px += 4)
					std::swap(px[0], px[2]);
			}
			WriteTextureToVRam(fbw, fbh, (u8 *)rttReadback.data(), dst,
							   rendContext->fb_W_CTRL, rendContext->fb_W_LINESTRIDE * 8, rendContext->fbClip);
		}
	}
	else
	{
		commandPool->EndFrame();
		if (texture != nullptr) {
			texture->dirty = 0;
			texture->unprotectVRam();
		}
	}

	currentCommandBuffer = nil;
	rttColorImage = nil;
	framebufferIndex = (framebufferIndex + 1) & 1;
}

MTLRenderPassDescriptor *MetalOITTextureDrawer::getOitRenderPassDescriptor(
	int renderPassIndex, int renderPassCount, bool initialPass)
{
	if (rttColorImage == nil)
		return nil;

	// Every pass renders straight into the RTT target; non-initial passes
	// load the previous pass's composite, and the resolve reads the
	// attachment in place via framebuffer fetch. The old scheme bounced
	// intermediate passes between ping-pong attachments so the resolve could
	// sample a "background" — but for the common single-pass RTT frame that
	// sibling was never written, so any TR-covered pixel composited over
	// black.
	(void)renderPassIndex;
	(void)renderPassCount;
	framebufferIndex = 0;
	MTLRenderPassDescriptor *desc = rttPassDescriptor;
	desc.colorAttachments[0].texture = rttColorImage;
	desc.colorAttachments[0].loadAction = initialPass ? MTLLoadActionClear : MTLLoadActionLoad;
	desc.colorAttachments[0].storeAction = MTLStoreActionStore;
	desc.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, 1.0);

	desc.depthAttachment.texture = rttDepthAttachment;
	desc.depthAttachment.loadAction = initialPass ? MTLLoadActionClear : MTLLoadActionLoad;
	desc.depthAttachment.storeAction = MTLStoreActionStore;
	desc.depthAttachment.clearDepth = 0.0;

	desc.stencilAttachment.texture = rttDepthAttachment;
	desc.stencilAttachment.loadAction = initialPass ? MTLLoadActionClear : MTLLoadActionLoad;
	desc.stencilAttachment.storeAction = MTLStoreActionStore;
	desc.stencilAttachment.clearStencil = 0;
	return desc;
}
