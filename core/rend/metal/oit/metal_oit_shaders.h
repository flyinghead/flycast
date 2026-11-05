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
// MSL sources for the Metal OIT resolve and TR modvol passes.
// The collect pass uses fs_main with pp_oit_mode in metal_shaders.mm.
#pragma once

#include <string>

static const char kMetalOITShaderPrologue[] = R"(
#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

// OIT_RASTER_ORDER (a compile-option macro, see MetalOITFragmentOrdering):
// the TR modvol marks, the modvol sums and the resolve all run in one render
// encoder and touch the shaded pixel's a-buffer list; raster order group 0
// makes overlapping fragments access it in draw submission order. Every
// access in that encoder must be in the group for the ordering to hold.
#if OIT_RASTER_ORDER
#define OIT_ORDERED [[raster_order_group(0)]]
#else
#define OIT_ORDERED
#endif

constant constexpr uint kEOL = 0xFFFFFFFFu;
constant constexpr uint kMaxFragmentsPerPixel = 32u;

constant constexpr uint SHADOW_STENCIL = 0x40000000u;
constant constexpr uint SHADOW_ACC     = 0x80000000u;

constant constexpr int ZERO                 = 0;
constant constexpr int ONE                  = 1;
constant constexpr int OTHER_COLOR          = 2;
constant constexpr int INVERSE_OTHER_COLOR  = 3;
constant constexpr int SRC_ALPHA            = 4;
constant constexpr int INVERSE_SRC_ALPHA    = 5;
constant constexpr int DST_ALPHA            = 6;
constant constexpr int INVERSE_DST_ALPHA    = 7;

struct Pixel {
	uint  color;
	float depth;
	uint  seq_num;
	uint  next;
};

struct OITPolyParam {
	int tsp_isp_pcw;
	int tsp1;
};

struct ResolveUniforms {
	uint  viewportWidth;
	uint  viewportHeight;
	uint  pixelBufferSize;
	float shadeScale;
	float4 ditherDivisor;
	uint  applyDither;
};

struct ResolveVertexOut {
	float4 position [[position]];
};

inline uint getPolyNumber(Pixel pixel) { return (pixel.seq_num & 0x3FFFFFFFu) >> 17; }
inline uint getPolyIndex(Pixel pixel) { return pixel.seq_num & 0x3FFFFFFFu; }
inline bool isShadowed(Pixel pixel) { return (pixel.seq_num & SHADOW_ACC) == SHADOW_ACC; }
inline bool isTwoVolumes(OITPolyParam pp) { return pp.tsp1 != -1; }

inline int getTspForArea(OITPolyParam pp, bool area1) { return area1 ? pp.tsp1 : pp.tsp_isp_pcw; }
inline int getSrcBlendFunc(OITPolyParam pp, bool area1) { return (getTspForArea(pp, area1) >> 29) & 7; }
inline int getDstBlendFunc(OITPolyParam pp, bool area1) { return (getTspForArea(pp, area1) >> 26) & 7; }
inline bool getSrcSelect(OITPolyParam pp, bool area1) { return ((getTspForArea(pp, area1) >> 25) & 1) != 0; }
inline bool getDstSelect(OITPolyParam pp, bool area1) { return ((getTspForArea(pp, area1) >> 24) & 1) != 0; }
inline bool getShadowEnable(OITPolyParam pp) { return (pp.tsp_isp_pcw & 1) != 0; }

inline float4 unpackColors(uint u)
{
	return float4(
		float((u >> 24) & 255u) / 255.0,
		float((u >> 16) & 255u) / 255.0,
		float((u >>  8) & 255u) / 255.0,
		float( u        & 255u) / 255.0);
}

constant float kDitherTable[16] = {
	 5, 13,  7, 15,
	 9,  1, 11,  3,
	 6, 14,  4, 12,
	10,  2,  8,  0
};
)";

static const char kMetalOITResolveVertex[] = R"(
vertex ResolveVertexOut oit_resolve_vertex(uint vid [[vertex_id]])
{
	float2 pos[3] = { float2(-1.0, -1.0), float2( 3.0, -1.0), float2(-1.0,  3.0) };
	ResolveVertexOut out;
	out.position = float4(pos[vid], 0.0, 1.0);
	return out;
}
)";

static const char kMetalOITResolveFragment[] = R"(
inline int fillAndSortFragmentArray(uint2 coords,
									device Pixel *pixel_buffer,
									device atomic_uint *abuffer_pointer,
									constant ResolveUniforms &ub,
									thread uint *pixel_list)
{
	uint idx = atomic_exchange_explicit(&abuffer_pointer[coords.x + coords.y * ub.viewportWidth],
										kEOL, memory_order_relaxed);
	if (idx == kEOL)
		return 0;

	int count = 1;
	pixel_list[0] = idx;
	idx = pixel_buffer[idx].next;
	for (; idx != kEOL && count < int(kMaxFragmentsPerPixel); ++count) {
		float depth = pixel_buffer[idx].depth;
		uint index = getPolyIndex(pixel_buffer[idx]);
		int j = count - 1;
		while (j >= 0) {
			uint jIdx = pixel_list[j];
			float jdepth = pixel_buffer[jIdx].depth;
			uint jindex = getPolyIndex(pixel_buffer[jIdx]);
			if (jdepth > depth || (jdepth == depth && jindex > index))
			{
				pixel_list[j + 1] = pixel_list[j];
				--j;
			}
			else
				break;
		}
		pixel_list[j + 1] = idx;
		idx = pixel_buffer[idx].next;
	}
	return count;
}

inline float4 resolveAlphaBlend(float2 pos,
								device Pixel *pixel_buffer,
								device atomic_uint *abuffer_pointer,
								device const OITPolyParam *tr_poly_params,
								float4 opaqueColor,
								constant ResolveUniforms &ub)
{
	uint2 coords = uint2(pos);
	uint pixel_list[kMaxFragmentsPerPixel];
	int num_frag = fillAndSortFragmentArray(coords, pixel_buffer, abuffer_pointer, ub, pixel_list);
	if (num_frag == 0)
		discard_fragment();

	// The opaque background is the CURRENT content of the color attachment,
	// read via framebuffer fetch — Metal's equivalent of the Vulkan resolve
	// subpass's OP+PT color input attachment, see oit_drawer.cpp. Sampling
	// the ping-pong sibling here composited every TR-covered pixel over a
	// buffer no pass had written -- black -- which ate HUD sprites and
	// opaque-applied shadows under any translucency.
	// NOTE: this comment lives inside a raw string; a close-paren followed
	// by a double quote would terminate it.
	float4 finalColor = opaqueColor;
	float4 secondaryBuffer = float4(0.0);

	for (int i = 0; i < num_frag; ++i) {
		const Pixel pixel = pixel_buffer[pixel_list[i]];
		const OITPolyParam pp = tr_poly_params[getPolyNumber(pixel)];
		bool area1 = false;
		bool shadowed = false;
		if (isShadowed(pixel)) {
			if (isTwoVolumes(pp))
				area1 = true;
			else
				shadowed = true;
		}

		float4 srcColor;
		if (getSrcSelect(pp, area1))
			srcColor = secondaryBuffer;
		else {
			srcColor = unpackColors(pixel.color);
			if (shadowed)
				srcColor.rgb *= ub.shadeScale;
		}

		float4 dstColor = getDstSelect(pp, area1) ? secondaryBuffer : finalColor;
		float4 srcCoef = float4(0.0);
		float4 dstCoef = float4(0.0);

		switch (getSrcBlendFunc(pp, area1)) {
			case ZERO: srcCoef = float4(0.0); break;
			case ONE: srcCoef = float4(1.0); break;
			case OTHER_COLOR: srcCoef = dstColor; break;
			case INVERSE_OTHER_COLOR: srcCoef = float4(1.0) - dstColor; break;
			case SRC_ALPHA: srcCoef = float4(srcColor.a); break;
			case INVERSE_SRC_ALPHA: srcCoef = float4(1.0 - srcColor.a); break;
			case DST_ALPHA: srcCoef = float4(dstColor.a); break;
			case INVERSE_DST_ALPHA: srcCoef = float4(1.0 - dstColor.a); break;
		}
		switch (getDstBlendFunc(pp, area1)) {
			case ZERO: dstCoef = float4(0.0); break;
			case ONE: dstCoef = float4(1.0); break;
			case OTHER_COLOR: dstCoef = srcColor; break;
			case INVERSE_OTHER_COLOR: dstCoef = float4(1.0) - srcColor; break;
			case SRC_ALPHA: dstCoef = float4(srcColor.a); break;
			case INVERSE_SRC_ALPHA: dstCoef = float4(1.0 - srcColor.a); break;
			case DST_ALPHA: dstCoef = float4(dstColor.a); break;
			case INVERSE_DST_ALPHA: dstCoef = float4(1.0 - dstColor.a); break;
		}

		const float4 result = clamp(dstColor * dstCoef + srcColor * srcCoef, 0.0, 1.0);
		if (getDstSelect(pp, area1))
			secondaryBuffer = result;
		else
			finalColor = result;
	}

	if (ub.applyDither != 0u) {
		float r = kDitherTable[int(fmod(pos.y, 4.0)) * 4 + int(fmod(pos.x, 4.0))];
		float4 dv = float4(r, r, r, 1.0) / ub.ditherDivisor;
		finalColor = clamp(floor(finalColor * 255.0 + dv) / 255.0, 0.0, 1.0);
	}

	return finalColor;
}

fragment float4 oit_resolve_fragment(
	ResolveVertexOut in [[stage_in]],
	device Pixel *pixel_buffer [[buffer(1)]] OIT_ORDERED,
	device atomic_uint *abuffer_pointer [[buffer(2)]] OIT_ORDERED,
	constant ResolveUniforms &ub [[buffer(3)]],
	device const OITPolyParam *tr_poly_params [[buffer(4)]],
	texture2d<float> backgroundTex [[texture(0)]])
{
	// The opaque background arrives as a plain texture, NOT via [[color(0)]]
	// framebuffer fetch. Framebuffer fetch is a TBDR feature the iOS
	// Simulator's Metal does not implement: newRenderPipelineStateWithDescriptor
	// rejected this function with "reading from a rendertarget is not
	// supported", so CreateResolvePipeline returned nil, drawResolve bailed
	// out, and every collected translucent fragment was silently dropped —
	// the Dreamcast BIOS boot swirl (370 translucent polys over 1 opaque
	// background poly) rendered as a bare background. The drawer now blits
	// the opaque attachment into a scratch texture between the collect and
	// resolve encoders and binds it here, which works on every Metal device.
	const float4 opaqueColor = backgroundTex.read(uint2(in.position.xy));
	return resolveAlphaBlend(in.position.xy, pixel_buffer, abuffer_pointer, tr_poly_params, opaqueColor, ub);
}
)";

static const char kMetalOITTrModvolFragment[] = R"(
constant int MV_MODE [[function_constant(0)]];
constant bool DIV_POS_Z [[function_constant(1)]];

struct ModVolVertexOut {
	float depth;
	float4 position [[position]];
};

inline float modvolLogDepth(float depth)
{
	float w = DIV_POS_Z ? (100000.0 / depth) : (100000.0 * depth);
	return log2(1.0 + max(w, -0.999999)) / 34.0;
}

// Mirrors MetalModVolPushBlock (metal_drawer.h): 32 bytes, float4-aligned.
struct ModVolClip {
	float4 clip_test;       // x0, y0, x1, y1 in framebuffer pixels (Inside clip only)
	float sp_shader_color;  // unused here (stencil passes' darkening alpha)
	int clip_inside;        // 1 = skip fragments inside clip_test
	float2 pad;
};

fragment void oit_tr_modvol_fragment(
	ModVolVertexOut in [[stage_in]],
	device Pixel *pixel_buffer [[buffer(1)]] OIT_ORDERED,
	device atomic_uint *abuffer_pointer [[buffer(2)]] OIT_ORDERED,
	constant ResolveUniforms &ub [[buffer(3)]],
	device const OITPolyParam *tr_poly_params [[buffer(4)]],
	constant ModVolClip &clip [[buffer(5)]])
{
	// TA user tile clip, Inside mode: the volume doesn't mark pixels inside the
	// rect (Outside mode is the scissor). No outputs, so a plain return suffices.
	if (clip.clip_inside != 0
		&& in.position.x >= clip.clip_test.x && in.position.x <= clip.clip_test.z
		&& in.position.y >= clip.clip_test.y && in.position.y <= clip.clip_test.w)
		return;

	const float frag_depth = modvolLogDepth(in.depth);
	const uint2 coords = uint2(in.position.xy);
	uint idx = atomic_load_explicit(&abuffer_pointer[coords.x + coords.y * ub.viewportWidth],
									memory_order_relaxed);
	int list_len = 0;
	while (idx != kEOL && list_len < int(kMaxFragmentsPerPixel))
	{
		const Pixel pixel = pixel_buffer[idx];
		const OITPolyParam pp = tr_poly_params[getPolyNumber(pixel)];
		if (getShadowEnable(pp))
		{
			device atomic_uint *seq_num = (device atomic_uint *)&pixel_buffer[idx].seq_num;
			if (MV_MODE == 0) {
				if (frag_depth >= pixel.depth)
					atomic_fetch_xor_explicit(seq_num, SHADOW_STENCIL, memory_order_relaxed);
			} else if (MV_MODE == 1) {
				if (frag_depth >= pixel.depth)
					atomic_fetch_or_explicit(seq_num, SHADOW_STENCIL, memory_order_relaxed);
			} else if (MV_MODE == 2) {
				const uint prev_val = atomic_fetch_and_explicit(seq_num, ~(SHADOW_STENCIL), memory_order_relaxed);
				if ((prev_val & (SHADOW_STENCIL | SHADOW_ACC)) == SHADOW_STENCIL)
					pixel_buffer[idx].seq_num = (pixel.seq_num & ~(1u << 31u)) | (1u << 31u);
			} else if (MV_MODE == 3) {
				const uint prev_val = atomic_fetch_and_explicit(seq_num, ~(SHADOW_STENCIL | SHADOW_ACC), memory_order_relaxed);
				if ((prev_val & (SHADOW_STENCIL | SHADOW_ACC)) == SHADOW_ACC)
					pixel_buffer[idx].seq_num = (pixel.seq_num & ~(1u << 31u)) | (1u << 31u);
			}
		}
		idx = pixel.next;
		++list_len;
	}
}
)";

// Tile-memory opaque depth, render passes after the first: the depth
// attachment is loaded with the earlier passes' depth, but the tile copy in
// colour attachment 1 is memoryless and starts cleared. This full-screen
// draw (oit_resolve_vertex) seeds it from a blitted copy of the depth.
static const char kMetalOITSeedOpaqueDepthFragment[] = R"(
struct SeedOpaqueDepthOut {
	float4 color [[color(0)]];
	float  opaque_depth [[color(1)]];
};

fragment SeedOpaqueDepthOut oit_seed_opaque_depth(ResolveVertexOut in [[stage_in]],
												  depth2d<float> depth_copy [[texture(4)]])
{
	SeedOpaqueDepthOut out;
	out.color = float4(0.0);
	out.opaque_depth = depth_copy.read(uint2(in.position.xy));
	return out;
}
)";

inline std::string GetMetalOITShaderSource()
{
	std::string src;
	src += kMetalOITShaderPrologue;
	src += kMetalOITResolveVertex;
	src += kMetalOITResolveFragment;
	src += kMetalOITTrModvolFragment;
	src += kMetalOITSeedOpaqueDepthFragment;
	return src;
}
