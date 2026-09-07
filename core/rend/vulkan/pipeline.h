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
#include "desc_set.h"
#include <array>
#include <unordered_map>

class BaseDescriptorSets
{
protected:
	void init(SamplerManager* samplerManager) {
		this->samplerManager = samplerManager;
	}
	void term();

	VulkanContext *getContext() const { return VulkanContext::Instance(); }
	const vk::DescriptorImageInfo& getEmptyImageInfo();
	const vk::DescriptorBufferInfo& getEmptyBufferInfo();

	SamplerManager* samplerManager = nullptr;
	vk::UniqueImageView dummyImageView;
	vk::UniqueImage dummyImage;
	Allocation dummyImageAllocation;
	vk::UniqueBuffer dummyBuffer;
	Allocation dummyBufferAllocation;
	vk::DescriptorImageInfo emptyImageInfo;
	vk::DescriptorBufferInfo emptyBufferInfo;
};

class DescriptorSets : BaseDescriptorSets
{
public:
	void init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout, vk::DescriptorSetLayout perPolyLayout);
	void term();

	void nextFrame();
	void updateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset,
			vk::ImageView fogImageView, vk::ImageView paletteImageView, vk::ImageView secAccum = {});
	void bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const PolyParam& poly, int polyNumber, vk::Buffer buffer,
			vk::DeviceSize uniformOffset, vk::DeviceSize lightOffset, bool punchThrough);
	void bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const ModifierVolumeParam& mvParam, int polyNumber, vk::Buffer buffer,
			vk::DeviceSize uniformOffset);

	void bindPerFrameDescriptorSets(vk::CommandBuffer cmdBuffer) {
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, perFrameDescSet, nullptr);
	}

private:
	vk::PipelineLayout pipelineLayout;
	DynamicDescSetAlloc perFrameAlloc;
	DynamicDescSetAlloc perPolyAlloc;
	vk::DescriptorSet perFrameDescSet = {};
	std::unordered_map<const void *, vk::DescriptorSet> perPolyDescSets;
};

class PipelineManager
{
public:
	virtual ~PipelineManager() = default;

	void Init(ShaderManager *shaderManager)
	{
		this->shaderManager = shaderManager;

		if (!perFrameLayout)
		{
			// Descriptor set and pipeline layout
			std::vector<vk::DescriptorSetLayoutBinding> perFrameBindings = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),			// vertex uniforms
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// fragment uniforms
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// fog texture
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// palette texture
			};
			if (GetContext()->supportsDynamicLocalRead())
				// secondary accumulator
				perFrameBindings.emplace_back(4, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment);
			std::array<vk::DescriptorSetLayoutBinding, 3> perPolyBindings = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// texture
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),			// Naomi2 uniforms
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),			// Naomi2 lights
			};
			perFrameLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), perFrameBindings));
			perPolyLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), perPolyBindings));
			std::array<vk::DescriptorSetLayout, 2> layouts = { *perFrameLayout, *perPolyLayout };
			vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, 24);
			pipelineLayout = GetContext()->GetDevice().createPipelineLayoutUnique(
					vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), layouts, pushConstant));
		}
	}

	void setRenderPass(vk::RenderPass renderPass)
	{
		if (this->renderPass != renderPass) {
			this->renderPass = renderPass;
			Reset();
		}
	}

	vk::Pipeline GetPipeline(u32 listType, bool sortTriangles, const PolyParam& pp, int gpuPalette, bool dithering)
	{
		u64 pipehash = hash(listType, sortTriangles, &pp, gpuPalette, dithering);
		const auto &pipeline = pipelines.find(pipehash);
		if (pipeline != pipelines.end())
			return pipeline->second.get();

		CreatePipeline(listType, sortTriangles, pp, gpuPalette, dithering);

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

	vk::Pipeline GetDepthPassPipeline(int cullMode, bool naomi2)
	{
		u32 pipehash = hash(cullMode, naomi2);
		const auto &pipeline = depthPassPipelines.find(pipehash);
		if (pipeline != depthPassPipelines.end())
			return pipeline->second.get();
		CreateDepthPassPipeline(cullMode, naomi2);

		return *depthPassPipelines[pipehash];
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
	void CreateModVolPipeline(ModVolMode mode, int cullMode, bool naomi2);
	void CreateDepthPassPipeline(int cullMode, bool naomi2);

	u64 hash(u32 listType, bool sortTriangles, const PolyParam *pp, int gpuPalette, bool dithering) const
	{
		u64 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
			| (((pp->tileclip >> 28) == 3) << 4);
		hash |= ((listType >> 1) << 5);
		bool ignoreTexAlpha = pp->tsp.IgnoreTexA || pp->tcw.PixelFmt == Pixel565;
		hash |= (pp->tsp.ShadInstr << 7) | (ignoreTexAlpha << 9) | (pp->tsp.UseAlpha << 10)
			| (pp->tsp.ColorClamp << 11) | ((config::Fog ? pp->tsp.FogCtrl : 2) << 12) | (pp->tsp.SrcInstr << 14)
			| (pp->tsp.DstInstr << 17);
		hash |= (pp->isp.ZWriteDis << 20) | (pp->isp.CullMode << 21) | (pp->isp.DepthMode << 23);
		hash |= ((u64)sortTriangles << 26) | ((u64)gpuPalette << 27) | ((u64)pp->isNaomi2() << 29);
		hash |= (u64)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 30;
		hash |= (u64)(pp->tcw.PixelFmt == PixelBumpMap) << 31;
		hash |= (u64)dithering << 32;
		if (GetContext()->supportsDynamicLocalRead()) {
			hash |= (u64)(pp->tsp.SrcSelect == 1) << 33;
			hash |= (u64)(pp->tsp.DstSelect == 1) << 34;
		}

		return hash;
	}
	u32 hash(ModVolMode mode, int cullMode, bool naomi2) const
	{
		return ((int)mode << 2) | cullMode | ((int)naomi2 << 5) | ((int)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 6);
	}
	u32 hash(int cullMode, bool naomi2) const
	{
		return cullMode | ((int)naomi2 << 2) | ((int)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 3);
	}

	vk::PipelineVertexInputStateCreateInfo GetMainVertexInputStateCreateInfo(bool full = true, bool naomi2 = false) const
	{
		// Vertex input state
		static const vk::VertexInputBindingDescription vertexBindingDescriptions[] =
		{
				{ 0, sizeof(Vertex) },
		};
		static const vk::VertexInputAttributeDescription vertexInputAttributeDescriptions[] =
		{
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, x)),	// pos
				vk::VertexInputAttributeDescription(1, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Vertex, col)),	// base color
				vk::VertexInputAttributeDescription(2, 0, vk::Format::eR8G8B8A8Unorm, offsetof(Vertex, spc)),	// offset color
				vk::VertexInputAttributeDescription(3, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, u)),		// tex coord
				vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, nx)),	// naomi2 normal
		};
		static const vk::VertexInputAttributeDescription vertexInputLightAttributeDescriptions[] =
		{
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, x)),	// pos
		};


		const vk::VertexInputAttributeDescription* attributeDescription = vertexInputLightAttributeDescriptions;
		u32 attributeDescriptionSize = std::size(vertexInputLightAttributeDescriptions);

		if (full)
		{
			attributeDescription = vertexInputAttributeDescriptions;

			if (naomi2)
				attributeDescriptionSize = std::size(vertexInputAttributeDescriptions);
			else
				// naomi2 normal not needed
				attributeDescriptionSize = std::size(vertexInputAttributeDescriptions) - 1;
		}

		return vk::PipelineVertexInputStateCreateInfo(
				vk::PipelineVertexInputStateCreateFlags(),
				std::size(vertexBindingDescriptions), vertexBindingDescriptions,
				attributeDescriptionSize, attributeDescription
		);
	}

	void CreatePipeline(u32 listType, bool sortTriangles, const PolyParam& pp, int gpuPalette, bool dithering);

	std::map<u64, vk::UniquePipeline> pipelines;
	std::map<u32, vk::UniquePipeline> modVolPipelines;
	std::map<u32, vk::UniquePipeline> depthPassPipelines;

	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout perFrameLayout;
	vk::UniqueDescriptorSetLayout perPolyLayout;

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	vk::RenderPass renderPass;
	ShaderManager *shaderManager = nullptr;
};
