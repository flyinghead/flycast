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
#include "oit_buffer.h"
#include "drawer.h"

class OITDrawer : public BaseDrawer
{
public:
	OITDrawer() = default;
	virtual ~OITDrawer() = default;
	bool Draw(const Texture *fogTexture);
	OITDrawer(const OITDrawer& other) = delete;
	OITDrawer(OITDrawer&& other) = default;
	OITDrawer& operator=(const OITDrawer& other) = delete;
	OITDrawer& operator=(OITDrawer&& other) = default;

	virtual vk::CommandBuffer NewFrame() = 0;
	virtual void EndFrame() = 0;

protected:
	void Init(SamplerManager *samplerManager, Allocator *allocator, OITPipelineManager *pipelineManager, OITBuffers *oitBuffers)
	{
		this->pipelineManager = pipelineManager;
		this->allocator = allocator;
		this->samplerManager = samplerManager;
		if (!quadBuffer)
			quadBuffer = std::unique_ptr<QuadBuffer>(new QuadBuffer(allocator));
		this->oitBuffers = oitBuffers;
	}
	void Term()
	{
		quadBuffer.reset();
		colorAttachments[0].reset();
		colorAttachments[1].reset();
		depthAttachment.reset();
	}
	virtual OITDescriptorSets& GetCurrentDescSet() = 0;
	virtual BufferData *GetMainBuffer(u32 size) = 0;
	virtual vk::Framebuffer GetCurrentFramebuffer() const = 0;
	void MakeBuffers(int width, int height);
	virtual vk::Format GetColorFormat() const = 0;

	OITPipelineManager *pipelineManager = nullptr;
	vk::Rect2D viewport;
	Allocator *allocator = nullptr;
	std::array<std::unique_ptr<FramebufferAttachment>, 2> colorAttachments;
	std::unique_ptr<FramebufferAttachment> depthAttachment;

private:
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, int pass,
			const PolyParam& poly, u32 first, u32 count);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, int pass,
			const List<PolyParam>& polys, u32 first, u32 count);
	template<bool Translucent>
	void DrawModifierVolumes(const vk::CommandBuffer& cmdBuffer, int first, int count);
	void UploadMainBuffer(const OITDescriptorSets::VertexShaderUniforms& vertexUniforms,
			const OITDescriptorSets::FragmentShaderUniforms& fragmentUniforms);

	struct {
		vk::DeviceSize indexOffset = 0;
		vk::DeviceSize modVolOffset = 0;
		vk::DeviceSize vertexUniformOffset = 0;
		vk::DeviceSize fragmentUniformOffset = 0;
		vk::DeviceSize polyParamsOffset = 0;
	} offsets;

	std::unique_ptr<QuadBuffer> quadBuffer;

	std::array<vk::UniqueFramebuffer, 2> tempFramebuffers;

	SamplerManager *samplerManager = nullptr;
	OITBuffers *oitBuffers = nullptr;
	int maxWidth = 0;
	int maxHeight = 0;
};

class OITScreenDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, Allocator *allocator, OITShaderManager *shaderManager, OITBuffers *oitBuffers)
	{
		if (!screenPipelineManager)
			screenPipelineManager = std::unique_ptr<OITPipelineManager>(new OITPipelineManager());
		screenPipelineManager->Init(shaderManager, oitBuffers);
		OITDrawer::Init(samplerManager, allocator, screenPipelineManager.get(), oitBuffers);

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
						screenPipelineManager->GetColorInputDSLayout());
			}
		vk::Extent2D viewport = GetContext()->GetViewPort();
		MakeFramebuffers(viewport.width, viewport.height);
	}
	void Term()
	{
		mainBuffers.clear();
		screenPipelineManager.reset();
		framebuffers.clear();
		descriptorSets.clear();
		OITDrawer::Term();
	}
	OITScreenDrawer() = default;
	OITScreenDrawer(const OITScreenDrawer& other) = delete;
	OITScreenDrawer(OITScreenDrawer&& other) = default;
	OITScreenDrawer& operator=(const OITScreenDrawer& other) = delete;
	OITScreenDrawer& operator=(OITScreenDrawer&& other) = default;

	virtual vk::CommandBuffer NewFrame() override;
	virtual void EndFrame() override
	{
		GetContext()->EndFrame();
	}
	vk::RenderPass GetRenderPass() { return screenPipelineManager->GetRenderPass(true, true); }

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
	virtual vk::Framebuffer GetCurrentFramebuffer() const override { return *framebuffers[GetCurrentImage()]; }
	virtual vk::Format GetColorFormat() const override { return GetContext()->GetColorFormat(); }

private:
	int GetCurrentImage() const { return GetContext()->GetCurrentImageIndex(); }
	void MakeFramebuffers(int width, int height);

	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::vector<OITDescriptorSets> descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
	std::unique_ptr<OITPipelineManager> screenPipelineManager;
};

class OITTextureDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, Allocator *allocator, RttOITPipelineManager *pipelineManager,
			TextureCache *textureCache, OITBuffers *oitBuffers)
	{
		OITDrawer::Init(samplerManager, allocator, pipelineManager, oitBuffers);

		descriptorSets.Init(samplerManager,
				pipelineManager->GetPipelineLayout(),
				pipelineManager->GetPerFrameDSLayout(),
				pipelineManager->GetPerPolyDSLayout(),
				pipelineManager->GetColorInputDSLayout());
		fence = GetContext()->GetDevice().createFenceUnique(vk::FenceCreateInfo());
		this->textureCache = textureCache;
	}
	void Term()
	{
		mainBuffer.reset();
		colorAttachment.reset();
		framebuffer.reset();
		fence.reset();
		OITDrawer::Term();
	}

	void SetCommandPool(CommandPool *commandPool) { this->commandPool = commandPool; }

	OITTextureDrawer() = default;
	OITTextureDrawer(const OITTextureDrawer& other) = delete;
	OITTextureDrawer(OITTextureDrawer&& other) = default;
	OITTextureDrawer& operator=(const OITTextureDrawer& other) = delete;
	OITTextureDrawer& operator=(OITTextureDrawer&& other) = default;
	virtual void EndFrame() override;

protected:
	virtual vk::CommandBuffer NewFrame() override;
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
	virtual vk::Framebuffer GetCurrentFramebuffer() const override { return *framebuffer; }
	virtual vk::Format GetColorFormat() const override { return vk::Format::eR8G8B8A8Unorm; }

private:
	u32 textureAddr = 0;

	Texture *texture = nullptr;
	vk::Image colorImage;
	vk::CommandBuffer currentCommandBuffer;
	vk::UniqueFramebuffer framebuffer;
	std::unique_ptr<FramebufferAttachment> colorAttachment;
	vk::UniqueFence fence;

	OITDescriptorSets descriptorSets;
	std::unique_ptr<BufferData> mainBuffer;
	CommandPool *commandPool = nullptr;
	TextureCache *textureCache = nullptr;
};
