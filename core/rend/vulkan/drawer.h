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
#include <memory>
#include "rend/sorter.h"
#include "rend/transform_matrix.h"
#include "vulkan.h"
#include "buffer.h"
#include "commandpool.h"
#include "pipeline.h"
#include "shaders.h"
#include "texture.h"

class BaseDrawer
{
public:
	void SetScissor(const vk::CommandBuffer& cmdBuffer, vk::Rect2D scissor)
	{
		if (scissor != currentScissor)
		{
			cmdBuffer.setScissor(0, scissor);
			currentScissor = scissor;
		}
	}

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	TileClipping SetTileClip(u32 val, vk::Rect2D& clipRect);
	void SetBaseScissor();

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
		fragUniforms.sp_FOG_DENSITY = fog_den_mant * powf(2.0f, fog_den_exp) * settings.rend.ExtraDepthScale;

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
	TransformMatrix<false> matrices;
};

class Drawer : public BaseDrawer
{
public:
	Drawer() = default;
	virtual ~Drawer() = default;
	bool Draw(const Texture *fogTexture);
	Drawer(const Drawer& other) = delete;
	Drawer(Drawer&& other) = default;
	Drawer& operator=(const Drawer& other) = delete;
	Drawer& operator=(Drawer&& other) = default;
	virtual vk::CommandBuffer BeginRenderPass() = 0;
	virtual void EndRenderPass() = 0;

protected:
	void Init(SamplerManager *samplerManager, PipelineManager *pipelineManager)
	{
		this->pipelineManager = pipelineManager;
		this->samplerManager = samplerManager;
	}
	virtual DescriptorSets& GetCurrentDescSet() = 0;
	virtual BufferData *GetMainBuffer(u32 size) = 0;

	PipelineManager *pipelineManager = nullptr;

private:
	void SortTriangles();
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
	void DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortTrigDrawParam>& polys);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const List<PolyParam>& polys, u32 first, u32 count);
	void DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count);
	void UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms);

	struct {
		vk::DeviceSize indexOffset = 0;
		vk::DeviceSize modVolOffset = 0;
		vk::DeviceSize vertexUniformOffset = 0;
		vk::DeviceSize fragmentUniformOffset = 0;
	} offsets;
	// Per-triangle sort results
	std::vector<std::vector<SortTrigDrawParam>> sortedPolys;
	std::vector<std::vector<u32>> sortedIndexes;
	u32 sortedIndexCount = 0;

	SamplerManager *samplerManager = nullptr;
};

class ScreenDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager)
	{
		if (!screenPipelineManager)
			screenPipelineManager = std::unique_ptr<PipelineManager>(new PipelineManager());
		screenPipelineManager->Init(shaderManager);
		Drawer::Init(samplerManager, screenPipelineManager.get());

		if (descriptorSets.size() > GetContext()->GetSwapChainSize())
			descriptorSets.resize(GetContext()->GetSwapChainSize());
		else
			while (descriptorSets.size() < GetContext()->GetSwapChainSize())
			{
				descriptorSets.push_back(DescriptorSets());
				descriptorSets.back().Init(samplerManager, screenPipelineManager->GetPipelineLayout(), screenPipelineManager->GetPerFrameDSLayout(), screenPipelineManager->GetPerPolyDSLayout());
			}
	}
	ScreenDrawer() = default;
	ScreenDrawer(const ScreenDrawer& other) = delete;
	ScreenDrawer(ScreenDrawer&& other) = default;
	ScreenDrawer& operator=(const ScreenDrawer& other) = delete;
	ScreenDrawer& operator=(ScreenDrawer&& other) = default;
	virtual vk::CommandBuffer BeginRenderPass() override;
	virtual void EndRenderPass() override
	{
		GetContext()->EndFrame();
	}

protected:
	virtual DescriptorSets& GetCurrentDescSet() override { return descriptorSets[GetCurrentImage()]; }
	virtual BufferData* GetMainBuffer(u32 size) override
	{
		if (mainBuffers.empty())
		{
			for (int i = 0; i < GetContext()->GetSwapChainSize(); i++)
				mainBuffers.push_back(std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(),
						std::max(512 * 1024u, size),
						vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer)));
		}
		else if (mainBuffers[GetCurrentImage()]->bufferSize < size)
		{
			u32 newSize = mainBuffers[GetCurrentImage()]->bufferSize;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing main buffer size %d -> %d", (u32)mainBuffers[GetCurrentImage()]->bufferSize, newSize);
			mainBuffers[GetCurrentImage()] = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(), newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer));
		}
		return mainBuffers[GetCurrentImage()].get();
	};

private:
	int GetCurrentImage() { return GetContext()->GetCurrentImageIndex(); }

	std::vector<DescriptorSets> descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
	std::unique_ptr<PipelineManager> screenPipelineManager;
};

class TextureDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, Allocator *allocator, RttPipelineManager *pipelineManager, TextureCache *textureCache)
	{
		Drawer::Init(samplerManager, pipelineManager);

		descriptorSets.Init(samplerManager, pipelineManager->GetPipelineLayout(), pipelineManager->GetPerFrameDSLayout(), pipelineManager->GetPerPolyDSLayout());
		fence = GetContext()->GetDevice().createFenceUnique(vk::FenceCreateInfo());
		this->allocator = allocator;
		this->textureCache = textureCache;
	}
	void SetCommandPool(CommandPool *commandPool) { this->commandPool = commandPool; }

	TextureDrawer() = default;
	TextureDrawer(const TextureDrawer& other) = delete;
	TextureDrawer(TextureDrawer&& other) = default;
	TextureDrawer& operator=(const TextureDrawer& other) = delete;
	TextureDrawer& operator=(TextureDrawer&& other) = default;
	virtual void EndRenderPass() override;

protected:
	virtual vk::CommandBuffer BeginRenderPass() override;
	DescriptorSets& GetCurrentDescSet() override { return descriptorSets; }

	virtual BufferData* GetMainBuffer(u32 size) override
	{
		if (!mainBuffer || mainBuffer->bufferSize < size)
		{
			u32 newSize = mainBuffer ? mainBuffer->bufferSize : 128 * 1024u;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing RTT main buffer size %d -> %d", !mainBuffer ? 0 : (u32)mainBuffer->bufferSize, newSize);
			mainBuffer = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(), newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer,
					allocator));
		}
		return mainBuffer.get();
	}

private:
	u32 width = 0;
	u32 height = 0;
	u32 textureAddr = 0;

	Texture *texture = nullptr;
	vk::Image colorImage;
	vk::CommandBuffer currentCommandBuffer;
	vk::UniqueFramebuffer framebuffer;
	std::unique_ptr<FramebufferAttachment> colorAttachment;
	std::unique_ptr<FramebufferAttachment> depthAttachment;
	vk::UniqueFence fence;

	DescriptorSets descriptorSets;
	std::unique_ptr<BufferData> mainBuffer;
	CommandPool *commandPool = nullptr;
	Allocator *allocator = nullptr;
	TextureCache *textureCache = nullptr;
};
