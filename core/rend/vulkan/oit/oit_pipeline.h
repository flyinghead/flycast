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
#pragma once
#include "../vulkan.h"
#include "oit_shaders.h"
#include "oit_renderpass.h"
#include "oit_buffer.h"
#include "../texture.h"
#include "../desc_set.h"
#include "../pipeline.h"

#include <glm/glm.hpp>
#include <unordered_map>

class OITDescriptorSets : BaseDescriptorSets
{
public:
	// std140 alignment required
	struct VertexShaderUniforms
	{
		glm::mat4 ndcMat;
	};

	// std140 alignment required
	struct FragmentShaderUniforms
	{
		float colorClampMin[4];
		float colorClampMax[4];
		float sp_FOG_COL_RAM[4];	// Only using 3 elements but easier for std140
		float sp_FOG_COL_VERT[4];	// same comment
		float ditherDivisor[4];
		float cp_AlphaTestValue;
		float sp_FOG_DENSITY;
		float shade_scale_factor;	// new for OIT
		u32 pixelBufferSize;
		u64 pixelBufferAddress;
		u32 viewportWidth;
		u32 _pad;
	};

	struct PushConstants
	{
		glm::vec4 clipTest;
		glm::ivec4 blend_mode0;	// Only using 2 elements but easier for std140
		float trilinearAlpha;
		float palette_index;
		int _pad[2];

		// two volume mode
		glm::ivec4 blend_mode1;	// Only using 2 elements but easier for std140
		int shading_instr0;
		int shading_instr1;
		int fog_control0;
		int fog_control1;
		int use_alpha0;
		int use_alpha1;
		int ignore_tex_alpha0;
		int ignore_tex_alpha1;
	};
	static_assert(sizeof(PushConstants) == 96, "PushConstants size changed. Update vertex push constant layout(offset) in vertex shaders");

	struct VtxPushConstants
	{
		int polyNumber;
	};

	void init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout,
			vk::DescriptorSetLayout perPolyLayout, vk::DescriptorSetLayout colorInputLayout);
	void term();

	void nextFrame();
	// FIXME way too many params
	void updateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset, vk::ImageView fogImageView,
			u32 polyParamsOffset, u32 polyParamsSize, vk::ImageView stencilImageView, vk::ImageView depthImageView,
			vk::ImageView paletteImageView, OITBuffers *oitBuffers);
	void updateColorInputDescSet(int index, vk::ImageView colorImageView);
	void bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const PolyParam& poly, int polyNumber, vk::Buffer buffer,
			vk::DeviceSize uniformOffset, vk::DeviceSize lightOffset, bool punchThrough);
	void bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const ModifierVolumeParam& mvParam, int polyNumber,
			vk::Buffer buffer, vk::DeviceSize uniformOffset);

	void bindPerFrameDescriptorSets(vk::CommandBuffer cmdBuffer) {
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, perFrameDescSet, nullptr);
	}

	void bindColorInputDescSet(vk::CommandBuffer cmdBuffer, int index) {
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 2, colorInputDescSets[index], nullptr);
	}

private:
	vk::PipelineLayout pipelineLayout;

	std::array<vk::DescriptorSet, 2> colorInputDescSets;
	DynamicDescSetAlloc perFrameAlloc;
	DynamicDescSetAlloc perPolyAlloc;
	DynamicDescSetAlloc colorInputAlloc;
	vk::DescriptorSet perFrameDescSet = {};
	std::unordered_map<const void *, vk::DescriptorSet> perPolyDescSets;
};

class OITPipelineManager
{
public:
	OITPipelineManager() : renderPasses(&ownRenderPasses) {}
	virtual ~OITPipelineManager() = default;
	virtual void Init(OITShaderManager *shaderManager, OITBuffers *oitBuffers);

	vk::Pipeline GetPipeline(u32 listType, bool autosort, const PolyParam& pp, Pass pass, int gpuPalette)
	{
		const bool useBDA = oitBuffers->getPixelBufferAddress();
		u64 pipehash = hash(listType, autosort, &pp, pass, gpuPalette, useBDA);
		const auto &pipeline = pipelines.find(pipehash);
		if (pipeline != pipelines.end())
			return pipeline->second.get();

		CreatePipeline(listType, autosort, pp, pass, gpuPalette, useBDA);

		return *pipelines[pipehash];
	}

	vk::Pipeline GetModifierVolumePipeline(ModVolMode mode, int cullMode, bool naomi2)
	{
		u32 pipehash = hash(mode, cullMode, naomi2, false);
		const auto &pipeline = modVolPipelines.find(pipehash);
		if (pipeline != modVolPipelines.end())
			return pipeline->second.get();
		CreateModVolPipeline(mode, cullMode, naomi2);

		return *modVolPipelines[pipehash];
	}
	vk::Pipeline GetTrModifierVolumePipeline(ModVolMode mode, int cullMode, bool naomi2)
	{
		checkMaxLayers();
		const bool useBDA = oitBuffers->getPixelBufferAddress();
		u32 pipehash = hash(mode, cullMode, naomi2, useBDA);
		const auto &pipeline = trModVolPipelines.find(pipehash);
		if (pipeline != trModVolPipelines.end())
			return pipeline->second.get();
		CreateTrModVolPipeline(mode, cullMode, naomi2, useBDA);

		return *trModVolPipelines[pipehash];
	}
	vk::Pipeline GetFinalPipeline(bool dithering)
	{
		checkMaxLayers();
		const bool useBDA = oitBuffers->getPixelBufferAddress();
		u32 pipehash = hash(dithering, useBDA);
		const auto &pipeline = finalPipelines.find(pipehash);
		if (pipeline != finalPipelines.end())
			return pipeline->second.get();
		CreateFinalPipeline(dithering, useBDA);

		return *finalPipelines[pipehash];
	}
	vk::Pipeline GetClearPipeline()
	{
		if (!clearPipeline)
			CreateClearPipeline();
		return *clearPipeline;
	}
	vk::PipelineLayout GetPipelineLayout() const { return *pipelineLayout; }
	vk::DescriptorSetLayout GetPerFrameDSLayout() const { return *perFrameLayout; }
	vk::DescriptorSetLayout GetPerPolyDSLayout() const { return *perPolyLayout; }
	vk::DescriptorSetLayout GetColorInputDSLayout() const { return *colorInputLayout; }

	vk::RenderPass GetRenderPass(bool initial, bool last, bool loadClear = false) { return renderPasses->GetRenderPass(initial, last, loadClear); }

private:
	void CreateModVolPipeline(ModVolMode mode, int cullMode, bool naomi2);
	void CreateTrModVolPipeline(ModVolMode mode, int cullMode, bool naomi2, bool useBDA);

	u64 hash(u32 listType, bool autosort, const PolyParam *pp, Pass pass, int gpuPalette, bool useBDA) const
	{
		u64 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
			| (((pp->tileclip >> 28) == 3) << 4);
		hash |= ((listType >> 1) << 5);
		if (pp->tcw1.full != (u32)-1 || pp->tsp1.full != (u32)-1)
		{
			// Two-volume mode
			hash |= ((u64)1 << 33) | (pp->tsp.ColorClamp << 11);
		}
		else
		{
			bool ignoreTexAlpha = pp->tsp.IgnoreTexA || pp->tcw.PixelFmt == Pixel565;
			hash |= (pp->tsp.ShadInstr << 7) | (ignoreTexAlpha << 9) | (pp->tsp.UseAlpha << 10)
				| (pp->tsp.ColorClamp << 11) | ((config::Fog ? pp->tsp.FogCtrl : 2) << 12)
				| (pp->tsp.SrcInstr << 14) | (pp->tsp.DstInstr << 17);
		}
		hash |= (pp->isp.ZWriteDis << 20) | (pp->isp.CullMode << 21) | ((autosort ? 6 : pp->isp.DepthMode) << 23);
		hash |= ((u64)gpuPalette << 26) | ((u64)pass << 28) | ((u64)pp->isNaomi2() << 30);
		hash |= (u64)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 31;
		hash |= (u64)(pp->tcw.PixelFmt == PixelBumpMap) << 32;
		hash |= (u64)useBDA << 33;

		return hash;
	}
	u32 hash(ModVolMode mode, int cullMode, bool naomi2, bool useBDA) const
	{
		return ((int)mode << 2) | cullMode | ((u32)naomi2 << 5) | ((u32)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 6) | ((u32)useBDA << 7);
	}
	u32 hash(bool dithering, bool useBDA) const
	{
		return (u32)dithering | ((u32)useBDA << 1);
	}

	vk::PipelineVertexInputStateCreateInfo GetMainVertexInputStateCreateInfo(bool full = true, bool naomi2 = false) const;

	void CreatePipeline(u32 listType, bool autosort, const PolyParam& pp, Pass pass, int gpuPalette, bool useBDA);
	void CreateFinalPipeline(bool dithering, bool useBDA);
	void CreateClearPipeline();
	void checkMaxLayers();

	std::map<u64, vk::UniquePipeline> pipelines;
	std::map<u32, vk::UniquePipeline> modVolPipelines;
	std::map<u32, vk::UniquePipeline> trModVolPipelines;
	std::map<u32, vk::UniquePipeline> finalPipelines;
	vk::UniquePipeline clearPipeline;

	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout perFrameLayout;
	vk::UniqueDescriptorSetLayout colorInputLayout;
	vk::UniqueDescriptorSetLayout perPolyLayout;
	RenderPasses ownRenderPasses;
	int maxLayers = 0;

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	RenderPasses *renderPasses;
	OITShaderManager *shaderManager = nullptr;
	OITBuffers *oitBuffers = nullptr;
};

class RttOITPipelineManager : public OITPipelineManager
{
public:
	RttOITPipelineManager() { renderPasses = &rttRenderPasses; }
	void Init(OITShaderManager *shaderManager, OITBuffers *oitBuffers) override
	{
		OITPipelineManager::Init(shaderManager, oitBuffers);

		renderToTextureBuffer = config::RenderToTextureBuffer;
		rttRenderPasses.Reset();
	}
	void CheckSettingsChange()
	{
		if (renderToTextureBuffer != config::RenderToTextureBuffer)
		{
			Init(shaderManager, oitBuffers);
		}
	}

private:
	bool renderToTextureBuffer = false;
	RttRenderPasses rttRenderPasses;
};
