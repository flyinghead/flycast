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
#include "SPIRV/GlslangToSpv.h"

static const char VertexShaderSource[] = R"(
#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define pp_Gouraud %d
#define ROTATE_90 %d

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

layout (std140, set = 0, binding = 0) uniform buffer
{
	vec4      scale;
	float     extra_depth_scale;
} uniformBuffer;

layout (location = 0) in vec4         in_pos;
layout (location = 1) in uvec4        in_base;
layout (location = 2) in uvec4        in_offs;
layout (location = 3) in mediump vec2 in_uv;

layout (location = 0) INTERPOLATION out lowp vec4 vtx_base;
layout (location = 1) INTERPOLATION out lowp vec4 vtx_offs;
layout (location = 2)               out mediump vec2 vtx_uv;

void main()
{
	vtx_base = vec4(in_base) / 255.0;
	vtx_offs = vec4(in_offs) / 255.0;
	vtx_uv = in_uv;
	vec4 vpos = in_pos;
	if (vpos.z < 0.0 || vpos.z > 3.4e37)
	{
		gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z);
		return;
	}

	vpos.w = uniformBuffer.extra_depth_scale / vpos.z;
	vpos.z = vpos.w;
#if ROTATE_90 == 1
	vpos.xy = vec2(vpos.y, -vpos.x); 
#endif
	vpos.xy = vpos.xy * uniformBuffer.scale.xy - uniformBuffer.scale.zw; 
	vpos.xy *= vpos.w; 
	gl_Position = vpos;
}
)";

static const char FragmentShaderSource[] = R"(
#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

#define cp_AlphaTest %d
#define pp_ClipTestMode %d
#define pp_UseAlpha %d
#define pp_Texture %d
#define pp_IgnoreTexA %d
#define pp_ShadInstr %d
#define pp_Offset %d
#define pp_FogCtrl %d
#define pp_Gouraud %d
#define pp_BumpMap %d
#define FogClamping %d
#define pp_TriLinear %d
#define PI 3.1415926

layout (location = 0) out vec4 FragColor;
#define gl_FragColor FragColor
#define FOG_CHANNEL r

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

layout (std140, set = 0, binding = 1) uniform buffer
{
	lowp float cp_AlphaTestValue;
	lowp vec4 pp_ClipTest;
	lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT;
	float sp_FOG_DENSITY;
	lowp float trilinear_alpha;
	lowp vec4 fog_clamp_min;
	lowp vec4 fog_clamp_max;
	float extra_depth_scale;
} uniformBuffer;

#if pp_Texture==1
layout (set = 1, binding = 0) uniform sampler2D tex;
#endif

// Vertex input
layout (location = 0) INTERPOLATION in lowp vec4 vtx_base;
layout (location = 1) INTERPOLATION in lowp vec4 vtx_offs;
layout (location = 2)               in mediump vec2 vtx_uv;

#if pp_FogCtrl != 2
layout (set = 1, binding = 1) uniform sampler2D fog_table;

lowp float fog_mode2(float w)
{
	float z = clamp(w * uniformBuffer.extra_depth_scale * uniformBuffer.sp_FOG_DENSITY, 1.0, 255.9999);
	float exp = floor(log2(z));
	float m = z * 16.0 / pow(2.0, exp) - 16.0;
	lowp float idx = floor(m) + exp * 16.0 + 0.5;
	vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
	return fog_coef.FOG_CHANNEL;
}
#endif

vec4 fog_clamp(lowp vec4 col)
{
#if FogClamping == 1
	return clamp(col, uniformBuffer.fog_clamp_min, uniformBuffer.fog_clamp_max);
#else
	return col;
#endif
}

void main()
{
	// Clip outside the box
	#if pp_ClipTestMode==1
		if (gl_FragCoord.x < uniformBuffer.pp_ClipTest.x || gl_FragCoord.x > uniformBuffer.pp_ClipTest.z
				|| gl_FragCoord.y < uniformBuffer.pp_ClipTest.y || gl_FragCoord.y > uniformBuffer.pp_ClipTest.w)
			discard;
	#endif
	// Clip inside the box
	#if pp_ClipTestMode==-1
		if (gl_FragCoord.x >= uniformBuffer.pp_ClipTest.x && gl_FragCoord.x <= uniformBuffer.pp_ClipTest.z
				&& gl_FragCoord.y >= uniformBuffer.pp_ClipTest.y && gl_FragCoord.y <= uniformBuffer.pp_ClipTest.w)
			discard;
	#endif
	
	lowp vec4 color=vtx_base;
	#if pp_UseAlpha==0
		color.a=1.0;
	#endif
	#if pp_FogCtrl==3
		color=vec4(uniformBuffer.sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));
	#endif
	#if pp_Texture==1
	{
		lowp vec4 texcol=texture(tex, vtx_uv);
		
		#if pp_BumpMap == 1
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1
				texcol.a=1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (uniformBuffer.cp_AlphaTestValue > texcol.a)
					discard;
			#endif 
		#endif
		#if pp_ShadInstr==0
		{
			color=texcol;
		}
		#endif
		#if pp_ShadInstr==1
		{
			color.rgb*=texcol.rgb;
			color.a=texcol.a;
		}
		#endif
		#if pp_ShadInstr==2
		{
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
		}
		#endif
		#if  pp_ShadInstr==3
		{
			color*=texcol;
		}
		#endif
		
		#if pp_Offset==1 && pp_BumpMap == 0
		{
			color.rgb+=vtx_offs.rgb;
		}
		#endif
	}
	#endif
	
	color = fog_clamp(color);
	
	#if pp_FogCtrl == 0
	{
		color.rgb=mix(color.rgb,uniformBuffer.sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); 
	}
	#endif
	#if pp_FogCtrl == 1 && pp_Offset==1 && pp_BumpMap == 0
	{
		color.rgb=mix(color.rgb,uniformBuffer.sp_FOG_COL_VERT.rgb,vtx_offs.a);
	}
	#endif
	
	#if pp_TriLinear == 1
	color *= uniformBuffer.trilinear_alpha;
	#endif
	
	#if cp_AlphaTest == 1
		color.a=1.0;
	#endif 
	//color.rgb=vec3(gl_FragCoord.w * uniformBuffer.sp_FOG_DENSITY / 128.0);

	float w = gl_FragCoord.w * 100000.0;
	gl_FragDepth = log2(1.0 + w) / 34.0;

	gl_FragColor = color;
}
)";

static const char ModVolShaderSource[] = R"(
#version 400
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

out vec4 FragColor;

layout (std140, binding = 1) uniform buffer
{
	lowp float sp_ShaderColor;
} uniformBuffer;

void main()
{
	float w = gl_FragCoord.w * 100000.0;
	gl_FragDepth = log2(1.0 + w) / 34.0;
	FragColor = vec4(0.0, 0.0, 0.0, uniformBuffer.sp_ShaderColor);
}
)";

static const TBuiltInResource DefaultTBuiltInResource = {
    /* .MaxLights = */ 32,
    /* .MaxClipPlanes = */ 6,
    /* .MaxTextureUnits = */ 32,
    /* .MaxTextureCoords = */ 32,
    /* .MaxVertexAttribs = */ 64,
    /* .MaxVertexUniformComponents = */ 4096,
    /* .MaxVaryingFloats = */ 64,
    /* .MaxVertexTextureImageUnits = */ 32,
    /* .MaxCombinedTextureImageUnits = */ 80,
    /* .MaxTextureImageUnits = */ 32,
    /* .MaxFragmentUniformComponents = */ 4096,
    /* .MaxDrawBuffers = */ 32,
    /* .MaxVertexUniformVectors = */ 128,
    /* .MaxVaryingVectors = */ 8,
    /* .MaxFragmentUniformVectors = */ 16,
    /* .MaxVertexOutputVectors = */ 16,
    /* .MaxFragmentInputVectors = */ 15,
    /* .MinProgramTexelOffset = */ -8,
    /* .MaxProgramTexelOffset = */ 7,
    /* .MaxClipDistances = */ 8,
    /* .MaxComputeWorkGroupCountX = */ 65535,
    /* .MaxComputeWorkGroupCountY = */ 65535,
    /* .MaxComputeWorkGroupCountZ = */ 65535,
    /* .MaxComputeWorkGroupSizeX = */ 1024,
    /* .MaxComputeWorkGroupSizeY = */ 1024,
    /* .MaxComputeWorkGroupSizeZ = */ 64,
    /* .MaxComputeUniformComponents = */ 1024,
    /* .MaxComputeTextureImageUnits = */ 16,
    /* .MaxComputeImageUniforms = */ 8,
    /* .MaxComputeAtomicCounters = */ 8,
    /* .MaxComputeAtomicCounterBuffers = */ 1,
    /* .MaxVaryingComponents = */ 60,
    /* .MaxVertexOutputComponents = */ 64,
    /* .MaxGeometryInputComponents = */ 64,
    /* .MaxGeometryOutputComponents = */ 128,
    /* .MaxFragmentInputComponents = */ 128,
    /* .MaxImageUnits = */ 8,
    /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
    /* .MaxCombinedShaderOutputResources = */ 8,
    /* .MaxImageSamples = */ 0,
    /* .MaxVertexImageUniforms = */ 0,
    /* .MaxTessControlImageUniforms = */ 0,
    /* .MaxTessEvaluationImageUniforms = */ 0,
    /* .MaxGeometryImageUniforms = */ 0,
    /* .MaxFragmentImageUniforms = */ 8,
    /* .MaxCombinedImageUniforms = */ 8,
    /* .MaxGeometryTextureImageUnits = */ 16,
    /* .MaxGeometryOutputVertices = */ 256,
    /* .MaxGeometryTotalOutputComponents = */ 1024,
    /* .MaxGeometryUniformComponents = */ 1024,
    /* .MaxGeometryVaryingComponents = */ 64,
    /* .MaxTessControlInputComponents = */ 128,
    /* .MaxTessControlOutputComponents = */ 128,
    /* .MaxTessControlTextureImageUnits = */ 16,
    /* .MaxTessControlUniformComponents = */ 1024,
    /* .MaxTessControlTotalOutputComponents = */ 4096,
    /* .MaxTessEvaluationInputComponents = */ 128,
    /* .MaxTessEvaluationOutputComponents = */ 128,
    /* .MaxTessEvaluationTextureImageUnits = */ 16,
    /* .MaxTessEvaluationUniformComponents = */ 1024,
    /* .MaxTessPatchComponents = */ 120,
    /* .MaxPatchVertices = */ 32,
    /* .MaxTessGenLevel = */ 64,
    /* .MaxViewports = */ 16,
    /* .MaxVertexAtomicCounters = */ 0,
    /* .MaxTessControlAtomicCounters = */ 0,
    /* .MaxTessEvaluationAtomicCounters = */ 0,
    /* .MaxGeometryAtomicCounters = */ 0,
    /* .MaxFragmentAtomicCounters = */ 8,
    /* .MaxCombinedAtomicCounters = */ 8,
    /* .MaxAtomicCounterBindings = */ 1,
    /* .MaxVertexAtomicCounterBuffers = */ 0,
    /* .MaxTessControlAtomicCounterBuffers = */ 0,
    /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
    /* .MaxGeometryAtomicCounterBuffers = */ 0,
    /* .MaxFragmentAtomicCounterBuffers = */ 1,
    /* .MaxCombinedAtomicCounterBuffers = */ 1,
    /* .MaxAtomicCounterBufferSize = */ 16384,
    /* .MaxTransformFeedbackBuffers = */ 4,
    /* .MaxTransformFeedbackInterleavedComponents = */ 64,
    /* .MaxCullDistances = */ 8,
    /* .MaxCombinedClipAndCullDistances = */ 8,
    /* .MaxSamples = */ 4,
    /* .maxMeshOutputVerticesNV = */ 256,
    /* .maxMeshOutputPrimitivesNV = */ 512,
    /* .maxMeshWorkGroupSizeX_NV = */ 32,
    /* .maxMeshWorkGroupSizeY_NV = */ 1,
    /* .maxMeshWorkGroupSizeZ_NV = */ 1,
    /* .maxTaskWorkGroupSizeX_NV = */ 32,
    /* .maxTaskWorkGroupSizeY_NV = */ 1,
    /* .maxTaskWorkGroupSizeZ_NV = */ 1,
    /* .maxMeshViewCountNV = */ 4,

    /* .limits = */ {
        /* .nonInductiveForLoops = */ 1,
        /* .whileLoops = */ 1,
        /* .doWhileLoops = */ 1,
        /* .generalUniformIndexing = */ 1,
        /* .generalAttributeMatrixVectorIndexing = */ 1,
        /* .generalVaryingIndexing = */ 1,
        /* .generalSamplerIndexing = */ 1,
        /* .generalVariableIndexing = */ 1,
        /* .generalConstantMatrixVectorIndexing = */ 1,
    }};

static EShLanguage translateShaderStage(vk::ShaderStageFlagBits stage)
{
	switch (stage)
	{
	case vk::ShaderStageFlagBits::eVertex:                  return EShLangVertex;
	case vk::ShaderStageFlagBits::eTessellationControl:     return EShLangTessControl;
	case vk::ShaderStageFlagBits::eTessellationEvaluation:  return EShLangTessEvaluation;
	case vk::ShaderStageFlagBits::eGeometry:                return EShLangGeometry;
	case vk::ShaderStageFlagBits::eFragment:                return EShLangFragment;
	case vk::ShaderStageFlagBits::eCompute:                 return EShLangCompute;
	case vk::ShaderStageFlagBits::eRaygenNV:                return EShLangRayGenNV;
	case vk::ShaderStageFlagBits::eAnyHitNV:                return EShLangAnyHitNV;
	case vk::ShaderStageFlagBits::eClosestHitNV:            return EShLangClosestHitNV;
	case vk::ShaderStageFlagBits::eMissNV:                  return EShLangMissNV;
	case vk::ShaderStageFlagBits::eIntersectionNV:          return EShLangIntersectNV;
	case vk::ShaderStageFlagBits::eCallableNV:              return EShLangCallableNV;
	case vk::ShaderStageFlagBits::eTaskNV:                  return EShLangTaskNV;
	case vk::ShaderStageFlagBits::eMeshNV:                  return EShLangMeshNV;
	default:
		die("Unknown shader stage");
		return EShLangVertex;
	}
}

static bool GLSLtoSPV(const vk::ShaderStageFlagBits shaderType, std::string const& glslShader, std::vector<unsigned int> &spvShader)
    {
      EShLanguage stage = translateShaderStage(shaderType);

      const char *shaderStrings[1];
      shaderStrings[0] = glslShader.data();

      glslang::TShader shader(stage);
      shader.setStrings(shaderStrings, 1);

      // Enable SPIR-V and Vulkan rules when parsing GLSL
      EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

      if (!shader.parse(&DefaultTBuiltInResource, 100, false, messages))
      {
        puts(shader.getInfoLog());
        puts(shader.getInfoDebugLog());
        return false;  // something didn't work
      }

      glslang::TProgram program;
      program.addShader(&shader);

      //
      // Program-level processing...
      //

      if (!program.link(messages))
      {
        puts(shader.getInfoLog());
        puts(shader.getInfoDebugLog());
        fflush(stdout);
        return false;
      }

      glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);
      return true;
    }

static vk::UniqueShaderModule createShaderModule(vk::UniqueDevice &device, vk::ShaderStageFlagBits shaderStage, std::string const& shaderText)
{
	std::vector<unsigned int> shaderSPV;
	bool ok = GLSLtoSPV(shaderStage, shaderText, shaderSPV);
	verify(ok);

	return device->createShaderModuleUnique(vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), shaderSPV.size() * sizeof(unsigned int), shaderSPV.data()));
}

vk::UniqueShaderModule ShaderManager::compileShader(const VertexShaderParams& params)
{
	char buf[sizeof(VertexShaderSource) * 2];

	sprintf(buf, VertexShaderSource, (int)params.gouraud, (int)params.rotate90);
	return createShaderModule(VulkanContext::Instance()->GetDevice(), vk::ShaderStageFlagBits::eVertex, buf);
}

vk::UniqueShaderModule ShaderManager::compileShader(const FragmentShaderParams& params)
{
	char buf[sizeof(FragmentShaderSource) * 2];

	sprintf(buf, FragmentShaderSource, (int)params.alphaTest, params.clipTest, (int)params.useAlpha, (int)params.texture,
			(int)params.ignoreTexAlpha, params.shaderInstr, (int)params.offset, params.fog, (int)params.gouraud,
			(int)params.bumpmap, (int)params.clamping, (int)params.trilinear);
	return createShaderModule(VulkanContext::Instance()->GetDevice(), vk::ShaderStageFlagBits::eFragment, buf);
}

vk::UniqueShaderModule ShaderManager::compileModVolShader()
{
	return createShaderModule(VulkanContext::Instance()->GetDevice(), vk::ShaderStageFlagBits::eFragment, ModVolShaderSource);
}
