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

#include "metal_drawer.h"
#include "rend/TexCache.h"
#include "hw/pvr/pvr_mem.h"
#include <cmath>
#include <cstring>
#include <mutex>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <TargetConditionals.h>

TileClipping MetalBaseDrawer::SetTileClip(id<MTLRenderCommandEncoder> encoder, u32 val, MTLScissorRect& clipRect) {
	// Since 31d5ddb92: tile clipping lives in TransformMatrix now
	// (rend/tileclip.h is gone). Mirrors Vulkan BaseDrawer::SetTileClip.
	Rect rect;
	TileClipping clipMode = matrices.getTileClip(val, rect);
	if (clipMode != TileClipping::Off)
	{
		clipRect.x = std::max(rect.origin.x, 0);
		clipRect.y = std::max(rect.origin.y, 0);
		clipRect.width = std::max(rect.size.x, 0);
		clipRect.height = std::max(rect.size.y, 0);
	}
	if (clipMode == TileClipping::Outside)
		SetScissor(encoder, clipRect);
	else
		SetScissor(encoder, baseScissor);

	return clipMode;
}

void MetalBaseDrawer::SetBaseScissor(MTLViewport viewport) {
	// Since 31d5ddb92 folded the widescreen / EmulateFramebuffer /
	// RenderToTextureBuffer / tile-vs-fb clip decisions into
	// TransformMatrix::getBaseScissor(), shared by every renderer; this is the
	// Vulkan BaseDrawer::SetBaseScissor body plus the clamp to the render
	// target that Metal validation requires (a scissor rect outside the
	// attachment is a validation abort, and the hardware scaler can push the
	// logical rect past the physical target, e.g. 2x vertical 480->960).
	const Rect scissor = matrices.getBaseScissor();
	const NSUInteger maxWidth = (NSUInteger)std::max(viewport.width, 0.0);
	const NSUInteger maxHeight = (NSUInteger)std::max(viewport.height, 0.0);

	baseScissor = MTLScissorRect();
	baseScissor.x = std::min((NSUInteger)std::max(scissor.origin.x, 0), maxWidth);
	baseScissor.y = std::min((NSUInteger)std::max(scissor.origin.y, 0), maxHeight);
	baseScissor.width = std::min((NSUInteger)std::max(scissor.size.x, 0), maxWidth - baseScissor.x);
	baseScissor.height = std::min((NSUInteger)std::max(scissor.size.y, 0), maxHeight - baseScissor.y);
	// Metal requires non-zero dimensions, and the rect must stay inside the target
	baseScissor.width = std::max(baseScissor.width, (NSUInteger)1);
	baseScissor.height = std::max(baseScissor.height, (NSUInteger)1);
	if (maxWidth > 0 && baseScissor.x + baseScissor.width > maxWidth)
		baseScissor.x = maxWidth - baseScissor.width;
	if (maxHeight > 0 && baseScissor.y + baseScissor.height > maxHeight)
		baseScissor.y = maxHeight - baseScissor.height;

	DEBUG_LOG(RENDERER, "SetBaseScissor: viewport %.0fx%.0f, base (%d,%d,%d,%d) -> clamped (%d,%d,%d,%d) vscale=%d hscale=%d",
			 viewport.width, viewport.height,
			 scissor.origin.x, scissor.origin.y, scissor.size.x, scissor.size.y,
			 (int)baseScissor.x, (int)baseScissor.y, (int)baseScissor.width, (int)baseScissor.height,
			 rendContext->scaler_ctl.vscalefactor, rendContext->scaler_ctl.hscale);
}

// Issue `setter` only when the encoder was not already given this exact value.
#define MTL_SET_IF_CHANGED(have, cached, value, setter)                        \
	do {                                                                       \
		if (!(have) || (cached) != (value)) {                                  \
			setter;                                                            \
			(cached) = (value);                                                \
			(have) = true;                                                     \
		}                                                                      \
	} while (0)

// A draw whose index range runs past the idx array UploadMainBuffer packed
// reads whatever follows it in the ring slice (the vertex/fragment uniforms) as
// indices (seen with Unreal Tournament). Without Metal validation that is an
// out-of-bounds GPU read: garbage geometry or a GPU fault. The cause is not
// known yet; this skips the draw and logs which site. Rate-limited: a bad frame
// can hit it for hundreds of polys.
bool MetalDrawer::indexRangeValid(u64 first, u64 count, const char *site) const
{
	if (first + count <= offsets.indexCount)
		return true;
	static std::atomic<u64> s_lastLogUs{0};
	static std::atomic<u32> s_suppressed{0};
	const u64 nowUs = (u64)std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
	u64 last = s_lastLogUs.load(std::memory_order_relaxed);
	if (nowUs - last >= 2'000'000 && s_lastLogUs.compare_exchange_strong(last, nowUs))
	{
		WARN_LOG(RENDERER, "Metal: skipped out-of-range draw at %s: first=%llu count=%llu idx=%llu "
				 "(op=%zu pt=%zu tr=%zu sorted=%zu modvol=%zu, %u more suppressed)",
				 site, (unsigned long long)first, (unsigned long long)count,
				 (unsigned long long)offsets.indexCount,
				 rendContext->global_param_op.size(), rendContext->global_param_pt.size(),
				 rendContext->global_param_tr.size(), rendContext->sortedTriangles.size(),
				 rendContext->global_param_mvo.size(), s_suppressed.exchange(0));
	}
	else
	{
		s_suppressed.fetch_add(1, std::memory_order_relaxed);
	}
	return false;
}

void MetalDrawer::DrawPoly(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const PolyParam &poly, u32 first, u32 count)
{
	if (!indexRangeValid(first, count, listType == ListType_Opaque ? "op"
						 : listType == ListType_Punch_Through ? "pt" : "tr"))
		return;
	MTLScissorRect scissorRect {};
	TileClipping tileClip = SetTileClip(encoder, poly.tileclip, scissorRect);

	float trilinearAlpha = 1.0f;
	if (poly.tsp.FilterMode > 1 && poly.pcw.Texture && listType != ListType_Punch_Through && poly.tcw.MipMapped == 1)
	{
		trilinearAlpha = 0.25f * (poly.tsp.MipMapD & 0x3);
		if (poly.tsp.FilterMode == 2)
			// Trilinear pass A
			trilinearAlpha = 1.0f - trilinearAlpha;
	}
	int gpuPalette = poly.texture == nullptr || !poly.texture->gpuPalette ? 0
																		  : poly.tsp.FilterMode + 1;
	float palette_index = 0.0f;
	if (gpuPalette != 0)
	{
		if (config::TextureFiltering == 1)
			gpuPalette = 1;
		else if (config::TextureFiltering == 2)
			gpuPalette = 2;
		if (poly.tcw.PixelFmt == PixelPal4)
			palette_index = float(poly.tcw.PalSelect << 4) / 1023.0f;
		else
			palette_index = float(poly.tcw.PalSelect >> 4 << 8) / 1023.0f;
	}

	std::array<float, 8> pushConstants {};

	if (tileClip == TileClipping::Inside || trilinearAlpha != 1.0f || gpuPalette != 0)
	{
		pushConstants = {
				(float)scissorRect.x,
				(float)scissorRect.y,
				(float)scissorRect.x + (float)scissorRect.width,
				(float)scissorRect.y + (float)scissorRect.height,
				trilinearAlpha,
				palette_index
		};
	} else {
		pushConstants = { 0, 0, 0, 0, 0, 0 };
	}

	MTL_SET_IF_CHANGED(drawState.havePushConstants, drawState.pushConstants, pushConstants,
					   [encoder setFragmentBytes:pushConstants.data() length:sizeof(pushConstants) atIndex:1]);

	bool shadowed = listType == ListType_Opaque || listType == ListType_Punch_Through;

	auto pipeline = pipelineManager->GetPipeline(listType, sortTriangles, poly, gpuPalette, dithering, true);
	if (pipeline == nil) {
		// Pipeline compile failed. Skip this draw
		// rather than asserting on setRenderPipelineState(nil); the poly is dropped
		// for this frame but the app stays alive.
		return;
	}
	{
		// nil doubles as "nothing cached yet" for the object slots — a real
		// pipeline is never nil here, the nil case returned above.
		bool havePipeline = drawState.pipeline != nil;
		MTL_SET_IF_CHANGED(havePipeline, drawState.pipeline, pipeline,
						   [encoder setRenderPipelineState:pipeline]);

		id<MTLDepthStencilState> depthStencil =
			pipelineManager->GetDepthStencilStates(listType, sortTriangles, shadowed, poly, true);
		bool haveDepthStencil = drawState.depthStencil != nil;
		MTL_SET_IF_CHANGED(haveDepthStencil, drawState.depthStencil, depthStencil,
						   [encoder setDepthStencilState:depthStencil]);

		const MTLCullMode cullMode = toMetalCullMode(poly.isp.CullMode);
		MTL_SET_IF_CHANGED(drawState.haveCullMode, drawState.cullMode, cullMode,
						   [encoder setCullMode:cullMode]);

		if (shadowed) {
			const u32 stencilRef = poly.pcw.Shadow != 0 ? 0x80u : 0u;
			MTL_SET_IF_CHANGED(drawState.haveStencilRef, drawState.stencilRef, stencilRef,
							   [encoder setStencilReferenceValue:stencilRef]);
		}
	}


	if (poly.texture != nullptr) {
		id<MTLTexture> texture = ((MetalTexture *)poly.texture)->GetReadOnlyTexture();
		bool haveTexture = drawState.fragTexture != nil;
		MTL_SET_IF_CHANGED(haveTexture, drawState.fragTexture, texture,
						   [encoder setFragmentTexture:texture atIndex:0]);

		// Texture sampler
		id<MTLSamplerState> samplerState = samplers->GetSampler(poly, listType == ListType_Punch_Through, false, true);
		if (samplerState == nil) {
			ERROR_LOG(RENDERER, "GetSampler returned nil! poly.texture=%p, FilterMode=%d, ClampU=%d, ClampV=%d, listType=%d",
					 poly.texture, (int)poly.tsp.FilterMode, (int)poly.tsp.ClampU, (int)poly.tsp.ClampV, (int)listType);
			// Skip binding nil sampler to prevent objc_msg_send crash
			return;
		}
		bool haveSampler = drawState.fragSampler != nil;
		MTL_SET_IF_CHANGED(haveSampler, drawState.fragSampler, samplerState,
						   [encoder setFragmentSamplerState:samplerState atIndex:0]);
	} else if (poly.pcw.Texture) {
		// Shader expects texture parameters but poly.texture is null
		// This can happen in some edge cases - bind default texture and sampler
		// Create a minimal 1x1 white texture as placeholder
		static id<MTLTexture> defaultTexture = nil;
		if (defaultTexture == nil) {
			MTLTextureDescriptor* texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
																								 width:1 height:1 mipmapped:NO];
			defaultTexture = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:texDesc];
			uint32_t whitePixel = 0xFFFFFFFF;
			[defaultTexture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&whitePixel bytesPerRow:4];
		}
		{
			bool haveTexture = drawState.fragTexture != nil;
			MTL_SET_IF_CHANGED(haveTexture, drawState.fragTexture, defaultTexture,
							   [encoder setFragmentTexture:defaultTexture atIndex:0]);
		}

		TSP defaultTsp = {};
		id<MTLSamplerState> defaultSampler = samplers->GetSampler(defaultTsp);
		if (defaultSampler == nil) {
			ERROR_LOG(RENDERER, "GetSampler returned nil for default TSP!");
			return;
		}
		bool haveDefaultSampler = drawState.fragSampler != nil;
		MTL_SET_IF_CHANGED(haveDefaultSampler, drawState.fragSampler, defaultSampler,
						   [encoder setFragmentSamplerState:defaultSampler atIndex:0]);
	}
	// Note: When poly.pcw.Texture is false, the shader is compiled without texture/sampler parameters,
	// so we don't bind anything in that case to avoid Metal validation errors

	// Whether this poly still needs the index-1 (and, on Naomi2, index-2)
	// vertex-buffer binds issued here.
	//
	// On a non-Naomi2 platform `offset` and `index` below both stay 0, so this
	// block re-bound curMainBuffer at currentBaseOffset + 0 to index 1 with
	// BYTE-IDENTICAL arguments for every textured polygon — thousands of ObjC
	// message sends per frame, on every Dreamcast game, for a binding that
	// never changes within a frame. Draw() now issues it once (see
	// hoistedVertexBind) and this block is skipped. Naomi2 polys still come
	// through: their offset really is per-polygon.
	const bool skipVertexBind = hoistedVertexBind && !poly.isNaomi2();
	if ((poly.pcw.Texture || poly.isNaomi2()) && !skipVertexBind)
	{
		u64 offset = 0;
		u32 index = 0;
		if (poly.isNaomi2())
		{
			switch (listType)
			{
				case ListType_Opaque:
					offset = offsets.naomi2OpaqueOffset;
					index = (u32)(&poly - &rendContext->global_param_op[0]);
					// Bounds check to prevent invalid offsets
					if (index >= rendContext->global_param_op.size()) {
						WARN_LOG(RENDERER, "Invalid Naomi2 opaque poly index: %u >= %zu", index, rendContext->global_param_op.size());
						return;
					}
					break;
				case ListType_Punch_Through:
					offset = offsets.naomi2PunchThroughOffset;
					index = (u32)(&poly - &rendContext->global_param_pt[0]);
					// Bounds check to prevent invalid offsets
					if (index >= rendContext->global_param_pt.size()) {
						WARN_LOG(RENDERER, "Invalid Naomi2 punch-through poly index: %u >= %zu", index, rendContext->global_param_pt.size());
						return;
					}
					break;
				case ListType_Translucent:
					offset = offsets.naomi2TranslucentOffset;
					index = (u32)(&poly - &rendContext->global_param_tr[0]);
					// Bounds check to prevent invalid offsets
					if (index >= rendContext->global_param_tr.size()) {
						WARN_LOG(RENDERER, "Invalid Naomi2 translucent poly index: %u >= %zu", index, rendContext->global_param_tr.size());
						return;
					}
					break;
			}
		}

		// Stride must be a multiple of the constant-buffer alignment so each
		// `base + index * size` binding stays aligned (256 on the Simulator).
		size_t size = sizeof(MetalN2VertexShaderUniforms) + MetalBufferPacker::align(sizeof(MetalN2VertexShaderUniforms), kConstantBufferAlign);
		u64 finalOffset = currentBaseOffset + offset + index * size;
		// Bounds check for final offset to prevent buffer overflow
		if (finalOffset >= [curMainBuffer length]) {
			WARN_LOG(RENDERER, "Buffer offset out of bounds: %llu >= %lu", finalOffset, (unsigned long)[curMainBuffer length]);
			return;
		}
		const DrawStateCache::BufferBinding n2Uniform { curMainBuffer, finalOffset };
		MTL_SET_IF_CHANGED(drawState.haveN2Uniform, drawState.n2Uniform, n2Uniform,
								 [encoder setVertexBuffer:n2Uniform.buffer offset:n2Uniform.offset atIndex:1]);

		if (offsets.lightsOffset != -1) {
			size = sizeof(N2LightModel) + MetalBufferPacker::align(sizeof(N2LightModel), kConstantBufferAlign);
			// Bounds check for lightModel to prevent buffer overflow
			if (poly.lightModel >= 256) {  // Reasonable upper bound for light models
				WARN_LOG(RENDERER, "Invalid light model index: %u", poly.lightModel);
				return;
			}
			u64 lightsOffset = currentBaseOffset + offsets.lightsOffset + poly.lightModel * size;
			// Bounds check for final lights offset
			if (lightsOffset >= [curMainBuffer length]) {
				WARN_LOG(RENDERER, "Lights buffer offset out of bounds: %llu >= %lu", lightsOffset, (unsigned long)[curMainBuffer length]);
				return;
			}
			const DrawStateCache::BufferBinding n2Lights { curMainBuffer, lightsOffset };
			MTL_SET_IF_CHANGED(drawState.haveN2Lights, drawState.n2Lights, n2Lights,
									 [encoder setVertexBuffer:n2Lights.buffer offset:n2Lights.offset atIndex:2]);
		}
	}

	MTLPrimitiveType primitive = sortTriangles && !config::PerStripSorting ? MTLPrimitiveTypeTriangle : MTLPrimitiveTypeTriangleStrip;

	[encoder drawIndexedPrimitives:primitive
						indexCount:count
						 indexType:MTLIndexTypeUInt32
					   indexBuffer:curMainBuffer
				 indexBufferOffset:currentBaseOffset + offsets.indexOffset + first * sizeof(u32)];
}

void MetalDrawer::DrawSorted(id<MTLRenderCommandEncoder> encoder, const std::vector<SortedTriangle> &polys, u32 first, u32 last, bool multipass)
{
	if (first == last)
		return;
	// A fresh encoder, or one another routine has just written state on, makes
	// every cached value a lie. Reset at every entry point that draws — the
	// cache is only ever valid within one uninterrupted run of DrawPoly calls.
	drawState.invalidate();


	[encoder pushDebugGroup:@"DrawSorted"];

	for (u32 idx = first; idx < last; idx++)
		DrawPoly(encoder, ListType_Translucent, true, rendContext->global_param_tr[polys[idx].polyIndex], polys[idx].first, polys[idx].count);
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		for (u32 idx = first; idx < last; idx++)
		{
			const SortedTriangle& param = polys[idx];
			const PolyParam& polyParam = rendContext->global_param_tr[param.polyIndex];
			if (polyParam.isp.ZWriteDis)
				continue;
			// Defensive: never bind a nil pipeline (asserts/crashes). The format
			// fix in CreateDepthPassPipeline keeps this non-nil; this guard means
			// a future pipeline-build failure degrades (skips the depth write)
			// instead of crashing.
			id<MTLRenderPipelineState> depthPipeline = pipelineManager->GetDepthPassPipeline(polyParam.isNaomi2());
			if (depthPipeline == nil)
				continue;
			[encoder setRenderPipelineState:depthPipeline];
			[encoder setDepthStencilState:pipelineManager->GetDepthPassDepthStencilStates(polyParam.isNaomi2())];

			MTLScissorRect scissorRect {};
			SetTileClip(encoder, polyParam.tileclip, scissorRect);

			[encoder setCullMode:toMetalCullMode(polyParam.isp.CullMode)];

			if (!indexRangeValid(param.first, param.count, "sorted-depth"))
				continue;
			[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
								indexCount:param.count
								 indexType:MTLIndexTypeUInt32
							   indexBuffer:curMainBuffer
						 indexBufferOffset:currentBaseOffset + offsets.indexOffset + param.first * sizeof(u32)];
		}
	}

	[encoder popDebugGroup];
}

void MetalDrawer::DrawList(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const std::vector<PolyParam> &polys, u32 first, u32 last)
{
	if (first == last)
		return;

	// A fresh encoder, or one another routine has just written state on, makes
	// every cached value a lie. Reset at every entry point that draws — the
	// cache is only ever valid within one uninterrupted run of DrawPoly calls.
	drawState.invalidate();

	[encoder pushDebugGroup:@"DrawList"];

	const PolyParam *pp_end = polys.data() + last;
	for (const PolyParam *pp = &polys[first]; pp != pp_end; pp++)
		if (pp->count > 2)
			DrawPoly(encoder, listType, sortTriangles, *pp, pp->first, pp->count);

	[encoder popDebugGroup];
}

void MetalDrawer::DrawModVols(id<MTLRenderCommandEncoder> encoder, int first, int count)
{
	if (count == 0 || rendContext->modtrig.empty() || !config::ModifierVolumes)
		return;

	// DrawModVols sets its own pipeline/depth-stencil on this encoder, so the
	// next DrawList must not trust anything cached from before it.
	drawState.invalidate();

	[encoder pushDebugGroup:@"DrawModVols"];
	[encoder setVertexBufferOffset:currentBaseOffset + offsets.modVolOffset atIndex:30];

	ModifierVolumeParam* params = &rendContext->global_param_mvo[first];

	int mod_base = -1;
	id<MTLRenderPipelineState> state;
	id<MTLDepthStencilState> depth_state;

	// The tile clip fields are refreshed per volume below (Inside mode is a shader discard).
	MetalModVolPushBlock pushBlock;
	pushBlock.shaderColor = 1 - FPU_SHAD_SCALE.scale_factor / 256.f;
	[encoder setFragmentBytes:&pushBlock length:sizeof(pushBlock) atIndex:1];

	for (int cmv = 0; cmv < count; cmv++) {
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (!param.isp.VolumeLast && mv_mode > 0) {
			state = pipelineManager->GetModifierVolumePipeline(ModVolMode::Or, param.isNaomi2());  // OR'ing (open volume or quad)
			depth_state = pipelineManager->GetModVolDepthStencilStates(ModVolMode::Or, param.isNaomi2());
		} else {
			state = pipelineManager->GetModifierVolumePipeline(ModVolMode::Xor, param.isNaomi2()); // XOR'ing (closed volume)
			depth_state = pipelineManager->GetModVolDepthStencilStates(ModVolMode::Xor, param.isNaomi2());
		}

		// Skip if the modvol pipeline failed to compile (Simulator compiler crash)
		// rather than asserting on setRenderPipelineState(nil).
		if (state == nil)
			continue;
		[encoder setRenderPipelineState:state];
		[encoder setDepthStencilState:depth_state];
		[encoder setCullMode:toMetalCullMode(param.isp.CullMode)];
		[encoder setStencilReferenceValue:2];
		MTLScissorRect scissorRect {};
		TileClipping clipMode = SetTileClip(encoder, param.tileclip, scissorRect);
		if (pushBlock.setTileClip(clipMode, scissorRect))
			[encoder setFragmentBytes:&pushBlock length:sizeof(pushBlock) atIndex:1];

		[encoder drawPrimitives:MTLPrimitiveTypeTriangle
					vertexStart:param.first * 3
					vertexCount:param.count * 3];

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			state = pipelineManager->GetModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isNaomi2());
			depth_state = pipelineManager->GetModVolDepthStencilStates(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isNaomi2());
			if (state != nil) {
				[encoder setRenderPipelineState:state];
				[encoder setDepthStencilState:depth_state];
				[encoder setCullMode:toMetalCullMode(param.isp.CullMode)];
				[encoder setStencilReferenceValue:1];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangle
							vertexStart: mod_base * 3
							vertexCount: (param.first + param.count - mod_base) * 3];
			}
			mod_base = -1;
		}
	}
	[encoder setVertexBufferOffset:currentBaseOffset atIndex:30];

	// Reset the tile clip before the screen-covering Final quad (Vulkan does
	// SetTileClip(0) here) — otherwise the last volume's Outside tile clip
	// leaves a stale scissor that crops the shadow/light darkening pass.
	MTLScissorRect fullScissor {};
	SetTileClip(encoder, 0, fullScissor);
	if (pushBlock.setTileClip(TileClipping::Off, fullScissor))
		[encoder setFragmentBytes:&pushBlock length:sizeof(pushBlock) atIndex:1];

	state = pipelineManager->GetModifierVolumePipeline(ModVolMode::Final, false);
	depth_state = pipelineManager->GetModVolDepthStencilStates(ModVolMode::Final, false);
	// Skip the final modvol pass if its pipeline failed to compile (Simulator
	// compiler crash) — but still balance the debug group below.
	if (state != nil && indexRangeValid(0, 4, "modvol-final")) {
		[encoder setRenderPipelineState:state];
		[encoder setDepthStencilState:depth_state];
		[encoder setCullMode:toMetalCullMode(0)];
		[encoder setStencilReferenceValue:0x81];
		[encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip
							indexCount:4
							 indexType:MTLIndexTypeUInt32
						   indexBuffer:curMainBuffer
					 indexBufferOffset:currentBaseOffset + offsets.indexOffset];
	}

	[encoder popDebugGroup];
}

void MetalDrawer::UploadMainBuffer(const MetalVertexShaderUniforms &vertexUniforms, const MetalFragmentShaderUniforms &fragmentUniforms) {
	MetalBufferPacker packer;

	// Vertex
	packer.add(rendContext->verts.data(), rendContext->verts.size() * sizeof(decltype(*rendContext->verts.data())));
	// Modifier Volumes
	offsets.modVolOffset = packer.add(rendContext->modtrig.data(), rendContext->modtrig.size() * sizeof(decltype(*rendContext->modtrig.data())));
	// Index
	offsets.indexOffset = packer.add(rendContext->idx.data(), rendContext->idx.size() * sizeof(decltype(*rendContext->idx.data())));
	offsets.indexCount = rendContext->idx.size();
	// Uniform buffers
	offsets.vertexUniformOffset = packer.addUniform(&vertexUniforms, sizeof(vertexUniforms));
	offsets.fragmentUniformOffset = packer.addUniform(&fragmentUniforms, sizeof(fragmentUniforms));

	std::vector<u8> n2uniforms;
	if (settings.platform.isNaomi2())
	{
		 packNaomi2Uniforms(packer, offsets, n2uniforms, false);
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

bool MetalDrawer::Draw(const MetalTexture *fogTexture, const MetalTexture *paletteTexture) {
	MetalFragmentShaderUniforms fragUniforms = MakeFragmentUniforms<MetalFragmentShaderUniforms>();
	// Only non-Naomi2 content has a frame-constant index-1 binding to hoist.
	// Keyed on the PLATFORM, matching UploadMainBuffer's own gate on
	// packNaomi2Uniforms -- if that did not run, the naomi2*Offset fields are
	// meaningless and DrawPoly must not be short-circuited on them.
	hoistedVertexBind = !settings.platform.isNaomi2();
	dithering = config::EmulateFramebuffer && rendContext->fb_W_CTRL.fb_dither && rendContext->fb_W_CTRL.fb_packmode <= 3;
	if (dithering) {
		switch (rendContext->fb_W_CTRL.fb_packmode)
		{
			case 0: // 0555 KRGB 16 bit
			case 3: // 1555 ARGB 16 bit
				fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[1] = fragUniforms.ditherDivisor[2] = 2.f;
				break;
			case 1: // 565 RGB 16 bit
				fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[2] = 2.f;
				fragUniforms.ditherDivisor[1] = 4.f;
				break;
			case 2: // 4444 ARGB 16 bit
				fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[1] = fragUniforms.ditherDivisor[2] = 1.f;
				break;
			default:
				break;
		}
		fragUniforms.ditherDivisor[3] = 1.f;
	}

	currentScissor = MTLScissorRect {};

	@autoreleasepool {
		id<MTLRenderCommandEncoder> renderEncoder = BeginRenderPass();

		[renderEncoder setFragmentTexture:fogTexture->GetTexture() atIndex:2];
		[renderEncoder setFragmentTexture:paletteTexture->GetTexture() atIndex:3];

		// Fog sampler
		TSP fogTsp = {};
		fogTsp.FilterMode = 1;
		fogTsp.ClampU = 1;
		fogTsp.ClampV = 1;
		id<MTLSamplerState> fogSampler = samplers->GetSampler(fogTsp);
		if (fogSampler == nil) {
			ERROR_LOG(RENDERER, "GetSampler returned nil for fog TSP!");
			return false;
		}
		[renderEncoder setFragmentSamplerState:fogSampler atIndex:2];

		// Palette sampler
		TSP palTsp = {};
		palTsp.FilterMode = 0;
		palTsp.ClampU = 1;
		palTsp.ClampV = 1;
		id<MTLSamplerState> paletteSampler = samplers->GetSampler(palTsp);
		if (paletteSampler == nil) {
			ERROR_LOG(RENDERER, "GetSampler returned nil for palette TSP!");
			return false;
		}
		[renderEncoder setFragmentSamplerState:paletteSampler atIndex:3];

		setFirstProvokingVertex(*rendContext);

		// Upload vertex and index buffers
		MetalVertexShaderUniforms vtxUniforms {};
		vtxUniforms.ndcMat = matrices.GetNormalMatrix();

		[renderEncoder setFrontFacingWinding:MTLWindingCounterClockwise];

		UploadMainBuffer(vtxUniforms, fragUniforms);

		[renderEncoder setVertexBuffer:curMainBuffer offset:currentBaseOffset + 0 atIndex:30];
		[renderEncoder setVertexBuffer:curMainBuffer offset:currentBaseOffset + offsets.vertexUniformOffset atIndex:0];
		if (hoistedVertexBind) {
			// The per-textured-poly index-1 bind, lifted out of DrawPoly. On a
			// non-Naomi2 platform its arguments are exactly these, every time.
			// This is the only encoder in the frame (BeginRenderPass runs once
			// above; every render pass below reuses it) and nothing else in
			// this drawer binds vertex index 1, so one bind covers the frame.
			//
			// Difference from the old code, stated rather than buried: index 1
			// is now bound even in a frame containing zero textured polys. A
			// buffer no shader reads is inert.
			[renderEncoder setVertexBuffer:curMainBuffer offset:currentBaseOffset + 0 atIndex:1];
		}
		[renderEncoder setFragmentBuffer:curMainBuffer offset:currentBaseOffset + offsets.fragmentUniformOffset atIndex:0];

		RenderPass previous_pass {};
		for (int render_pass = 0; render_pass < (int)rendContext->render_passes.size(); render_pass++) {
			const RenderPass& current_pass = rendContext->render_passes[render_pass];

			DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
					  current_pass.op_count - previous_pass.op_count,
					  current_pass.pt_count - previous_pass.pt_count,
					  current_pass.tr_count - previous_pass.tr_count,
					  current_pass.mvo_count - previous_pass.mvo_count, current_pass.autosort);
			DrawList(renderEncoder, ListType_Opaque, false, rendContext->global_param_op, previous_pass.op_count, current_pass.op_count);
			DrawList(renderEncoder, ListType_Punch_Through, false, rendContext->global_param_pt, previous_pass.pt_count, current_pass.pt_count);
			DrawModVols(renderEncoder, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);
			if (current_pass.autosort) {
				if (!config::PerStripSorting)
					DrawSorted(renderEncoder, rendContext->sortedTriangles, previous_pass.sorted_tr_count, current_pass.sorted_tr_count, render_pass + 1 < (int)rendContext->render_passes.size());
				else
					DrawList(renderEncoder, ListType_Translucent, true, rendContext->global_param_tr, previous_pass.tr_count, current_pass.tr_count);
			} else {
				DrawList(renderEncoder, ListType_Translucent, false, rendContext->global_param_tr, previous_pass.tr_count, current_pass.tr_count);
			}
			previous_pass = current_pass;
		}
	}

	curMainBuffer = nil;

	return !rendContext->isRTT;
}

void MetalTextureDrawer::Init(MetalSamplers *samplers, MetalShaders *shaders, MetalTextureCache *textureCache)
{
	MetalDrawer::Init(samplers, MetalPipelineManager(shaders));

	this->textureCache = textureCache;

	rttPassDescriptor = [[MTLRenderPassDescriptor alloc] init];
}

id<MTLRenderCommandEncoder> MetalTextureDrawer::BeginRenderPass() {
	DEBUG_LOG(RENDERER, "RenderToTexture packmode=%d stride=%d - %d x %d @ %06x", rendContext->fb_W_CTRL.fb_packmode, rendContext->fb_W_LINESTRIDE * 8,
			  rendContext->fbClip.size.x, rendContext->fbClip.size.y, rendContext->fb_W_SOF1 & VRAM_MASK);

	textureAddr = rendContext->fb_W_SOF1 & VRAM_MASK;
	// Since 31d5ddb92: the render target is exactly the framebuffer size
	// Renderer_if already computed for this context (getScaledFramebufferSize:
	// pow2 of the tile/fb clip, upscaled, when rendering into the texture
	// cache; the raw tile-clip size, scaler-adjusted, when writing back to
	// VRAM). The old getFramebufferWidth/getRenderToTextureDimensions helpers
	// are gone; mirrors Vulkan TextureDrawer::BeginRenderPass.
	u32 width = rendContext->framebufferWidth;
	u32 height = rendContext->framebufferHeight;

	// Validate dimensions to prevent crashes with extreme values
	if (width == 0 || height == 0 || width > 16384 || height > 16384) {
		ERROR_LOG(RENDERER, "Invalid RTT dimensions: %dx%d, falling back to safe values", width, height);
		width = std::max(8u, std::min(width, 1024u));
		height = std::max(8u, std::min(height, 1024u));
		DEBUG_LOG(RENDERER, "RTT Fallback dimensions: %dx%d", width, height);
	}
	matrices.CalcMatrices(rendContext, width, height);
	DEBUG_LOG(RENDERER, "RTT Dimensions: %dx%d", width, height);

	id<MTLCommandBuffer> commandBuffer = commandPool->Allocate();

	if (!depthAttachment || width > depthAttachment.width || height > depthAttachment.height)
	{
		DEBUG_LOG(RENDERER, "Creating new depth attachment: %dx%d", width, height);
		MTLTextureDescriptor *depthDescriptor = [[MTLTextureDescriptor alloc] init];
		depthDescriptor.width = width;
		depthDescriptor.height = height;
		depthDescriptor.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
		depthDescriptor.usage = MTLTextureUsageRenderTarget;
		// Simulate shared memory on Apple Silicon
		// but not simulator, where we can't use shared memory
#if (TARGET_OS_IOS || TARGET_OS_TV) && !TARGET_OS_SIMULATOR
		depthDescriptor.storageMode = MTLStorageModeShared;
#else
		depthDescriptor.storageMode = MTLStorageModePrivate;
#endif

		depthAttachment = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:depthDescriptor];
		[depthAttachment setLabel:@"Rtt Depth Attachment"];
	} else {
		DEBUG_LOG(RENDERER, "Reusing existing depth attachment: %dx%d", (int)depthAttachment.width, (int)depthAttachment.height);
	}

	id<MTLTexture> colorImage;

	// Branch orientation matches Vulkan (drawer.cpp:511): the DEFAULT config
	// (RenderToTextureBuffer=false) renders straight into the texture-cache
	// texture the game will sample; readback mode renders into a scratch
	// attachment and writes the pixels back to emulated VRAM at EndRenderPass.
	// This was inverted: the default path rendered into the scratch that
	// nothing reads, so every render-to-texture effect sampled STALE VRAM —
	// THPS2/Mat Hoffman's RTT blob shadow showed the font atlas ("shadow too
	// dark and too big" with garbled glyphs).
	if (!config::RenderToTextureBuffer)
	{
		// Texture-cache key: the unscaled pow2 PVR framebuffer size (upstream
		// getPvrFramebufferSize), not the upscaled render-target size.
		int wpo2, hpo2;
		getPvrFramebufferSize(*rendContext, wpo2, hpo2);
		texture = textureCache->getRTTexture(textureAddr, rendContext->fb_W_CTRL.fb_packmode, wpo2, hpo2);
		if (textureCache->IsInFlight(texture, false))
		{
			texture->CreateReadOnlyCopy(commandBuffer);
			texture->deferDeleteResource(commandPool);
		}
		textureCache->SetInFlight(texture);

		// Check if we need to recreate the texture. Usage matters too: the
		// cache entry may hold a texture decoded by the regular upload path
		// (ShaderRead only) — binding that as a render-pass color attachment
		// is a Metal validation abort.
		bool needsRecreation = !texture->GetTexture() ||
							   texture->GetTexture().width != width ||
							   texture->GetTexture().height != height ||
							   !(texture->GetTexture().usage & MTLTextureUsageRenderTarget);

		if (needsRecreation)
		{
			DEBUG_LOG(RENDERER, "Creating RTT texture: %dx%d", width, height);
			MTLTextureDescriptor *colorDescriptor = [[MTLTextureDescriptor alloc] init];
			colorDescriptor.width = width;
			colorDescriptor.height = height;
			colorDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
			colorDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			// Apple Silicon has unified memory — Shared storage lets the CPU
			// read this RTT's pixels back to emulated VRAM via getBytes:
			// directly, with zero copy. The legacy Private+blit-to-staging
			// pattern (PC-GPU idiom) wastes an extra texture, a blit pass,
			// and a CPU/GPU sync point. See `MetalTextureDrawer::EndRenderPass`
			// for the matching readback simplification.
			colorDescriptor.storageMode = MTLStorageModeShared;

			id<MTLTexture> newTexture = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:colorDescriptor];
			[newTexture setLabel:@"Rtt Color Attachment"];
			texture->SetTexture(newTexture, width, height);
		} else {
			DEBUG_LOG(RENDERER, "Reusing existing RTT texture: %dx%d", (int)texture->GetTexture().width, (int)texture->GetTexture().height);
		}
		colorImage = texture->GetTexture();
	}
	else
	{
		if (!colorAttachment || width > colorAttachment.width || height > colorAttachment.height)
		{
			DEBUG_LOG(RENDERER, "Creating new color attachment: %dx%d", width, height);
			MTLTextureDescriptor *colorDescriptor = [[MTLTextureDescriptor alloc] init];
			colorDescriptor.width = width;
			colorDescriptor.height = height;
			colorDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
			colorDescriptor.usage = MTLTextureUsageRenderTarget;
			// Readback target: EndRenderPass getBytes: requires CPU-visible
			// storage. Private aborts in the simulator's MTLSimDriver and is
			// invalid for getBytes on macOS too; Shared is free on Apple
			// Silicon unified memory.
			colorDescriptor.storageMode = MTLStorageModeShared;
			colorAttachment = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:colorDescriptor];
			[colorAttachment setLabel:@"Rtt Color Attachment"];
		} else {
			DEBUG_LOG(RENDERER, "Reusing existing color attachment: %dx%d", (int)colorAttachment.width, (int)colorAttachment.height);
		}
		colorImage = colorAttachment;
	}

	auto colorAttachmentDesc = rttPassDescriptor.colorAttachments[0];
	[colorAttachmentDesc setTexture:colorImage];
	[colorAttachmentDesc setLoadAction:MTLLoadActionClear];
	[colorAttachmentDesc setStoreAction:MTLStoreActionStore];
	[colorAttachmentDesc setClearColor:MTLClearColorMake(0.0, 0.0, 0.0, 1.0)];

	auto depthAttachmentDesc = rttPassDescriptor.depthAttachment;
	[depthAttachmentDesc setTexture:depthAttachment];
	[depthAttachmentDesc setLoadAction:MTLLoadActionClear];
	[depthAttachmentDesc setStoreAction:MTLStoreActionDontCare];
	[depthAttachmentDesc setClearDepth:0.0];

	auto stencilAttachmentDesc = rttPassDescriptor.stencilAttachment;
	[stencilAttachmentDesc setTexture:depthAttachment];
	[stencilAttachmentDesc setLoadAction:MTLLoadActionClear];
	[stencilAttachmentDesc setStoreAction:MTLStoreActionDontCare];
	[stencilAttachmentDesc setClearStencil:0];

	currentEncoder = [commandBuffer renderCommandEncoderWithDescriptor:rttPassDescriptor];
	[currentEncoder pushDebugGroup:@"RenderToTexture"];

	// Log actual render target dimensions from the encoder
	DEBUG_LOG(RENDERER, "ACTUAL render target dimensions: color=%dx%d, depth=%dx%d",
			  (int)colorImage.width, (int)colorImage.height,
			  (int)depthAttachment.width, (int)depthAttachment.height);

	// Viewport is the logical target (width x height, what CalcMatrices was
	// given) — the readback attachment is grow-only and may be larger. The
	// base scissor comes from TransformMatrix::getBaseScissor() (tile clip, or
	// tile ∩ fb clip when rendering into the texture cache), clamped to the
	// viewport. Same as Vulkan TextureDrawer::BeginRenderPass after 31d5ddb92.
	MTLViewport viewport = {
			0.0,
			0.0,
			(double)width,
			(double)height,
			1.0,
			0.0
	};
	[currentEncoder setViewport:viewport];
	SetBaseScissor(viewport);
	DEBUG_LOG(RENDERER, "RTT Viewport: %.0fx%.0f baseScissor (%d,%d,%d,%d)", viewport.width, viewport.height,
			  (int)baseScissor.x, (int)baseScissor.y, (int)baseScissor.width, (int)baseScissor.height);
	SetScissor(currentEncoder, baseScissor);

	return currentEncoder;
}

void MetalTextureDrawer::EndRenderPass()
{
	[currentEncoder popDebugGroup];
	[currentEncoder endEncoding];
	currentEncoder = nil;

	// Readback covers the whole (scaler-adjusted) framebuffer; WriteTextureToVRam
	// applies fbClip itself now (31d5ddb92).
	u32 fbw = rendContext->framebufferWidth;
	u32 fbh = rendContext->framebufferHeight;

	if (config::RenderToTextureBuffer)
	{
		// Ensure GPU work that rendered into the RTT texture has completed
		commandPool->EndFrameAndWait();

		// Source is the RTT color image we rendered into this pass. It's
		// MTLStorageModeShared (see RTT texture creation above) so we can
		// read it directly via getBytes — no intermediate staging buffer
		// and no second blit + waitUntilCompleted sync point. The
		// EndFrameAndWait above is the only CPU/GPU sync needed in this
		// path; Apple Silicon's unified memory makes the readback itself
		// zero-copy.
		//
		// Previously this allocated a Shared MTLBuffer every frame, queued
		// a blit CB to copy texture → buffer, and waitUntilCompleted'd the
		// blit. On RTT-heavy scenes (Sonic Adventure reflections, DOA2
		// intro, etc.) that pattern cost an additional 1–5 ms per RTT
		// (sometimes more under GPU saturation). Removed.
		id<MTLTexture> srcTexture = texture ? texture->GetTexture() : colorAttachment;
		if (!srcTexture) {
			ERROR_LOG(RENDERER, "RTT readback failed: no source texture");
			MetalDrawer::EndRenderPass();
			return;
		}

		// Never read past the attachment (the dimension fallback above can shrink it).
		fbw = std::min<u32>(fbw, (u32)srcTexture.width);
		fbh = std::min<u32>(fbh, (u32)srcTexture.height);
		const u32 rowBytes = fbw * 4; // BGRA8
		rttReadback.init(fbw, fbh);
		[srcTexture getBytes:rttReadback.data()
				 bytesPerRow:rowBytes
				  fromRegion:MTLRegionMake2D(0, 0, fbw, fbh)
				 mipmapLevel:0];

		// CPU-side convert and write back to VRAM in the Dreamcast format
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
		// Unlock and invalidate the texture-cache pages this writeback covers BEFORE
		// writing. Two reasons:
		// - Left to the write itself, every locked page raises a protection fault that
		//   the core's signal handler resolves. That is invisible on a device, but with
		//   a debugger attached it stops as EXC_BAD_ACCESS in TexPixelWriter::write.
		// - A texture that re-reads the render target in another pixel format must
		//   decode it again (THPS2 renders its shadow in 565 and samples it as 4444, so
		//   alpha = red's top bits; a stale decode is an opaque decal that darkens the
		//   whole floor with no blob).
		{
			const u32 lineBytes = std::max<u32>(rendContext->fb_W_LINESTRIDE * 8, fbw * 2);
			const u32 bytes = std::max<u32>(fbh, 1u) * lineBytes;
			const u32 start = textureAddr & VRAM_MASK;
			for (u32 off = start & ~(PAGE_SIZE - 1); off < start + bytes && off < VRAM_SIZE; off += PAGE_SIZE)
				VramLockedWriteOffset(off);
		}
		WriteTextureToVRam(fbw, fbh, (u8 *)rttReadback.data(), dst, rendContext->fb_W_CTRL, rendContext->fb_W_LINESTRIDE * 8, rendContext->fbClip);
	}
	else
	{
		commandPool->EndFrame();
		if (texture) {
			texture->dirty = 0;
			texture->unprotectVRam();
		}
	}

	MetalDrawer::EndRenderPass();
}

void MetalScreenDrawer::Init(MetalSamplers *samplers, MetalShaders *shaders, const MTLViewport &viewport) {
	emulateFramebuffer = config::EmulateFramebuffer;
	this->shaderManager = shaders;

	// Validate viewport dimensions
	if (viewport.width <= 0 || viewport.height <= 0 || viewport.width > 16384 || viewport.height > 16384) {
		WARN_LOG(RENDERER, "Invalid viewport dimensions: %.0fx%.0f, skipping initialization", viewport.width, viewport.height);
		return;
	}

	bool dimensionsChanged = this->viewport.height != viewport.height ||
			this->viewport.width != viewport.width ||
			this->viewport.originX != viewport.originX ||
			this->viewport.originY != viewport.originY ||
			this->viewport.zfar != viewport.zfar ||
			this->viewport.znear != viewport.znear;

	if (dimensionsChanged) {
		DEBUG_LOG(RENDERER, "MetalScreenDrawer dimension change: %.0fx%.0f -> %.0fx%.0f",
				 this->viewport.width, this->viewport.height, viewport.width, viewport.height);

		if (!framebuffers.empty()) {
			verify(commandPool != nullptr);
			commandPool->addToFlight(new MetalDeleter(std::move(framebuffers)));
		}
		if (depthAttachment) {
			class ResourceDeleter : public MetalDeletable
			{
			public:
				ResourceDeleter(id<MTLTexture> texture)
				{
					std::swap(this->texture, texture);
				}

				~ResourceDeleter() override {
					texture = nil;
				}

			private:
				id<MTLTexture> texture = nil;
			};

			commandPool->addToFlight(new ResourceDeleter(depthAttachment));
		}

		depthAttachment = nil;
		clearPassDescriptors.clear();
		loadPassDescriptors.clear();
		clearNeeded.clear();
	}
	this->viewport = viewport;

	if (depthAttachment == nil)
	{
		MTLTextureDescriptor *descriptor = [[MTLTextureDescriptor alloc] init];
		descriptor.width = (NSUInteger)viewport.width;
		descriptor.height = (NSUInteger)viewport.height;
		descriptor.pixelFormat = MTLPixelFormatDepth32Float_Stencil8;
		descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
#if (TARGET_OS_IOS || TARGET_OS_TV) && !TARGET_OS_SIMULATOR
		descriptor.storageMode = MTLStorageModeShared;
#else
		descriptor.storageMode = MTLStorageModePrivate;
#endif
		depthAttachment = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:descriptor];
		[depthAttachment setLabel:@"Screen Depth Attachment"];
	}

	if (framebuffers.size() > 3)
	{
		framebuffers.resize(3);
		loadPassDescriptors.resize(3);
		clearPassDescriptors.resize(3);
		clearNeeded.resize(3);
	}
	else
	{
		while (framebuffers.size() < 3)
		{
			MTLTextureDescriptor *texDescriptor = [[MTLTextureDescriptor alloc] init];
			texDescriptor.width = viewport.width;
			texDescriptor.height = viewport.height;
			texDescriptor.pixelFormat = MTLPixelFormatBGRA8Unorm;
			texDescriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;

			id<MTLTexture> colorAttachment = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:texDescriptor];
			framebuffers.push_back(colorAttachment);

			MTLRenderPassDescriptor *passDescriptor = [[MTLRenderPassDescriptor alloc] init];
			auto depth = passDescriptor.depthAttachment;
			[depth setTexture:depthAttachment];
			[depth setLoadAction:MTLLoadActionClear];
			[depth setStoreAction:MTLStoreActionDontCare];
			// Reversed log depth: greater = closer, so "empty" must clear to
			// 0.0 (farthest), matching Vulkan's ClearDepthStencilValue{0,0}
			// (drawer.cpp:571) and the RTT descriptor above. Metal's default
			// clearDepth is 1.0 = nearest, which fails every Greater/GEqual
			// test on pixels the background plane doesn't cover.
			[depth setClearDepth:0.0];

			auto stencil = passDescriptor.stencilAttachment;
			[stencil setTexture:depthAttachment];
			[stencil setLoadAction:MTLLoadActionClear];
			[stencil setStoreAction:MTLStoreActionDontCare];
			[stencil setClearStencil:0];

			auto color = passDescriptor.colorAttachments[0];
			[color setTexture:colorAttachment];
			[color setLoadAction:MTLLoadActionLoad];
			[color setStoreAction:MTLStoreActionStore];

			loadPassDescriptors.push_back(passDescriptor);

			MTLRenderPassDescriptor *clearPassDescriptor = [passDescriptor copy];
			[clearPassDescriptor.colorAttachments[0] setLoadAction:MTLLoadActionClear];

			clearPassDescriptors.push_back(clearPassDescriptor);
			clearNeeded.push_back(true);
		}
	}
	frameRendered = false;

	MetalDrawer::Init(samplers, MetalPipelineManager(shaderManager));
}

id<MTLRenderCommandEncoder> MetalScreenDrawer::BeginRenderPass() {
	DEBUG_LOG(RENDERER, "=== SCREEN DRAWER BeginRenderPass ===");

	if (!renderPassStarted)
	{
		frameRendered = false;
		id<MTLCommandBuffer> commandBuffer = commandPool->Allocate();
		MTLRenderPassDescriptor* passDescriptor = clearNeeded[GetCurrentImage()] || rendContext->clearFramebuffer ? clearPassDescriptors[GetCurrentImage()] : loadPassDescriptors[GetCurrentImage()];
		clearNeeded[GetCurrentImage()] = false;
		currentEncoder = [commandBuffer renderCommandEncoderWithDescriptor:passDescriptor];
		renderPassStarted = true;

		DEBUG_LOG(RENDERER, "Screen render pass started, imageIndex=%d", GetCurrentImage());
	}

	[currentEncoder setViewport:viewport];
	DEBUG_LOG(RENDERER, "Screen viewport set: %.0fx%.0f at (%.0f,%.0f)", viewport.width, viewport.height, viewport.originX, viewport.originY);

	matrices.CalcMatrices(rendContext, viewport.width, viewport.height);

	SetBaseScissor(viewport);
	DEBUG_LOG(RENDERER, "Screen baseScissor calculated: (%d,%d,%d,%d)",
			 (int)baseScissor.x, (int)baseScissor.y, (int)baseScissor.width, (int)baseScissor.height);

	DEBUG_LOG(RENDERER, "About to call SetScissor with baseScissor");
	SetScissor(currentEncoder, baseScissor);
	DEBUG_LOG(RENDERER, "SetScissor completed");

	return currentEncoder;
}

void MetalBaseDrawer::scaleAndWriteFramebuffer(id<MTLTexture> finalTexture)
{
	u32 width = rendContext->globClip.x;
	u32 height = rendContext->globClip.y;
	// Since 31d5ddb92: the scaler (SCALER_CTL) target size and the final
	// VRAM clip (tile clip scaled, intersected with the fb clip) are computed
	// once in getWriteFBToVramParams for every renderer.
	glm::ivec2 scaledSize;
	Rect finalClip;
	getWriteFBToVramParams(*rendContext, scaledSize, finalClip);

	id<MTLTexture> scaledTexture = nil;

	// Scale the framebuffer if needed
	if (scaledSize.x != (int)width || scaledSize.y != (int)height)
	{
		const u32 scaledW = scaledSize.x;
		const u32 scaledH = scaledSize.y;

		if (cachedScaledTexture == nil || cachedScaledWidth != scaledW || cachedScaledHeight != scaledH)
		{
			MTLTextureDescriptor *texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																								width:scaledW
																							   height:scaledH
																							mipmapped:NO];
			texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
			texDesc.storageMode = MTLStorageModePrivate;
			cachedScaledTexture = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:texDesc];
			cachedScaledWidth = scaledW;
			cachedScaledHeight = scaledH;
		}
		scaledTexture = cachedScaledTexture;

		id<MTLCommandBuffer> commandBuffer = commandPool->Allocate();
		id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

		// NOTE: a blit copy, not a scale (Vulkan blitImage filters here); the
		// copy is bounded to the smaller of the two so it cannot overrun the
		// scaled target when the scaler shrinks (hscale / vscale > 1).
		[blitEncoder copyFromTexture:finalTexture
						 sourceSlice:0
						 sourceLevel:0
						sourceOrigin:MTLOriginMake(0, 0, 0)
						  sourceSize:MTLSizeMake(std::min({ width, scaledW, (u32)finalTexture.width }),
												 std::min({ height, scaledH, (u32)finalTexture.height }), 1)
						   toTexture:scaledTexture
					destinationSlice:0
					destinationLevel:0
				   destinationOrigin:MTLOriginMake(0, 0, 0)];

		[blitEncoder endEncoding];

		finalTexture = scaledTexture;
		width = scaledW;
		height = scaledH;
	}

	// Readback target (BGRA to match framebuffer format). Cached across frames:
	// EndFrameAndWait() below is the sync point, so last frame's copy is
	// complete before this frame's blit writes into it again.
	if (cachedReadableTexture == nil || cachedReadableWidth != width || cachedReadableHeight != height)
	{
		MTLTextureDescriptor *readableDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
																								 width:width
																								height:height
																							 mipmapped:NO];
		readableDesc.usage = MTLTextureUsageShaderRead;
#if TARGET_OS_IOS || TARGET_OS_TV
		// iOS/tvOS: Use shared memory (unified memory architecture)
		readableDesc.storageMode = MTLStorageModeShared;
#else
		// macOS: Use managed memory for CPU readback
		// TODO: MacOS Can use shared memory on Apple silcon, but must detect first
		readableDesc.storageMode = MTLStorageModeManaged;
#endif
		cachedReadableTexture = [MetalContext::Instance()->GetDevice() newTextureWithDescriptor:readableDesc];
		cachedReadableWidth = width;
		cachedReadableHeight = height;
	}
	id<MTLTexture> readableTexture = cachedReadableTexture;

	id<MTLCommandBuffer> commandBuffer = commandPool->Allocate();
	id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

	// The OIT drawer's attachments are sized from its viewport, not from
	// globClip, so bound the copy by the source too: an oversized sourceSize
	// is a validation error.
	[blitEncoder copyFromTexture:finalTexture
					 sourceSlice:0
					 sourceLevel:0
					sourceOrigin:MTLOriginMake(0, 0, 0)
					  sourceSize:MTLSizeMake(std::min(width, (u32)finalTexture.width),
											 std::min(height, (u32)finalTexture.height), 1)
					   toTexture:readableTexture
				destinationSlice:0
				destinationLevel:0
			   destinationOrigin:MTLOriginMake(0, 0, 0)];

#if !TARGET_OS_IOS && !TARGET_OS_TV
	// macOS: Explicit synchronization needed for managed memory
	[blitEncoder synchronizeResource:readableTexture];
#endif
	[blitEncoder endEncoding];

	// Submit command buffer and wait (matches Vulkan's EndFrameAndWait)
	commandPool->EndFrameAndWait();

	// Read back the texture data
	fbReadback.init(width, height);
	[readableTexture getBytes:fbReadback.data()
				  bytesPerRow:width * 4
				   fromRegion:MTLRegionMake2D(0, 0, width, height)
				  mipmapLevel:0];

	// Write to VRAM (use BGRA template specialization to match Metal's BGRA format).
	// WriteFramebuffer clamps finalClip to width/height itself.
	WriteFramebuffer<2, 1, 0, 3>(width, height, (u8 *)fbReadback.data(), rendContext->fb_W_SOF1 & VRAM_MASK,
			rendContext->fb_W_CTRL, rendContext->fb_W_LINESTRIDE * 8, finalClip);

	// Both textures are owned by the cache now -- just drop the local refs.
	scaledTexture = nil;
	readableTexture = nil;
}

void MetalScreenDrawer::EndRenderPass() {
	if (!renderPassStarted)
		return;

	[currentEncoder endEncoding];
	currentEncoder = nil;

	if (emulateFramebuffer)
	{
		// scaleAndWriteFramebuffer will handle EndFrameAndWait internally
		scaleAndWriteFramebuffer(framebuffers[GetCurrentImage()]);
	}
	else
	{
		commandPool->EndFrame();
		aspectRatio = getOutputFramebufferAspectRatio();
	}
	MetalDrawer::EndRenderPass();
	frameRendered = true;
}
