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

#include "metal_shaders.h"

#include "metal_context.h"

static const char* metalErrorString(NSError* error)
{
	if (error == nil)
		return "no NSError (Metal specialization failed silently)";
	const char* desc = error.localizedDescription.UTF8String;
	return desc ? desc : "unknown Metal error";
}

static const char VertexShaderSource[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant bool pp_gouraud [[function_constant(0)]];
constant bool div_pos_z [[function_constant(1)]];
constant bool pp_area1 [[function_constant(2)]];   // two-volume: use the second parameter set

constant bool is_flat = pp_gouraud == 0;
constant bool is_not_flat = !is_flat;

struct VertexShaderUniforms
{
	float4x4 ndc_mat;
};

struct VertexIn
{
	float4 in_pos [[attribute(0)]];
	float4 in_base [[attribute(1)]];
	float4 in_offs [[attribute(2)]];
	float2 in_uv [[attribute(3)]];
	float4 in_base1 [[attribute(4)]];
	float4 in_offs1 [[attribute(5)]];
	float2 in_uv1 [[attribute(6)]];
};

struct VertexOut
{
	float4 flat_vtx_base [[flat, function_constant(is_flat)]];
	float4 flat_vtx_offs [[flat, function_constant(is_flat)]];
	float4 vtx_base [[function_constant(is_not_flat)]];
	float4 vtx_offs [[function_constant(is_not_flat)]];
	float3 vtx_uv;
	float4 position [[position, invariant]];
};

struct VertexOutOIT
{
	float4 flat_vtx_base [[flat, function_constant(is_flat)]];
	float4 flat_vtx_offs [[flat, function_constant(is_flat)]];
	float4 vtx_base [[function_constant(is_not_flat)]];
	float4 vtx_offs [[function_constant(is_not_flat)]];
	float3 vtx_uv;
	uint vtx_index [[flat]];
	float4 position [[position, invariant]];
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant VertexShaderUniforms& uniforms [[buffer(0)]])
{
	float4 vpos = uniforms.ndc_mat * in.in_pos;
	const float4 v_base = pp_area1 ? in.in_base1 : in.in_base;
	const float4 v_offs = pp_area1 ? in.in_offs1 : in.in_offs;
	const float2 v_uv = pp_area1 ? in.in_uv1 : in.in_uv;

	if (div_pos_z) {
		vpos /= vpos.z;
		vpos.z = vpos.w;
	}

	VertexOut out = {};
	if (is_flat) {
		out.flat_vtx_base = v_base;
		out.flat_vtx_offs = v_offs;
	} else {
		out.vtx_base = v_base;
		out.vtx_offs = v_offs;
	}
	out.vtx_uv = float3(v_uv, vpos.z);

	if (pp_gouraud && !div_pos_z) {
		if (is_flat) {
			out.flat_vtx_base *= vpos.z;
			out.flat_vtx_offs *= vpos.z;
		} else {
			out.vtx_base *= vpos.z;
			out.vtx_offs *= vpos.z;
		}
	}

	if (!div_pos_z) {
		out.vtx_uv.xy *= vpos.z;
		vpos.w = 1.0;
		vpos.z = 0.0;
	}

	out.position = vpos;

	return out;
}

vertex VertexOutOIT vs_main_oit(VertexIn in [[stage_in]], constant VertexShaderUniforms& uniforms [[buffer(0)]],
								constant int& poly_number [[buffer(1)]], uint vid [[vertex_id]])
{
	float4 vpos = uniforms.ndc_mat * in.in_pos;
	const float4 v_base = pp_area1 ? in.in_base1 : in.in_base;
	const float4 v_offs = pp_area1 ? in.in_offs1 : in.in_offs;
	const float2 v_uv = pp_area1 ? in.in_uv1 : in.in_uv;

	if (div_pos_z) {
		vpos /= vpos.z;
		vpos.z = vpos.w;
	}

	VertexOutOIT out = {};
	if (is_flat) {
		out.flat_vtx_base = v_base;
		out.flat_vtx_offs = v_offs;
	} else {
		out.vtx_base = v_base;
		out.vtx_offs = v_offs;
	}
	out.vtx_uv = float3(v_uv, vpos.z);

	if (pp_gouraud && !div_pos_z) {
		if (is_flat) {
			out.flat_vtx_base *= vpos.z;
			out.flat_vtx_offs *= vpos.z;
		} else {
			out.vtx_base *= vpos.z;
			out.vtx_offs *= vpos.z;
		}
	}

	if (!div_pos_z) {
		out.vtx_uv.xy *= vpos.z;
		vpos.w = 1.0;
		vpos.z = 0.0;
	}

	out.position = vpos;
	out.vtx_index = uint(poly_number) + vid;

	return out;
}
)";

static const char FragmentShaderSource[] = R"(
#include <metal_stdlib>
#include <metal_atomic>
#define PI 3.1415926

using namespace metal;

// OIT per-pixel linked-list element, gated under pp_oit_mode. Layout
// must match the `Pixel` struct used by the resolve pass in
// metal_oit_shaders.h (16 B: packed RGBA8 color, depth, seq_num, next).
struct OITPixel {
	uint  color;
	float depth;
	uint  seq_num;
	uint  next;
};

struct OITViewportUniforms {
	uint viewportWidth;
	uint viewportHeight;
	uint pixelBufferSize;
};

constant bool cp_alpha_test [[function_constant(0)]];
constant bool pp_clip_inside [[function_constant(1)]];
constant bool pp_use_alpha [[function_constant(2)]];
constant bool pp_texture [[function_constant(3)]];
constant bool pp_ignore_tex_a [[function_constant(4)]];
constant int pp_shad_instr [[function_constant(5)]];
constant bool pp_offset [[function_constant(6)]];
constant int pp_fog_ctrl [[function_constant(7)]];
constant bool pp_gouraud [[function_constant(8)]];
constant bool pp_bump_map [[function_constant(9)]];
constant bool color_clamping [[function_constant(10)]];
constant bool pp_trilinear [[function_constant(11)]];
constant int pp_palette [[function_constant(12)]];
constant bool div_pos_z [[function_constant(13)]];
constant bool dithering [[function_constant(14)]];
// When true, fs_main appends a fragment to the OIT per-pixel linked list
// instead of writing the colour to the framebuffer.
constant bool pp_oit_mode [[function_constant(15)]];

constant bool has_fog_table = pp_fog_ctrl != 2;
constant bool has_palette = pp_palette != 0;
constant bool is_flat = pp_gouraud == 0;
constant bool is_not_flat = !is_flat;

struct FragmentShaderUniforms
{
	float4 color_clamp_min;
	float4 color_clamp_max;
	float4 sp_fog_col_ram;
	float4 sp_fog_col_vert;
	float4 dither_divisor;
	float cp_alpha_test_value;
	float sp_fog_density;
};

struct PushBlock
{
	float4 clip_test;
	float trilinear_alpha;
	float palette_index;
	float2 pad;
};

struct VertexOut
{
	float4 flat_vtx_base [[flat, function_constant(is_flat)]];
	float4 flat_vtx_offs [[flat, function_constant(is_flat)]];
	float4 vtx_base [[function_constant(is_not_flat)]];
	float4 vtx_offs [[function_constant(is_not_flat)]];
	float3 vtx_uv;
	uint vtx_index [[flat, function_constant(pp_oit_mode)]];
	float4 position [[position]];
};

struct FragmentOut
{
	float4 color [[color(0)]];
	float depth [[depth(any)]];
};

float fog_mode2(float w, constant FragmentShaderUniforms& uniforms,
				texture2d<float> fog_table, sampler fog_table_sampler)
{
	float z = 0.0;

	if (div_pos_z) {
		z = clamp(uniforms.sp_fog_density / w, 1.0, 255.9999);
	} else {
		z = clamp(uniforms.sp_fog_density * w, 1.0, 255.9999);
	}

	float exp = floor(log2(z));
	float m = z * 16.0 / powr(2.0, exp) - 16.0;
	float idx = floor(m) + exp * 16.0 + 0.5;
	float4 fog_coef = fog_table.sample(fog_table_sampler, float2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
	return fog_coef.r;
}

float4 color_clamp(float4 col, constant FragmentShaderUniforms& uniforms)
{
	if (color_clamping)
	{
		return clamp(col, uniforms.color_clamp_min, uniforms.color_clamp_max);
	} else {
		return col;
	}
}

float4 get_palette_entry(texture2d<float> palette, sampler palette_sampler,
						 float col_idx, constant PushBlock& push_constants)
{
	float2 c = float2(col_idx * 255.0 / 1023.0 + push_constants.palette_index, 0.5);
	return palette.sample(palette_sampler, c);
}

float4 palette_pixel(texture2d<float> texture, sampler texture_sampler,
					 texture2d<float> palette, sampler palette_sampler,
					 float3 coords, constant PushBlock& push_constants)
{
	if (div_pos_z) {
		return get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, coords.xy).r, push_constants);
	} else {
		return get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, float2(coords.xy / coords.z)).r, push_constants);
	}
}

float4 palette_pixel_bilinear(texture2d<float> texture, sampler texture_sampler,
							  texture2d<float> palette, sampler palette_sampler,
							  float3 coords, constant PushBlock& push_constants)
{
	if (!div_pos_z) {
		coords.xy /= coords.z;
	}

	float2 tex_size = float2(texture.get_width(), texture.get_height());
	float2 pix_coord = coords.xy * tex_size - 0.5; // Coordinates of top left pixel
	float2 origin_pix_coords = floor(pix_coord);

	float2 sample_uv = (origin_pix_coords + 0.5) / tex_size; // UV coordinates of center of top left pixel

	// Sample from all surrounding texels
	float4 c00 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv).r, push_constants);
	float4 c01 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv, int2(0, 1)).r, push_constants);
	float4 c11 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv, int2(1, 1)).r, push_constants);
	float4 c10 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv, int2(1, 0)).r, push_constants);

	float2 weight = pix_coord - origin_pix_coords;

	// Bi-linear mixing
	float4 temp0 = mix(c00, c10, weight.x);
	float4 temp1 = mix(c01, c11, weight.x);
	return mix(temp0, temp1, weight.y);
}

fragment FragmentOut fs_main(VertexOut in [[stage_in]], constant FragmentShaderUniforms& uniforms [[buffer(0)]],
							 constant PushBlock& push_constants [[buffer(1)]],
							 texture2d<float> tex [[texture(0), function_constant(pp_texture)]], sampler tex_sampler [[sampler(0), function_constant(pp_texture)]],
							 texture2d<float> fog_table [[texture(2), function_constant(has_fog_table)]], sampler fog_table_sampler [[sampler(2), function_constant(has_fog_table)]],
							 texture2d<float> palette [[texture(3), function_constant(has_palette)]], sampler palette_sampler [[sampler(3), function_constant(has_palette)]],
							 // OIT collect bindings — only required when pp_oit_mode is true.
							 // The drawer must bind buffers 2-5 on the collect render encoder for the
							 // collect pass to function correctly. Non-OIT pipelines never see
							 // these parameters (function_constant gating elides them entirely).
							 device atomic_uint*       oit_counter   [[buffer(2), function_constant(pp_oit_mode)]],
							 device OITPixel*          oit_pixels    [[buffer(3), function_constant(pp_oit_mode)]],
							 device atomic_uint*       oit_head      [[buffer(4), function_constant(pp_oit_mode)]],
							 constant OITViewportUniforms& oit_vp    [[buffer(5), function_constant(pp_oit_mode)]])
{
	// Clip inside the box
	if (pp_clip_inside) {
		if (in.position.x >= push_constants.clip_test.x && in.position.x <= push_constants.clip_test.z
			&& in.position.y >= push_constants.clip_test.y && in.position.y <= push_constants.clip_test.w)
			discard_fragment();
	}

	float4 color;
	float4 offset;

	if (is_flat) {
		color = in.flat_vtx_base;
		offset = in.flat_vtx_offs;
	} else {
		color = in.vtx_base;
		offset = in.vtx_offs;
	}

	if (pp_gouraud && !div_pos_z) {
		color /= in.vtx_uv.z;
		offset /= in.vtx_uv.z;
	}

	if (!pp_use_alpha) {
		color.a = 1.0;
	}

	if (pp_fog_ctrl == 3) {
		color = float4(uniforms.sp_fog_col_ram.rgb, fog_mode2(in.vtx_uv.z, uniforms, fog_table, fog_table_sampler));
	}

	if (pp_texture) {
		float4 tex_col;

		if (pp_palette == 0) {
			if (div_pos_z) {
				tex_col = tex.sample(tex_sampler, in.vtx_uv.xy);
			} else {
				tex_col = tex.sample(tex_sampler, float2(in.vtx_uv.xy / in.vtx_uv.z));
			}
		} else {
			if (pp_palette == 1) {
				tex_col = palette_pixel(tex, tex_sampler, palette, palette_sampler, in.vtx_uv, push_constants);
			} else {
				tex_col = palette_pixel_bilinear(tex, tex_sampler, palette, palette_sampler, in.vtx_uv, push_constants);
			}
		}

		if (pp_bump_map) {
			float s = PI / 2.0 * (tex_col.a * 15.0 * 16.0 + tex_col.r * 15.0) / 255.0;
			float r = 2.0 * PI * (tex_col.g * 15.0 * 16.0 + tex_col.b * 15.0) / 255.0;
			tex_col.a = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0 * PI * offset.b), 0.0, 1.0);
			tex_col.rgb = float3(1.0, 1.0, 1.0);
		} else {
			if (pp_ignore_tex_a)
				tex_col.a = 1.0;
		}

		if (pp_shad_instr == 0) {
			color = tex_col;
		} else if (pp_shad_instr == 1) {
			color.rgb *= tex_col.rgb;
			color.a = tex_col.a;
		} else if (pp_shad_instr == 2) {
			color.rgb = mix(color.rgb, tex_col.rgb, tex_col.a);
		} else if (pp_shad_instr == 3) {
			color *= tex_col;
		}

		if (pp_offset && !pp_bump_map) {
			color.rgb += offset.rgb;
		}
	}

	color = color_clamp(color, uniforms);

	if (pp_fog_ctrl == 0) {
		color.rgb = mix(color.rgb, uniforms.sp_fog_col_ram.rgb, fog_mode2(in.vtx_uv.z, uniforms, fog_table, fog_table_sampler));
	}

	if (pp_fog_ctrl == 1 && pp_offset && !pp_bump_map) {
		color.rgb = mix(color.rgb, uniforms.sp_fog_col_vert.rgb, offset.a);
	}

	if (pp_trilinear)
		color *= push_constants.trilinear_alpha;

	if (cp_alpha_test) {
		color.a = round(color.a * 255.0) / 255.0;
		if (uniforms.cp_alpha_test_value > color.a)
			discard_fragment();
		color.a = 1.0;
	}

	float w;

	if (div_pos_z) {
		w = 100000.0 / in.vtx_uv.z;
	} else {
		w = 100000.0 * in.vtx_uv.z;
	}

	float depth = log2(1.0 + max(w, -0.999999)) / 34.0;

	if (dithering) {
		constexpr float dither_table[16] = {
			5, 13,  7, 15,
			9,  1, 11,  3,
			6, 14,  4, 12,
			10, 2,  8,  0
		};

		float r = dither_table[int(fmod(in.position.y, 4.0)) * 4 + int(fmod(in.position.x, 4.0))];
		float4 dv = float4(r, r, r, 1.0) / uniforms.dither_divisor;
		color = clamp(floor(color * 255 + dv) / 255, 0, 1);
	}

	if (pp_oit_mode) {
		// OIT collect path. The colour computed above is the per-poly
		// fragment colour we want to deposit into the per-pixel linked
		// list; the resolve pass will sort by depth and back-to-front
		// blend.
		//
		// Step 1: atomically allocate a slot in the pixel buffer.
		uint idx = atomic_fetch_add_explicit(oit_counter, 1u, memory_order_relaxed);
		if (idx >= oit_vp.pixelBufferSize) {
			// Buffer overflow — drop the fragment. MSL's
			// discard_fragment() doesn't early-exit like GLSL's
			// `discard`, so we must guard the writes below as well.
			// Matches the Vulkan OIT behaviour at
			// core/rend/vulkan/oit/oit_shaders.cpp:123.
			discard_fragment();
			return FragmentOut { float4(0.0), depth };
		}

		// Step 2: compute the per-pixel head-pointer cell.
		uint x = uint(in.position.x);
		uint y = uint(in.position.y);
		uint cell = x + y * oit_vp.viewportWidth;

		// Step 3: swap our slot index into the head; the previous head
		// becomes our `next` link.
		uint prev_head = atomic_exchange_explicit(&oit_head[cell], idx, memory_order_relaxed);

		// Step 4: pack the colour (RGBA8) and write the pixel record.
		float4 c = clamp(color, 0.0, 1.0);
		uint packed = (uint(c.r * 255.0) << 24)
					| (uint(c.g * 255.0) << 16)
					| (uint(c.b * 255.0) <<  8)
					|  uint(c.a * 255.0);

		oit_pixels[idx].color   = packed;
		oit_pixels[idx].depth   = depth;
		oit_pixels[idx].seq_num = in.vtx_index;
		oit_pixels[idx].next    = prev_head;

		// The pipeline's colour writeMask is None for the OIT collect
		// pass — the return value is discarded. We still return a valid
		// FragmentOut so Metal can validate the function signature.
		return FragmentOut { float4(0.0), depth };
	}

	return FragmentOut { color, depth };
}
)";

static const char BlitShader[] = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
	float4 position [[position]];
	float2 texCoord;
};

vertex VertexOut vs_main(uint vertexID [[vertex_id]]) {
	// Predefined positions and texture coordinates for a full-screen quad
	float4 positions[4] = {
		float4(-1.0, -1.0, 0.0, 1.0), // Bottom-left
		float4( 1.0, -1.0, 0.0, 1.0), // Bottom-right
		float4(-1.0,  1.0, 0.0, 1.0), // Top-left
		float4( 1.0,  1.0, 0.0, 1.0)  // Top-right
	};

	float2 texCoords[4] = {
		float2(0.0, 1.0), // Bottom-left
		float2(1.0, 1.0), // Bottom-right
		float2(0.0, 0.0), // Top-left
		float2(1.0, 0.0)  // Top-right
	};

	VertexOut out;
	out.position = positions[vertexID];
	out.texCoord = texCoords[vertexID];
	return out;
}

fragment float4 fs_main(VertexOut in [[stage_in]],
							  texture2d<float> sourceTexture [[texture(0)]]) {
	constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);
	return sourceTexture.sample(textureSampler, in.texCoord);
}
)";

static const char ModVolShaderSource[] = R"(
#include <metal_stdlib>

using namespace metal;

constant bool div_pos_z [[function_constant(0)]];

struct VertexShaderUniforms
{
	float4x4 ndc_mat;
};

struct VertexIn
{
	float4 in_pos [[attribute(0)]];
};

struct VertexOut
{
	float depth;
	float4 position [[position]];
};

struct FragmentOut
{
	float4 color [[color(0)]];
	float depth [[depth(any)]];
};

// Mirrors MetalModVolPushBlock (metal_drawer.h): 32 bytes, float4-aligned.
struct PushBlock
{
	float4 clip_test;       // x0, y0, x1, y1 in framebuffer pixels (Inside clip only)
	float sp_shader_color;  // Final pass darkening alpha
	int clip_inside;        // 1 = discard fragments inside clip_test
	float2 pad;
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant VertexShaderUniforms& uniforms [[buffer(0)]]) {
	float4 vpos = uniforms.ndc_mat * in.in_pos;

	VertexOut out = {};

	if (div_pos_z) {
		vpos /= vpos.z;
		vpos.z = vpos.w;
		out.depth = vpos.w;
	} else {
		out.depth = vpos.z;
		vpos.w = 1.0;
		vpos.z = 0.0;
	}

	out.position = vpos;
	return out;
}

fragment FragmentOut fs_main(VertexOut in [[stage_in]],
							 constant PushBlock& push_constants [[buffer(1)]]) {
	FragmentOut out = {};

	// TA user tile clip, Inside mode: the volume doesn't mark pixels inside the
	// rect (Outside mode is the scissor). Same test as the main fs_main
	// pp_clip_inside branch; discard_fragment skips the stencil op below.
	if (push_constants.clip_inside != 0
		&& in.position.x >= push_constants.clip_test.x && in.position.x <= push_constants.clip_test.z
		&& in.position.y >= push_constants.clip_test.y && in.position.y <= push_constants.clip_test.w)
		discard_fragment();

	float w;

	if (div_pos_z) {
		w = 100000.0 / in.depth;
	} else {
		w = 100000.0 * in.depth;
	}

	out.depth = log2(1.0 + max(w, -0.999999)) / 34.0;
	out.color = float4(0.0, 0.0, 0.0, push_constants.sp_shader_color);
	return out;
}
)";

static const char QuadShaderSource[] = R"(
#include <metal_stdlib>

using namespace metal;

constant bool rotate_quad [[function_constant(0)]];
constant bool ignore_tex_alpha [[function_constant(1)]];

struct VertexIn
{
	float3 pos [[attribute(0)]];
	float2 uv [[attribute(1)]];
};

struct VertexOut
{
	float4 position [[position]];
	float2 uv;
};

struct PushBlock
{
	float4 color;
};

vertex VertexOut vs_main(VertexIn in [[stage_in]]) {
	VertexOut out = {};

	if (!rotate_quad) {
		out.position = float4(in.pos, 1.0);
	} else {
		out.position = float4(-in.pos.y, in.pos.x, in.pos.z, 1.0);
	}

	out.uv = in.uv;
	return out;
}

fragment float4 fs_main(VertexOut in [[stage_in]],
						constant PushBlock& push_constants [[buffer(0)]],
						texture2d<float> tex [[texture(0)]],
						sampler samp [[sampler(0)]]) {
	float4 color;

	if (ignore_tex_alpha) {
		color.rgb = push_constants.color.rgb * tex.sample(samp, in.uv).rgb;
		color.a = push_constants.color.a;
	} else {
		color = push_constants.color * tex.sample(samp, in.uv);
	}

	return color;
}
)";

static const char N2VertexShaderSource[] = R"(
#include <metal_stdlib>

using namespace metal;

constant bool pp_gouraud [[function_constant(0)]];

constant bool is_flat = pp_gouraud == 0;
constant bool is_not_flat = !is_flat;

struct VertexShaderUniforms
{
	float4x4 ndc_mat;
};

struct N2VertexShaderUniforms
{
	float4x4 mv_mat;
	float4x4 normal_mat;
	float4x4 proj_mat;
	int2 env_mapping;
	int bump_mapping;
	int poly_number;

	float2 gloss_coef;
	int2 constant_color;
};

struct N2VertexIn
{
	float4 in_pos [[attribute(0)]];
	float4 in_base [[attribute(1)]];
	float4 in_offs [[attribute(2)]];
	float2 in_uv [[attribute(3)]];
	float3 in_normal [[attribute(7)]];
};

struct VertexOut
{
	float4 flat_vtx_base [[flat, function_constant(is_flat)]];
	float4 flat_vtx_offs [[flat, function_constant(is_flat)]];
	float4 vtx_base [[function_constant(is_not_flat)]];
	float4 vtx_offs [[function_constant(is_not_flat)]];
	float3 vtx_uv;
	float4 position [[position, invariant]];
};

struct VertexOutOIT
{
	float4 flat_vtx_base [[flat, function_constant(is_flat)]];
	float4 flat_vtx_offs [[flat, function_constant(is_flat)]];
	float4 vtx_base [[function_constant(is_not_flat)]];
	float4 vtx_offs [[function_constant(is_not_flat)]];
	float3 vtx_uv;
	uint vtx_index [[flat]];
	float4 position [[position, invariant]];
};

#define PI 3.1415926

#define LMODE_SINGLE_SIDED 0
#define LMODE_DOUBLE_SIDED 1
#define LMODE_DOUBLE_SIDED_WITH_TOLERANCE 2
#define LMODE_SPECIAL_EFFECT 3
#define LMODE_THIN_SURFACE 4
#define LMODE_BUMP_MAP 5

#define ROUTING_SPEC_TO_OFFSET 1
#define ROUTING_DIFF_TO_OFFSET 2
#define ROUTING_ATTENUATION 1	// not handled
#define ROUTING_FOG 2			// not handled
#define ROUTING_ALPHA 4
#define ROUTING_SUB 8

struct N2Light
{
	float4 color;
	float4 direction; // For parallel/spot
	float4 position; // For spot/point

	int parallel;
	int routing;
	int dmode;
	int smode;

	int2 diffuse;
	int2 specular;

	float attn_dist_a;
	float attn_dist_b;
	float attn_angle_a; // For spot
	float attn_angle_b;

	int dist_attn_mode; // For spot/point
	int _pad1;
	int _pad2;
	int _pad3;
};

struct N2Lights
{
	N2Light lights[16];
	float4 ambient_base[2];
	float4 ambient_offset[2];
	int2 ambient_material_base;
	int2 ambient_material_offset;
	int light_count;
	int use_base_over;
	int bump_id0;
	int bump_id1;
};

void w_divide(thread float4& vpos, float4x4 ndc_mat, thread VertexOut& out)
{
	vpos = float4(vpos.xy / vpos.w, 1.0 / vpos.w, 1.0);
	vpos = ndc_mat * vpos;
	if (pp_gouraud == 1) {
		if (is_flat) {
			out.flat_vtx_base *= vpos.z;
			out.flat_vtx_offs *= vpos.z;
		} else {
			out.vtx_base *= vpos.z;
			out.vtx_offs *= vpos.z;
		}
	}
	out.vtx_uv = float3(out.vtx_uv.xy * vpos.z, vpos.z);
	vpos.w = 1.0;
	vpos.z = 0.0;
}

void w_divide_oit(thread float4& vpos, float4x4 ndc_mat, thread VertexOutOIT& out)
{
	vpos = float4(vpos.xy / vpos.w, 1.0 / vpos.w, 1.0);
	vpos = ndc_mat * vpos;
	if (pp_gouraud == 1) {
		if (is_flat) {
			out.flat_vtx_base *= vpos.z;
			out.flat_vtx_offs *= vpos.z;
		} else {
			out.vtx_base *= vpos.z;
			out.vtx_offs *= vpos.z;
		}
	}
	out.vtx_uv = float3(out.vtx_uv.xy * vpos.z, vpos.z);
	vpos.w = 1.0;
	vpos.z = 0.0;
}

void compute_colors(constant N2VertexShaderUniforms& n2_uniforms, constant N2Lights& n2_lights,
				   thread float4& base_col, thread float4& offset_col, int vol_idx, float3 position, float3 normal)
{
	if (n2_uniforms.constant_color[vol_idx] == 1)
		return;

	float3 diffuse = float3(0.0);
	float3 specular = float3(0.0);
	float diffuse_alpha = 0.0;
	float specular_alpha = 0.0;
	float3 reflect_dir = reflect(normalize(position), normal);
	const float BASE_FACTOR = 2.0;

	for (int i = 0; i < n2_lights.light_count; i++)
	{
		float3 light_dir; // direction to the light
		float3 light_color = n2_lights.lights[i].color.rgb;
		if (n2_lights.lights[i].parallel == 1)
		{
			light_dir = normalize(n2_lights.lights[i].direction.xyz);
		}
		else
		{
			light_dir = normalize(n2_lights.lights[i].position.xyz - position);
			if (n2_lights.lights[i].attn_dist_a != 1.0 || n2_lights.lights[i].attn_dist_b != 0.0)
			{
				float distance = length(n2_lights.lights[i].position.xyz - position);
				if (n2_lights.lights[i].dist_attn_mode == 0)
					distance = 1.0 / distance;
				light_color *= clamp(n2_lights.lights[i].attn_dist_b * distance + n2_lights.lights[i].attn_dist_a, 0.0, 1.0);
			}
			if (n2_lights.lights[i].attn_angle_a != 1.0 || n2_lights.lights[i].attn_angle_b != 0.0)
			{
				float3 spot_dir = n2_lights.lights[i].direction.xyz;
				float cos_angle = 1.0 - max(0.0, dot(light_dir, spot_dir));
				light_color *= clamp(cos_angle * n2_lights.lights[i].attn_angle_b + n2_lights.lights[i].attn_angle_a, 0.0, 1.0);
			}
		}
		if (n2_lights.lights[i].diffuse[vol_idx] == 1)
		{
			float factor = (n2_lights.lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (n2_lights.lights[i].dmode == LMODE_SINGLE_SIDED)
				factor *= max(dot(normal, light_dir), 0.0);
			else if (n2_lights.lights[i].dmode == LMODE_DOUBLE_SIDED)
				factor *= abs(dot(normal, light_dir));

			if ((n2_lights.lights[i].routing & ROUTING_ALPHA) != 0)
				diffuse_alpha += light_color.r * factor;
			else
			{
				if ((n2_lights.lights[i].routing & ROUTING_DIFF_TO_OFFSET) == 0)
					diffuse += light_color * factor * base_col.rgb;
				else
					specular += light_color * factor * base_col.rgb;
			}
		}
		if (n2_lights.lights[i].specular[vol_idx] == 1)
		{
			float factor = (n2_lights.lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (n2_lights.lights[i].smode == LMODE_SINGLE_SIDED)
				factor *= clamp(powr(max(dot(light_dir, reflect_dir), 0.0), n2_uniforms.gloss_coef[vol_idx]), 0.0, 1.0);
			else if (n2_lights.lights[i].smode == LMODE_DOUBLE_SIDED)
				factor *= clamp(powr(abs(dot(light_dir, reflect_dir)), n2_uniforms.gloss_coef[vol_idx]), 0.0, 1.0);

			if ((n2_lights.lights[i].routing & ROUTING_ALPHA) != 0)
				specular_alpha += light_color.r * factor;
			else
			{
				if ((n2_lights.lights[i].routing & ROUTING_SPEC_TO_OFFSET) == 0)
					diffuse += light_color * factor * offset_col.rgb;
				else
					specular += light_color * factor * offset_col.rgb;
			}
		}
	}
	// ambient light
	if (n2_lights.ambient_material_base[vol_idx] == 1)
		diffuse += n2_lights.ambient_base[vol_idx].rgb * base_col.rgb;
	else
		diffuse += n2_lights.ambient_base[vol_idx].rgb;
	if (n2_lights.ambient_material_offset[vol_idx] == 1)
		specular += n2_lights.ambient_offset[vol_idx].rgb * offset_col.rgb;
	else
		specular += n2_lights.ambient_offset[vol_idx].rgb;
	base_col.rgb = diffuse;
	offset_col.rgb = specular;

	base_col.a += diffuse_alpha;
	offset_col.a += specular_alpha;
	if (n2_lights.use_base_over == 1)
	{
		float4 overflow = max(base_col - float4(1.0), 0.0);
		offset_col += overflow;
	}
	base_col = clamp(base_col, 0.0, 1.0);
	offset_col = clamp(offset_col, 0.0, 1.0);
}

void compute_env_map(thread float3& uv, float3 position, float3 normal)
{
	// Spherical mapping
	// float3 r = reflect(normalize(position), normal);
	// float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
	// uv.xy += r.xy / m + 0.5;

	// Cheap env mapping
	uv.xy += (normal.xy / 2.0) + 0.5;
	uv.xy = clamp(uv.xy, 0.0, 1.0);
}

vertex VertexOut vs_main(N2VertexIn in [[stage_in]],
						 constant VertexShaderUniforms& uniforms [[buffer(0)]],
						 constant N2VertexShaderUniforms& n2_uniforms [[buffer(1)]],
						 constant N2Lights& n2_lights [[buffer(2)]])
{
	float4 vpos = n2_uniforms.mv_mat * in.in_pos;

	float3 vnorm = normalize(float3x3(n2_uniforms.normal_mat[0].xyz,
									  n2_uniforms.normal_mat[1].xyz,
									  n2_uniforms.normal_mat[2].xyz) *  in.in_normal);

	float4 base = in.in_base;
	float4 offset = in.in_offs;

	// TODO bump mapping
	if (n2_uniforms.bump_mapping == 0) {
		compute_colors(n2_uniforms, n2_lights,
					  base, offset, 0, vpos.xyz, vnorm);
		base += offset;
	}

	VertexOut out = {};

	if (is_flat) {
		out.flat_vtx_base = base;
		out.flat_vtx_offs = offset;
	} else {
		out.vtx_base = base;
		out.vtx_offs = offset;
	}

	out.vtx_uv.xy = in.in_uv;

	if (n2_uniforms.env_mapping[0] == 1)
		compute_env_map(out.vtx_uv, vpos.xyz, vnorm);

	vpos = n2_uniforms.proj_mat * vpos;
	w_divide(vpos, uniforms.ndc_mat, out);

	out.position = vpos;

	return out;
}

vertex VertexOutOIT vs_main_oit(N2VertexIn in [[stage_in]],
								constant VertexShaderUniforms& uniforms [[buffer(0)]],
								constant N2VertexShaderUniforms& n2_uniforms [[buffer(1)]],
								constant N2Lights& n2_lights [[buffer(2)]],
								uint vid [[vertex_id]])
{
	float4 vpos = n2_uniforms.mv_mat * in.in_pos;

	float3 vnorm = normalize(float3x3(n2_uniforms.normal_mat[0].xyz,
									  n2_uniforms.normal_mat[1].xyz,
									  n2_uniforms.normal_mat[2].xyz) *  in.in_normal);

	float4 base = in.in_base;
	float4 offset = in.in_offs;

	// TODO bump mapping
	if (n2_uniforms.bump_mapping == 0) {
		compute_colors(n2_uniforms, n2_lights,
					  base, offset, 0, vpos.xyz, vnorm);
		base += offset;
	}

	VertexOutOIT out = {};

	if (is_flat) {
		out.flat_vtx_base = base;
		out.flat_vtx_offs = offset;
	} else {
		out.vtx_base = base;
		out.vtx_offs = offset;
	}

	out.vtx_uv.xy = in.in_uv;

	if (n2_uniforms.env_mapping[0] == 1)
		compute_env_map(out.vtx_uv, vpos.xyz, vnorm);

	vpos = n2_uniforms.proj_mat * vpos;
	w_divide_oit(vpos, uniforms.ndc_mat, out);

	out.position = vpos;
	out.vtx_index = uint(n2_uniforms.poly_number) + vid;

	return out;
}
)";

extern const char MTLN2ModVolVertexShaderSource[] = R"(
#include <metal_stdlib>

using namespace metal;

struct VertexShaderUniforms
{
	float4x4 ndc_mat;
};

struct N2VertexShaderUniforms
{
	float4x4 mv_mat;
	float4x4 normal_mat;
	float4x4 proj_mat;
	int2 env_mapping;
	int bump_mapping;
	int poly_number;

	float2 gloss_coef;
	int2 constant_color;
};

struct VertexIn
{
	float4 in_pos [[attribute(0)]];
};

struct VertexOut
{
	float depth;
	float4 position [[position]];
};

void w_divide(thread float4& vpos, float4x4 ndc_mat, thread VertexOut& out)
{
	vpos = float4(vpos.xy / vpos.w, 1.0 / vpos.w, 1.0);
	vpos = ndc_mat * vpos;
	out.depth = vpos.z;
	vpos.w = 1.0;
	vpos.z = 0.0;
}

vertex VertexOut vs_main(VertexIn in [[stage_in]],
						 constant VertexShaderUniforms& uniforms [[buffer(0)]],
						 constant N2VertexShaderUniforms& n2_uniforms [[buffer(1)]])
{
	VertexOut out = {};

	float4 vpos = n2_uniforms.mv_mat * in.in_pos;
	vpos.z = min(vpos.z, -0.001);
	vpos = n2_uniforms.proj_mat * vpos;
	w_divide(vpos, uniforms.ndc_mat, out);

	out.position = vpos;

	return out;
}
)";

MetalShaders::MetalShaders() {
	auto device = MetalContext::Instance()->GetDevice();

	MTLCompileOptions* compileOptions = [[MTLCompileOptions alloc] init];
	[compileOptions setFastMathEnabled:YES];
	// Each poly compiles its own function-constant variant, so the same mesh
	// edge can be rasterized by two differently-optimized vertex functions
	// (OP face vs PT eyes/brows). Under fast math the position path
	// (w_divide's two divides) is free to codegen differently per variant,
	// opening sub-pixel cracks and Z poke-through on shared edges (Shenmue
	// faces). preserveInvariance + [[invariant]] on the position outputs pins
	// the position math to bit-identical results across variants; fast math
	// still applies to everything unmarked.
	if (@available(macOS 11.0, iOS 14.0, tvOS 14.0, *))
		[compileOptions setPreserveInvariance:YES];

	NSError* error = nil;
	fragmentShaderLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:FragmentShaderSource] options:compileOptions error:&error];
	fragmentShaderConstants = [[MTLFunctionConstantValues alloc] init];

	if (!fragmentShaderLibrary) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

	vertexShaderLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:VertexShaderSource] options:compileOptions error:&error];
	vertexShaderConstants = [[MTLFunctionConstantValues alloc] init];

	if (!vertexShaderLibrary) {
		ERROR_LOG(RENDERER, "vertexShaderLibrary compile failed: %s", metalErrorString(error));
		assert(false);
	}

	n2VertexShaderLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:N2VertexShaderSource] options:compileOptions error:&error];

	if (!n2VertexShaderLibrary) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

	blitShaderLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:BlitShader] options:compileOptions error:&error];

	if (!blitShaderLibrary) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

	blitVertexShader = [blitShaderLibrary newFunctionWithName:@"vs_main" constantValues:[[MTLFunctionConstantValues alloc] init] error:&error];

	if (!blitVertexShader) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

	blitFragmentShader = [blitShaderLibrary newFunctionWithName:@"fs_main" constantValues:[[MTLFunctionConstantValues alloc] init] error:&error];

	if (!blitFragmentShader) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

	modVolShaderLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:ModVolShaderSource] options:compileOptions error:&error];
	modVolShaderConstants = [[MTLFunctionConstantValues alloc] init];

	if (!modVolShaderLibrary) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

	n2ModVolVertexShaderLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:MTLN2ModVolVertexShaderSource] options:compileOptions error:&error];

	if (!n2ModVolVertexShaderLibrary) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

	quadShaderLibrary = [device newLibraryWithSource:[NSString stringWithUTF8String:QuadShaderSource] options:compileOptions error:&error];
	quadShaderConstants = [[MTLFunctionConstantValues alloc] init];

	if (!quadShaderLibrary) {
		ERROR_LOG(RENDERER, "%s", [[error localizedDescription] UTF8String]);
		assert(false);
	}

}

id<MTLFunction> MetalShaders::compileShader(const MetalVertexShaderParams &params) {
	MTLFunctionConstantValues* constants = [[MTLFunctionConstantValues alloc] init];
	[constants setConstantValue:&params.gouraud type:MTLDataTypeBool atIndex:0];

	NSError* error = nil;
	id<MTLFunction> function = nil;
	const char* entryPoint = params.oitCollect ? "vs_main_oit" : "vs_main";

	if (params.naomi2) {
		function = [n2VertexShaderLibrary newFunctionWithName:[NSString stringWithUTF8String:entryPoint]
											   constantValues:constants
														error:&error];
	} else {
		[constants setConstantValue:&params.divPosZ type:MTLDataTypeBool atIndex:1];
		[constants setConstantValue:&params.area1 type:MTLDataTypeBool atIndex:2];
		function = [vertexShaderLibrary newFunctionWithName:[NSString stringWithUTF8String:entryPoint]
											 constantValues:constants
													  error:&error];
	}

	if (!function) {
		ERROR_LOG(RENDERER,
				  "compileShader(vertex %s): %s (gouraud=%d naomi2=%d divPosZ=%d oitCollect=%d)",
				  entryPoint, metalErrorString(error),
				  params.gouraud, params.naomi2, params.divPosZ, params.oitCollect);
		return nil;
	}

	return function;
}

id<MTLFunction> MetalShaders::compileShader(const MetalFragmentShaderParams &params) {
	[fragmentShaderConstants setConstantValue:&params.alphaTest type:MTLDataTypeBool atIndex:0];
	[fragmentShaderConstants setConstantValue:&params.insideClipTest type:MTLDataTypeBool atIndex:1];
	[fragmentShaderConstants setConstantValue:&params.useAlpha type:MTLDataTypeBool atIndex:2];
	[fragmentShaderConstants setConstantValue:&params.texture type:MTLDataTypeBool atIndex:3];
	[fragmentShaderConstants setConstantValue:&params.ignoreTexAlpha type:MTLDataTypeBool atIndex:4];
	[fragmentShaderConstants setConstantValue:&params.shaderInstr type:MTLDataTypeInt atIndex:5];
	[fragmentShaderConstants setConstantValue:&params.offset type:MTLDataTypeBool atIndex:6];
	[fragmentShaderConstants setConstantValue:&params.fog type:MTLDataTypeInt atIndex:7];
	[fragmentShaderConstants setConstantValue:&params.gouraud type:MTLDataTypeBool atIndex:8];
	[fragmentShaderConstants setConstantValue:&params.bumpmap type:MTLDataTypeBool atIndex:9];
	[fragmentShaderConstants setConstantValue:&params.clamping type:MTLDataTypeBool atIndex:10];
	[fragmentShaderConstants setConstantValue:&params.trilinear type:MTLDataTypeBool atIndex:11];
	[fragmentShaderConstants setConstantValue:&params.palette type:MTLDataTypeInt atIndex:12];
	[fragmentShaderConstants setConstantValue:&params.divPosZ type:MTLDataTypeBool atIndex:13];
	[fragmentShaderConstants setConstantValue:&params.dithering type:MTLDataTypeBool atIndex:14];
	[fragmentShaderConstants setConstantValue:&params.oitMode type:MTLDataTypeBool atIndex:15];

	NSError* error = nil;

	id<MTLFunction> function = [fragmentShaderLibrary newFunctionWithName:@"fs_main" constantValues:fragmentShaderConstants error:&error];

	if (!function) {
		ERROR_LOG(RENDERER, "compileShader(fragment): %s", metalErrorString(error));
		return nil;
	}

	return function;
}

id<MTLFunction> MetalShaders::compileShader(const MetalModVolShaderParams &params) {
	[modVolShaderConstants setConstantValue:&params.divPosZ type:MTLDataTypeBool atIndex:0];

	NSError* error = nil;

	id<MTLFunction> function;

	if (params.naomi2) {
		function = [n2ModVolVertexShaderLibrary newFunctionWithName:@"vs_main" constantValues:[[MTLFunctionConstantValues alloc] init] error:&error];
	} else {
		function = [modVolShaderLibrary newFunctionWithName:@"vs_main" constantValues:modVolShaderConstants error:&error];
	}

	if (!function) {
		ERROR_LOG(RENDERER, "compileShader(modvol): %s", metalErrorString(error));
		return nil;
	}

	return function;
}

id<MTLFunction> MetalShaders::compileShader(bool divPosZ) {
	[modVolShaderConstants setConstantValue:&divPosZ type:MTLDataTypeBool atIndex:0];

	NSError* error = nil;

	id<MTLFunction> function = [modVolShaderLibrary newFunctionWithName:@"fs_main" constantValues:modVolShaderConstants error:&error];

	if (!function) {
		ERROR_LOG(RENDERER, "compileShader(modvol fragment): %s", metalErrorString(error));
		return nil;
	}

	return function;
}

id<MTLFunction> MetalShaders::compileQuadVertexShader(bool rotate)
{
	[quadShaderConstants setConstantValue:&rotate type:MTLDataTypeBool atIndex:0];

	NSError* error = nil;

	id<MTLFunction> function = [quadShaderLibrary newFunctionWithName:@"vs_main" constantValues:quadShaderConstants error:&error];
	if (!function) {
		ERROR_LOG(RENDERER, "compileShader(quad vertex): %s", metalErrorString(error));
		return nil;
	}

	return function;
}

id<MTLFunction> MetalShaders::compileQuadFragmentShader(bool ignoreTexAlpha)
{
	[quadShaderConstants setConstantValue:&ignoreTexAlpha type:MTLDataTypeBool atIndex:1];

	NSError* error = nil;

	id<MTLFunction> function = [quadShaderLibrary newFunctionWithName:@"fs_main" constantValues:quadShaderConstants error:&error];
	if (!function) {
		ERROR_LOG(RENDERER, "compileShader(quad fragment): %s", metalErrorString(error));
		return nil;
	}

	return function;
}
