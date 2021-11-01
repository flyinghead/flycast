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

#include <glm/glm.hpp>
#include <tuple>

class OITDescriptorSets
{
public:
	OITDescriptorSets() = default;
	OITDescriptorSets(OITDescriptorSets&&) = default;
	OITDescriptorSets(const OITDescriptorSets&) = delete;
	OITDescriptorSets& operator=(OITDescriptorSets&&) = default;
	OITDescriptorSets& operator=(const OITDescriptorSets&) = delete;

	// std140 alignment required
	struct VertexShaderUniforms
	{
		glm::mat4 normal_matrix;
	};

	// std140 alignment required
	struct FragmentShaderUniforms
	{
		float colorClampMin[4];
		float colorClampMax[4];
		float sp_FOG_COL_RAM[4];	// Only using 3 elements but easier for std140
		float sp_FOG_COL_VERT[4];	// same comment
		float cp_AlphaTestValue;
		float sp_FOG_DENSITY;
		float shade_scale_factor;	// new for OIT
		u32 pixelBufferSize;
		u32 viewportWidth;
	};

	struct PushConstants
	{
		glm::vec4 clipTest;
		glm::ivec4 blend_mode0;	// Only using 2 elements but easier for std140
		float trilinearAlpha;
		int pp_Number;
		float palette_index;
		int _pad;

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

	void Init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout,
			vk::DescriptorSetLayout perPolyLayout, vk::DescriptorSetLayout colorInputLayout)
	{
		this->samplerManager = samplerManager;
		this->pipelineLayout = pipelineLayout;
		this->perFrameLayout = perFrameLayout;
		this->perPolyLayout = perPolyLayout;
		this->colorInputLayout = colorInputLayout;
	}
	// FIXME way too many params
	void UpdateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset, vk::ImageView fogImageView,
			u32 polyParamsOffset, u32 polyParamsSize, vk::ImageView stencilImageView, vk::ImageView depthImageView,
			vk::ImageView paletteImageView)
	{
		if (perFrameDescSets.empty())
		{
			perFrameDescSets = GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), 1, &perFrameLayout));
		}
		perFrameDescSetsInFlight.emplace_back(std::move(perFrameDescSets.back()));
		perFrameDescSets.pop_back();
		vk::DescriptorSet perFrameDescSet = *perFrameDescSetsInFlight.back();

		std::vector<vk::DescriptorBufferInfo> bufferInfos;
		bufferInfos.emplace_back(buffer, vertexUniformOffset, sizeof(VertexShaderUniforms));
		bufferInfos.emplace_back(buffer, fragmentUniformOffset, sizeof(FragmentShaderUniforms));

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.emplace_back(perFrameDescSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfos[0], nullptr);
		writeDescriptorSets.emplace_back(perFrameDescSet, 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfos[1], nullptr);
		if (fogImageView)
		{
			TSP fogTsp = {};
			fogTsp.FilterMode = 1;
			fogTsp.ClampU = 1;
			fogTsp.ClampV = 1;
			vk::Sampler fogSampler = samplerManager->GetSampler(fogTsp);
			static vk::DescriptorImageInfo imageInfo;
			imageInfo = { fogSampler, fogImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
			writeDescriptorSets.emplace_back(perFrameDescSet, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);
		}
		if (paletteImageView)
		{
			TSP palTsp = {};
			palTsp.FilterMode = 0;
			palTsp.ClampU = 1;
			palTsp.ClampV = 1;
			vk::Sampler palSampler = samplerManager->GetSampler(palTsp);
			static vk::DescriptorImageInfo imageInfo;
			imageInfo = { palSampler, paletteImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
			writeDescriptorSets.emplace_back(perFrameDescSet, 6, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);
		}
		if (polyParamsSize > 0)
		{
			static vk::DescriptorBufferInfo polyParamsBufferInfo;
			polyParamsBufferInfo = vk::DescriptorBufferInfo(buffer, polyParamsOffset, polyParamsSize);
			writeDescriptorSets.emplace_back(perFrameDescSet, 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &polyParamsBufferInfo, nullptr);
		}
		vk::DescriptorImageInfo stencilImageInfo(vk::Sampler(), stencilImageView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
		writeDescriptorSets.emplace_back(perFrameDescSet, 4, 0, 1, vk::DescriptorType::eInputAttachment, &stencilImageInfo, nullptr, nullptr);
		vk::DescriptorImageInfo depthImageInfo(vk::Sampler(), depthImageView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
		writeDescriptorSets.emplace_back(perFrameDescSet, 5, 0, 1, vk::DescriptorType::eInputAttachment, &depthImageInfo, nullptr, nullptr);

		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void UpdateColorInputDescSet(int index, vk::ImageView colorImageView)
	{
		if (!colorInputDescSets[index])
		{
			colorInputDescSets[index] = std::move(GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), 1, &colorInputLayout)).front());
		}
		vk::DescriptorImageInfo colorImageInfo(vk::Sampler(), colorImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::WriteDescriptorSet writeDescriptorSet(*colorInputDescSets[index], 0, 0, 1, vk::DescriptorType::eInputAttachment, &colorImageInfo, nullptr, nullptr);

		GetContext()->GetDevice().updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
	}

	void SetTexture(Texture *texture0, TSP tsp0, Texture *texture1, TSP tsp1)
	{
		auto index = std::make_tuple(texture0, tsp0.full & SamplerManager::TSP_Mask,
				texture1, tsp1.full & SamplerManager::TSP_Mask);
		if (perPolyDescSetsInFlight.find(index) != perPolyDescSetsInFlight.end())
			return;

		if (perPolyDescSets.empty())
		{
			std::vector<vk::DescriptorSetLayout> layouts(10, perPolyLayout);
			perPolyDescSets = GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), layouts.size(), &layouts[0]));
		}
		vk::DescriptorImageInfo imageInfo0(samplerManager->GetSampler(tsp0), texture0->GetReadOnlyImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.emplace_back(*perPolyDescSets.back(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo0, nullptr, nullptr);

		if (texture1 != nullptr)
		{
			vk::DescriptorImageInfo imageInfo1(samplerManager->GetSampler(tsp1), texture1->GetReadOnlyImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

			writeDescriptorSets.emplace_back(*perPolyDescSets.back(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo1, nullptr, nullptr);
		}
		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
		perPolyDescSetsInFlight[index] = std::move(perPolyDescSets.back());
		perPolyDescSets.pop_back();
	}

	void BindPerFrameDescriptorSets(vk::CommandBuffer cmdBuffer)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &perFrameDescSetsInFlight.back().get(), 0, nullptr);
	}

	void BindColorInputDescSet(vk::CommandBuffer cmdBuffer, int index)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 2, 1, &colorInputDescSets[index].get(), 0, nullptr);
	}

	void BindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, Texture *texture0, TSP tsp0, Texture *texture1, TSP tsp1)
	{
		auto index = std::make_tuple(texture0, tsp0.full & SamplerManager::TSP_Mask, texture1, tsp1.full & SamplerManager::TSP_Mask);
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, 1,
				&perPolyDescSetsInFlight[index].get(), 0, nullptr);
	}

	void Reset()
	{
		for (auto& pair : perPolyDescSetsInFlight)
			perPolyDescSets.emplace_back(std::move(pair.second));
		perPolyDescSetsInFlight.clear();
		for (auto& descset : perFrameDescSetsInFlight)
			perFrameDescSets.emplace_back(std::move(descset));
		perFrameDescSetsInFlight.clear();
	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	vk::DescriptorSetLayout perFrameLayout;
	vk::DescriptorSetLayout perPolyLayout;
	vk::DescriptorSetLayout colorInputLayout;
	vk::PipelineLayout pipelineLayout;

	std::vector<vk::UniqueDescriptorSet> perFrameDescSets;
	std::vector<vk::UniqueDescriptorSet> perFrameDescSetsInFlight;
	std::array<vk::UniqueDescriptorSet, 2> colorInputDescSets;
	std::vector<vk::UniqueDescriptorSet> perPolyDescSets;
	std::map<std::tuple<Texture *, u32, Texture *, u32>, vk::UniqueDescriptorSet> perPolyDescSetsInFlight;

	SamplerManager* samplerManager;
};

class OITPipelineManager
{
public:
	OITPipelineManager() : renderPasses(&ownRenderPasses) {}
	virtual ~OITPipelineManager() = default;

	virtual void Init(OITShaderManager *shaderManager, OITBuffers *oitBuffers)
	{
		this->shaderManager = shaderManager;

		if (!perFrameLayout)
		{
			// Descriptor set and pipeline layout
			vk::DescriptorSetLayoutBinding perFrameBindings[] = {
					{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },			// vertex uniforms
					{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// fragment uniforms
					{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// fog texture
					{ 3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// Tr poly params
					{ 4, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// stencil input attachment
					{ 5, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// depth input attachment
					{ 6, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// palette texture
			};
			perFrameLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perFrameBindings), perFrameBindings));

			vk::DescriptorSetLayoutBinding colorInputBindings[] = {
					{ 0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// color input attachment
			};
			colorInputLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(colorInputBindings), colorInputBindings));

			vk::DescriptorSetLayoutBinding perPolyBindings[] = {
					{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture 0
					{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture 1 (for 2-volume mode)
			};
			perPolyLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perPolyBindings), perPolyBindings));

			vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(OITDescriptorSets::PushConstants));
			vk::DescriptorSetLayout layouts[] = { *perFrameLayout, *perPolyLayout, *colorInputLayout, oitBuffers->GetDescriptorSetLayout() };
			pipelineLayout = GetContext()->GetDevice().createPipelineLayoutUnique(
					vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), ARRAY_SIZE(layouts), layouts, 1, &pushConstant));
		}

		pipelines.clear();
		modVolPipelines.clear();
	}

	vk::Pipeline GetPipeline(u32 listType, bool autosort, const PolyParam& pp, Pass pass, bool gpuPalette)
	{
		u32 pipehash = hash(listType, autosort, &pp, pass, gpuPalette);
		const auto &pipeline = pipelines.find(pipehash);
		if (pipeline != pipelines.end())
			return pipeline->second.get();

		CreatePipeline(listType, autosort, pp, pass, gpuPalette);

		return *pipelines[pipehash];
	}

	vk::Pipeline GetModifierVolumePipeline(ModVolMode mode, int cullMode)
	{
		u32 pipehash = hash(mode, cullMode);
		const auto &pipeline = modVolPipelines.find(pipehash);
		if (pipeline != modVolPipelines.end())
			return pipeline->second.get();
		CreateModVolPipeline(mode, cullMode);

		return *modVolPipelines[pipehash];
	}
	vk::Pipeline GetTrModifierVolumePipeline(ModVolMode mode, int cullMode)
	{
		u32 pipehash = hash(mode, cullMode);
		const auto &pipeline = trModVolPipelines.find(pipehash);
		if (pipeline != trModVolPipelines.end())
			return pipeline->second.get();
		CreateTrModVolPipeline(mode, cullMode);

		return *trModVolPipelines[pipehash];
	}
	vk::Pipeline GetFinalPipeline()
	{
		if (!finalPipeline)
			CreateFinalPipeline();
		return *finalPipeline;
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

	vk::RenderPass GetRenderPass(bool initial, bool last) { return renderPasses->GetRenderPass(initial, last); }

private:
	void CreateModVolPipeline(ModVolMode mode, int cullMode);
	void CreateTrModVolPipeline(ModVolMode mode, int cullMode);

	u32 hash(u32 listType, bool autosort, const PolyParam *pp, Pass pass, bool gpuPalette) const
	{
		u32 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
			| (((pp->tileclip >> 28) == 3) << 4);
		hash |= ((listType >> 1) << 5);
		if (pp->tcw1.full != (u32)-1 || pp->tsp1.full != (u32)-1)
		{
			// Two-volume mode
			hash |= (1 << 31) | (pp->tsp.ColorClamp << 11);
		}
		else
		{
			bool ignoreTexAlpha = pp->tsp.IgnoreTexA || pp->tcw.PixelFmt == Pixel565;
			hash |= (pp->tsp.ShadInstr << 7) | (ignoreTexAlpha << 9) | (pp->tsp.UseAlpha << 10)
				| (pp->tsp.ColorClamp << 11) | ((config::Fog ? pp->tsp.FogCtrl : 2) << 12)
				| (pp->tsp.SrcInstr << 14) | (pp->tsp.DstInstr << 17);
		}
		hash |= (pp->isp.ZWriteDis << 20) | (pp->isp.CullMode << 21) | ((autosort ? 6 : pp->isp.DepthMode) << 23);
		hash |= ((u32)gpuPalette << 26) | ((u32)pass << 27);

		return hash;
	}
	u32 hash(ModVolMode mode, int cullMode) const
	{
		return ((int)mode << 2) | cullMode;
	}

	vk::PipelineVertexInputStateCreateInfo GetMainVertexInputStateCreateInfo(bool full = true) const
	{
		// Vertex input state
		static const vk::VertexInputBindingDescription vertexBindingDescriptions[] =
		{
				{ 0, sizeof(Vertex) },
		};
		static const vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] =
		{
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, x)),	// pos
				vk::VertexInputAttributeDescription(1, 0, vk::Format::eR8G8B8A8Uint, offsetof(Vertex, col)),	// base color
				vk::VertexInputAttributeDescription(2, 0, vk::Format::eR8G8B8A8Uint, offsetof(Vertex, spc)),	// offset color
				vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, u)),		// tex coord
				vk::VertexInputAttributeDescription(4, 0, vk::Format::eR8G8B8A8Uint, offsetof(Vertex, col1)),	// base1 color
				vk::VertexInputAttributeDescription(5, 0, vk::Format::eR8G8B8A8Uint, offsetof(Vertex, spc1)),	// offset1 color
				vk::VertexInputAttributeDescription(6, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, u1)),		// tex1 coord
		};
		static const vk::VertexInputAttributeDescription vertexInputLightAttributeDescriptions[] =
		{
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, x)),	// pos
		};
		return vk::PipelineVertexInputStateCreateInfo(
				vk::PipelineVertexInputStateCreateFlags(),
				ARRAY_SIZE(vertexBindingDescriptions),
				vertexBindingDescriptions,
				full ? ARRAY_SIZE(vertexInputAttributeDescriptions) : ARRAY_SIZE(vertexInputLightAttributeDescriptions),
				full ? vertexInputAttributeDescriptions : vertexInputLightAttributeDescriptions);
	}

	void CreatePipeline(u32 listType, bool autosort, const PolyParam& pp, Pass pass, bool gpuPalette);
	void CreateFinalPipeline();
	void CreateClearPipeline();

	std::map<u32, vk::UniquePipeline> pipelines;
	std::map<u32, vk::UniquePipeline> modVolPipelines;
	std::map<u32, vk::UniquePipeline> trModVolPipelines;
	vk::UniquePipeline finalPipeline;
	vk::UniquePipeline clearPipeline;

	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout perFrameLayout;
	vk::UniqueDescriptorSetLayout colorInputLayout;
	vk::UniqueDescriptorSetLayout perPolyLayout;
	RenderPasses ownRenderPasses;

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	RenderPasses *renderPasses;
	OITShaderManager *shaderManager = nullptr;
};

class RttOITPipelineManager : public OITPipelineManager
{
public:
	RttOITPipelineManager() { renderPasses = &rttRenderPasses; }
	void Init(OITShaderManager *shaderManager, OITBuffers *oitBuffers) override
	{
		this->oitBuffers = oitBuffers;
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
	OITBuffers *oitBuffers = nullptr;
};
