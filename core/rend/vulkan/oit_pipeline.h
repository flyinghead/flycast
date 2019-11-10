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
#include <tuple>
#include <glm/glm.hpp>
#include "vulkan.h"
#include "oit_shaders.h"
#include "texture.h"
#include "hw/pvr/ta_ctx.h"

class OITDescriptorSets
{
public:
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
		float extra_depth_scale;
		float shade_scale_factor;	// new for OIT
	};

	struct PushConstants
	{
		glm::vec4 clipTest;
		glm::ivec4 blend_mode0;	// Only using 2 elements but easier for std140
		float trilinearAlpha;
		int pp_Number;
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

	void Init(SamplerManager* samplerManager, vk::PipelineLayout pipelineLayout, vk::DescriptorSetLayout perFrameLayout,
			vk::DescriptorSetLayout perPolyLayout, vk::DescriptorSetLayout pass1Layout, vk::DescriptorSetLayout pass2Layout)
	{
		this->samplerManager = samplerManager;
		this->pipelineLayout = pipelineLayout;
		this->perFrameLayout = perFrameLayout;
		this->perPolyLayout = perPolyLayout;
		this->pass1Layout = pass1Layout;
		this->pass2Layout = pass2Layout;
	}
	// FIXME way too many params
	void UpdateUniforms(vk::Buffer buffer, u32 vertexUniformOffset, u32 fragmentUniformOffset, vk::ImageView fogImageView,
			vk::Buffer pixelBuffer, vk::DeviceSize pixelBufferSize, vk::Buffer pixelCounterBuffer, u32 polyParamsOffset,
			u32 polyParamsSize, vk::ImageView pointerImageView)
	{
		if (!perFrameDescSet)
		{
			perFrameDescSet = std::move(GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), 1, &perFrameLayout)).front());
		}
		std::vector<vk::DescriptorBufferInfo> bufferInfos;
		bufferInfos.push_back(vk::DescriptorBufferInfo(buffer, vertexUniformOffset, sizeof(VertexShaderUniforms)));
		bufferInfos.push_back(vk::DescriptorBufferInfo(buffer, fragmentUniformOffset, sizeof(FragmentShaderUniforms)));

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfos[0], nullptr));
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSet, 1, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferInfos[1], nullptr));
		if (fogImageView)
		{
			TSP fogTsp = {};
			fogTsp.FilterMode = 1;
			fogTsp.ClampU = 1;
			fogTsp.ClampV = 1;
			vk::Sampler fogSampler = samplerManager->GetSampler(fogTsp);
			static vk::DescriptorImageInfo imageInfo;
			imageInfo = { fogSampler, fogImageView, vk::ImageLayout::eShaderReadOnlyOptimal };
			writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSet, 2, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo, nullptr, nullptr));
		}
		vk::DescriptorBufferInfo pixelBufferInfo(pixelBuffer, 0, pixelBufferSize);
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSet, 3, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &pixelBufferInfo, nullptr));
		vk::DescriptorBufferInfo pixelCounterBufferInfo(pixelCounterBuffer, 0, 4);
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSet, 4, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &pixelCounterBufferInfo, nullptr));
		vk::DescriptorImageInfo pointerImageInfo(vk::Sampler(), pointerImageView, vk::ImageLayout::eGeneral);
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSet, 5, 0, 1, vk::DescriptorType::eStorageImage, &pointerImageInfo, nullptr, nullptr));
		if (polyParamsSize > 0)
		{
			static vk::DescriptorBufferInfo polyParamsBufferInfo;
			polyParamsBufferInfo = vk::DescriptorBufferInfo(buffer, polyParamsOffset, polyParamsSize);
			writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perFrameDescSet, 6, 0, 1, vk::DescriptorType::eStorageBuffer, nullptr, &polyParamsBufferInfo, nullptr));
		}

		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void UpdatePass1Uniforms(vk::ImageView stencilImageView, vk::ImageView depthImageView)
	{
		if (!pass1DescSet)
		{
			pass1DescSet = std::move(GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), 1, &pass1Layout)).front());
		}
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		vk::DescriptorImageInfo stencilImageInfo(vk::Sampler(), stencilImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*pass1DescSet, 0, 0, 1, vk::DescriptorType::eInputAttachment, &stencilImageInfo, nullptr, nullptr));
		vk::DescriptorImageInfo depthImageInfo(vk::Sampler(), depthImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*pass1DescSet, 1, 0, 1, vk::DescriptorType::eInputAttachment, &depthImageInfo, nullptr, nullptr));

		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void UpdatePass2Uniforms(vk::ImageView colorImageView)
	{
		if (!pass2DescSet)
		{
			pass2DescSet = std::move(GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), 1, &pass2Layout)).front());
		}
		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		vk::DescriptorImageInfo colorImageInfo(vk::Sampler(), colorImageView, vk::ImageLayout::eShaderReadOnlyOptimal);
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*pass2DescSet, 0, 0, 1, vk::DescriptorType::eInputAttachment, &colorImageInfo, nullptr, nullptr));

		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
	}

	void SetTexture(u64 textureId0, TSP tsp0, u64 textureId1, TSP tsp1)
	{
		auto index = std::make_tuple(textureId0, tsp0.full & SamplerManager::TSP_Mask,
				textureId1, tsp1.full & SamplerManager::TSP_Mask);
		if (perPolyDescSetsInFlight.find(index) != perPolyDescSetsInFlight.end())
			return;

		if (perPolyDescSets.empty())
		{
			std::vector<vk::DescriptorSetLayout> layouts(10, perPolyLayout);
			perPolyDescSets = GetContext()->GetDevice().allocateDescriptorSetsUnique(
					vk::DescriptorSetAllocateInfo(GetContext()->GetDescriptorPool(), layouts.size(), &layouts[0]));
		}
		Texture *texture = reinterpret_cast<Texture *>(textureId0);
		vk::DescriptorImageInfo imageInfo0(samplerManager->GetSampler(tsp0), texture->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

		std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perPolyDescSets.back(), 0, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo0, nullptr, nullptr));

		if (textureId1 != -1)
		{
			Texture *texture1 = reinterpret_cast<Texture *>(textureId1);
			vk::DescriptorImageInfo imageInfo1(samplerManager->GetSampler(tsp1), texture1->GetImageView(), vk::ImageLayout::eShaderReadOnlyOptimal);

			writeDescriptorSets.push_back(vk::WriteDescriptorSet(*perPolyDescSets.back(), 1, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo1, nullptr, nullptr));
		}
		GetContext()->GetDevice().updateDescriptorSets(writeDescriptorSets, nullptr);
		perPolyDescSetsInFlight[index] = std::move(perPolyDescSets.back());
		perPolyDescSets.pop_back();
	}

	void BindPerFrameDescriptorSets(vk::CommandBuffer cmdBuffer)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &perFrameDescSet.get(), 0, nullptr);
	}

	void BindPass1DescriptorSets(vk::CommandBuffer cmdBuffer)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 2, 1, &pass1DescSet.get(), 0, nullptr);
	}

	void BindPass2DescriptorSets(vk::CommandBuffer cmdBuffer)
	{
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 3, 1, &pass2DescSet.get(), 0, nullptr);
	}

	void BindPerPolyDescriptorSets(vk::CommandBuffer cmdBuffer, u64 textureId0, TSP tsp0, u64 textureId1, TSP tsp1)
	{
		auto index = std::make_tuple(textureId0, tsp0.full & SamplerManager::TSP_Mask, textureId1, tsp1.full & SamplerManager::TSP_Mask);
		cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 1, 1,
				&perPolyDescSetsInFlight[index].get(), 0, nullptr);
	}

	void Reset()
	{
		for (auto& pair : perPolyDescSetsInFlight)
			perPolyDescSets.emplace_back(std::move(pair.second));
		perPolyDescSetsInFlight.clear();

	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	vk::DescriptorSetLayout perFrameLayout;
	vk::DescriptorSetLayout perPolyLayout;
	vk::DescriptorSetLayout pass1Layout;
	vk::DescriptorSetLayout pass2Layout;
	vk::PipelineLayout pipelineLayout;

	vk::UniqueDescriptorSet perFrameDescSet;
	vk::UniqueDescriptorSet pass1DescSet;
	vk::UniqueDescriptorSet pass2DescSet;
	std::vector<vk::UniqueDescriptorSet> perPolyDescSets;
	std::map<std::tuple<u64, u32, u64, u32>, vk::UniqueDescriptorSet> perPolyDescSetsInFlight;

	SamplerManager* samplerManager;
};

class OITPipelineManager
{
public:
	virtual ~OITPipelineManager() = default;

	virtual void Init(OITShaderManager *shaderManager)
	{
		this->shaderManager = shaderManager;

		if (!perFrameLayout)
		{
			// Descriptor set and pipeline layout
			vk::DescriptorSetLayoutBinding perFrameBindings[] = {
					{ 0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eVertex },			// vertex uniforms
					{ 1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// fragment uniforms
					{ 2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// fog texture
					{ 3, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// pixel buffer
					{ 4, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// pixel counter
					{ 5, vk::DescriptorType::eStorageImage, 1, vk::ShaderStageFlagBits::eFragment },		// a-buffer pointers
					{ 6, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eFragment },		// Tr poly params
			};
			perFrameLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perFrameBindings), perFrameBindings));

			vk::DescriptorSetLayoutBinding pass1Bindings[] = {
					{ 0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// stencil input attachment
					{ 1, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// depth input attachment
			};
			pass1Layout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(pass1Bindings), pass1Bindings));

			vk::DescriptorSetLayoutBinding pass2Bindings[] = {
					{ 0, vk::DescriptorType::eInputAttachment, 1, vk::ShaderStageFlagBits::eFragment },		// color input attachment
			};
			pass2Layout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(pass2Bindings), pass2Bindings));

			vk::DescriptorSetLayoutBinding perPolyBindings[] = {
					{ 0, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture 0
					{ 1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment },// texture 1 (for 2-volume mode)
			};
			perPolyLayout = GetContext()->GetDevice().createDescriptorSetLayoutUnique(
					vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), ARRAY_SIZE(perPolyBindings), perPolyBindings));

			vk::PushConstantRange pushConstant(vk::ShaderStageFlagBits::eFragment, 0, sizeof(OITDescriptorSets::PushConstants));
			vk::DescriptorSetLayout layouts[] = { *perFrameLayout, *perPolyLayout, *pass1Layout, *pass2Layout };
			pipelineLayout = GetContext()->GetDevice().createPipelineLayoutUnique(
					vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), ARRAY_SIZE(layouts), layouts, 1, &pushConstant));
		}

		MakeRenderPass();
		pipelines.clear();
		modVolPipelines.clear();
	}

	vk::Pipeline GetPipeline(u32 listType, bool autosort, const PolyParam& pp, int pass)
	{
		u32 pipehash = hash(listType, autosort, &pp, pass);
		const auto &pipeline = pipelines.find(pipehash);
		if (pipeline != pipelines.end())
			return pipeline->second.get();

		CreatePipeline(listType, autosort, pp, pass);

		return *pipelines[pipehash];
	}

	vk::Pipeline GetModifierVolumePipeline(ModVolMode mode)
	{
		if (modVolPipelines.empty() || !modVolPipelines[(size_t)mode])
			CreateModVolPipeline(mode);
		return *modVolPipelines[(size_t)mode];
	}
	vk::Pipeline GetTrModifierVolumePipeline(ModVolMode mode)
	{
		if (trModVolPipelines.empty() || !trModVolPipelines[(size_t)mode])
			CreateTrModVolPipeline(mode);
		return *trModVolPipelines[(size_t)mode];
	}
	vk::Pipeline GetFinalPipeline(bool autosort)
	{
		vk::UniquePipeline& pipeline = autosort ? finalAutosortPipeline : finalNosortPipeline;
		if (!pipeline)
			CreateFinalPipeline(autosort);
		return *pipeline;
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
	vk::DescriptorSetLayout GetPass1DSLayout() const { return *pass1Layout; }
	vk::DescriptorSetLayout GetPass2DSLayout() const { return *pass2Layout; }

	vk::RenderPass GetRenderPass() const { return *renderPass; }

private:
	void MakeRenderPass();
	void CreateModVolPipeline(ModVolMode mode);
	void CreateTrModVolPipeline(ModVolMode mode);

	u32 hash(u32 listType, bool autosort, const PolyParam *pp, int pass) const
	{
		u32 hash = pp->pcw.Gouraud | (pp->pcw.Offset << 1) | (pp->pcw.Texture << 2) | (pp->pcw.Shadow << 3)
			| (((pp->tileclip >> 28) == 3) << 4);
		hash |= ((listType >> 1) << 5);
		if (pp->tcw1.full != -1 || pp->tsp1.full != -1)
		{
			// Two-volume mode
			hash |= (1 << 31) | (pp->tsp.ColorClamp << 11);
		}
		else
		{
			hash |= (pp->tsp.ShadInstr << 7) | (pp->tsp.IgnoreTexA << 9) | (pp->tsp.UseAlpha << 10)
				| (pp->tsp.ColorClamp << 11) | ((settings.rend.Fog ? pp->tsp.FogCtrl : 2) << 12)
				| (pp->tsp.SrcInstr << 14) | (pp->tsp.DstInstr << 17);
		}
		hash |= (pp->isp.ZWriteDis << 20) | (pp->isp.CullMode << 21) | ((autosort ? 6 : pp->isp.DepthMode) << 23);
		hash |= (u32)pass << 26;

		return hash;
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

	void CreatePipeline(u32 listType, bool autosort, const PolyParam& pp, int pass);
	void CreateFinalPipeline(bool autosort);
	void CreateClearPipeline();

	std::map<u32, vk::UniquePipeline> pipelines;
	std::vector<vk::UniquePipeline> modVolPipelines;
	std::vector<vk::UniquePipeline> trModVolPipelines;
	vk::UniquePipeline finalAutosortPipeline;
	vk::UniquePipeline finalNosortPipeline;
	vk::UniquePipeline clearPipeline;

	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout perFrameLayout;
	vk::UniqueDescriptorSetLayout pass1Layout;
	vk::UniqueDescriptorSetLayout pass2Layout;
	vk::UniqueDescriptorSetLayout perPolyLayout;

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	vk::UniqueRenderPass renderPass;
	OITShaderManager *shaderManager;
};

class RttOITPipelineManager : public OITPipelineManager
{
public:
	void Init(OITShaderManager *shaderManager) override
	{
		OITPipelineManager::Init(shaderManager);

		// FIXME RTT render pass
		renderToTextureBuffer = settings.rend.RenderToTextureBuffer;
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

	    renderPass = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), 2, attachmentDescriptions,
	    		1, &subpass, renderToTextureBuffer ? ARRAY_SIZE(vramWriteDeps) : ARRAY_SIZE(dependencies), renderToTextureBuffer ? vramWriteDeps : dependencies));
	}
	void CheckSettingsChange()
	{
		if (renderToTextureBuffer != settings.rend.RenderToTextureBuffer)
			Init(shaderManager);
	}

private:
	bool renderToTextureBuffer;
};
