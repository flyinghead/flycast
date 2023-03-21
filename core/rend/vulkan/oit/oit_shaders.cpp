/*
    Created on: Nov 6, 2019

	Copyright 2019 flyinghead

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
#include "oit_shaders.h"
#include "../compiler.h"
#include "rend/gl4/glsl.h"
#include "cfg/option.h"

static const char OITVertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 ndcMat;
} uniformBuffer;

layout (push_constant) uniform constants
{
	layout(offset = 96) int polyNumber;
} pushConstants;

layout (location = 0) in vec4         in_pos;
layout (location = 1) in uvec4        in_base;
layout (location = 2) in uvec4        in_offs;
layout (location = 3) in mediump vec2 in_uv;
layout (location = 4) in uvec4        in_base1;						// New for OIT, only for OP/PT with 2-volume
layout (location = 5) in uvec4        in_offs1;
layout (location = 6) in mediump vec2 in_uv1;

layout (location = 0) INTERPOLATION out highp vec4 vtx_base;
layout (location = 1) INTERPOLATION out highp vec4 vtx_offs;
layout (location = 2) out highp vec3 vtx_uv;
layout (location = 3) INTERPOLATION out highp vec4 vtx_base1;		// New for OIT, only for OP/PT with 2-volume
layout (location = 4) INTERPOLATION out highp vec4 vtx_offs1;
layout (location = 5) out highp vec2 vtx_uv1;
layout (location = 6) flat out uint vtx_index;

void main()
{
	vec4 vpos = uniformBuffer.ndcMat * in_pos;
#if DIV_POS_Z == 1
	vpos /= vpos.z;
	vpos.z = vpos.w;
#endif
	vtx_base = vec4(in_base) / 255.0;
	vtx_offs = vec4(in_offs) / 255.0;
	vtx_uv = vec3(in_uv, vpos.z);
	vtx_base1 = vec4(in_base1) / 255.0;
	vtx_offs1 = vec4(in_offs1) / 255.0;
	vtx_uv1 = in_uv1;
#if pp_Gouraud == 1 && DIV_POS_Z != 1
	vtx_base *= vpos.z;
	vtx_offs *= vpos.z;
	vtx_base1 *= vpos.z;
	vtx_offs1 *= vpos.z;
#endif
	vtx_index = uint(pushConstants.polyNumber) + uint(gl_VertexIndex);
#if DIV_POS_Z != 1
	vtx_uv.xy *= vpos.z;
	vtx_uv1 *= vpos.z;
	vpos.w = 1.0;
	vpos.z = 0.0;
#endif
	gl_Position = vpos;
}
)";

static const char OITShaderHeader[] = R"(
precision highp float;

layout (std140, set = 0, binding = 1) uniform FragmentShaderUniforms
{
	vec4 colorClampMin;
	vec4 colorClampMax;
	vec4 sp_FOG_COL_RAM;
	vec4 sp_FOG_COL_VERT;
	float cp_AlphaTestValue;
	float sp_FOG_DENSITY;
	float shade_scale_factor;
	uint pixelBufferSize;
	uint viewportWidth;
} uniformBuffer;

layout(set = 0, binding = 9) buffer abufferPointer_ {
	uint pointers[];
} abufferPointer;

layout(set = 0, binding = 8) buffer PixelCounter_ {
	uint buffer_index;
} PixelCounter;
)"
OIT_POLY_PARAM
R"(

layout (set = 0, binding = 7, std430) coherent restrict buffer PixelBuffer_ {
	Pixel pixels[];
} PixelBuffer;

uint getNextPixelIndex()
{
	uint index = atomicAdd(PixelCounter.buffer_index, 1);
	// we should be able to simply use PixelBuffer.pixels.length()
	// but a regression in the adreno 600 driver (v502) forces us
	// to use a uniform.
	if (index >= uniformBuffer.pixelBufferSize)
		// Buffer overflow
		discard;
	
	return index;
}

layout (set = 0, binding = 3, std430) readonly buffer TrPolyParamBuffer {
	PolyParam tr_poly_params[];
} TrPolyParam;

)";

static const char OITFragmentShaderSource[] = R"(
#define PI 3.1415926

#define PASS_DEPTH 0
#define PASS_COLOR 1
#define PASS_OIT 2

#if PASS == PASS_DEPTH || PASS == PASS_COLOR
layout (location = 0) out vec4 FragColor;
#define gl_FragColor FragColor
#endif

#if pp_TwoVolumes == 1
#define IF(x) if (x)
#else
#define IF(x)
#endif

layout (push_constant) uniform pushBlock
{
	vec4 clipTest;
	ivec4 blend_mode0;
	float trilinearAlpha;
	float palette_index;

	// two volume mode
	ivec4 blend_mode1;
	int shading_instr0;
	int shading_instr1;
	int fog_control0;
	int fog_control1;
	int use_alpha0;
	int use_alpha1;
	int ignore_tex_alpha0;
	int ignore_tex_alpha1;
} pushConstants;

#if pp_Texture == 1
layout (set = 1, binding = 0) uniform sampler2D tex0;
#if pp_TwoVolumes == 1
layout (set = 1, binding = 1) uniform sampler2D tex1;
#endif
#endif
#if pp_Palette == 1
layout (set = 0, binding = 6) uniform sampler2D palette;
#endif

#if PASS == PASS_COLOR
layout (input_attachment_index = 0, set = 0, binding = 4) uniform usubpassInput shadow_stencil;
#endif
#if PASS == PASS_OIT
layout (input_attachment_index = 0, set = 0, binding = 5) uniform subpassInput DepthTex;
#endif

// Vertex input
layout (location = 0) INTERPOLATION in highp vec4 vtx_base;
layout (location = 1) INTERPOLATION in highp vec4 vtx_offs;
layout (location = 2) in highp vec3 vtx_uv;
layout (location = 3) INTERPOLATION in highp vec4 vtx_base1;			// new for OIT. Only if 2 vol
layout (location = 4) INTERPOLATION in highp vec4 vtx_offs1;
layout (location = 5) in highp vec2 vtx_uv1;
layout (location = 6) flat in uint vtx_index;

#if pp_FogCtrl != 2 || pp_TwoVolumes == 1
layout (set = 0, binding = 2) uniform sampler2D fog_table;

float fog_mode2(float w)
{
	float z = clamp(
#if DIV_POS_Z == 1
					uniformBuffer.sp_FOG_DENSITY / w
#else
					uniformBuffer.sp_FOG_DENSITY * w
#endif
													, 1.0, 255.9999);
	float exp = floor(log2(z));
	float m = z * 16.0 / pow(2.0, exp) - 16.0;
	float idx = floor(m) + exp * 16.0 + 0.5;
	vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
	return fog_coef.r;
}
#endif

vec4 colorClamp(vec4 col)
{
// TODO This can change in two-volume mode
#if ColorClamping == 1
	return clamp(col, uniformBuffer.colorClampMin, uniformBuffer.colorClampMax);
#else
	return col;
#endif
}

#if pp_Palette == 1

vec4 palettePixel(sampler2D tex, vec3 coords)
{
#if DIV_POS_Z == 1
	float texIdx = texture(tex, coords.xy).r;
#else
	float texIdx = textureProj(tex, coords).r;
#endif
	vec4 c = vec4(texIdx * 255.0 / 1023.0 + pushConstants.palette_index, 0.5, 0.0, 0.0);
	return texture(palette, c.xy);
}

#endif

void main()
{
	setFragDepth(vtx_uv.z);

	#if PASS == PASS_OIT
		// Manual depth testing
		float frontDepth = subpassLoad(DepthTex).r;
		if (gl_FragDepth < frontDepth)
			discard;
	#endif

	// Clip inside the box
	#if pp_ClipInside == 1
		if (gl_FragCoord.x >= pushConstants.clipTest.x && gl_FragCoord.x <= pushConstants.clipTest.z
				&& gl_FragCoord.y >= pushConstants.clipTest.y && gl_FragCoord.y <= pushConstants.clipTest.w)
			discard;
	#endif
	
	vec4 color = vtx_base;
	vec4 offset = vtx_offs;
	bool area1 = false;
	ivec2 cur_blend_mode = pushConstants.blend_mode0.xy;
	
	#if pp_TwoVolumes == 1
		bool cur_use_alpha = pushConstants.use_alpha0 != 0;
		bool cur_ignore_tex_alpha = pushConstants.ignore_tex_alpha0 != 0;
		int cur_shading_instr = pushConstants.shading_instr0;
		int cur_fog_control = pushConstants.fog_control0;
		#if PASS == PASS_COLOR
			uvec4 stencil = subpassLoad(shadow_stencil);
			if (stencil.r == 0x81u) {
				color = vtx_base1;
				offset = vtx_offs1;
				area1 = true;
				cur_blend_mode = pushConstants.blend_mode1.xy;
				cur_use_alpha = pushConstants.use_alpha1 != 0;
				cur_ignore_tex_alpha = pushConstants.ignore_tex_alpha1 != 0;
				cur_shading_instr = pushConstants.shading_instr1;
				cur_fog_control = pushConstants.fog_control1;
			}
		#endif
	#endif
	#if pp_Gouraud == 1 && DIV_POS_Z != 1
		color /= vtx_uv.z;
		offset /= vtx_uv.z;
	#endif

	#if pp_UseAlpha == 0 || pp_TwoVolumes == 1
		IF (!cur_use_alpha)
			color.a = 1.0;
	#endif
	#if pp_FogCtrl == 3 || pp_TwoVolumes == 1 // LUT Mode 2
		IF (cur_fog_control == 3)
			color = vec4(uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z));
	#endif
	#if pp_Texture==1
	{
		vec4 texcol;
		#if pp_TwoVolumes == 1
			if (area1)
				#if pp_Palette == 0
					#if DIV_POS_Z == 1
						texcol = texture(tex1, vtx_uv1);
					#else
						texcol = textureProj(tex1, vec3(vtx_uv1, vtx_uv.z));
					#endif
				#else
					texcol = palettePixel(tex1, vec3(vtx_uv1, vtx_uv.z));
				#endif
			else
		#endif
		#if pp_Palette == 0
			#if DIV_POS_Z == 1
				texcol = texture(tex0, vtx_uv.xy);
			#else
				texcol = textureProj(tex0, vtx_uv);
			#endif
		#else
				texcol = palettePixel(tex0, vtx_uv);
		#endif
		#if pp_BumpMap == 1
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0 * PI * offset.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1
				IF(cur_ignore_tex_alpha)
					texcol.a = 1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (uniformBuffer.cp_AlphaTestValue > texcol.a)
					discard;
				texcol.a = 1.0;
			#endif 
		#endif
		#if pp_ShadInstr == 0 || pp_TwoVolumes == 1 // DECAL
		IF(cur_shading_instr == 0)
		{
			color = texcol;
		}
		#endif
		#if pp_ShadInstr == 1 || pp_TwoVolumes == 1 // MODULATE
		IF(cur_shading_instr == 1)
		{
			color.rgb *= texcol.rgb;
			color.a = texcol.a;
		}
		#endif
		#if pp_ShadInstr == 2 || pp_TwoVolumes == 1 // DECAL ALPHA
		IF(cur_shading_instr == 2)
		{
			color.rgb = mix(color.rgb, texcol.rgb, texcol.a);
		}
		#endif
		#if pp_ShadInstr == 3 || pp_TwoVolumes == 1 // MODULATE ALPHA
		IF(cur_shading_instr == 3)
		{
			color *= texcol;
		}
		#endif
		
		#if pp_Offset == 1 && pp_BumpMap == 0
		{
			color.rgb += offset.rgb;
		}
		#endif
	}
	#endif
	#if PASS == PASS_COLOR && pp_TwoVolumes == 0
		uvec4 stencil = subpassLoad(shadow_stencil);
		if (stencil.r == 0x81u)
			color.rgb *= uniformBuffer.shade_scale_factor;
	#endif
	
	color = colorClamp(color);
	
	#if pp_FogCtrl == 0 || pp_TwoVolumes == 1 // LUT
		IF(cur_fog_control == 0)
		{
			color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z)); 
		}
	#endif
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex
		IF(cur_fog_control == 1)
		{
			color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_VERT.rgb, offset.a);
		}
	#endif
	
	color *= pushConstants.trilinearAlpha;
	
	//color.rgb = vec3(vtx_uv.z * uniformBuffer.sp_FOG_DENSITY / 128.0);
	
	#if PASS == PASS_COLOR 
		FragColor = color;
	#elif PASS == PASS_OIT
		ivec2 coords = ivec2(gl_FragCoord.xy);
		uint idx =  getNextPixelIndex();
		
		Pixel pixel;
		pixel.color = packColors(clamp(color, vec4(0.0), vec4(1.0)));
		pixel.depth = gl_FragDepth;
		pixel.seq_num = vtx_index;
		pixel.next = atomicExchange(abufferPointer.pointers[coords.x + coords.y * uniformBuffer.viewportWidth], idx);
		PixelBuffer.pixels[idx] = pixel;
		
	#endif
}
)";

static const char OITModifierVolumeShader[] = R"(
layout (location = 0) in highp float depth;

void main()
{
	setFragDepth(depth);
}
)";

static const char OITFinalShaderSource[] = R"(
layout (input_attachment_index = 0, set = 2, binding = 0) uniform subpassInput tex;

layout (location = 0) out vec4 FragColor;

uint pixel_list[MAX_PIXELS_PER_FRAGMENT];


int fillAndSortFragmentArray(ivec2 coords)
{
	// Load fragments into a local memory array for sorting
	uint idx = abufferPointer.pointers[coords.x + coords.y * uniformBuffer.viewportWidth];
	if (idx == EOL)
		return 0;
	int count = 1;
	pixel_list[0] = idx;
	idx = PixelBuffer.pixels[idx].next;
	for (; idx != EOL && count < MAX_PIXELS_PER_FRAGMENT; count++)
	{
		float depth = PixelBuffer.pixels[idx].depth;
		uint index = getPolyIndex(PixelBuffer.pixels[idx]);
		int j = count - 1;
		float jdepth = PixelBuffer.pixels[pixel_list[j]].depth;
		uint jindex = getPolyIndex(PixelBuffer.pixels[pixel_list[j]]);
		while (j >= 0
			   && (jdepth > depth
				   || (jdepth == depth && jindex > index)))
		{
			pixel_list[j + 1] = pixel_list[j];
			j--;
			if (j >= 0)
			{
				jdepth = PixelBuffer.pixels[pixel_list[j]].depth;
				jindex = getPolyIndex(PixelBuffer.pixels[pixel_list[j]]);
			}
		}
		pixel_list[j + 1] = idx;
		idx = PixelBuffer.pixels[idx].next;
	}
	return count;
}

// Blend fragments back-to-front
vec4 resolveAlphaBlend(ivec2 coords) {
	
	// Copy and sort fragments into a local array
	int num_frag = fillAndSortFragmentArray(coords);
	
	vec4 finalColor = subpassLoad(tex);
	vec4 secondaryBuffer = vec4(0.0); // Secondary accumulation buffer
	
	for (int i = 0; i < num_frag; i++)
	{
		const Pixel pixel = PixelBuffer.pixels[pixel_list[i]];
		const PolyParam pp = TrPolyParam.tr_poly_params[getPolyNumber(pixel)];
		bool area1 = false;
		bool shadowed = false;
		if (isShadowed(pixel))
		{
			if (isTwoVolumes(pp))
				area1 = true;
			else
				shadowed = true;
		}
		vec4 srcColor;
		if (getSrcSelect(pp, area1))
			srcColor = secondaryBuffer;
		else
		{
			srcColor = unpackColors(pixel.color);
			if (shadowed)
				srcColor.rgb *= uniformBuffer.shade_scale_factor;
		}
		vec4 dstColor = getDstSelect(pp, area1) ? secondaryBuffer : finalColor;
		vec4 srcCoef;
		vec4 dstCoef;
		
		int srcBlend = getSrcBlendFunc(pp, area1);
		switch (srcBlend)
		{
			case ZERO:
				srcCoef = vec4(0.0);
				break;
			case ONE:
				srcCoef = vec4(1.0);
				break;
			case OTHER_COLOR:
				srcCoef = finalColor;
				break;
			case INVERSE_OTHER_COLOR:
				srcCoef = vec4(1.0) - dstColor;
				break;
			case SRC_ALPHA:
				srcCoef = vec4(srcColor.a);
				break;
			case INVERSE_SRC_ALPHA:
				srcCoef = vec4(1.0 - srcColor.a);
				break;
			case DST_ALPHA:
				srcCoef = vec4(dstColor.a);
				break;
			case INVERSE_DST_ALPHA:
				srcCoef = vec4(1.0 - dstColor.a);
				break;
		}
		int dstBlend = getDstBlendFunc(pp, area1);
		switch (dstBlend)
		{
			case ZERO:
				dstCoef = vec4(0.0);
				break;
			case ONE:
				dstCoef = vec4(1.0);
				break;
			case OTHER_COLOR:
				dstCoef = srcColor;
				break;
			case INVERSE_OTHER_COLOR:
				dstCoef = vec4(1.0) - srcColor;
				break;
			case SRC_ALPHA:
				dstCoef = vec4(srcColor.a);
				break;
			case INVERSE_SRC_ALPHA:
				dstCoef = vec4(1.0 - srcColor.a);
				break;
			case DST_ALPHA:
				dstCoef = vec4(dstColor.a);
				break;
			case INVERSE_DST_ALPHA:
				dstCoef = vec4(1.0 - dstColor.a);
				break;
		}
		const vec4 result = clamp(dstColor * dstCoef + srcColor * srcCoef, 0.0, 1.0);
		if (getDstSelect(pp, area1))
			secondaryBuffer = result;
		else
			finalColor = result;
	}

	return finalColor;
	
}

void main(void)
{
	ivec2 coords = ivec2(gl_FragCoord.xy);
	// Compute and output final color for the frame buffer
	// Visualize the number of layers in use
	//FragColor = vec4(float(fillAndSortFragmentArray(coords)) / MAX_PIXELS_PER_FRAGMENT * 4, 0, 0, 1);
	FragColor = resolveAlphaBlend(coords);

	// Reset pointers
	abufferPointer.pointers[coords.x + coords.y * uniformBuffer.viewportWidth] = EOL;
}
)";

static const char OITClearShaderSource[] = R"(
void main(void)
{
	ivec2 coords = ivec2(gl_FragCoord.xy);

	// Reset pointers
	abufferPointer.pointers[coords.x + coords.y * uniformBuffer.viewportWidth] = EOL;
}
)";

static const char OITTranslucentModvolShaderSource[] = R"(
layout (location = 0) in highp float depth;

// Must match ModifierVolumeMode enum values
#define MV_XOR		 0
#define MV_OR		 1
#define MV_INCLUSION 2
#define MV_EXCLUSION 3

void main()
{
#if MV_MODE == MV_XOR || MV_MODE == MV_OR
	setFragDepth(depth);
#endif
	ivec2 coords = ivec2(gl_FragCoord.xy);
	
	uint idx = abufferPointer.pointers[coords.x + coords.y * uniformBuffer.viewportWidth];
	int list_len = 0;
	while (idx != EOL && list_len < MAX_PIXELS_PER_FRAGMENT)
	{
		const Pixel pixel = PixelBuffer.pixels[idx];
		const PolyParam pp = TrPolyParam.tr_poly_params[getPolyNumber(pixel)];
		if (getShadowEnable(pp))
		{
#if MV_MODE == MV_XOR
			if (gl_FragDepth >= pixel.depth)
				atomicXor(PixelBuffer.pixels[idx].seq_num, SHADOW_STENCIL);
#elif MV_MODE == MV_OR
			if (gl_FragDepth >= pixel.depth)
				atomicOr(PixelBuffer.pixels[idx].seq_num, SHADOW_STENCIL);
#elif MV_MODE == MV_INCLUSION
			uint prev_val = atomicAnd(PixelBuffer.pixels[idx].seq_num, ~(SHADOW_STENCIL));
			if ((prev_val & (SHADOW_STENCIL|SHADOW_ACC)) == SHADOW_STENCIL)
				PixelBuffer.pixels[idx].seq_num = bitfieldInsert(pixel.seq_num, 1u, 31, 1);
#elif MV_MODE == MV_EXCLUSION
			uint prev_val = atomicAnd(PixelBuffer.pixels[idx].seq_num, ~(SHADOW_STENCIL|SHADOW_ACC));
			if ((prev_val & (SHADOW_STENCIL|SHADOW_ACC)) == SHADOW_ACC)
				PixelBuffer.pixels[idx].seq_num = bitfieldInsert(pixel.seq_num, 1u, 31, 1);
#endif
		}
		idx = pixel.next;
		list_len++;
	}
}
)";

static const char OITFinalVertexShaderSource[] = R"(
layout (location = 0) in vec3 in_pos;

layout (push_constant) uniform pushBlock
{
	int polyNumber_not_used;
} pushConstants;

void main()
{
	gl_Position = vec4(in_pos, 1.0);
}
)";

static const char OITN2VertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 ndcMat;
} uniformBuffer;

layout (push_constant) uniform constants
{
	layout(offset = 96) int polyNumber_not_used;
} pushConstants;

layout (location = 0) in vec4         in_pos;
layout (location = 1) in uvec4        in_base;
layout (location = 2) in uvec4        in_offs;
layout (location = 3) in mediump vec2 in_uv;
layout (location = 4) in uvec4        in_base1;
layout (location = 5) in uvec4        in_offs1;
layout (location = 6) in mediump vec2 in_uv1;
layout (location = 7) in vec3         in_normal;

layout (location = 0) INTERPOLATION out highp vec4 vtx_base;
layout (location = 1) INTERPOLATION out highp vec4 vtx_offs;
layout (location = 2) out highp vec3 vtx_uv;
layout (location = 3) INTERPOLATION out highp vec4 vtx_base1;
layout (location = 4) INTERPOLATION out highp vec4 vtx_offs1;
layout (location = 5) out highp vec2 vtx_uv1;
layout (location = 6) flat out uint vtx_index;

void wDivide(inout vec4 vpos)
{
	vpos = vec4(vpos.xy / vpos.w, 1.0 / vpos.w, 1.0);
	vpos = uniformBuffer.ndcMat * vpos;
#if pp_Gouraud == 1
	vtx_base *= vpos.z;
	vtx_offs *= vpos.z;
#if pp_TwoVolumes == 1
	vtx_base1 *= vpos.z;
	vtx_offs1 *= vpos.z;
#endif
#endif
	vtx_uv = vec3(vtx_uv.xy * vpos.z, vpos.z);
#if pp_TwoVolumes == 1
	vtx_uv1 *= vpos.z;
#endif
	vpos.w = 1.0;
	vpos.z = 0.0;
}

void main()
{
	vec4 vpos = n2Uniform.mvMat * in_pos;
	vtx_base = vec4(in_base) / 255.0;
	vtx_offs = vec4(in_offs) / 255.0;

	#if LIGHT_ON == 1
	vec3 vnorm = normalize(mat3(n2Uniform.normalMat) * in_normal);
	#endif

	#if pp_TwoVolumes == 1
		vtx_base1 = vec4(in_base1) / 255.0;
		vtx_offs1 = vec4(in_offs1) / 255.0;
		vtx_uv1 = in_uv1;
		#if LIGHT_ON == 1
			// FIXME need offset0 and offset1 for bump maps
			if (n2Uniform.bumpMapping == 1)
				computeBumpMap(vtx_offs, vtx_offs1, vpos.xyz, in_normal, n2Uniform.normalMat);
			else
			{
				computeColors(vtx_base1, vtx_offs1, 1, vpos.xyz, vnorm);
				#if pp_Texture == 0
					vtx_base1 += vtx_offs1;
				#endif
			}
			if (n2Uniform.envMapping[1] == 1)
				computeEnvMap(vtx_uv1.xy, vpos.xyz, vnorm);
		#endif
	#endif
	#if LIGHT_ON == 1
	if (n2Uniform.bumpMapping == 0)
	{
		computeColors(vtx_base, vtx_offs, 0, vpos.xyz, vnorm);
		#if pp_Texture == 0
				vtx_base += vtx_offs;
		#endif
	}
	#endif

	vtx_uv.xy = in_uv;
	#if LIGHT_ON == 1
		if (n2Uniform.envMapping[0] == 1)
			computeEnvMap(vtx_uv.xy, vpos.xyz, vnorm);
	#endif

	vpos = n2Uniform.projMat * vpos;
	wDivide(vpos);
	vtx_index = uint(n2Uniform.polyNumber) + uint(gl_VertexIndex);

	gl_Position = vpos;
}
)";

extern const char ModVolVertexShaderSource[];
extern const char N2ModVolVertexShaderSource[];
extern const char N2LightShaderSource[];

vk::UniqueShaderModule OITShaderManager::compileShader(const VertexShaderParams& params)
{
	VulkanSource src;
	src.addConstant("pp_Gouraud", (int)params.gouraud)
		.addConstant("DIV_POS_Z", (int)params.divPosZ)
		.addSource(GouraudSource);
	if (params.naomi2)
		src.addConstant("pp_TwoVolumes", (int)params.twoVolume)
			.addConstant("LIGHT_ON", (int)params.lightOn)
			.addConstant("pp_Texture", (int)params.texture)
			.addSource(N2LightShaderSource)
			.addSource(OITN2VertexShaderSource);
	else
		src.addSource(OITVertexShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}

vk::UniqueShaderModule OITShaderManager::compileShader(const FragmentShaderParams& params)
{
	VulkanSource src;
	src.addConstant("cp_AlphaTest", (int)params.alphaTest)
		.addConstant("pp_ClipInside", (int)params.insideClipTest)
		.addConstant("pp_UseAlpha", (int)params.useAlpha)
		.addConstant("pp_Texture", (int)params.texture)
		.addConstant("pp_IgnoreTexA", (int)params.ignoreTexAlpha)
		.addConstant("pp_ShadInstr", params.shaderInstr)
		.addConstant("pp_Offset", (int)params.offset)
		.addConstant("pp_FogCtrl", params.fog)
		.addConstant("pp_TwoVolumes", (int)params.twoVolume)
		.addConstant("pp_Gouraud", (int)params.gouraud)
		.addConstant("pp_BumpMap", (int)params.bumpmap)
		.addConstant("ColorClamping", (int)params.clamping)
		.addConstant("pp_Palette", (int)params.palette)
		.addConstant("DIV_POS_Z", (int)params.divPosZ)
		.addConstant("PASS", (int)params.pass)
		.addSource(GouraudSource)
		.addSource(OITShaderHeader)
		.addSource(OITFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule OITShaderManager::compileFinalShader()
{
	VulkanSource src;
	src.addConstant("MAX_PIXELS_PER_FRAGMENT", config::PerPixelLayers)
		.addSource(OITShaderHeader)
		.addSource(OITFinalShaderSource);

	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}
vk::UniqueShaderModule OITShaderManager::compileFinalVertexShader()
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, VulkanSource().addSource(OITFinalVertexShaderSource).generate());
}

vk::UniqueShaderModule OITShaderManager::compileClearShader()
{
	VulkanSource src;
	src.addSource(OITShaderHeader)
		.addSource(OITClearShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule OITShaderManager::compileShader(const ModVolShaderParams& params)
{
	VulkanSource src;
	if (params.naomi2)
		src.addSource(N2ModVolVertexShaderSource);
	else
		src.addConstant("DIV_POS_Z", (int)params.divPosZ)
			.addSource(ModVolVertexShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}
vk::UniqueShaderModule OITShaderManager::compileModVolFragmentShader(bool divPosZ)
{
	VulkanSource src;
	src.addConstant("DIV_POS_Z", (int)divPosZ)
		.addSource(OITShaderHeader)
		.addSource(OITModifierVolumeShader);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}
vk::UniqueShaderModule OITShaderManager::compileShader(const TrModVolShaderParams& params)
{
	VulkanSource src;
	src.addConstant("MAX_PIXELS_PER_FRAGMENT", config::PerPixelLayers)
		.addConstant("MV_MODE", (int)params.mode)
		.addConstant("DIV_POS_Z", (int)params.divPosZ)
		.addSource(OITShaderHeader)
		.addSource(OITTranslucentModvolShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}
