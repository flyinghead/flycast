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

static const char OITVertexShaderSource[] = R"(#version 450

#define pp_Gouraud %d

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 normal_matrix;
} uniformBuffer;

layout (location = 0) in vec4         in_pos;
layout (location = 1) in uvec4        in_base;
layout (location = 2) in uvec4        in_offs;
layout (location = 3) in mediump vec2 in_uv;
layout (location = 4) in uvec4        in_base1;						// New for OIT, only for OP/PT with 2-volume
layout (location = 5) in uvec4        in_offs1;
layout (location = 6) in mediump vec2 in_uv1;

layout (location = 0) INTERPOLATION out lowp vec4 vtx_base;
layout (location = 1) INTERPOLATION out lowp vec4 vtx_offs;
layout (location = 2)               out mediump vec2 vtx_uv;
layout (location = 3) INTERPOLATION out lowp vec4 vtx_base1;		// New for OIT, only for OP/PT with 2-volume
layout (location = 4) INTERPOLATION out lowp vec4 vtx_offs1;
layout (location = 5)               out mediump vec2 vtx_uv1;

void main()
{
	vtx_base = vec4(in_base) / 255.0;
	vtx_offs = vec4(in_offs) / 255.0;
	vtx_uv = in_uv;
	vtx_base1 = vec4(in_base1) / 255.0;								// New for OIT, only for OP/PT with 2-volume
	vtx_offs1 = vec4(in_offs1) / 255.0;
	vtx_uv1 = in_uv1;
	vec4 vpos = uniformBuffer.normal_matrix * in_pos;
	vpos.w = 1.0 / vpos.z;
	vpos.z = vpos.w;
	vpos.xy *= vpos.w; 
	gl_Position = vpos;
}
)";

static const char OITShaderHeader[] = R"(#version 450

layout (std140, set = 0, binding = 1) uniform FragmentShaderUniforms
{
	vec4 colorClampMin;
	vec4 colorClampMax;
	vec4 sp_FOG_COL_RAM;
	vec4 sp_FOG_COL_VERT;
	float cp_AlphaTestValue;
	float sp_FOG_DENSITY;
	float shade_scale_factor;
} uniformBuffer;

layout(set = 3, binding = 2, r32ui) uniform coherent restrict uimage2D abufferPointerImg;
struct Pixel {
	uint color;
	float depth;
	uint seq_num;
	uint next;
};
#define EOL 0xFFFFFFFFu
layout (set = 3, binding = 0, std430) coherent restrict buffer PixelBuffer_ {
	Pixel pixels[];
} PixelBuffer;
layout(set = 3, binding = 1) buffer PixelCounter_ {
	uint buffer_index;
} PixelCounter;

#define ZERO				0
#define ONE					1
#define OTHER_COLOR			2
#define INVERSE_OTHER_COLOR	3
#define SRC_ALPHA			4
#define INVERSE_SRC_ALPHA	5
#define DST_ALPHA			6
#define INVERSE_DST_ALPHA	7

uint getNextPixelIndex()
{
	uint index = atomicAdd(PixelCounter.buffer_index, 1);
	if (index >= PixelBuffer.pixels.length())
		// Buffer overflow
		discard;
	
	return index;
}

void setFragDepth(void)
{
	float w = 100000.0 * gl_FragCoord.w;
	gl_FragDepth = log2(1.0 + w) / 34.0;
}
struct PolyParam {
	int tsp_isp_pcw;
	int tsp1;
};
layout (set = 0, binding = 3, std430) readonly buffer TrPolyParamBuffer {
	PolyParam tr_poly_params[];
} TrPolyParam;

#define GET_TSP_FOR_AREA int tsp = area1 ? pp.tsp1 : pp.tsp_isp_pcw;

int getSrcBlendFunc(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return (tsp >> 29) & 7;
}

int getDstBlendFunc(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return (tsp >> 26) & 7;
}

bool getSrcSelect(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return ((tsp >> 25) & 1) != 0;
}

bool getDstSelect(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return ((tsp >> 24) & 1) != 0;
}

int getFogControl(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return (tsp >> 22) & 3;
}

bool getUseAlpha(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return ((tsp >> 20) & 1) != 0;
}

bool getIgnoreTexAlpha(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return ((tsp >> 19) & 1) != 0;
}

int getShadingInstruction(const PolyParam pp, bool area1)
{
	GET_TSP_FOR_AREA
	return (tsp >> 6) & 3;
}

int getDepthFunc(const PolyParam pp)
{
	return (pp.tsp_isp_pcw >> 13) & 7;
}

bool getDepthMask(const PolyParam pp)
{
	return ((pp.tsp_isp_pcw >> 10) & 1) != 1;
}

bool getShadowEnable(const PolyParam pp)
{
	return (pp.tsp_isp_pcw & 1) != 0;
}

uint getPolyNumber(const Pixel pixel)
{
	return pixel.seq_num & 0x3FFFFFFFu;
}

#define SHADOW_STENCIL 0x40000000u
#define SHADOW_ACC	   0x80000000u

bool isShadowed(const Pixel pixel)
{
	return (pixel.seq_num & SHADOW_ACC) == SHADOW_ACC;
}

bool isTwoVolumes(const PolyParam pp)
{
	return pp.tsp1 != -1;
}

uint packColors(vec4 v)
{
	return (uint(round(v.r * 255.0)) << 24) | (uint(round(v.g * 255.0)) << 16) | (uint(round(v.b * 255.0)) << 8) | uint(round(v.a * 255.0));
}

vec4 unpackColors(uint u)
{
	return vec4(float((u >> 24) & 255) / 255.0, float((u >> 16) & 255) / 255.0, float((u >> 8) & 255) / 255.0, float(u & 255) / 255.0);
}
)";

static const char OITFragmentShaderSource[] = R"(
#define cp_AlphaTest %d
#define pp_ClipInside %d
#define pp_UseAlpha %d
#define pp_Texture %d
#define pp_IgnoreTexA %d
#define pp_ShadInstr %d
#define pp_Offset %d
#define pp_FogCtrl %d
#define pp_TwoVolumes %d
#define pp_Gouraud %d
#define pp_BumpMap %d
#define ColorClamping %d
#define PASS %d
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

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

layout (push_constant) uniform pushBlock
{
	vec4 clipTest;
	ivec4 blend_mode0;
	float trilinearAlpha;
	int pp_Number;

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

#if PASS == PASS_COLOR
layout (input_attachment_index = 0, set = 0, binding = 4) uniform usubpassInput shadow_stencil;
#endif
#if PASS == PASS_OIT
layout (input_attachment_index = 0, set = 0, binding = 5) uniform subpassInput DepthTex;
#endif

// Vertex input
layout (location = 0) INTERPOLATION in lowp vec4 vtx_base;
layout (location = 1) INTERPOLATION in lowp vec4 vtx_offs;
layout (location = 2)               in mediump vec2 vtx_uv;
layout (location = 3) INTERPOLATION in lowp vec4 vtx_base1;			// new for OIT. Only if 2 vol
layout (location = 4) INTERPOLATION in lowp vec4 vtx_offs1;
layout (location = 5)               in mediump vec2 vtx_uv1;

#if pp_FogCtrl != 2 || pp_TwoVolumes == 1
layout (set = 0, binding = 2) uniform sampler2D fog_table;

float fog_mode2(float w)
{
	float z = clamp(w * uniformBuffer.sp_FOG_DENSITY, 1.0, 255.9999);
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

void main()
{
	setFragDepth();

	#if PASS == PASS_OIT
		// Manual depth testing
		highp float frontDepth = subpassLoad(DepthTex).r;
		if (gl_FragDepth < frontDepth)
			discard;
	#endif

	// Clip inside the box
	#if pp_ClipInside == 1
		if (gl_FragCoord.x >= pushConstants.clipTest.x && gl_FragCoord.x <= pushConstants.clipTest.z
				&& gl_FragCoord.y >= pushConstants.clipTest.y && gl_FragCoord.y <= pushConstants.clipTest.w)
			discard;
	#endif
	
	highp vec4 color = vtx_base;
	lowp vec4 offset = vtx_offs;
	mediump vec2 uv = vtx_uv;
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
				uv = vtx_uv1;
				area1 = true;
				cur_blend_mode = pushConstants.blend_mode1.xy;
				cur_use_alpha = pushConstants.use_alpha1 != 0;
				cur_ignore_tex_alpha = pushConstants.ignore_tex_alpha1 != 0;
				cur_shading_instr = pushConstants.shading_instr1;
				cur_fog_control = pushConstants.fog_control1;
			}
		#endif
	#endif

	#if pp_UseAlpha == 0 || pp_TwoVolumes == 1
		IF (!cur_use_alpha)
			color.a = 1.0;
	#endif
	#if pp_FogCtrl == 3 || pp_TwoVolumes == 1 // LUT Mode 2
		IF (cur_fog_control == 3)
			color = vec4(uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(gl_FragCoord.w));
	#endif
	#if pp_Texture==1
	{
		highp vec4 texcol;
		#if pp_TwoVolumes == 1
			if (area1)
				texcol = texture(tex1, uv);
			else
		#endif
			texcol = texture(tex0, uv);
		#if pp_BumpMap == 1
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1
				IF(cur_ignore_tex_alpha)
					texcol.a = 1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (uniformBuffer.cp_AlphaTestValue > texcol.a)
					discard;
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
			color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(gl_FragCoord.w)); 
		}
	#endif
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex
		IF(cur_fog_control == 1)
		{
			color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_VERT.rgb, offset.a);
		}
	#endif
	
	color *= pushConstants.trilinearAlpha;
	
	#if cp_AlphaTest == 1
		color.a = 1.0;
	#endif 
	
	//color.rgb=vec3(gl_FragCoord.w * uniformBuffer.sp_FOG_DENSITY / 128.0);
	
	#if PASS == PASS_COLOR 
		FragColor = color;
	#elif PASS == PASS_OIT
		// Discard as many pixels as possible
		switch (cur_blend_mode.y) // DST
		{
		case ONE:
			switch (cur_blend_mode.x) // SRC
			{
				case ZERO:
					discard;
				case ONE:
				case OTHER_COLOR:
				case INVERSE_OTHER_COLOR:
					if (color == vec4(0.0))
						discard;
					break;
				case SRC_ALPHA:
					if (color.a == 0.0 || color.rgb == vec3(0.0))
						discard;
					break;
				case INVERSE_SRC_ALPHA:
					if (color.a == 1.0 || color.rgb == vec3(0.0))
						discard;
					break;
			}
			break;
		case OTHER_COLOR:
			if (cur_blend_mode.x == ZERO && color == vec4(1.0))
				discard;
			break;
		case INVERSE_OTHER_COLOR:
			if (cur_blend_mode.x <= SRC_ALPHA && color == vec4(0.0))
				discard;
			break;
		case SRC_ALPHA:
			if ((cur_blend_mode.x == ZERO || cur_blend_mode.x == INVERSE_SRC_ALPHA) && color.a == 1.0)
				discard;
			break;
		case INVERSE_SRC_ALPHA:
			switch (cur_blend_mode.x) // SRC
			{
				case ZERO:
				case SRC_ALPHA:
					if (color.a == 0.0)
						discard;
					break;
				case ONE:
				case OTHER_COLOR:
				case INVERSE_OTHER_COLOR:
					if (color == vec4(0.0))
						discard;
					break;
			}
			break;
		}
		
		ivec2 coords = ivec2(gl_FragCoord.xy);
		uint idx =  getNextPixelIndex();
		
		Pixel pixel;
		pixel.color = packColors(clamp(color, vec4(0.0), vec4(1.0)));
		pixel.depth = gl_FragDepth;
		pixel.seq_num = uint(pushConstants.pp_Number);
		pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx);
		PixelBuffer.pixels[idx] = pixel;
		
	#endif
}
)";

static const char OITModifierVolumeShader[] = R"(

void main()
{
	setFragDepth();
}
)";

#define MAX_PIXELS_PER_FRAGMENT "32"

static const char OITFinalShaderSource[] =
"#define MAX_PIXELS_PER_FRAGMENT " MAX_PIXELS_PER_FRAGMENT
R"(
layout (input_attachment_index = 0, set = 2, binding = 0) uniform subpassInput tex;

layout (location = 0) out vec4 FragColor;

uint pixel_list[MAX_PIXELS_PER_FRAGMENT];


int fillAndSortFragmentArray(ivec2 coords)
{
	// Load fragments into a local memory array for sorting
	uint idx = imageLoad(abufferPointerImg, coords).x;
	int count = 0;
	for (; idx != EOL && count < MAX_PIXELS_PER_FRAGMENT; count++)
	{
		const Pixel p = PixelBuffer.pixels[idx];
		int j = count - 1;
		Pixel jp = PixelBuffer.pixels[pixel_list[j]];
		while (j >= 0
			   && (jp.depth > p.depth
				   || (jp.depth == p.depth && getPolyNumber(jp) > getPolyNumber(p))))
		{
			pixel_list[j + 1] = pixel_list[j];
			j--;
			jp = PixelBuffer.pixels[pixel_list[j]];
		}
		pixel_list[j + 1] = idx;
		idx = p.next;
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
}
)";

static const char OITClearShaderSource[] = R"(
void main(void)
{
	ivec2 coords = ivec2(gl_FragCoord.xy);

	// Reset pointers
	imageStore(abufferPointerImg, coords, uvec4(EOL));
}
)";

static const char OITTranslucentModvolShaderSource[] =
"#define MAX_PIXELS_PER_FRAGMENT " MAX_PIXELS_PER_FRAGMENT
R"(
#define MV_MODE %d

// Must match ModifierVolumeMode enum values
#define MV_XOR		 0
#define MV_OR		 1
#define MV_INCLUSION 2
#define MV_EXCLUSION 3

void main()
{
#if MV_MODE == MV_XOR || MV_MODE == MV_OR
	setFragDepth();
#endif
	ivec2 coords = ivec2(gl_FragCoord.xy);
	
	uint idx = imageLoad(abufferPointerImg, coords).x;
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

static const char OITFinalVertexShaderSource[] = R"(#version 430

layout (location = 0) in vec3 in_pos;

void main()
{
	gl_Position = vec4(in_pos, 1.0);
}
)";

extern const char ModVolVertexShaderSource[];

vk::UniqueShaderModule OITShaderManager::compileShader(const VertexShaderParams& params)
{
	char buf[sizeof(OITVertexShaderSource) * 2];

	sprintf(buf, OITVertexShaderSource, (int)params.gouraud);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, buf);
}

vk::UniqueShaderModule OITShaderManager::compileShader(const FragmentShaderParams& params)
{
	char buf[(sizeof(OITShaderHeader) + sizeof(OITFragmentShaderSource)) * 2];

	strcpy(buf, OITShaderHeader);
	sprintf(buf + strlen(buf), OITFragmentShaderSource, (int)params.alphaTest, (int)params.insideClipTest, (int)params.useAlpha,
			(int)params.texture, (int)params.ignoreTexAlpha, params.shaderInstr, (int)params.offset, params.fog,
			(int)params.twoVolume, (int)params.gouraud, (int)params.bumpmap, (int)params.clamping, (int)params.pass);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, buf);
}

vk::UniqueShaderModule OITShaderManager::compileFinalShader()
{
	std::string source = OITShaderHeader;
	source += OITFinalShaderSource;
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, source);
}
vk::UniqueShaderModule OITShaderManager::compileFinalVertexShader()
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, OITFinalVertexShaderSource);
}
vk::UniqueShaderModule OITShaderManager::compileClearShader()
{
	std::string source = OITShaderHeader;
	source += OITClearShaderSource;
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, source);
}
vk::UniqueShaderModule OITShaderManager::compileModVolVertexShader()
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, ModVolVertexShaderSource);
}
vk::UniqueShaderModule OITShaderManager::compileModVolFragmentShader()
{
	std::string source = OITShaderHeader;
	source += OITModifierVolumeShader;
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, source);
}
void OITShaderManager::compileTrModVolFragmentShader(ModVolMode mode)
{
	if (trModVolShaders.empty())
		trModVolShaders.resize((size_t)ModVolMode::Final);
	char buf[(sizeof(OITShaderHeader) + sizeof(OITTranslucentModvolShaderSource)) * 2];

	strcpy(buf, OITShaderHeader);
	sprintf(buf + strlen(buf), OITTranslucentModvolShaderSource, (int)mode);
	trModVolShaders[(size_t)mode] = ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, buf);
}
