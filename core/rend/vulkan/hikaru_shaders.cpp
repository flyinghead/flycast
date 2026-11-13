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
#include "hikaru_shaders.h"
#include "compiler.h"
#include "utils.h"

// Shading record, same layout as hikaru::Shading
static const char ShadingUniforms[] = R"(
struct Light
{
	vec4 color;
	vec4 ambient;
	vec4 vec;
	vec4 att;
	vec4 spot;
};

layout (std140, set = 0, binding = 1) uniform ShadingUniforms
{
	vec4 base0;
	vec4 base1;
	vec4 ambient;
	vec4 matSpecular;
	vec4 fogColor;
	vec4 fogParams;		// fog (z = 1): x density, y offset; depth cue (z = 2): x scale, y start
	ivec4 modes;		// lit, texture mode, light count, alternate mode
	vec4 alphaTest;		// min, max, mode, double-sided
	vec4 clipZ;			// near plane, near clip enabled, LOD bias, depth bias scale
	Light lights[4];
} shading;
)";

// Vertices stay in object space. Each vertex picks its modelview (mode bit 0: the second one) and
// may leave out its rotation (bit 1) or its translation (bit 2).
// They are lit per pixel in eye space (looking down -Z). The position is output in clip space.
// Vulkan clips 0 <= z <= w: z = w - near clips at the near plane, the depth is written by the fragment shader.
static const char VertexShaderSource[] = R"(
layout (std140, set = 0, binding = 0) uniform DrawUniforms
{
	mat4 modelview0;
	mat4 modelview1;
	mat4 projection;	// eye space to screen pixels (640x480, y down) times w
	vec4 params;		// x: 0x501 pixel pattern
} draw;

layout (location = 0) in vec3 in_pos;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_uv;
layout (location = 3) in float in_alpha;
layout (location = 4) in uint in_mode;

layout (location = 0) out highp vec4 vtx_uv;		// xy: texture coordinates, w: eye depth
layout (location = 1) out highp float vtx_alpha;
layout (location = 2) out highp vec3 vtx_eye;
layout (location = 3) out highp vec3 vtx_normal;

void main()
{
	mat4 mv = (in_mode & 1u) != 0u ? draw.modelview1 : draw.modelview0;
	vec3 eye = in_pos;
	vec3 normal = in_normal;
	if ((in_mode & 2u) == 0u)
	{
		eye = mat3(mv) * eye;
		normal = mat3(mv) * normal;
	}
	if ((in_mode & 4u) == 0u)
		eye += mv[3].xyz;
	vec4 pos = draw.projection * vec4(eye, 1.0);
	// Vulkan clip space: y points down
	float z = shading.clipZ.y != 0.0 ? pos.w - shading.clipZ.x : 0.0;
	gl_Position = vec4(pos.x / 320.0 - pos.w, pos.y / 240.0 - pos.w, z, pos.w);
	vtx_uv = vec4(in_uv, 0.0, pos.w);
	vtx_alpha = in_alpha;
	vtx_eye = eye;
	vtx_normal = normal;
}
)";

static const char FragmentShaderSource[] = R"(
#if TEXTURED == 1
layout (set = 0, binding = 2) uniform sampler2D tex;
#endif

layout (location = 0) in highp vec4 vtx_uv;
layout (location = 1) in highp float vtx_alpha;
layout (location = 2) in highp vec3 vtx_eye;
layout (location = 3) in highp vec3 vtx_normal;

layout (location = 0) out vec4 FragColor;

layout (std140, set = 0, binding = 0) uniform DrawUniforms
{
	mat4 modelview0;
	mat4 modelview1;
	mat4 projection;
	vec4 params;		// x: 0x501 pixel pattern
} draw;

void addLight(Light l, vec3 eye, vec3 N, inout vec3 diffuse, inout vec3 specular)
{
	vec3 L;
	float atten = 1.0;
	if (l.vec.w == 1.0)
	{
		vec3 delta = l.vec.xyz - eye;
		float d = length(delta);
		L = delta / max(d, 1e-6);
		if (l.att.z == 0.0)
		{
			float f = l.att.w == 0.0 ? d : l.att.w == 1.0 ? d * d : l.att.w == 2.0 ? 1.0 / d : 1.0 / (d * d);
			atten = clamp(l.att.x * (l.att.y + f), 0.0, 1.0);
		}
	}
	else
		L = normalize(l.vec.xyz);
	if (any(notEqual(l.spot.xyz, vec3(0.0))))
		atten *= clamp(dot(L, l.spot.xyz) - l.spot.w, 0.0, 1.0);	// spot light: fade = saturate(L . spot.xyz - spot.w), spot.xyz = axis toward the light * scale
	float nl = max(dot(N, L), 0.0);
	diffuse += atten * (nl * l.color.rgb + l.ambient.rgb);
	if (nl > 0.0 && shading.matSpecular.w > 0.0)
	{
		vec3 R = reflect(-L, N);
		float s = max(dot(R, -normalize(eye)), 0.0);
		specular += atten * pow(s, shading.matSpecular.w) * l.color.rgb;
	}
}

void main()
{
	// 0x501: only the pixels of the 4x4 pattern are drawn
	ivec2 cell = ivec2(gl_FragCoord.xy) & 3;
	if (((int(draw.params.x) >> (cell.x + cell.y * 4)) & 1) == 0)
		discard;
	float eyeDepth = vtx_uv.w;
	float alpha = vtx_alpha;
	vec4 res;
	res.a = alpha;
#if TEXTURED == 1
	vec4 texCol = texture(tex, vtx_uv.xy, shading.clipZ.z);
	if (shading.modes.w != 0)
	{
		res.rgb = mix(shading.base0.rgb, texCol.rgb, alpha);
		res.a = texCol.a;
	}
	else if (shading.modes.y == 0)
	{
		res.rgb = texCol.rgb;
		res.a *= texCol.a;
	}
	else if (shading.modes.y == 1)
		res.rgb = mix(shading.base0.rgb, texCol.rgb, texCol.a);
	else if (shading.modes.y == 2)
	{
		res.rgb = shading.base1.rgb * texCol.rgb;
		res.a *= texCol.a;
	}
	else if (shading.modes.y == 5)
	{
		res.rgb = mix(shading.base0.rgb, shading.base1.rgb, texCol.rgb);
		res.a *= texCol.a;
	}
	else
	{
		res.rgb = shading.base0.rgb;
		res.a *= texCol.a;
	}
#else
	if (shading.modes.w != 0)
	{
		res.rgb = mix(shading.base0.rgb, shading.base1.rgb, alpha);
		res.a = 1.0;
	}
	else
		res.rgb = shading.base0.rgb;
#endif
	vec3 eye = vtx_eye;
	if (shading.modes.x != 0)
	{
		vec3 N = normalize(vtx_normal);
		if (shading.alphaTest.w != 0.0 && gl_FrontFacing)	// the back faces of the Hikaru are the rasterizer's front faces
			N = -N;	// double-sided: the back face takes the reversed normal
		vec3 diffuse = vec3(0.0);
		vec3 specular = vec3(0.0);
		for (int i = 0; i < shading.modes.z; i++)
			addLight(shading.lights[i], eye, N, diffuse, specular);
		res.rgb *= shading.ambient.rgb + diffuse;
		res.rgb += shading.matSpecular.rgb * specular;
	}
	res = clamp(res, 0.0, 1.0);
	if (shading.fogParams.z == 1.0)
	{
		// visibility: 0 = fully fogged
		float fa = clamp(shading.fogParams.x * (eye.z + shading.fogParams.y), 0.0, 1.0);
		res.rgb = mix(shading.fogColor.rgb, res.rgb, fa);
	}

	if (shading.alphaTest.z == 1.0)
	{
		// alpha-tested polygons: keep the range, then opaque
		if (res.a < shading.alphaTest.x || res.a > shading.alphaTest.y)
			discard;
		res.a = 1.0;
	}
	else if (shading.alphaTest.z == 2.0 && res.a < 1.0 / 255.0)
		discard;

	// logarithmic depth of 1 / eye depth: greater is nearer
	if (shading.fogParams.z == 2.0)
	{
		// depth cue: the polygon fades into what is behind it with its eye depth (Odin)
		float cue = clamp((-eye.z - shading.fogParams.y) * shading.fogParams.x, 0.0, 1.0);
		if (cue >= 1.0)
			discard;
		res.a = 1.0 - cue;
	}
	gl_FragDepth = clamp(log2(1.0 + 100000.0 / max(eyeDepth * shading.clipZ.w, 1e-6)) / 34.0, 0.0, 1.0);
	FragColor = res;
}
)";

// Full-screen passes: the frame blender and the final picture. Point sampled.
static const char ScreenVertexShaderSource[] = R"(
layout (location = 0) out vec2 vtx_uv;

void main()
{
	vtx_uv = vec2(float(gl_VertexIndex & 1), float(gl_VertexIndex >> 1));
	gl_Position = vec4(vtx_uv * 2.0 - 1.0, 0.0, 1.0);
}
)";

static const char ScreenUniforms[] = R"(
layout (set = 0, binding = 0) uniform sampler2D slot0;
layout (set = 0, binding = 1) uniform sampler2D slot1;
layout (set = 0, binding = 2) uniform sampler2D slot2;

layout (push_constant) uniform pushBlock
{
	vec4 blendFactor;	// x: frame blend factor
	vec4 scroll0;		// xy: layer scroll of each slot, in texture coordinates
	vec4 scroll1;
	vec4 scroll2;
	ivec4 slotKind;		// per slot: 0 none, 1 3D picture, 2 layer. Blender: x = 0 when the source reads as black
	vec4 keyed3D;		// x: 1 when the 3D picture is keyed on its alpha
} pc;

layout (location = 0) in vec2 vtx_uv;
layout (location = 0) out vec4 FragColor;
)";

// Frame blender: the source with the blend factor as alpha, blended over the target 3D buffer
static const char BlendFragmentShaderSource[] = R"(
void main()
{
	vec3 c = pc.slotKind.x != 0 ? texture(slot0, vtx_uv).rgb : vec3(0.0);
	FragColor = vec4(c, pc.blendFactor.x);
}
)";

// Final picture: the topmost slot whose pixel is not transparent wins, the bottom slot is opaque
static const char CompositeFragmentShaderSource[] = R"(
bool sampleSlot(sampler2D tex, int kind, vec2 offset, out vec4 c)
{
	if (kind == 1)
	{
		c = texture(tex, vtx_uv);
		return pc.keyed3D.x != 0.0 ? c.a != 0.0 : any(notEqual(c.rgb, vec3(0.0)));
	}
	// the layers are sampled with repeat: the scroll wraps
	c = texture(tex, vtx_uv + offset);
	return c.a != 0.0;
}

void main()
{
	vec4 c = vec4(0.0);
	if (pc.slotKind.z != 0 && sampleSlot(slot2, pc.slotKind.z, pc.scroll2.xy, c))
		;
	else if (pc.slotKind.y != 0 && sampleSlot(slot1, pc.slotKind.y, pc.scroll1.xy, c))
		;
	else if (pc.slotKind.x != 0)
		sampleSlot(slot0, pc.slotKind.x, pc.scroll0.xy, c);
	else
		c = vec4(0.0);
	FragColor = vec4(c.rgb, 1.0);
}
)";

vk::UniqueShaderModule HikaruShaderManager::compileVertexShader()
{
	VulkanSource src;
	src.addSource(ShadingUniforms)
		.addSource(VertexShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}

vk::UniqueShaderModule HikaruShaderManager::compileFragmentShader(bool textured)
{
	VulkanSource src;
	src.addConstant("TEXTURED", (int)textured)
		.addSource(ShadingUniforms)
		.addSource(FragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}

vk::UniqueShaderModule HikaruShaderManager::compileScreenVertexShader()
{
	VulkanSource src;
	src.addSource(ScreenVertexShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eVertex, src.generate());
}

vk::UniqueShaderModule HikaruShaderManager::compileScreenFragmentShader(bool composite)
{
	VulkanSource src;
	src.addSource(ScreenUniforms)
		.addSource(composite ? CompositeFragmentShaderSource : BlendFragmentShaderSource);
	return ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, src.generate());
}
