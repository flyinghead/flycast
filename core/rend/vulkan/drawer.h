/*
	Created on: Oct 8, 2019

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
#include "rend/tileclip.h"
#include "rend/transform_matrix.h"
#include "vulkan.h"
#include "buffer.h"
#include "commandpool.h"
#include "pipeline.h"
#include "shaders.h"
#include "texture.h"

#include <memory>
#include <vector>
#include <glm/gtc/type_ptr.hpp>

class BaseDrawer
{
public:
	void SetCommandPool(CommandPool *commandPool) { this->commandPool = commandPool; }

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	TileClipping SetTileClip(u32 val, vk::Rect2D& clipRect);
	void SetBaseScissor(const vk::Extent2D& viewport = vk::Extent2D());
	void scaleAndWriteFramebuffer(vk::CommandBuffer commandBuffer, FramebufferAttachment *finalFB);

	void SetScissor(vk::CommandBuffer cmdBuffer, const vk::Rect2D& scissor)
	{
		if (scissor != currentScissor)
		{
			cmdBuffer.setScissor(0, scissor);
			currentScissor = scissor;
		}
	}

	template<typename T>
	T MakeFragmentUniforms()
	{
		T fragUniforms;

		//VERT and RAM fog color constants
		FOG_COL_VERT.getRGBColor(fragUniforms.sp_FOG_COL_VERT);
		FOG_COL_RAM.getRGBColor(fragUniforms.sp_FOG_COL_RAM);

		//Fog density constant
		fragUniforms.sp_FOG_DENSITY = FOG_DENSITY.get() * config::ExtraDepthScale;

		pvrrc.fog_clamp_min.getRGBAColor(fragUniforms.colorClampMin);
		pvrrc.fog_clamp_max.getRGBAColor(fragUniforms.colorClampMax);

		fragUniforms.cp_AlphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

		return fragUniforms;
	}

	template<typename Offsets>
	void packNaomi2Uniforms(BufferPacker& packer, Offsets& offsets, std::vector<u8>& n2uniforms, bool trModVolIncluded)
	{
		size_t n2UniformSize = sizeof(N2VertexShaderUniforms) + align(sizeof(N2VertexShaderUniforms), GetContext()->GetUniformBufferAlignment());
		int items = pvrrc.global_param_op.size() + pvrrc.global_param_pt.size() + pvrrc.global_param_tr.size() + pvrrc.global_param_mvo.size();
		if (trModVolIncluded)
			items += pvrrc.global_param_mvo_tr.size();
		n2uniforms.resize(items * n2UniformSize);
		size_t bufIdx = 0;
		auto addUniform = [&](const PolyParam& pp, int polyNumber) {
			if (pp.isNaomi2())
			{
				N2VertexShaderUniforms& uni = *(N2VertexShaderUniforms *)&n2uniforms[bufIdx];
				memcpy(glm::value_ptr(uni.mvMat), pvrrc.matrices[pp.mvMatrix].mat, sizeof(uni.mvMat));
				memcpy(glm::value_ptr(uni.normalMat), pvrrc.matrices[pp.normalMatrix].mat, sizeof(uni.normalMat));
				memcpy(glm::value_ptr(uni.projMat), pvrrc.matrices[pp.projMatrix].mat, sizeof(uni.projMat));
				uni.bumpMapping = pp.pcw.Texture == 1 && pp.tcw.PixelFmt == PixelBumpMap;
				uni.polyNumber = polyNumber;
				for (size_t i = 0; i < 2; i++)
				{
					uni.envMapping[i] = pp.envMapping[i];
					uni.glossCoef[i] = pp.glossCoef[i];
					uni.constantColor[i] = pp.constantColor[i];
				}
			}
			bufIdx += n2UniformSize;
		};
		for (const PolyParam& pp : pvrrc.global_param_op)
			addUniform(pp, 0);
		size_t ptOffset = bufIdx;
		for (const PolyParam& pp : pvrrc.global_param_pt)
			addUniform(pp, 0);
		size_t trOffset = bufIdx;
		if (!pvrrc.global_param_tr.empty())
		{
			u32 firstVertexIdx = pvrrc.idx[pvrrc.global_param_tr[0].first];
			for (const PolyParam& pp : pvrrc.global_param_tr)
				addUniform(pp, ((&pp - &pvrrc.global_param_tr[0]) << 17) - firstVertexIdx);
		}
		size_t mvOffset = bufIdx;
		for (const ModifierVolumeParam& mvp : pvrrc.global_param_mvo)
		{
			if (mvp.isNaomi2())
			{
				N2VertexShaderUniforms& uni = *(N2VertexShaderUniforms *)&n2uniforms[bufIdx];
				memcpy(glm::value_ptr(uni.mvMat), pvrrc.matrices[mvp.mvMatrix].mat, sizeof(uni.mvMat));
				memcpy(glm::value_ptr(uni.projMat), pvrrc.matrices[mvp.projMatrix].mat, sizeof(uni.projMat));
			}
			bufIdx += n2UniformSize;
		}
		size_t trMvOffset = bufIdx;
		if (trModVolIncluded)
			for (const ModifierVolumeParam& mvp : pvrrc.global_param_mvo_tr)
			{
				if (mvp.isNaomi2())
				{
					N2VertexShaderUniforms& uni = *(N2VertexShaderUniforms *)&n2uniforms[bufIdx];
					memcpy(glm::value_ptr(uni.mvMat), pvrrc.matrices[mvp.mvMatrix].mat, sizeof(uni.mvMat));
					memcpy(glm::value_ptr(uni.projMat), pvrrc.matrices[mvp.projMatrix].mat, sizeof(uni.projMat));
				}
				bufIdx += n2UniformSize;
			}
		offsets.naomi2OpaqueOffset = packer.addUniform(n2uniforms.data(), bufIdx);
		offsets.naomi2PunchThroughOffset = offsets.naomi2OpaqueOffset + ptOffset;
		offsets.naomi2TranslucentOffset = offsets.naomi2OpaqueOffset + trOffset;
		offsets.naomi2ModVolOffset = offsets.naomi2OpaqueOffset + mvOffset;
		offsets.naomi2TrModVolOffset = offsets.naomi2OpaqueOffset + trMvOffset;
	}

	vk::DeviceSize packNaomi2Lights(BufferPacker& packer)
	{
		vk::DeviceSize offset = -1;

		size_t n2LightSize = sizeof(N2LightModel) + align(sizeof(N2LightModel), GetContext()->GetUniformBufferAlignment());
		if (n2LightSize == sizeof(N2LightModel) && !pvrrc.lightModels.empty())
		{
			offset = packer.addUniform(&pvrrc.lightModels[0], pvrrc.lightModels.size() * sizeof(decltype(pvrrc.lightModels[0])));
		}
		else
		{
			for (const N2LightModel& model : pvrrc.lightModels)
			{
				vk::DeviceSize ioffset = packer.addUniform(&model, sizeof(N2LightModel));
				if (offset == (vk::DeviceSize)-1)
					offset = ioffset;
			}
		}

		return offset;
	}

	vk::Rect2D baseScissor;
	vk::Rect2D currentScissor;
	TransformMatrix<COORD_VULKAN> matrices;
	CommandPool *commandPool = nullptr;
};

class Drawer : public BaseDrawer
{
public:
	virtual ~Drawer() = default;

	void Term()
	{
		descriptorSets.term();
		mainBuffers.clear();
	}

	bool Draw(const Texture *fogTexture, const Texture *paletteTexture);
	virtual void EndRenderPass() { renderPass++; }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return currentCommandBuffer; }

protected:
	virtual u32 GetSwapChainSize() { return GetContext()->GetSwapChainSize(); }
	virtual vk::CommandBuffer BeginRenderPass() = 0;
	void NewImage()
	{
		descriptorSets.nextFrame();
		imageIndex = (imageIndex + 1) % GetSwapChainSize();
		if (perStripSorting != config::PerStripSorting)
		{
			perStripSorting = config::PerStripSorting;
			pipelineManager->Reset();
		}
		renderPass = 0;
	}

	void Init(SamplerManager *samplerManager, PipelineManager *pipelineManager)
	{
		this->pipelineManager = pipelineManager;
		this->samplerManager = samplerManager;

		descriptorSets.init(samplerManager, pipelineManager->GetPipelineLayout(), pipelineManager->GetPerFrameDSLayout(), pipelineManager->GetPerPolyDSLayout());
	}

	int GetCurrentImage() const { return imageIndex; }

	BufferData* GetMainBuffer(u32 size)
	{
		u32 bufferIndex = imageIndex + renderPass * GetSwapChainSize();
		while (mainBuffers.size() <= bufferIndex)
		{
			mainBuffers.push_back(std::make_unique<BufferData>(std::max(512 * 1024u, size),
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer));
		}
		if (mainBuffers[bufferIndex]->bufferSize < size)
		{
			u32 newSize = (u32)mainBuffers[bufferIndex]->bufferSize;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing main buffer size %d -> %d", (u32)mainBuffers[bufferIndex]->bufferSize, newSize);
			mainBuffers[bufferIndex] = std::make_unique<BufferData>(newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer);
		}
		return mainBuffers[bufferIndex].get();
	}

	vk::CommandBuffer currentCommandBuffer;
	SamplerManager *samplerManager = nullptr;

private:
	void SortTriangles();
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
	void DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last);
	void DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count);
	void UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms);

	int imageIndex = 0;
	int renderPass = 0;
	struct {
		vk::DeviceSize indexOffset = 0;
		vk::DeviceSize modVolOffset = 0;
		vk::DeviceSize vertexUniformOffset = 0;
		vk::DeviceSize fragmentUniformOffset = 0;
		vk::DeviceSize naomi2OpaqueOffset = 0;
		vk::DeviceSize naomi2PunchThroughOffset = 0;
		vk::DeviceSize naomi2TranslucentOffset = 0;
		vk::DeviceSize naomi2ModVolOffset = 0;
		vk::DeviceSize naomi2TrModVolOffset = 0;
		vk::DeviceSize lightsOffset = 0;
	} offsets;
	DescriptorSets descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
	PipelineManager *pipelineManager = nullptr;
	bool perStripSorting = false;
	bool dithering = false;
};

class ScreenDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager, const vk::Extent2D& viewport);

	void Term()
	{
		screenPipelineManager.reset();
		renderPassLoad.reset();
		renderPassClear.reset();
		framebuffers.clear();
		colorAttachments.clear();
		depthAttachment.reset();
		Drawer::Term();
	}

	vk::RenderPass GetRenderPass() const { return *renderPassClear; }
	void EndRenderPass() override;
	bool PresentFrame()
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
		GetContext()->PresentFrame(colorAttachments[GetCurrentImage()]->GetImage(),
				colorAttachments[GetCurrentImage()]->GetImageView(), viewport, aspectRatio);

		return true;
	}

protected:
	vk::CommandBuffer BeginRenderPass() override;
	u32 GetSwapChainSize() override { return 2; }

private:
	std::unique_ptr<PipelineManager> screenPipelineManager;

	vk::UniqueRenderPass renderPassLoad;
	vk::UniqueRenderPass renderPassClear;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::vector<std::unique_ptr<FramebufferAttachment>> colorAttachments;
	std::unique_ptr<FramebufferAttachment> depthAttachment;
	vk::Extent2D viewport;
	ShaderManager *shaderManager = nullptr;
	std::vector<bool> transitionNeeded;
	std::vector<bool> clearNeeded;
	bool frameRendered = false;
	float aspectRatio = 0.f;
};

class TextureDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager, TextureCache *textureCache);

	void Term()
	{
		rttPipelineManager.reset();
		framebuffers.clear();
		colorAttachment.reset();
		depthAttachment.reset();
		Drawer::Term();
	}

	void EndRenderPass() override;

protected:
	vk::CommandBuffer BeginRenderPass() override;

private:
	u32 width = 0;
	u32 height = 0;
	u32 textureAddr = 0;
	std::unique_ptr<RttPipelineManager> rttPipelineManager;

	Texture *texture = nullptr;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::unique_ptr<FramebufferAttachment> colorAttachment;
	std::unique_ptr<FramebufferAttachment> depthAttachment;
	TextureCache *textureCache = nullptr;
};
