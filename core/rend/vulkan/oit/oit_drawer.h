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
	u32 GetSwapChainSize() { return 2; }
	void Init(SamplerManager *samplerManager, OITPipelineManager *pipelineManager, OITBuffers *oitBuffers)
	{
		this->pipelineManager = pipelineManager;
		this->samplerManager = samplerManager;
		if (!quadBuffer)
			quadBuffer = std::make_unique<QuadBuffer>();
		this->oitBuffers = oitBuffers;
		descriptorSets.init(samplerManager,
				pipelineManager->GetPipelineLayout(),
				pipelineManager->GetPerFrameDSLayout(),
				pipelineManager->GetPerPolyDSLayout(),
				pipelineManager->GetColorInputDSLayout());
	}
	void Term()
	{
		quadBuffer.reset();
		colorAttachments[0].reset();
		colorAttachments[1].reset();
		tempFramebuffers[0].reset();
		tempFramebuffers[1].reset();
		depthAttachments[0].reset();
		depthAttachments[1].reset();
		mainBuffers.clear();
		descriptorSets.term();
		maxWidth = 0;
		maxHeight = 0;
	}

	int GetCurrentImage() const { return imageIndex; }

	void NewImage()
	{
		descriptorSets.nextFrame();
		imageIndex = (imageIndex + 1) % GetSwapChainSize();
		renderPass = 0;
	}

	BufferData* GetMainBuffer(u32 size)
	{
		u32 bufferIndex = imageIndex + renderPass * GetSwapChainSize();
		while (mainBuffers.size() <= bufferIndex)
		{
			mainBuffers.push_back(std::make_unique<BufferData>(std::max(512 * 1024u, size),
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer
					| vk::BufferUsageFlagBits::eStorageBuffer));
		}
		if (mainBuffers[bufferIndex]->bufferSize < size)
		{
			u32 newSize = (u32)mainBuffers[bufferIndex]->bufferSize;
			while (newSize < size)
				newSize *= 2;
			INFO_LOG(RENDERER, "Increasing main buffer size %d -> %d", (u32)mainBuffers[bufferIndex]->bufferSize, newSize);
			mainBuffers[bufferIndex] = std::make_unique<BufferData>(newSize,
					vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer
					| vk::BufferUsageFlagBits::eStorageBuffer);
		}
		return mainBuffers[bufferIndex].get();
	}

	void MakeBuffers(int width, int height);
	virtual vk::Framebuffer GetFinalFramebuffer() const = 0;

	vk::Rect2D viewport;
	std::array<std::unique_ptr<FramebufferAttachment>, 2> colorAttachments;
	std::array<std::unique_ptr<FramebufferAttachment>, 2> depthAttachments;
	vk::CommandBuffer currentCommandBuffer;
	std::vector<bool> clearNeeded;

private:
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool autosort, Pass pass,
			const PolyParam& poly, u32 first, u32 count);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, Pass pass,
			const std::vector<PolyParam>& polys, u32 first, u32 last);
	template<bool Translucent>
	void DrawModifierVolumes(const vk::CommandBuffer& cmdBuffer, int first, int count, const ModifierVolumeParam *modVolParams);
	void UploadMainBuffer(const OITDescriptorSets::VertexShaderUniforms& vertexUniforms,
			const OITDescriptorSets::FragmentShaderUniforms& fragmentUniforms);

	struct {
		vk::DeviceSize indexOffset = 0;
		vk::DeviceSize modVolOffset = 0;
		vk::DeviceSize vertexUniformOffset = 0;
		vk::DeviceSize fragmentUniformOffset = 0;
		vk::DeviceSize polyParamsOffset = 0;
		vk::DeviceSize polyParamsSize = 0;
		vk::DeviceSize naomi2OpaqueOffset = 0;
		vk::DeviceSize naomi2PunchThroughOffset = 0;
		vk::DeviceSize naomi2TranslucentOffset = 0;
		vk::DeviceSize naomi2ModVolOffset = 0;
		vk::DeviceSize naomi2TrModVolOffset = 0;
		vk::DeviceSize lightsOffset = 0;
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
	OITDescriptorSets descriptorSets;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
};

class OITScreenDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, OITShaderManager *shaderManager, OITBuffers *oitBuffers,
			const vk::Extent2D& viewport)
	{
		if (!screenPipelineManager)
			screenPipelineManager = std::make_unique<OITPipelineManager>();
		screenPipelineManager->Init(shaderManager, oitBuffers);
		OITDrawer::Init(samplerManager, screenPipelineManager.get(), oitBuffers);

		MakeFramebuffers(viewport);
		GetContext()->PresentFrame(vk::Image(), vk::ImageView(), viewport, 0);
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
		if (config::EmulateFramebuffer)
		{
			scaleAndWriteFramebuffer(currentCommandBuffer, finalColorAttachments[GetCurrentImage()].get());
		}
		else
		{
			currentCommandBuffer.end();
			commandPool->EndFrame();
			aspectRatio = getOutputFramebufferAspectRatio();
		}
		currentCommandBuffer = nullptr;
		OITDrawer::EndFrame();
		frameRendered = true;
	}

	bool PresentFrame()
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
		GetContext()->PresentFrame(finalColorAttachments[GetCurrentImage()]->GetImage(),
				finalColorAttachments[GetCurrentImage()]->GetImageView(), viewport.extent, aspectRatio);

		return true;
	}
	vk::RenderPass GetRenderPass() const { return screenPipelineManager->GetRenderPass(false, true); }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return currentCommandBuffer; }

protected:
	vk::Framebuffer GetFinalFramebuffer() const override { return *framebuffers[GetCurrentImage()]; }

private:
	void MakeFramebuffers(const vk::Extent2D& viewport);

	std::vector<std::unique_ptr<FramebufferAttachment>> finalColorAttachments;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::unique_ptr<OITPipelineManager> screenPipelineManager;
	std::vector<bool> transitionNeeded;
	bool frameRendered = false;
	float aspectRatio = 0.f;
};

class OITTextureDrawer : public OITDrawer
{
public:
	void Init(SamplerManager *samplerManager, OITShaderManager *shaderManager,
			TextureCache *textureCache, OITBuffers *oitBuffers)
	{
		if (!rttPipelineManager)
			rttPipelineManager = std::make_unique<RttOITPipelineManager>();
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

private:
	u32 textureAddr = 0;

	Texture *texture = nullptr;
	vk::Image colorImage;
	std::unique_ptr<FramebufferAttachment> colorAttachment;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	std::unique_ptr<RttOITPipelineManager> rttPipelineManager;

	TextureCache *textureCache = nullptr;
};
