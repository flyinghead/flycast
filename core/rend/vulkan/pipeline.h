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

class DescriptorSets
{
public:
	void init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout, vk::DescriptorSetLayout perPolyLayout)
	{
		this->samplerManager = samplerManager;
		this->pipelineLayout = pipelineLayout;
		perFrameAlloc.setLayout(perFrameLayout);
		perPolyAlloc.setLayout(perPolyLayout);

	}
	void updateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset, vk::ImageView fogImageView, vk::ImageView paletteImageView)
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
			writeDescriptorSets.emplace_back(perFrameDescSet, 3, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
		}
		getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
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

			vk::DescriptorImageInfo imageInfo;
			if (poly.texture != nullptr)
			{
				imageInfo = vk::DescriptorImageInfo(samplerManager->GetSampler(poly.tsp),
						((Texture *)poly.texture)->GetReadOnlyImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);
				writeDescriptorSets.emplace_back(perPolyDescSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
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
				lightBufferInfo = vk::DescriptorBufferInfo{ buffer, lightOffset + poly.lightModel * size, sizeof(N2LightModel) };
				writeDescriptorSets.emplace_back(perPolyDescSet, 3, 0, vk::DescriptorType::eUniformBuffer, nullptr, lightBufferInfo);
			}

			getContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
			perPolyDescSets[&poly] = perPolyDescSet;
		}
		else
			perPolyDescSet = it->second;
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, perPolyDescSet, nullptr);
	}

	void bindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, const ModifierVolumeParam& mvParam, int polyNumber, vk::Buffer buffer,
			vk::DeviceSize uniformOffset)
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

	void nextFrame()
	{
		perFrameAlloc.nextFrame();
		perPolyAlloc.nextFrame();
		perFrameDescSet = vk::DescriptorSet{};
		perPolyDescSets.clear();
	}

	void term()
	{
		perFrameAlloc.term();
		perPolyAlloc.term();
	}

private:
	VulkanContext *getContext() const { return VulkanContext::Instance(); }

	vk::PipelineLayout pipelineLayout;
	DynamicDescSetAlloc perFrameAlloc;
	DynamicDescSetAlloc perPolyAlloc;
	vk::DescriptorSet perFrameDescSet = {};
	std::unordered_map<const void *, vk::DescriptorSet> perPolyDescSets;

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
			std::array<vk::DescriptorSetLayoutBinding, 4> perFrameBindings = {
					vk::DescriptorSetLayoutBinding(0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex),			// vertex uniforms
					vk::DescriptorSetLayoutBinding(1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment),		// fragment uniforms
					vk::DescriptorSetLayoutBinding(2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// fog texture
					vk::DescriptorSetLayoutBinding(3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment),// palette texture
			};
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
		hash |= ((u32)sortTriangles << 26) | ((u32)gpuPalette << 27) | ((u32)pp->isNaomi2() << 28);
		hash |= (u32)(!settings.platform.isNaomi2() && config::NativeDepthInterpolation) << 29;

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
				vk::VertexInputAttributeDescription(4, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, nx)),	// naomi2 normal
		};
		static const vk::VertexInputAttributeDescription vertexInputLightAttributeDescriptions[] =
		{
				vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, x)),	// pos
		};
		return vk::PipelineVertexInputStateCreateInfo(
				vk::PipelineVertexInputStateCreateFlags(),
				std::size(vertexBindingDescriptions),
				vertexBindingDescriptions,
				full ? std::size(vertexInputAttributeDescriptions) : std::size(vertexInputLightAttributeDescriptions),
				full ? vertexInputAttributeDescriptions : vertexInputLightAttributeDescriptions);
	}

	void CreatePipeline(u32 listType, bool sortTriangles, const PolyParam& pp, bool gpuPalette);

	std::map<u32, vk::UniquePipeline> pipelines;
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
	    vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, nullptr, colorReference, nullptr, &depthReference);
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
	    		1, &subpass, renderToTextureBuffer ? std::size(vramWriteDeps) : std::size(dependencies), renderToTextureBuffer ? vramWriteDeps : dependencies));

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
			vk::DescriptorSetLayoutBinding binding(0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment); // texture
			descSetLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), binding));
			pipelineLayout = GetContext()->GetDevice().createPipelineLayoutUnique(
					vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), descSetLayout.get()));
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
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), descSetLayout.get())).front());
		}
		vk::DescriptorImageInfo imageInfo(*sampler, imageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		vk::WriteDescriptorSet writeDescriptorSet(*descriptorSet, 0, 0, vk::DescriptorType::eCombinedImageSampler, imageInfo);
		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSet, nullptr);
	}

	void Term()
	{
		descriptorSet.reset();
		pipeline.reset();
		sampler.reset();
		pipelineLayout.reset();
		descSetLayout.reset();
	}

	vk::Pipeline GetPipeline()
	{
		if (!pipeline)
			CreatePipeline();
		return *pipeline;
	}

	void BindDescriptorSets(vk::CommandBuffer cmdBuffer) const
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, descriptorSet.get(), nullptr);
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
