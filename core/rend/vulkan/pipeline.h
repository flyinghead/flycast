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
#pragma once
#include "vulkan.h"
#include "shaders.h"
#include "texture.h"
#include "utils.h"
#include "vulkan_context.h"
#include <array>

class DescriptorSets
{
public:
	DescriptorSets() = default;
	DescriptorSets(DescriptorSets &&) = default;
	DescriptorSets(const DescriptorSets &) = delete;
	DescriptorSets& operator=(DescriptorSets &&) = default;
	DescriptorSets& operator=(const DescriptorSets &) = delete;

	void Init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout, vk::DescriptorSetLayout perPolyLayout)
	{
		this->samplerManager = samplerManager;
		this->pipelineLayout = pipelineLayout;
		this->perFrameLayout = perFrameLayout;
		this->perPolyLayout = perPolyLayout;

	}
	void UpdateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset, vk::ImageView fogImageView, vk::ImageView paletteImageView)
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
			writeDescriptorSets.emplace_back(perFrameDescSet, 3, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);
		}
		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void SetTexture(Texture *texture, TSP tsp)
	{
		auto& inFlight = perPolyDescSetsInFlight;
		std::pair<Texture *, u32> index = std::make_pair(texture, tsp.full & SamplerManager::TSP_Mask);
		if (inFlight.find(index) != inFlight.end())
			return;

		if (perPolyDescSets.empty())
		{
			std::vector<vk::DescriptorSetLayout> layouts(10, perPolyLayout);
			perPolyDescSets = GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), layouts.size(), &layouts[0]));
		}
		vk::DescriptorImageInfo imageInfo(samplerManager->GetSampler(tsp), texture->GetReadOnlyImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.emplace_back(*perPolyDescSets.back(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);

		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
		inFlight[index] = std::move(perPolyDescSets.back());
		perPolyDescSets.pop_back();
	}

	void BindPerFrameDescriptorSets(vk::CommandBuffer cmdBuffer)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &perFrameDescSetsInFlight.back().get(), 0, nullptr);
	}

	void BindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, Texture *texture, TSP tsp)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, 1,
				&perPolyDescSetsInFlight[std::make_pair(texture, tsp.full & SamplerManager::TSP_Mask)].get(), 0, nullptr);
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
	vk::PipelineLayout pipelineLayout;

	std::vector<vk::UniqueDescriptorSet> perFrameDescSets;
	std::vector<vk::UniqueDescriptorSet> perFrameDescSetsInFlight;
	std::vector<vk::UniqueDescriptorSet> perPolyDescSets;
	std::map<std::pair<Texture *, u32>, vk::UniqueDescriptorSet> perPolyDescSetsInFlight;

	SamplerManager* samplerManager = nullptr;
};

class PipelineManager
{
public:
	virtual ~PipelineManager() = default;

	void Init(ShaderManager *shaderManager, vk::RenderPass renderPass)
	{
		this->shaderManager = shaderManager;

		if (!perFrameLayout)
		{
			// Descriptor set and pipeline layout
			vk::DescriptorSetLayoutBinding perFrameBindings[] = {
					{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },			// vertex uniforms
					{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// fragment uniforms
					{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// fog texture
					{ 3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// palette texture
			};
			vk::DescriptorSetLayoutBinding perPolyBindings[] = {
					{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture
			};
			perFrameLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perFrameBindings), perFrameBindings));
			perPolyLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perPolyBindings), perPolyBindings));
			vk::DescriptorSetLayout layouts[] = { *perFrameLayout, *perPolyLayout };
			vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, 24);
			pipelineLayout = GetContext()->GetDevice().createPipelineLayoutUnique(
					vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), ARRAY_SIZE(layouts), layouts, 1, &pushConstant));
		}

		if (this->renderPass != renderPass)
		{
			this->renderPass = renderPass;
			Reset();
		}
	}

	vk::Pipeline GetPipeline(u32 listType, bool sortTriangles, const PolyParam& pp, bool gpuPalette)
	{
		u32 pipehash = hash(listType, sortTriangles, &pp, gpuPalette);
		const auto &pipeline = pipelines.find(pipehash);
		if (pipeline != pipelines.end())
			return pipeline->second.get();

		CreatePipeline(listType, sortTriangles, pp, gpuPalette);

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

	vk::Pipeline GetDepthPassPipeline(int cullMode)
	{
		cullMode = std::max(std::min(cullMode, (int)depthPassPipelines.size() - 1), 0);
		const auto &pipeline = depthPassPipelines[cullMode];
		if (!pipeline)
			CreateDepthPassPipeline(cullMode);

		return *pipeline;
	}

	void Reset()
	{
		pipelines.clear();
		modVolPipelines.clear();
	}

	vk::PipelineLayout GetPipelineLayout() const { return *pipelineLayout; }
	vk::DescriptorSetLayout GetPerFrameDSLayout() const { return *perFrameLayout; }
	vk::DescriptorSetLayout GetPerPolyDSLayout() const { return *perPolyLayout; }
	vk::RenderPass GetRenderPass() const { return renderPass; }

private:
	void CreateModVolPipeline(ModVolMode mode, int cullMode);
	void CreateDepthPassPipeline(int cullMode);

	u32 hash(u32 listType, bool sortTriangles, const PolyParam *pp, bool gpuPalette) const
	{
		u32 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
			| (((pp->tileclip >> 28) == 3) << 4);
		hash |= ((listType >> 1) << 5);
		bool ignoreTexAlpha = pp->tsp.IgnoreTexA || pp->tcw.PixelFmt == Pixel565;
		hash |= (pp->tsp.ShadInstr << 7) | (ignoreTexAlpha << 9) | (pp->tsp.UseAlpha << 10)
			| (pp->tsp.ColorClamp << 11) | ((config::Fog ? pp->tsp.FogCtrl : 2) << 12) | (pp->tsp.SrcInstr << 14)
			| (pp->tsp.DstInstr << 17);
		hash |= (pp->isp.ZWriteDis << 20) | (pp->isp.CullMode << 21) | (pp->isp.DepthMode << 23);
		hash |= ((u32)sortTriangles << 26) | ((u32)gpuPalette << 27);

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

	void CreatePipeline(u32 listType, bool sortTriangles, const PolyParam& pp, bool gpuPalette);

	std::map<u32, vk::UniquePipeline> pipelines;
	std::map<u32, vk::UniquePipeline> modVolPipelines;
	std::array<vk::UniquePipeline, 4> depthPassPipelines;

	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout perFrameLayout;
	vk::UniqueDescriptorSetLayout perPolyLayout;

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	vk::RenderPass renderPass;
	ShaderManager *shaderManager = nullptr;
};

class RttPipelineManager : public PipelineManager
{
public:
	void Init(ShaderManager *shaderManager)
	{
		// RTT render pass
		renderToTextureBuffer = config::RenderToTextureBuffer;
	    vk::AttachmentDescription attachmentDescriptions[] = {
	    		vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
	    				vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
						vk::ImageLayout::eColorAttachmentOptimal,
						renderToTextureBuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal),
				vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
						vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
						vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal),
	    };
	    vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	    vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
	    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, 0, nullptr, 1, &colorReference, nullptr, &depthReference);
	    vk::SubpassDependency dependencies[] {
	    	vk::SubpassDependency(VK_SUBPASS_EXTERNAL, 0, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
	    			vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite),
			vk::SubpassDependency(0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead),
	    };
	    vk::SubpassDependency vramWriteDeps[] {
			vk::SubpassDependency(0, VK_SUBPASS_EXTERNAL,
					vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eHost,
					vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eHostRead),
	    };

	    rttRenderPass = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), 2, attachmentDescriptions,
	    		1, &subpass, renderToTextureBuffer ? ARRAY_SIZE(vramWriteDeps) : ARRAY_SIZE(dependencies), renderToTextureBuffer ? vramWriteDeps : dependencies));

		PipelineManager::Init(shaderManager, *rttRenderPass);
	}

	void CheckSettingsChange()
	{
		if (renderToTextureBuffer != config::RenderToTextureBuffer)
			Init(shaderManager);
	}

private:
	vk::UniqueRenderPass rttRenderPass;
	bool renderToTextureBuffer = false;
};

class OSDPipeline
{
public:
	void Init(ShaderManager *shaderManager, vk::ImageView imageView, vk::RenderPass renderPass)
	{
		this->shaderManager = shaderManager;
		if (!pipelineLayout)
		{
			vk::DescriptorSetLayoutBinding bindings[] = {
					{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture
			};
			descSetLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(bindings), bindings));
			pipelineLayout = GetContext()->GetDevice().createPipelineLayoutUnique(
					vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), 1, &descSetLayout.get()));
		}
		if (!sampler)
		{
			sampler = GetContext()->GetDevice().createSamplerUnique(
					vk::SamplerCreateInfo(vk::SamplerCreateFlags(), vk::Filter::eLinear, vk::Filter::eLinear,
										vk::SamplerMipmapMode::eLinear, vk::SamplerAddressMode::eClampToEdge, vk::SamplerAddressMode::eClampToEdge,
										vk::SamplerAddressMode::eClampToEdge, 0.0f, false, 16.0f, false,
										vk::CompareOp::eNever, 0.0f, 0.0f, vk::BorderColor::eFloatOpaqueBlack));
		}
		if (this->renderPass != renderPass)
		{
			this->renderPass = renderPass;
			pipeline.reset();
		}
		if (!descriptorSet)
		{
			descriptorSet = std::move(GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), 1, &descSetLayout.get())).front());
		}
		vk::DescriptorImageInfo imageInfo(*sampler, imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.emplace_back(*descriptorSet, 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr);
		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	vk::Pipeline GetPipeline()
	{
		if (!pipeline)
			CreatePipeline();
		return *pipeline;
	}

	void BindDescriptorSets(vk::CommandBuffer cmdBuffer) const
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, 1, &descriptorSet.get(), 0, nullptr);
	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	void CreatePipeline();

	vk::RenderPass renderPass;
	vk::UniquePipeline pipeline;
	vk::UniqueSampler sampler;
	vk::UniqueDescriptorSet descriptorSet;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout descSetLayout;
	ShaderManager *shaderManager = nullptr;
};
