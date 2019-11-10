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
#include <memory>
#include "rend/sorter.h"
#include "rend/transform_matrix.h"
#include "vulkan.h"
#include "buffer.h"
#include "commandpool.h"
#include "oit_pipeline.h"
#include "oit_shaders.h"
#include "texture.h"
#include "quad.h"

class OITDrawer
{
public:
	OITDrawer() = default;
	virtual ~OITDrawer() = default;
	bool Draw(const Texture *fogTexture);
	OITDrawer(const OITDrawer& other) = delete;
	OITDrawer(OITDrawer&& other) = default;
	OITDrawer& operator=(const OITDrawer& other) = delete;
	OITDrawer& operator=(OITDrawer&& other) = default;

	virtual vk::CommandBuffer BeginRenderPass() = 0;
	virtual void EndRenderPass() = 0;
	void SetScissor(const vk::CommandBuffer& cmdBuffer, vk::Rect2D scissor)
	{
		if (scissor != currentScissor)
		{
			cmdBuffer.setScissor(0, scissor);
			currentScissor = scissor;
		}
	}

protected:
	void Init(SamplerManager *samplerManager, Allocator *allocator, OITPipelineManager *pipelineManager)
	{
		this->pipelineManager = pipelineManager;
		this->allocator = allocator;
		this->samplerManager = samplerManager;
		if (!quadBuffer)
			quadBuffer = std::unique_ptr<QuadBuffer>(new QuadBuffer(allocator));
		vk::Extent2D viewport = GetContext()->GetViewPort();
		MakeAttachments(viewport.width, viewport.height);
		MakeFramebuffers(viewport.width, viewport.height);
		MakeBuffers(viewport.width, viewport.height);
	}
	virtual OITDescriptorSets& GetCurrentDescSet() = 0;
	virtual BufferData *GetMainBuffer(u32 size) = 0;

	VulkanContext *GetContext() const { return VulkanContext::Instance(); }

	OITPipelineManager *pipelineManager = nullptr;
	vk::Rect2D baseScissor;
	TransformMatrix<false> matrices;
	Allocator *allocator = nullptr;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::unique_ptr<FramebufferAttachment> abufferPointerAttachment;
	bool abufferPointerTransitionNeeded = false;

private:
	TileClipping SetTileClip(u32 val, vk::Rect2D& clipRect);
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, int pass,
			const PolyParam& poly, u32 first, u32 count);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, int pass,
			const List<PolyParam>& polys, u32 first, u32 count);
	template<bool Translucent>
	void DrawModifierVolumes(const vk::CommandBuffer& cmdBuffer, int first, int count);
	void UploadMainBuffer(const OITDescriptorSets::VertexShaderUniforms& vertexUniforms,
			const OITDescriptorSets::FragmentShaderUniforms& fragmentUniforms);
	u32 align(vk::DeviceSize offset, u32 alignment)
	{
		return (u32)(alignment - (offset & (alignment - 1)));
	}
	void MakeAttachments(int width, int height);
	void MakeFramebuffers(int width, int height);
	void MakeBuffers(int width, int height);

	struct {
		vk::DeviceSize indexOffset = 0;
		vk::DeviceSize modVolOffset = 0;
		vk::DeviceSize vertexUniformOffset = 0;
		vk::DeviceSize fragmentUniformOffset = 0;
		vk::DeviceSize polyParamsOffset = 0;
	} offsets;

	std::unique_ptr<QuadBuffer> quadBuffer;

	vk::ImageView colorImageView;
	vk::ImageView depthImageView;
	vk::ImageView stencilImageView;

	std::unique_ptr<FramebufferAttachment> colorAttachment;
	std::unique_ptr<FramebufferAttachment> depthAttachment;
	std::unique_ptr<FramebufferAttachment> depth2Attachment;

	SamplerManager *samplerManager = nullptr;
	vk::Rect2D currentScissor;
	std::unique_ptr<BufferData> pixelBuffer;
protected: //FIXME
	std::unique_ptr<BufferData> pixelCounter;
	std::unique_ptr<BufferData> pixelCounterReset;
};

class OITScreenDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, Allocator *allocator, OITShaderManager *shaderManager)
	{
		if (!screenPipelineManager)
			screenPipelineManager = std::unique_ptr<OITPipelineManager>(new OITPipelineManager());
		screenPipelineManager->Init(shaderManager);
		OITDrawer::Init(samplerManager, allocator, screenPipelineManager.get());

		if (descriptorSets.size() > GetContext()->GetSwapChainSize())
			descriptorSets.resize(GetContext()->GetSwapChainSize());
		else
			while (descriptorSets.size() < GetContext()->GetSwapChainSize())
			{
				descriptorSets.push_back(OITDescriptorSets());
				descriptorSets.back().Init(samplerManager,
						screenPipelineManager->GetPipelineLayout(),
						screenPipelineManager->GetPerFrameDSLayout(),
						screenPipelineManager->GetPerPolyDSLayout(),
						screenPipelineManager->GetPass1DSLayout(),
						screenPipelineManager->GetPass2DSLayout());
			}
	}
	OITScreenDrawer() = default;
	OITScreenDrawer(const OITScreenDrawer& other) = delete;
	OITScreenDrawer(OITScreenDrawer&& other) = default;
	OITScreenDrawer& operator=(const OITScreenDrawer& other) = delete;
	OITScreenDrawer& operator=(OITScreenDrawer&& other) = default;

	virtual vk::CommandBuffer BeginRenderPass() override;
	virtual void EndRenderPass() override
	{
		GetContext()->EndFrame();
	}

protected:
	virtual OITDescriptorSets& GetCurrentDescSet() override { return descriptorSets[GetCurrentImage()]; }
	virtual BufferData* GetMainBuffer(u32 size) override
	{
		if (mainBuffers.empty())
		{
			for (int i = 0; i < GetContext()->GetSwapChainSize(); i++)
				mainBuffers.push_back(std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(),
						std::max(512 * 1024u, size),
						vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer
						| vk::BufferUsageFlagBits::eStorageBuffer)));
		}
		else if (mainBuffers[GetCurrentImage()]->bufferSize < size)
		{
			u32 newSize = mainBuffers[GetCurrentImage()]->bufferSize;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing main buffer size %d -> %d", (u32)mainBuffers[GetCurrentImage()]->bufferSize, newSize);
			mainBuffers[GetCurrentImage()] = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(), newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer
					| vk::BufferUsageFlagBits::eStorageBuffer));
		}
		return mainBuffers[GetCurrentImage()].get();
	};

private:
	int GetCurrentImage() { return GetContext()->GetCurrentImageIndex(); }

	std::vector<OITDescriptorSets> descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
	std::unique_ptr<OITPipelineManager> screenPipelineManager;
};

class OITTextureDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, Allocator *allocator, RttOITPipelineManager *pipelineManager, TextureCache *textureCache)
	{
		OITDrawer::Init(samplerManager, allocator, pipelineManager);

		descriptorSets.Init(samplerManager,
				pipelineManager->GetPipelineLayout(),
				pipelineManager->GetPerFrameDSLayout(),
				pipelineManager->GetPerPolyDSLayout(),
				pipelineManager->GetPass1DSLayout(),
				pipelineManager->GetPass2DSLayout());
		fence = GetContext()->GetDevice().createFenceUnique(vk::FenceCreateInfo());
		this->textureCache = textureCache;
	}
	void SetCommandPool(CommandPool *commandPool) { this->commandPool = commandPool; }

	OITTextureDrawer() = default;
	OITTextureDrawer(const OITTextureDrawer& other) = delete;
	OITTextureDrawer(OITTextureDrawer&& other) = default;
	OITTextureDrawer& operator=(const OITTextureDrawer& other) = delete;
	OITTextureDrawer& operator=(OITTextureDrawer&& other) = default;
	virtual void EndRenderPass() override;

protected:
	virtual vk::CommandBuffer BeginRenderPass() override;
	OITDescriptorSets& GetCurrentDescSet() override { return descriptorSets; }

	virtual BufferData* GetMainBuffer(u32 size) override
	{
		if (!mainBuffer || mainBuffer->bufferSize < size)
		{
			u32 newSize = mainBuffer ? mainBuffer->bufferSize : 128 * 1024u;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing RTT main buffer size %d -> %d", !mainBuffer ? 0 : (u32)mainBuffer->bufferSize, newSize);
			mainBuffer = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice(), newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer
					| vk::BufferUsageFlagBits::eStorageBuffer, allocator));
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

	OITDescriptorSets descriptorSets;
	std::unique_ptr<BufferData> mainBuffer;
	CommandPool *commandPool = nullptr;
	TextureCache *textureCache = nullptr;
};
