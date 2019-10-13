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
#include "vulkan.h"
#include "buffer.h"
#include "commandpool.h"
#include "pipeline.h"
#include "shaders.h"
#include "texture.h"

enum class TileClipping {
	Inside,		// render stuff outside the region
	Off,
	Outside		// render stuff inside the region
};

class Drawer
{
public:
	Drawer() = default;
	virtual ~Drawer() {}
	bool Draw(const Texture *fogTexture);
	Drawer(const Drawer& other) = delete;
	Drawer(Drawer&& other) = default;
	Drawer& operator=(const Drawer& other) = delete;
	Drawer& operator=(Drawer&& other) = default;

protected:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager)
	{
		this->samplerManager = samplerManager;
		pipelineManager->Init(shaderManager);
	}
	virtual DescriptorSets& GetCurrentDescSet() = 0;
	virtual BufferData *GetMainBuffer(u32 size) = 0;
	virtual vk::CommandBuffer BeginRenderPass() = 0;
	virtual void EndRenderPass() = 0;

	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	std::unique_ptr<PipelineManager> pipelineManager;
	vk::Rect2D baseScissor;
	// temp stuff
	float scale_x = 1.f;
	float scale_y = 1.f;

private:
	TileClipping SetTileClip(u32 val, vk::Rect2D& clipRect);
	void SortTriangles();
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
	void DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortTrigDrawParam>& polys);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const List<PolyParam>& polys, u32 first, u32 count);
	void DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count);
	void UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms, u32& vertexUniformsOffset);
	void SetScissor(const vk::CommandBuffer& cmdBuffer, vk::Rect2D scissor)
	{
		if (scissor != currentScissor)
		{
			cmdBuffer.setScissor(0, scissor);
			currentScissor = scissor;
		}
	}

	// Per-triangle sort results
	std::vector<std::vector<SortTrigDrawParam>> sortedPolys;
	std::vector<std::vector<u32>> sortedIndexes;
	u32 sortedIndexCount = 0;

	SamplerManager *samplerManager = nullptr;
	vk::Rect2D currentScissor;
};

class ScreenDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager)
	{
		if (!pipelineManager)
			pipelineManager = std::unique_ptr<PipelineManager>(new PipelineManager());
		Drawer::Init(samplerManager, shaderManager);

		if (descriptorSets.size() > GetContext()->GetSwapChainSize())
			descriptorSets.resize(GetContext()->GetSwapChainSize());
		else
			while (descriptorSets.size() < GetContext()->GetSwapChainSize())
			{
				descriptorSets.push_back(DescriptorSets());
				descriptorSets.back().Init(samplerManager, pipelineManager->GetPipelineLayout(), pipelineManager->GetPerFrameDSLayout(), pipelineManager->GetPerPolyDSLayout());
			}
	}
	ScreenDrawer() = default;
	ScreenDrawer(const ScreenDrawer& other) = delete;
	ScreenDrawer(ScreenDrawer&& other) = default;
	ScreenDrawer& operator=(const ScreenDrawer& other) = delete;
	ScreenDrawer& operator=(ScreenDrawer&& other) = default;

protected:
	virtual DescriptorSets& GetCurrentDescSet() override { return descriptorSets[GetCurrentImage()]; }
	virtual BufferData* GetMainBuffer(u32 size) override
	{
		if (mainBuffers.empty())
		{
			for (int i = 0; i < GetContext()->GetSwapChainSize(); i++)
				mainBuffers.push_back(std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice().get(),
						std::max(512 * 1024u, size),
						vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer)));
		}
		else if (mainBuffers[GetCurrentImage()]->bufferSize < size)
		{
			u32 newSize = mainBuffers[GetCurrentImage()]->bufferSize;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing main buffer size %d -> %d", (u32)mainBuffers[GetCurrentImage()]->bufferSize, newSize);
			mainBuffers[GetCurrentImage()] = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice().get(), newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer));
		}
		return mainBuffers[GetCurrentImage()].get();
	};

	virtual vk::CommandBuffer BeginRenderPass() override;

	virtual void EndRenderPass() override
	{
		GetContext()->EndFrame();
	}

private:
	int GetCurrentImage() { return GetContext()->GetCurrentImageIndex(); }

	std::vector<DescriptorSets> descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
};

class TextureDrawer : public Drawer
{
public:
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager, VulkanAllocator *texAllocator)
	{
		pipelineManager = std::unique_ptr<RttPipelineManager>(new RttPipelineManager());
		Drawer::Init(samplerManager, shaderManager);

		descriptorSets.Init(samplerManager, pipelineManager->GetPipelineLayout(), pipelineManager->GetPerFrameDSLayout(), pipelineManager->GetPerPolyDSLayout());
		fence = GetContext()->GetDevice()->createFenceUnique(vk::FenceCreateInfo());
		this->texAllocator = texAllocator;
	}
	void SetCommandPool(CommandPool *commandPool) { this->commandPool = commandPool; }

	TextureDrawer() = default;
	TextureDrawer(const TextureDrawer& other) = delete;
	TextureDrawer(TextureDrawer&& other) = default;
	TextureDrawer& operator=(const TextureDrawer& other) = delete;
	TextureDrawer& operator=(TextureDrawer&& other) = default;

protected:
	virtual vk::CommandBuffer BeginRenderPass() override;
	virtual void EndRenderPass() override;
	DescriptorSets& GetCurrentDescSet() override { return descriptorSets; }

	virtual BufferData* GetMainBuffer(u32 size) override
	{
		if (!mainBuffer || mainBuffer->bufferSize < size)
		{
			u32 newSize = mainBuffer ? mainBuffer->bufferSize : 128 * 1024u;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing RTT main buffer size %d -> %d", !mainBuffer ? 0 : (u32)mainBuffer->bufferSize, newSize);
			mainBuffer = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), *GetContext()->GetDevice(), newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer));
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
	VulkanAllocator *texAllocator = nullptr;
};
