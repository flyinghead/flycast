/*
 *  Created on: Oct 3, 2019

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
#include "vulkan.h"
#include "shaders.h"
#include "compiler.h"
#include "utils.h"

static const char VertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 normal_matrix;
} uniformBuffer;

layout (location = 0) in vec4         in_pos;
layout (location = 1) in uvec4        in_base;
layout (location = 2) in uvec4        in_offs;
layout (location = 3) in mediump vec2 in_uv;

layout (location = 0) INTERPOLATION out highp vec4 vtx_base;
layout (location = 1) INTERPOLATION out highp vec4 vtx_offs;
layout (location = 2) noperspective out highp vec3 vtx_uv;

void main()
{
	vec4 vpos = uniformBuffer.normal_matrix * in_pos;
	vtx_base = vec4(in_base) / 255.0;
	vtx_offs = vec4(in_offs) / 255.0;
	vtx_uv = vec3(in_uv * vpos.z, vpos.z);
#if pp_Gouraud == 1
	vtx_base *= vpos.z;
	vtx_offs *= vpos.z;
#endif
	vpos.w = 1.0;
	vpos.z = 0.0;
	gl_Position = vpos;
}
)";

static const char FragmentShaderSource[] = R"(
#define PI 3.1415926

layout (location = 0) out vec4 FragColor;
#define gl_FragColor FragColor

layout (std140, set = 0, binding = 1) uniform FragmentShaderUniforms
{
	vec4 colorClampMin;
	vec4 colorClampMax;
	vec4 sp_FOG_COL_RAM;
	vec4 sp_FOG_COL_VERT;
	float cp_AlphaTestValue;
	float sp_FOG_DENSITY;
} uniformBuffer;

layout (push_constant) uniform pushBlock
{
	vec4 clipTest;
	float trilinearAlpha;
	float palette_index;
} pushConstants;

#if pp_Texture == 1
layout (set = 1, binding = 0) uniform sampler2D tex;
#endif
#if pp_Palette == 1
layout (set = 0, binding = 3) uniform sampler2D palette;
#endif

// Vertex input
layout (location = 0) INTERPOLATION in highp vec4 vtx_base;
layout (location = 1) INTERPOLATION in highp vec4 vtx_offs;
layout (location = 2) noperspective in highp vec3 vtx_uv;

#if pp_FogCtrl != 2
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
#if ColorClamping == 1
	return clamp(col, uniformBuffer.colorClampMin, uniformBuffer.colorClampMax);
#else
	return col;
#endif
}

#if pp_Palette == 1

vec4 palettePixel(sampler2D tex, vec3 coords)
{
	vec4 c = vec4(textureProj(tex, coords).r * 255.0 / 1023.0 + pushConstants.palette_index, 0.5, 0.0, 0.0);
	return texture(palette, c.xy);
}

#endif

void main()
{
	// Clip inside the box
	#if pp_ClipInside == 1
		if (gl_FragCoord.x >= pushConstants.clipTest.x && gl_FragCoord.x <= pushConstants.clipTest.z
				&& gl_FragCoord.y >= pushConstants.clipTest.y && gl_FragCoord.y <= pushConstants.clipTest.w)
			discard;
	#endif
	
	highp vec4 color = vtx_base;
	highp vec4 offset = vtx_offs;
	#if pp_Gouraud == 1
		color /= vtx_uv.z;
		offset /= vtx_uv.z;
	#endif
	#if pp_UseAlpha == 0
		color.a = 1.0;
	#endif
	#if pp_FogCtrl == 3
		color = vec4(uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(gl_FragCoord.w));
	#endif
	#if pp_Texture == 1
	{
		#if pp_Palette == 0
			vec4 texcol = textureProj(tex, vtx_uv);
		#else
			vec4 texcol = palettePixel(tex, vtx_uv);
		#endif
		
		#if pp_BumpMap == 1
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0 * PI * offset.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA == 1
				texcol.a = 1.0;
			#endif
			
			#if cp_AlphaTest == 1
				if (uniformBuffer.cp_AlphaTestValue > texcol.a)
					discard;
				texcol.a = 1.0;
			#endif 
		#endif
		#if pp_ShadInstr == 0
		{
			color = texcol;
		}
		#endif
		#if pp_ShadInstr == 1
		{
			color.rgb *= texcol.rgb;
			color.a = texcol.a;
		}
		#endif
		#if pp_ShadInstr == 2
		{
			color.rgb = mix(color.rgb, texcol.rgb, texcol.a);
		}
		#endif
		#if  pp_ShadInstr == 3
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
	
	color = colorClamp(color);
	
	#if pp_FogCtrl == 0
	{
		color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z)); 
	}
	#endif
	#if pp_FogCtrl == 1 && pp_Offset==1 && pp_BumpMap == 0
	{
		color.rgb = mix(color.rgb, uniformBuffer.sp_FOG_COL_VERT.rgb, offset.a);
	}
	#endif
	
	#if pp_TriLinear == 1
	color *= pushConstants.trilinearAlpha;
	#endif
	
	//color.rgb = vec3(gl_FragCoord.w * uniformBuffer.sp_FOG_DENSITY / 128.0);

	highp float w = vtx_uv.z * 100000.0;
	gl_FragDepth = log2(1.0 + w) / 34.0;

	gl_FragColor = color;
}
)";

extern const char ModVolVertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 normal_matrix;
} uniformBuffer;

layout (location = 0) in vec4 in_pos;
layout (location = 0) noperspective out highp float depth;

void main()
{
	vec4 vpos = uniformBuffer.normal_matrix * in_pos;
	depth = vpos.z;
	vpos.w = 1.0;
	vpos.z = 0.0;
	gl_Position = vpos;
}
)";

static const char ModVolFragmentShaderSource[] = R"(
layout (location = 0) noperspective in highp float depth;
layout (location = 0) out vec4 FragColor;

layout (push_constant) uniform pushBlock
{
	float sp_ShaderColor;
} pushConstants;

void main()
{
	highp float w = depth * 100000.0;
	gl_FragDepth = log2(1.0 + w) / 34.0;
	FragColor = vec4(0.0, 0.0, 0.0, pushConstants.sp_ShaderColor);
}
)";

static const char QuadVertexShaderSource[] = R"(
layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec2 in_uv;

layout (location = 0) out vec2 outUV;

void main()
{
#if ROTATE == 0
	gl_Position = vec4(in_pos, 1.0);
#else
	gl_Position = vec4(in_pos.y, -in_pos.x, in_pos.z, 1.0);
#endif
	outUV = in_uv;
}
)";

static const char QuadFragmentShaderSource[] = R"(
layout (set = 0, binding = 0) uniform sampler2D tex;

layout (push_constant) uniform pushBlock
{
	vec4 color;
} pushConstants;

layout (location = 0) in vec2 inUV;
layout (location = 0) out vec4 FragColor;

void main() 
{
#if IGNORE_TEX_ALPHA == 1
	FragColor.rgb = pushConstants.color.rgb * texture(tex, inUV).rgb;
	FragColor.a = pushConstants.color.a;
#else
	FragColor = pushConstants.color * texture(tex, inUV);
#endif
}
)";

static const char OSDVertexShaderSource[] = R"(
layout (location = 0) in vec4 inPos;
layout (location = 1) in uvec4 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 0) out lowp vec4 outColor;
layout (location = 1) out mediump vec2 outUV;

void main() 
{
	outColor = inColor / 255.0;
	outUV = inUV;
	gl_Position = inPos;
}
)";

static const char OSDFragmentShaderSource[] = R"(
layout (binding = 0) uniform sampler2D tex;
layout (location = 0) in lowp vec4 inColor;
layout (location = 1) in mediump vec2 inUV;
layout (location = 0) out vec4 FragColor;

void main() 
{
	FragColor = inColor * texture(tex, inUV);
}
)";

vk::UniqueShaderModule ShaderManager::compileShader(const VertexShaderParams& params)
{
	VulkanSource src;
	src.addConstant("pp_Gouraud", (int)params.gouraud)
			.addSource(GouraudSource)
			.addSource(VertexShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileShader(const FragmentShaderParams& params)
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
		.addConstant("pp_Gouraud", (int)params.gouraud)
		.addConstant("pp_BumpMap", (int)params.bumpmap)
		.addConstant("ColorClamping", (int)params.clamping)
		.addConstant("pp_TriLinear", (int)params.trilinear)
		.addConstant("pp_Palette", (int)params.palette)
		.addSource(GouraudSource)
		.addSource(FragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileModVolVertexShader()
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, VulkanSource().addSource(ModVolVertexShaderSource).generate());
}

vk::UniqueShaderModule ShaderManager::compileModVolFragmentShader()
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, VulkanSource().addSource(ModVolFragmentShaderSource).generate());
}

vk::UniqueShaderModule ShaderManager::compileQuadVertexShader(bool rotate)
{
	VulkanSource src;
	src.addConstant("ROTATE", (int)rotate)
			.addSource(QuadVertexShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileQuadFragmentShader(bool ignoreTexAlpha)
{
	VulkanSource src;
	src.addConstant("IGNORE_TEX_ALPHA", (int)ignoreTexAlpha)
			.addSource(QuadFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,src.generate());
}

vk::UniqueShaderModule ShaderManager::compileOSDVertexShader()
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, VulkanSource().addSource(OSDVertexShaderSource).generate());
}

vk::UniqueShaderModule ShaderManager::compileOSDFragmentShader()
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, VulkanSource().addSource(OSDFragmentShaderSource).generate());
}
