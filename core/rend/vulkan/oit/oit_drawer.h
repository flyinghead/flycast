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
	virtual void EndFrame() = 0;

protected:
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

	void NewImage() {
		descriptorSets.nextFrame();
	}

	BufferData* GetMainBuffer(u32 size) {
		return BaseDrawer::GetMainBuffer(size, vk::BufferUsageFlagBits::eStorageBuffer);
	}

	void MakeBuffers(int width, int height, vk::ImageUsageFlags colorUsage = {});
	virtual vk::Framebuffer getFramebuffer(int renderPass, int renderPassCount) = 0;
	virtual int getFramebufferIndex() { return 0; }

	vk::Rect2D viewport;
	std::array<std::unique_ptr<FramebufferAttachment>, 2> colorAttachments;
	std::array<std::unique_ptr<FramebufferAttachment>, 2> depthAttachments;
	std::array<vk::UniqueFramebuffer, 2> tempFramebuffers;
	vk::CommandBuffer currentCommandBuffer;
	std::vector<bool> clearNeeded;
	int maxWidth = 0;
	int maxHeight = 0;

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

	OITPipelineManager *pipelineManager = nullptr;
	SamplerManager *samplerManager = nullptr;
	OITBuffers *oitBuffers = nullptr;
	bool needAttachmentTransition = false;
	bool needDepthTransition = false;
	OITDescriptorSets descriptorSets;
	vk::Buffer curMainBuffer;
	bool dithering = false;
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
		OITDrawer::Term();
	}

	vk::CommandBuffer NewFrame() override;

	void EndFrame() override
	{
		if (!frameStarted)
			return;
		frameStarted = false;
		if (config::EmulateFramebuffer) {
			scaleAndWriteFramebuffer(currentCommandBuffer, colorAttachments[framebufferIndex].get());
		}
		else
		{
			currentCommandBuffer.end();
			commandPool->EndFrame();
			aspectRatio = getOutputFramebufferAspectRatio();
		}
		currentCommandBuffer = nullptr;
		frameRendered = true;
	}

	bool PresentFrame()
	{
		EndFrame();
		if (!frameRendered)
			return false;
		frameRendered = false;
		GetContext()->PresentFrame(colorAttachments[framebufferIndex]->GetImage(),
				colorAttachments[framebufferIndex]->GetImageView(), viewport.extent, aspectRatio);

		return true;
	}
	vk::RenderPass GetRenderPass() const { return screenPipelineManager->GetRenderPass(false, true); }
	vk::CommandBuffer GetCurrentCommandBuffer() const { return currentCommandBuffer; }

protected:
	vk::Framebuffer getFramebuffer(int renderPass, int renderPassCount) override;
	int getFramebufferIndex() override { return framebufferIndex; }

private:
	void MakeFramebuffers(const vk::Extent2D& viewport);

	std::unique_ptr<OITPipelineManager> screenPipelineManager;
	bool frameRendered = false;
	float aspectRatio = 0.f;
	bool frameStarted = false;
	int framebufferIndex = 0;
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
		framebuffer.reset();
		colorAttachment.reset();
		rttPipelineManager.reset();
		OITDrawer::Term();
	}

	void EndFrame() override;

protected:
	vk::CommandBuffer NewFrame() override;
	vk::Framebuffer getFramebuffer(int renderPass, int renderPassCount) override;

private:
	u32 textureAddr = 0;

	Texture *texture = nullptr;
	vk::Image colorImage;
	std::unique_ptr<FramebufferAttachment> colorAttachment;
	std::unique_ptr<RttOITPipelineManager> rttPipelineManager;
	vk::UniqueFramebuffer framebuffer;

	TextureCache *textureCache = nullptr;
};
