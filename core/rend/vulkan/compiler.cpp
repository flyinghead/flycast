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
#include <glm/glm.hpp>
#include "compiler.h"
#include "SPIRV/GlslangToSpv.h"
#include "vulkan_context.h"

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
        /* .nonInductiveForLoops = */ true,
        /* .whileLoops = */ true,
        /* .doWhileLoops = */ true,
        /* .generalUniformIndexing = */ true,
        /* .generalAttributeMatrixVectorIndexing = */ true,
        /* .generalVaryingIndexing = */ true,
        /* .generalSamplerIndexing = */ true,
        /* .generalVariableIndexing = */ true,
        /* .generalConstantMatrixVectorIndexing = */ true,
}};

int ShaderCompiler::initCount;

void ShaderCompiler::Init()
{
	if (initCount++ == 0)
		verify(glslang::InitializeProcess());
}
void ShaderCompiler::Term()
{
	if (--initCount == 0)
		glslang::FinalizeProcess();
	initCount = std::max(initCount, 0);
}

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
		ERROR_LOG(RENDERER, "%s", shader.getInfoLog());
		ERROR_LOG(RENDERER, "%s", shader.getInfoDebugLog());
        return false;  // something didn't work
	}

	glslang::TProgram program;
	program.addShader(&shader);

	//
	// Program-level processing...
	//

	if (!program.link(messages))
	{
		ERROR_LOG(RENDERER, "%s", shader.getInfoLog());
		ERROR_LOG(RENDERER, "%s", shader.getInfoDebugLog());
		return false;
	}

	glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);
	return true;
}

vk::UniqueShaderModule ShaderCompiler::Compile(vk::ShaderStageFlagBits shaderStage, std::string const& shaderText)
{
	std::vector<unsigned int> shaderSPV;
	bool ok = GLSLtoSPV(shaderStage, shaderText, shaderSPV);
	verify(ok);

	return VulkanContext::Instance()->GetDevice().createShaderModuleUnique
			(vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), shaderSPV.size() * sizeof(unsigned int), shaderSPV.data()));
}
