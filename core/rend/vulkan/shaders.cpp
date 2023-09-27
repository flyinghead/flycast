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
	mat4 ndcMat;
} uniformBuffer;

layout (location = 0) in vec4         in_pos;
layout (location = 1) in uvec4        in_base;
layout (location = 2) in uvec4        in_offs;
layout (location = 3) in mediump vec2 in_uv;

layout (location = 0) INTERPOLATION out highp vec4 vtx_base;
layout (location = 1) INTERPOLATION out highp vec4 vtx_offs;
layout (location = 2) out highp vec3 vtx_uv;

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
#if pp_Gouraud == 1 && DIV_POS_Z != 1
	vtx_base *= vpos.z;
	vtx_offs *= vpos.z;
#endif

#if DIV_POS_Z != 1
	vtx_uv.xy *= vpos.z;
	vpos.w = 1.0;
	vpos.z = 0.0;
#endif
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
	vec4 ditherColorMax;
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
layout (location = 2) in highp vec3 vtx_uv;

#if pp_FogCtrl != 2
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
	// Clip inside the box
	#if pp_ClipInside == 1
		if (gl_FragCoord.x >= pushConstants.clipTest.x && gl_FragCoord.x <= pushConstants.clipTest.z
				&& gl_FragCoord.y >= pushConstants.clipTest.y && gl_FragCoord.y <= pushConstants.clipTest.w)
			discard;
	#endif
	
	highp vec4 color = vtx_base;
	highp vec4 offset = vtx_offs;
	#if pp_Gouraud == 1 && DIV_POS_Z != 1
		color /= vtx_uv.z;
		offset /= vtx_uv.z;
	#endif
	#if pp_UseAlpha == 0
		color.a = 1.0;
	#endif
	#if pp_FogCtrl == 3
		color = vec4(uniformBuffer.sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z));
	#endif
	#if pp_Texture == 1
	{
		#if pp_Palette == 0
			#if DIV_POS_Z == 1
				vec4 texcol = texture(tex, vtx_uv.xy);
			#else
				vec4 texcol = textureProj(tex, vtx_uv);
			#endif
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
	
	#if cp_AlphaTest == 1
		color.a = round(color.a * 255.0) / 255.0;
		if (uniformBuffer.cp_AlphaTestValue > color.a)
			discard;
		color.a = 1.0;
	#endif

	//color.rgb = vec3(gl_FragCoord.w * uniformBuffer.sp_FOG_DENSITY / 128.0);

#if DIV_POS_Z == 1
	highp float w = 100000.0 / vtx_uv.z;
#else
	highp float w = 100000.0 * vtx_uv.z;
#endif
	gl_FragDepth = log2(1.0 + max(w, -0.999999)) / 34.0;

#if DITHERING == 1
	float ditherTable[16] = float[](
		 0.9375,  0.1875,  0.75,  0.,   
		 0.4375,  0.6875,  0.25,  0.5,
		 0.8125,  0.0625,  0.875, 0.125,
		 0.3125,  0.5625,  0.375, 0.625	
	);
	float r = ditherTable[int(mod(gl_FragCoord.y, 4.)) * 4 + int(mod(gl_FragCoord.x, 4.))];
	// 31 for 5-bit color, 63 for 6 bits, 15 for 4 bits
	color += r / uniformBuffer.ditherColorMax;
	// avoid rounding
	color = floor(color * 255.) / 255.;
#endif
	gl_FragColor = color;
}
)";

extern const char ModVolVertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 ndcMat;
} uniformBuffer;

layout (location = 0) in vec4 in_pos;
layout (location = 0) out highp float depth;

void main()
{
	vec4 vpos = uniformBuffer.ndcMat * in_pos;
#if DIV_POS_Z == 1
	vpos /= vpos.z;
	vpos.z = vpos.w;
	depth = vpos.w;
#else
	depth = vpos.z;
	vpos.w = 1.0;
	vpos.z = 0.0;
#endif
	gl_Position = vpos;
}
)";

static const char ModVolFragmentShaderSource[] = R"(
layout (location = 0) in highp float depth;
layout (location = 0) out vec4 FragColor;

layout (push_constant) uniform pushBlock
{
	float sp_ShaderColor;
} pushConstants;

void main()
{
#if DIV_POS_Z == 1
	highp float w = 100000.0 / depth;
#else
	highp float w = 100000.0 * depth;
#endif
	gl_FragDepth = log2(1.0 + max(w, -0.999999)) / 34.0;
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

extern const char N2LightShaderSource[] = R"(

layout (std140, set = 1, binding = 2) uniform N2VertexShaderUniforms
{
	mat4 mvMat;
	mat4 normalMat;
	mat4 projMat;
	ivec2 envMapping;
	int bumpMapping;
	int polyNumber;

	vec2 glossCoef;
	ivec2 constantColor;
} n2Uniform;

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
	vec4 color;
	vec4 direction;	// For parallel/spot
	vec4 position;	// For spot/point

	int parallel;
	int routing;
	int dmode;
	int smode;

	ivec2 diffuse;
	ivec2 specular;

	float attnDistA;
	float attnDistB;
	float attnAngleA;	// For spot
	float attnAngleB;

	int distAttnMode;	// For spot/point
	int _pad1;
	int _pad2;
	int _pad3;
};

layout (std140, set = 1, binding = 3) uniform N2Lights
{
	N2Light lights[16];
	vec4 ambientBase[2];
	vec4 ambientOffset[2];
	ivec2 ambientMaterialBase;
	ivec2 ambientMaterialOffset;
	int lightCount;
	int useBaseOver;
	int bumpId0;
	int bumpId1;
} n2Lights;

void computeColors(inout vec4 baseCol, inout vec4 offsetCol, in int volIdx, in vec3 position, in vec3 normal)
{
	if (n2Uniform.constantColor[volIdx] == 1)
		return;
	vec3 diffuse = vec3(0.0);
	vec3 specular = vec3(0.0);
	float diffuseAlpha = 0.0;
	float specularAlpha = 0.0;
	vec3 reflectDir = reflect(normalize(position), normal);
	const float BASE_FACTOR = 2.0;

	for (int i = 0; i < n2Lights.lightCount; i++)
	{
		vec3 lightDir; // direction to the light
		vec3 lightColor = n2Lights.lights[i].color.rgb;
		if (n2Lights.lights[i].parallel == 1)
		{
			lightDir = normalize(n2Lights.lights[i].direction.xyz);
		}
		else
		{
			lightDir = normalize(n2Lights.lights[i].position.xyz - position);
			if (n2Lights.lights[i].attnDistA != 1.0 || n2Lights.lights[i].attnDistB != 0.0)
			{
				float distance = length(n2Lights.lights[i].position.xyz - position);
				if (n2Lights.lights[i].distAttnMode == 0)
					distance = 1.0 / distance;
				lightColor *= clamp(n2Lights.lights[i].attnDistB * distance + n2Lights.lights[i].attnDistA, 0.0, 1.0);
			}
			if (n2Lights.lights[i].attnAngleA != 1.0 || n2Lights.lights[i].attnAngleB != 0.0)
			{
				vec3 spotDir = n2Lights.lights[i].direction.xyz;
				float cosAngle = 1.0 - max(0.0, dot(lightDir, spotDir));
				lightColor *= clamp(cosAngle * n2Lights.lights[i].attnAngleB + n2Lights.lights[i].attnAngleA, 0.0, 1.0);
			}
		}
		if (n2Lights.lights[i].diffuse[volIdx] == 1)
		{
			float factor = (n2Lights.lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (n2Lights.lights[i].dmode == LMODE_SINGLE_SIDED)
				factor *= max(dot(normal, lightDir), 0.0);
			else if (n2Lights.lights[i].dmode == LMODE_DOUBLE_SIDED)
				factor *= abs(dot(normal, lightDir));

			if ((n2Lights.lights[i].routing & ROUTING_ALPHA) != 0)
				diffuseAlpha += lightColor.r * factor;
			else
			{
				if ((n2Lights.lights[i].routing & ROUTING_DIFF_TO_OFFSET) == 0)
					diffuse += lightColor * factor * baseCol.rgb;
				else
					specular += lightColor * factor * baseCol.rgb;
			}
		}
		if (n2Lights.lights[i].specular[volIdx] == 1)
		{
			float factor = (n2Lights.lights[i].routing & ROUTING_SUB) != 0 ? -BASE_FACTOR : BASE_FACTOR;
			if (n2Lights.lights[i].smode == LMODE_SINGLE_SIDED)
				factor *= clamp(pow(max(dot(lightDir, reflectDir), 0.0), n2Uniform.glossCoef[volIdx]), 0.0, 1.0);
			else if (n2Lights.lights[i].smode == LMODE_DOUBLE_SIDED)
				factor *= clamp(pow(abs(dot(lightDir, reflectDir)), n2Uniform.glossCoef[volIdx]), 0.0, 1.0);

			if ((n2Lights.lights[i].routing & ROUTING_ALPHA) != 0)
				specularAlpha += lightColor.r * factor;
			else
			{
				if ((n2Lights.lights[i].routing & ROUTING_SPEC_TO_OFFSET) == 0)
					diffuse += lightColor * factor * offsetCol.rgb;
				else
					specular += lightColor * factor * offsetCol.rgb;
			}
		}
	}
	// ambient light
	if (n2Lights.ambientMaterialBase[volIdx] == 1)
		diffuse += n2Lights.ambientBase[volIdx].rgb * baseCol.rgb;
	else
		diffuse += n2Lights.ambientBase[volIdx].rgb;
	if (n2Lights.ambientMaterialOffset[volIdx] == 1)
		specular += n2Lights.ambientOffset[volIdx].rgb * offsetCol.rgb;
	else
		specular += n2Lights.ambientOffset[volIdx].rgb;
	baseCol.rgb = diffuse;
	offsetCol.rgb = specular;

	baseCol.a += diffuseAlpha;
	offsetCol.a += specularAlpha;
	if (n2Lights.useBaseOver == 1)
	{
		vec4 overflow = max(baseCol - vec4(1.0), 0.0);
		offsetCol += overflow;
	}
	baseCol = clamp(baseCol, 0.0, 1.0);
	offsetCol = clamp(offsetCol, 0.0, 1.0);
}

void computeEnvMap(inout vec2 uv, in vec3 position, in vec3 normal)
{
	// Spherical mapping
	//vec3 r = reflect(normalize(position), normal);
	//float m = 2.0 * sqrt(r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
	//uv += r.xy / m + 0.5;

	// Cheap env mapping
	uv += normal.xy / 2.0 + 0.5;
	uv = clamp(uv, 0.0, 1.0);
}

void computeBumpMap(inout vec4 color0, in vec4 color1, in vec3 position, in vec3 normal, in mat4 normalMat)
{
	// TODO
	//if (n2Lights.bumpId0 == -1)
		return;
	normal = normalize(normal);
	vec3 tangent = color0.xyz;
	if (tangent.x > 0.5)
		tangent.x -= 1.0;
	if (tangent.y > 0.5)
		tangent.y -= 1.0;
	if (tangent.z > 0.5)
		tangent.z -= 1.0;
	tangent = normalize(tangent);
	vec3 bitangent = color1.xyz;
	if (bitangent.x > 0.5)
		bitangent.x -= 1.0;
	if (bitangent.y > 0.5)
		bitangent.y -= 1.0;
	if (bitangent.z > 0.5)
		bitangent.z -= 1.0;
	bitangent = normalize(bitangent);

	float scaleDegree = color0.w;
	float scaleOffset = color1.w;

	vec3 lightDir; // direction to the light
	if (n2Lights.lights[n2Lights.bumpId0].parallel == 1)
		lightDir = n2Lights.lights[n2Lights.bumpId0].direction.xyz;
	else
		lightDir = n2Lights.lights[n2Lights.bumpId0].position.xyz - position;
	lightDir = normalize(lightDir * mat3(normalMat));

	float n = dot(lightDir, normal);
	float cosQ = dot(lightDir, tangent);
	float sinQ = dot(lightDir, bitangent);

	float sinT = clamp(n, 0.0, 1.0);
	float k1 = 1.0 - scaleDegree;
	float k2 = scaleDegree * sinT;
	float k3 = scaleDegree * sqrt(1.0 - sinT * sinT); // cos T

	float q = acos(cosQ);
	if (sinQ < 0.0)
		q = 2.0 * PI - q;

	color0.r = k2;
	color0.g = k3;
	color0.b = q / PI / 2.0;
	color0.a = k1;
	color0 = clamp(color0, 0.0, 1.0);
}
)";

static const char N2VertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 ndcMat;
} uniformBuffer;

layout (location = 0) in vec4         in_pos;
layout (location = 1) in uvec4        in_base;
layout (location = 2) in uvec4        in_offs;
layout (location = 3) in mediump vec2 in_uv;
layout (location = 4) in vec3         in_normal;

layout (location = 0) INTERPOLATION out highp vec4 vtx_base;
layout (location = 1) INTERPOLATION out highp vec4 vtx_offs;
layout (location = 2) out highp vec3 vtx_uv;

void wDivide(inout vec4 vpos)
{
	vpos = vec4(vpos.xy / vpos.w, 1.0 / vpos.w, 1.0);
	vpos = uniformBuffer.ndcMat * vpos;
#if pp_Gouraud == 1
	vtx_base *= vpos.z;
	vtx_offs *= vpos.z;
#endif
	vtx_uv = vec3(vtx_uv.xy * vpos.z, vpos.z);
	vpos.w = 1.0;
	vpos.z = 0.0;
}

void main()
{
	vec4 vpos = n2Uniform.mvMat * in_pos;
	vtx_base = vec4(in_base) / 255.0;
	vtx_offs = vec4(in_offs) / 255.0;

	vec3 vnorm = normalize(mat3(n2Uniform.normalMat) * in_normal);

	// TODO bump mapping
	if (n2Uniform.bumpMapping == 0)
	{
		computeColors(vtx_base, vtx_offs, 0, vpos.xyz, vnorm);
		#if pp_Texture == 0
				vtx_base += vtx_offs;
		#endif
	}

	vtx_uv.xy = in_uv;
	if (n2Uniform.envMapping[0] == 1)
		computeEnvMap(vtx_uv.xy, vpos.xyz, vnorm);

	vpos = n2Uniform.projMat * vpos;
	wDivide(vpos);

	gl_Position = vpos;
}
)";

extern const char N2ModVolVertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform VertexShaderUniforms
{
	mat4 ndcMat;
} uniformBuffer;

layout (std140, set = 1, binding = 2) uniform N2VertexShaderUniforms
{
	mat4 mvMat;
	mat4 normalMat;
	mat4 projMat;
	ivec2 envMapping;
	int bumpMapping;
	int polyNumber;

	vec2 glossCoef;
	ivec2 constantColor;
} n2Uniform;

layout (location = 0) in vec4 in_pos;
layout (location = 0) out highp float depth;

void wDivide(inout vec4 vpos)
{
	vpos = vec4(vpos.xy / vpos.w, 1.0 / vpos.w, 1.0);
	vpos = uniformBuffer.ndcMat * vpos;
	depth = vpos.z;
	vpos.w = 1.0;
	vpos.z = 0.0;
}

void main()
{
	vec4 vpos = n2Uniform.mvMat * in_pos;
	vpos = n2Uniform.projMat * vpos;
	wDivide(vpos);

	gl_Position = vpos;
}
)";

vk::UniqueShaderModule ShaderManager::compileShader(const VertexShaderParams& params)
{
	VulkanSource src;
	if (!params.naomi2)
	{
		src.addConstant("pp_Gouraud", (int)params.gouraud)
				.addConstant("DIV_POS_Z", (int)params.divPosZ)
				.addSource(GouraudSource)
				.addSource(VertexShaderSource);
	}
	else
	{
		src.addConstant("pp_Gouraud", (int)params.gouraud)
				.addSource(GouraudSource)
				.addSource(N2LightShaderSource)
				.addSource(N2VertexShaderSource);
	}
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
		.addConstant("DIV_POS_Z", (int)params.divPosZ)
		.addConstant("DITHERING", (int)params.dithering)
		.addSource(GouraudSource)
		.addSource(FragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule ShaderManager::compileShader(const ModVolShaderParams& params)
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex,
			VulkanSource().addConstant("DIV_POS_Z", (int)params.divPosZ)
				.addSource(params.naomi2 ? N2ModVolVertexShaderSource : ModVolVertexShaderSource).generate());
}

vk::UniqueShaderModule ShaderManager::compileModVolFragmentShader(bool divPosZ)
{
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment,
			VulkanSource().addConstant("DIV_POS_Z", (int)divPosZ)
				.addSource(ModVolFragmentShaderSource).generate());
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
