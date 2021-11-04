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
#include "rend/sorter.h"
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

class BaseDrawer
{
public:
	void SetCommandPool(CommandPool *commandPool) { this->commandPool = commandPool; }

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	TileClipping SetTileClip(u32 val, vk::Rect2D& clipRect);
	void SetBaseScissor(const vk::Extent2D& viewport = vk::Extent2D());
	void SetProvokingVertices();

	void SetScissor(const vk::CommandBuffer& cmdBuffer, const vk::Rect2D& scissor)
	{
		if (scissor != currentScissor)
		{
			cmdBuffer.setScissor(0, scissor);
			currentScissor = scissor;
		}
	}

	u32 align(vk::DeviceSize offset, u32 alignment)
	{
		return (u32)(alignment - (offset & (alignment - 1)));
	}

	template<typename T>
	T MakeFragmentUniforms()
	{
		T fragUniforms;

		//VERT and RAM fog color constants
		u8* fog_colvert_bgra = (u8*)&FOG_COL_VERT;
		u8* fog_colram_bgra = (u8*)&FOG_COL_RAM;
		fragUniforms.sp_FOG_COL_VERT[0] = fog_colvert_bgra[2] / 255.0f;
		fragUniforms.sp_FOG_COL_VERT[1] = fog_colvert_bgra[1] / 255.0f;
		fragUniforms.sp_FOG_COL_VERT[2] = fog_colvert_bgra[0] / 255.0f;

		fragUniforms.sp_FOG_COL_RAM[0] = fog_colram_bgra[2] / 255.0f;
		fragUniforms.sp_FOG_COL_RAM[1] = fog_colram_bgra[1] / 255.0f;
		fragUniforms.sp_FOG_COL_RAM[2] = fog_colram_bgra[0] / 255.0f;

		//Fog density constant
		u8* fog_density = (u8*)&FOG_DENSITY;
		float fog_den_mant = fog_density[1] / 128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
		s32 fog_den_exp = (s8)fog_density[0];
		fragUniforms.sp_FOG_DENSITY = fog_den_mant * powf(2.0f, fog_den_exp) * config::ExtraDepthScale;

		fragUniforms.colorClampMin[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
		fragUniforms.colorClampMin[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
		fragUniforms.colorClampMin[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
		fragUniforms.colorClampMin[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;

		fragUniforms.colorClampMax[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
		fragUniforms.colorClampMax[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
		fragUniforms.colorClampMax[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
		fragUniforms.colorClampMax[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;

		fragUniforms.cp_AlphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

		return fragUniforms;
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
	bool Draw(const Texture *fogTexture, const Texture *paletteTexture);
	virtual void EndRenderPass() { renderPass++; }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return currentCommandBuffer; }

protected:
	virtual size_t GetSwapChainSize() { return GetContext()->GetSwapChainSize(); }
	virtual vk::CommandBuffer BeginRenderPass() = 0;
	void NewImage()
	{
		GetCurrentDescSet().Reset();
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

		size_t size = GetSwapChainSize();
		if (descriptorSets.size() > size)
			descriptorSets.resize(size);
		else
			while (descriptorSets.size() < size)
			{
				descriptorSets.emplace_back();
				descriptorSets.back().Init(samplerManager, pipelineManager->GetPipelineLayout(), pipelineManager->GetPerFrameDSLayout(), pipelineManager->GetPerPolyDSLayout());
			}
	}
	int GetCurrentImage() const { return imageIndex; }
	DescriptorSets& GetCurrentDescSet() { return descriptorSets[GetCurrentImage()]; }

	BufferData* GetMainBuffer(u32 size)
	{
		u32 bufferIndex = imageIndex + renderPass * GetSwapChainSize();
		while (mainBuffers.size() <= bufferIndex)
		{
			mainBuffers.push_back(std::unique_ptr<BufferData>(new BufferData(std::max(512 * 1024u, size),
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer)));
		}
		if (mainBuffers[bufferIndex]->bufferSize < size)
		{
			u32 newSize = mainBuffers[bufferIndex]->bufferSize;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing main buffer size %d -> %d", (u32)mainBuffers[bufferIndex]->bufferSize, newSize);
			mainBuffers[bufferIndex] = std::unique_ptr<BufferData>(new BufferData(newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer));
		}
		return mainBuffers[bufferIndex].get();
	};

	vk::CommandBuffer currentCommandBuffer;
	SamplerManager *samplerManager = nullptr;

private:
	void SortTriangles();
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
	void DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortTrigDrawParam>& polys, bool multipass);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const List<PolyParam>& polys, u32 first, u32 last);
	void DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count);
	void UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms);

	int imageIndex = 0;
	int renderPass = 0;
	struct {
		vk::DeviceSize indexOffset = 0;
		vk::DeviceSize modVolOffset = 0;
		vk::DeviceSize vertexUniformOffset = 0;
		vk::DeviceSize fragmentUniformOffset = 0;
	} offsets;
	std::vector<DescriptorSets> descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
	PipelineManager *pipelineManager = nullptr;

	// Per-triangle sort results
	std::vector<std::vector<SortTrigDrawParam>> sortedPolys;
	std::vector<std::vector<u32>> sortedIndexes;
	u32 sortedIndexCount = 0;
	bool perStripSorting = false;
};

class ScreenDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager, const vk::Extent2D& viewport);
	vk::RenderPass GetRenderPass() const { return *renderPassClear; }
	void EndRenderPass() override;
	bool PresentFrame()
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
		GetContext()->PresentFrame(colorAttachments[GetCurrentImage()]->GetImage(),
				colorAttachments[GetCurrentImage()]->GetImageView(), viewport);
		NewImage();

		return true;
	}

protected:
	vk::CommandBuffer BeginRenderPass() override;
	size_t GetSwapChainSize() override { return 2; }

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
};

class TextureDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager, TextureCache *textureCache);
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
