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

#include <glm/glm.hpp>
#include <unordered_map>

class OITDescriptorSets
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
			vk::DescriptorSetLayout perPolyLayout, vk::DescriptorSetLayout colorInputLayout)
	{
		this->samplerManager = samplerManager;
		this->pipelineLayout = pipelineLayout;

		perFrameAlloc.setLayout(perFrameLayout);
		perPolyAlloc.setLayout(perPolyLayout);
		colorInputAlloc.setLayout(colorInputLayout);
	}

	// FIXME way too many params
	void updateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset, vk::ImageView fogImageView,
			u32 polyParamsOffset, u32 polyParamsSize, vk::ImageView stencilImageView, vk::ImageView depthImageView,
			vk::ImageView paletteImageView, OITBuffers *oitBuffers)
	{
		if (!perFrameDescSet)
			perFrameDescSet = perFrameAlloc.alloc();

		std::vector<vk::DescriptorBufferInfo> bufferInfos;
		bufferInfos.emplace_back(buffer, vertexUniformOffset, sizeof(VertexShaderUniforms));
		bufferInfos.emplace_back(buffer, fragmentUniformOffset, sizeof(FragmentShaderUniforms));

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.emplace_back(perFrameDescSet, 0, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfos[0]);
		writeDescriptorSets.emplace_back(perFrameDescSet, 1, 0, vk::DescriptorType::eUniformBuffer, nullptr, bufferInfos[1]);
		if (fogImageView)
		{
			TSP fogTsp = {};
			fogTsp.FilterMode = 1;
			fogTsp.ClampU = 1;
			fogTsp.ClampV = 1;
			vk::Sampler fogSampler = samplerManager->GetSampler(fogTsp);
			static vk::DescriptorImageInfo imageInfo;
			imageInfo = { fogSampler, fogImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
			writeDescriptorSets.emplace_back(perFrameDescSet, 2, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
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
			writeDescriptorSets.emplace_back(perFrameDescSet, 6, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
		}
		if (polyParamsSize > 0)
		{
			static vk::DescriptorBufferInfo polyParamsBufferInfo;
			polyParamsBufferInfo = vk::DescriptorBufferInfo(buffer, polyParamsOffset, polyParamsSize);
			writeDescriptorSets.emplace_back(perFrameDescSet, 3, 0, vk::DescriptorType::eStorageBuffer, nullptr, polyParamsBufferInfo);
		}
		vk::DescriptorImageInfo stencilImageInfo(vk::Sampler(), stencilImageView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
		writeDescriptorSets.emplace_back(perFrameDescSet, 4, 0, vk::DescriptorType::eInputAttachment, stencilImageInfo);
		vk::DescriptorImageInfo depthImageInfo(vk::Sampler(), depthImageView, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
		writeDescriptorSets.emplace_back(perFrameDescSet, 5, 0, vk::DescriptorType::eInputAttachment, depthImageInfo);
		oitBuffers->updateDescriptorSet(perFrameDescSet, writeDescriptorSets);

		getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void updateColorInputDescSet(int index, vk::ImageView colorImageView)
	{
		if (!colorInputDescSets[index])
			colorInputDescSets[index] = colorInputAlloc.alloc();

		vk::DescriptorImageInfo colorImageInfo(vk::Sampler(), colorImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::WriteDescriptorSet writeDescriptorSet(colorInputDescSets[index], 0, 0, vk::DescriptorType::eInputAttachment, colorImageInfo);

		getContext()->GetDevice().updateDescriptorSets(writeDescriptorSet, nullptr);
	}

	void bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const PolyParam& poly, int polyNumber, vk::Buffer buffer,
			vk::DeviceSize uniformOffset, vk::DeviceSize lightOffset)
	{
		vk::DescriptorSet perPolyDescSet;
		auto it = perPolyDescSets.find(&poly);
		if (it == perPolyDescSets.end())
		{
			perPolyDescSet = perPolyAlloc.alloc();
			std::vector<vk::WriteDescriptorSet> writeDescriptorSets;

			vk::DescriptorImageInfo imageInfo0;
			if (poly.texture != nullptr)
			{
				imageInfo0 = vk::DescriptorImageInfo{ samplerManager->GetSampler(poly.tsp), ((Texture *)poly.texture)->GetReadOnlyImageView(),
						vk::ImageLayout::eShaderReadOnlyOptimal };
				writeDescriptorSets.emplace_back(perPolyDescSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo0);
			}
			vk::DescriptorImageInfo imageInfo1;
			if (poly.texture1 != nullptr)
			{
				imageInfo1 = vk::DescriptorImageInfo{ samplerManager->GetSampler(poly.tsp1), ((Texture *)poly.texture1)->GetReadOnlyImageView(),
					vk::ImageLayout::eShaderReadOnlyOptimal };
				writeDescriptorSets.emplace_back(perPolyDescSet, 1, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo1);
			}

			vk::DescriptorBufferInfo uniBufferInfo;
			vk::DescriptorBufferInfo lightBufferInfo;
			if (poly.isNaomi2())
			{
				const vk::DeviceSize uniformAlignment = VulkanContext::Instance()->GetUniformBufferAlignment();
				size_t size = sizeof(N2VertexShaderUniforms) + align(sizeof(N2VertexShaderUniforms), uniformAlignment);
				uniBufferInfo = vk::DescriptorBufferInfo{ buffer, uniformOffset + polyNumber * size, sizeof(N2VertexShaderUniforms) };
				writeDescriptorSets.emplace_back(perPolyDescSet, 2, 0, vk::DescriptorType::eUniformBuffer, nullptr, uniBufferInfo);

				size = sizeof(N2LightModel) + align(sizeof(N2LightModel), uniformAlignment);
				// light at index 0 is no light
				if (poly.lightModel != nullptr)
					lightBufferInfo = vk::DescriptorBufferInfo{ buffer, lightOffset + (poly.lightModel - pvrrc.lightModels.head() + 1) * size, sizeof(N2LightModel) };
				else
					lightBufferInfo = vk::DescriptorBufferInfo{ buffer, lightOffset, sizeof(N2LightModel) };
				writeDescriptorSets.emplace_back(perPolyDescSet, 3, 0, vk::DescriptorType::eUniformBuffer, nullptr, lightBufferInfo);
			}

			getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
			perPolyDescSets[&poly] = perPolyDescSet;
		}
		else
			perPolyDescSet = it->second;
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, perPolyDescSet, nullptr);
	}

	void bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const ModifierVolumeParam& mvParam, int polyNumber, vk::Buffer buffer, vk::DeviceSize uniformOffset)
	{
		if (!mvParam.isNaomi2())
			return;
		vk::DescriptorSet perPolyDescSet;
		auto it = perPolyDescSets.find(&mvParam);
		if (it == perPolyDescSets.end())
		{
			perPolyDescSet = perPolyAlloc.alloc();

			const vk::DeviceSize uniformAlignment = VulkanContext::Instance()->GetUniformBufferAlignment();
			size_t size = sizeof(N2VertexShaderUniforms) + align(sizeof(N2VertexShaderUniforms), uniformAlignment);
			vk::DescriptorBufferInfo uniBufferInfo{ buffer, uniformOffset + polyNumber * size, sizeof(N2VertexShaderUniforms) };
			vk::WriteDescriptorSet writeDescriptorSet(perPolyDescSet, 2, 0, vk::DescriptorType::eUniformBuffer, nullptr, uniBufferInfo);

			getContext()->GetDevice().updateDescriptorSets(writeDescriptorSet, nullptr);
			perPolyDescSets[&mvParam] = perPolyDescSet;
		}
		else
			perPolyDescSet = it->second;
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, perPolyDescSet, nullptr);
	}

	void bindPerFrameDescriptorSets(vk::CommandBuffer cmdBuffer)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, perFrameDescSet, nullptr);
	}

	void bindColorInputDescSet(vk::CommandBuffer cmdBuffer, int index)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 2, colorInputDescSets[index], nullptr);
	}

	void nextFrame()
	{
		perFrameDescSet = vk::DescriptorSet{};
		colorInputDescSets[0] = vk::DescriptorSet{};
		colorInputDescSets[1] = vk::DescriptorSet{};
		perPolyDescSets.clear();
		perFrameAlloc.nextFrame();
		perPolyAlloc.nextFrame();
		colorInputAlloc.nextFrame();
	}

	void term()
	{
		perFrameAlloc.term();
		perPolyAlloc.term();
		colorInputAlloc.term();
	}

private:
	VulkanContext *getContext() const { return VulkanContext::Instance(); }

	vk::PipelineLayout pipelineLayout;

	std::array<vk::DescriptorSet, 2> colorInputDescSets;
	DynamicDescSetAlloc perFrameAlloc;
	DynamicDescSetAlloc perPolyAlloc;
	DynamicDescSetAlloc colorInputAlloc;
	vk::DescriptorSet perFrameDescSet = {};
	std::unordered_map<const void *, vk::DescriptorSet> perPolyDescSets;

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
			vk::Device device = GetContext()->GetDevice();
			// Descriptor set and pipeline layout
			std::array<vk::DescriptorSetLayoutBinding, 10> perFrameBindings = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),			// vertex uniforms
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// fragment uniforms
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// fog texture
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// Tr poly params
					vk::DescriptorSetLayoutBinding(4, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),		// stencil input attachment
					vk::DescriptorSetLayoutBinding(5, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment),		// depth input attachment
					vk::DescriptorSetLayoutBinding(6, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// palette texture
					// OIT buffers
					vk::DescriptorSetLayoutBinding(7, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// pixel buffer
					vk::DescriptorSetLayoutBinding(8, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// pixel counter
					vk::DescriptorSetLayoutBinding(9, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// a-buffer pointers
			};
			perFrameLayout = device.createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), perFrameBindings));

			vk::DescriptorSetLayoutBinding colorInputBinding(0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment);		// color input attachment
			colorInputLayout = device.createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), colorInputBinding));

			std::array<vk::DescriptorSetLayoutBinding, 4> perPolyBindings = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),	// texture 0
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),	// texture 1 (for 2-volume mode)
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),				// Naomi2 uniforms
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),				// Naomi2 lights
			};
			perPolyLayout = device.createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), perPolyBindings));

			std::array<vk::PushConstantRange, 2> pushConstants = {
					vk::PushConstantRange(vk::ShaderStageFlagBits::eFragment, 0, sizeof(OITDescriptorSets::PushConstants)),
					vk::PushConstantRange(vk::ShaderStageFlagBits::eVertex, sizeof(OITDescriptorSets::PushConstants), sizeof(OITDescriptorSets::VtxPushConstants)),
			};

			std::array<vk::DescriptorSetLayout, 3> layouts = { *perFrameLayout, *perPolyLayout, *colorInputLayout };
			pipelineLayout = device.createPipelineLayoutUnique(
					vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), layouts, pushConstants));
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

	vk::Pipeline GetModifierVolumePipeline(ModVolMode mode, int cullMode, bool naomi2)
	{
		u32 pipehash = hash(mode, cullMode, naomi2);
		const auto &pipeline = modVolPipelines.find(pipehash);
		if (pipeline != modVolPipelines.end())
			return pipeline->second.get();
		CreateModVolPipeline(mode, cullMode, naomi2);

		return *modVolPipelines[pipehash];
	}
	vk::Pipeline GetTrModifierVolumePipeline(ModVolMode mode, int cullMode, bool naomi2)
	{
		u32 pipehash = hash(mode, cullMode, naomi2);
		const auto &pipeline = trModVolPipelines.find(pipehash);
		if (pipeline != trModVolPipelines.end())
			return pipeline->second.get();
		CreateTrModVolPipeline(mode, cullMode, naomi2);

		return *trModVolPipelines[pipehash];
	}
	vk::Pipeline GetFinalPipeline()
	{
		if (!finalPipeline || maxLayers != config::PerPixelLayers)
		{
			CreateFinalPipeline();
			maxLayers = config::PerPixelLayers;
		}
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
	void CreateModVolPipeline(ModVolMode mode, int cullMode, bool naomi2);
	void CreateTrModVolPipeline(ModVolMode mode, int cullMode, bool naomi2);

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
		hash |= ((u32)gpuPalette << 26) | ((u32)pass << 27) | ((u32)pp->isNaomi2() << 29);
		hash |= (u32)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 30;

		return hash;
	}
	u32 hash(ModVolMode mode, int cullMode, bool naomi2) const
	{
		return ((int)mode << 2) | cullMode | ((u32)naomi2 << 5) | ((u32)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 6);
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
				vk::VertexInputAttributeDescription(7, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, nx)),	// naomi2 normal
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
	int maxLayers = 0;

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
