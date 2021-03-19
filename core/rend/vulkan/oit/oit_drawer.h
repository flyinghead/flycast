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
#include "rend/sorter.h"
#include "rend/transform_matrix.h"
#include "../vulkan.h"
#include "../buffer.h"
#include "../commandpool.h"
#include "oit_pipeline.h"
#include "oit_shaders.h"
#include "../texture.h"
#include "../quad.h"
#include "oit_buffer.h"
#include "../drawer.h"

#include <array>
#include <memory>
#include <vector>

class OITDrawer : public BaseDrawer
{
public:
	virtual ~OITDrawer() = default;
	bool Draw(const Texture *fogTexture, const Texture *paletteTexture);

	virtual vk::CommandBuffer NewFrame() = 0;
	virtual void EndFrame() {  renderPass++; };

protected:
	void Init(SamplerManager *samplerManager, OITPipelineManager *pipelineManager, OITBuffers *oitBuffers)
	{
		this->pipelineManager = pipelineManager;
		this->samplerManager = samplerManager;
		if (!quadBuffer)
			quadBuffer = std::unique_ptr<QuadBuffer>(new QuadBuffer());
		this->oitBuffers = oitBuffers;
		if (descriptorSets.size() > GetContext()->GetSwapChainSize())
			descriptorSets.resize(GetContext()->GetSwapChainSize());
		else
			while (descriptorSets.size() < GetContext()->GetSwapChainSize())
			{
				descriptorSets.emplace_back();
				descriptorSets.back().Init(samplerManager,
						pipelineManager->GetPipelineLayout(),
						pipelineManager->GetPerFrameDSLayout(),
						pipelineManager->GetPerPolyDSLayout(),
						pipelineManager->GetColorInputDSLayout());
			}
	}
	void Term()
	{
		quadBuffer.reset();
		colorAttachments[0].reset();
		colorAttachments[1].reset();
		tempFramebuffers[0].reset();
		tempFramebuffers[1].reset();
		depthAttachment.reset();
		mainBuffers.clear();
		descriptorSets.clear();
	}

	int GetCurrentImage() const { return imageIndex; }

	void NewImage()
	{
		GetCurrentDescSet().Reset();
		imageIndex = (imageIndex + 1) % GetContext()->GetSwapChainSize();
		renderPass = 0;
	}

	OITDescriptorSets& GetCurrentDescSet() { return descriptorSets[GetCurrentImage()]; }

	BufferData* GetMainBuffer(u32 size)
	{
		u32 bufferIndex = imageIndex + renderPass * GetContext()->GetSwapChainSize();
		while (mainBuffers.size() <= bufferIndex)
		{
			mainBuffers.push_back(std::unique_ptr<BufferData>(new BufferData(std::max(512 * 1024u, size),
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer
					| vk::BufferUsageFlagBits::eStorageBuffer)));
		}
		if (mainBuffers[bufferIndex]->bufferSize < size)
		{
			u32 newSize = mainBuffers[bufferIndex]->bufferSize;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing main buffer size %d -> %d", (u32)mainBuffers[bufferIndex]->bufferSize, newSize);
			mainBuffers[bufferIndex] = std::unique_ptr<BufferData>(new BufferData(newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer
					| vk::BufferUsageFlagBits::eStorageBuffer));
		}
		return mainBuffers[bufferIndex].get();
	};

	void MakeBuffers(int width, int height);
	virtual vk::Format GetColorFormat() const = 0;
	virtual vk::Framebuffer GetFinalFramebuffer() const = 0;

	vk::Rect2D viewport;
	std::array<std::unique_ptr<FramebufferAttachment>, 2> colorAttachments;
	std::unique_ptr<FramebufferAttachment> depthAttachment;
	vk::CommandBuffer currentCommandBuffer;
	std::vector<bool> clearNeeded;

private:
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool autosort, Pass pass,
			const PolyParam& poly, u32 first, u32 count);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, Pass pass,
			const List<PolyParam>& polys, u32 first, u32 last);
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
		vk::DeviceSize polyParamsSize = 0;
	} offsets;

	std::unique_ptr<QuadBuffer> quadBuffer;

	std::array<vk::UniqueFramebuffer, 2> tempFramebuffers;

	OITPipelineManager *pipelineManager = nullptr;
	SamplerManager *samplerManager = nullptr;
	OITBuffers *oitBuffers = nullptr;
	int maxWidth = 0;
	int maxHeight = 0;
	bool needDepthTransition = false;
	int imageIndex = 0;
	int renderPass = 0;
	std::vector<OITDescriptorSets> descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
};

class OITScreenDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, OITShaderManager *shaderManager, OITBuffers *oitBuffers,
			const vk::Extent2D& viewport)
	{
		if (!screenPipelineManager)
			screenPipelineManager = std::unique_ptr<OITPipelineManager>(new OITPipelineManager());
		screenPipelineManager->Init(shaderManager, oitBuffers);
		OITDrawer::Init(samplerManager, screenPipelineManager.get(), oitBuffers);

		MakeFramebuffers(viewport);
		GetContext()->PresentFrame(vk::ImageView(), viewport);
	}
	void Term()
	{
		screenPipelineManager.reset();
		framebuffers.clear();
		finalColorAttachments.clear();
		OITDrawer::Term();
	}

	vk::CommandBuffer NewFrame() override;
	void EndFrame() override
	{
		currentCommandBuffer.endRenderPass();
		currentCommandBuffer.end();
		currentCommandBuffer = nullptr;
		commandPool->EndFrame();
		OITDrawer::EndFrame();
		frameRendered = true;
	}

	bool PresentFrame()
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
		GetContext()->PresentFrame(finalColorAttachments[GetCurrentImage()]->GetImageView(), viewport.extent);
		NewImage();

		return true;
	}

protected:
	vk::Framebuffer GetFinalFramebuffer() const override { return *framebuffers[GetCurrentImage()]; }
	vk::Format GetColorFormat() const override { return GetContext()->GetColorFormat(); }

private:
	void MakeFramebuffers(const vk::Extent2D& viewport);

	std::vector<std::unique_ptr<FramebufferAttachment>> finalColorAttachments;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::unique_ptr<OITPipelineManager> screenPipelineManager;
	std::vector<bool> transitionNeeded;
	bool frameRendered = false;
};

class OITTextureDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, OITShaderManager *shaderManager,
			TextureCache *textureCache, OITBuffers *oitBuffers)
	{
		if (!rttPipelineManager)
			rttPipelineManager = std::unique_ptr<RttOITPipelineManager>(new RttOITPipelineManager());
		rttPipelineManager->Init(shaderManager, oitBuffers);
		OITDrawer::Init(samplerManager, rttPipelineManager.get(), oitBuffers);

		this->textureCache = textureCache;
	}
	void Term()
	{
		colorAttachment.reset();
		framebuffers.clear();
		rttPipelineManager.reset();
		OITDrawer::Term();
	}

	void EndFrame() override;

protected:
	vk::CommandBuffer NewFrame() override;
	vk::Framebuffer GetFinalFramebuffer() const override { return *framebuffers[GetCurrentImage()]; }
	vk::Format GetColorFormat() const override { return vk::Format::eR8G8B8A8Unorm; }

private:
	u32 textureAddr = 0;

	Texture *texture = nullptr;
	vk::Image colorImage;
	std::unique_ptr<FramebufferAttachment> colorAttachment;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::unique_ptr<RttOITPipelineManager> rttPipelineManager;

	TextureCache *textureCache = nullptr;
};
