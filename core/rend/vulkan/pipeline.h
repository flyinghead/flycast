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
#include "hw/pvr/ta_ctx.h"

class DescriptorSets
{
public:
	vk::PipelineLayout GetPipelineLayout() const { return *pipelineLayout; }

	void Init()
	{
		// Descriptor set and pipeline layout
		vk::DescriptorSetLayoutBinding perFrameBindings[] = {
				{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },			// vertex uniforms
				{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// fragment uniforms
				{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// fog texture
		};
		vk::DescriptorSetLayoutBinding perPolyBindings[] = {
				{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture
		};
		perFrameLayout = GetContext()->GetDevice()->createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perFrameBindings), perFrameBindings));
		perPolyLayout = GetContext()->GetDevice()->createDescriptorSetLayoutUnique(
				vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perPolyBindings), perPolyBindings));
		vk::DescriptorSetLayout layouts[] = { *perFrameLayout, *perPolyLayout };
		vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, 20);
		pipelineLayout = GetContext()->GetDevice()->createPipelineLayoutUnique(
				vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), ARRAY_SIZE(layouts), layouts, 1, &pushConstant));
	}

	void UpdateUniforms(const vk::Buffer& vertexUniformBuffer, const vk::Buffer& fragmentUniformBuffer)
	{
		while (perFrameDescSets.empty())
		{
			std::vector<vk::DescriptorSetLayout> layouts(GetContext()->GetSwapChainSize(), *perFrameLayout);
			perFrameDescSets = GetContext()->GetDevice()->allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), layouts.size(), &layouts[0]));
		}
		while (perPolyDescSets.size() < GetContext()->GetSwapChainSize())
			perPolyDescSets.push_back(std::vector<vk::UniqueDescriptorSet>());
		while (perPolyDescSetsInFlight.size() < GetContext()->GetSwapChainSize())
			perPolyDescSetsInFlight.push_back(std::map<std::pair<u64, u32>, vk::UniqueDescriptorSet>());
		int currentImage = GetContext()->GetCurrentImageIndex();
		std::vector<vk::DescriptorBufferInfo> bufferInfos;
		bufferInfos.push_back(vk::DescriptorBufferInfo(vertexUniformBuffer, 0, VK_WHOLE_SIZE));
		bufferInfos.push_back(vk::DescriptorBufferInfo(fragmentUniformBuffer, 0, VK_WHOLE_SIZE));
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSets[currentImage], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfos[0], nullptr));
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSets[currentImage], 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfos[1], nullptr));
		GetContext()->GetDevice()->updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void SetTexture(u64 textureId, TSP tsp)
	{
		int currentImage = GetContext()->GetCurrentImageIndex();
		auto& inFlight = perPolyDescSetsInFlight[currentImage];
		std::pair<u64, u32> index = std::make_pair(textureId, tsp.full & SamplerManager::TSP_Mask);
		if (inFlight.find(index) != inFlight.end())
			return;

		auto& descSets = perPolyDescSets[currentImage];
		if (descSets.empty())
		{
			std::vector<vk::DescriptorSetLayout> layouts(10, *perPolyLayout);
			descSets = GetContext()->GetDevice()->allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), layouts.size(), &layouts[0]));
		}
		Texture *texture = reinterpret_cast<Texture *>(textureId);
		vk::DescriptorImageInfo imageInfo(samplerManager.GetSampler(tsp), texture->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*descSets.back(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr));

		GetContext()->GetDevice()->updateDescriptorSets(writeDescriptorSets, nullptr);
		inFlight[index] = std::move(descSets.back());
		descSets.pop_back();
	}
/* TODO Should be part of per-frame descriptor set
	void SetFogTexture(Texture & texture)
	{
		vk::DescriptorImageInfo imageInfo = texture.GetDescriptorImageInfo();

		int currentImage = GetContext()->GetCurrentImageIndex();
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perPolyDescSets[currentImage], 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr));

		GetContext()->GetDevice()->updateDescriptorSets(writeDescriptorSets, nullptr);
	}
*/
	void BindPerFrameDescriptorSets(vk::CommandBuffer cmdBuffer)
	{
		int currentImage = GetContext()->GetCurrentImageIndex();
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, 1, &perFrameDescSets[currentImage].get(), 0, nullptr);
	}

	void BindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, u64 textureId, TSP tsp)
	{
		int currentImage = GetContext()->GetCurrentImageIndex();
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 1, 1,
				&perPolyDescSetsInFlight[currentImage][std::make_pair(textureId, tsp.full & SamplerManager::TSP_Mask)].get(), 0, nullptr);
	}

	void Reset()
	{
		int currentImage = GetContext()->GetCurrentImageIndex();
		for (auto& pair : perPolyDescSetsInFlight[currentImage])
			perPolyDescSets[currentImage].emplace_back(std::move(pair.second));
		perPolyDescSetsInFlight[currentImage].clear();

	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	vk::UniqueDescriptorSetLayout perFrameLayout;
	vk::UniqueDescriptorSetLayout perPolyLayout;
	vk::UniquePipelineLayout pipelineLayout;

	std::vector<vk::UniqueDescriptorSet> perFrameDescSets;
	std::vector<std::vector<vk::UniqueDescriptorSet>> perPolyDescSets;
	std::vector<std::map<std::pair<u64, u32>, vk::UniqueDescriptorSet>> perPolyDescSetsInFlight;

	SamplerManager samplerManager;
};

class PipelineManager
{
public:
	void Init()
	{
		shaderManager.Init();
		descriptorSets.Init();
	}
	DescriptorSets& GetDescriptorSets() { return descriptorSets; }

	vk::Pipeline GetPipeline(u32 listType, bool sortTriangles, const PolyParam& pp)
	{
		u32 pipehash = hash(listType, sortTriangles, &pp);
		const auto &pipeline = pipelines.find(pipehash);
		if (pipeline != pipelines.end())
			return pipeline->second.get();

		CreatePipeline(listType, sortTriangles, pp);

		return *pipelines[pipehash];
	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	u32 hash(u32 listType, bool sortTriangles, const PolyParam *pp)
	{
		u32 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
			| ((pp->tileclip >> 28) << 4);
		hash |= ((listType >> 1) << 6);
		hash |= (pp->tsp.ShadInstr << 8) | (pp->tsp.IgnoreTexA << 10) | (pp->tsp.UseAlpha << 11)
			| (pp->tsp.ColorClamp << 12) | (pp->tsp.FogCtrl << 13) | (pp->tsp.SrcInstr << 15)
			| (pp->tsp.DstInstr << 18);
		hash |= (pp->isp.ZWriteDis << 21) | (pp->isp.CullMode << 22) | (pp->isp.DepthMode << 24);
		hash |= (u32)sortTriangles << 27;
// TODO		hash |= (u32)rotate90 << 28;

		return hash;
	}
	void CreatePipeline(u32 listType, bool sortTriangles, const PolyParam& pp);

	std::map<u32, vk::UniquePipeline> pipelines;
	ShaderManager shaderManager;
	DescriptorSets descriptorSets;
};


