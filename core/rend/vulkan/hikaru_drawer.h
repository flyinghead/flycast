/*
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
// Sega Hikaru renderer for Vulkan, independent of the PVR / Naomi 2 rendering path.
// It draws a hikaru::DrawList with its own shaders, pipelines, textures (taken from the
// Hikaru texture store) and render targets. Used by both the standard and the OIT renderers.
// The 3D picture goes into one of the persistent 3D buffers (the frame blender ends that render
// pass), then the final picture is made from the shown 3D buffer and the 2D layers.
#pragma once
#include "vulkan.h"
#include "drawer.h"
#include "desc_set.h"
#include "hikaru_shaders.h"
#include "hw/hikaru/hikaru_drawlist.h"

#include <map>
#include <memory>
#include <vector>

class HikaruPipelineManager
{
public:
	void Init(HikaruShaderManager *shaderManager, vk::RenderPass renderPass, vk::RenderPass compositePass);
	void Term()
	{
		pipelines.clear();
		blendPipeline.reset();
		compositePipeline.reset();
		pipelineLayout.reset();
		descSetLayout.reset();
		screenPipelineLayout.reset();
		screenDescSetLayout.reset();
		samplers.clear();
		screenSamplers[0].reset();
		screenSamplers[1].reset();
	}

	vk::Pipeline GetPipeline(bool textured, const hikaru::Draw& draw)
	{
		u32 h = hash(textured, draw);
		auto it = pipelines.find(h);
		if (it != pipelines.end())
			return it->second.get();
		CreatePipeline(textured, draw);
		return *pipelines[h];
	}
	// Frame blender (in the 3D render pass) or final picture (in the composite render pass)
	vk::Pipeline GetScreenPipeline(bool composite)
	{
		vk::UniquePipeline& pipeline = composite ? compositePipeline : blendPipeline;
		if (!pipeline)
			CreateScreenPipeline(composite);
		return *pipeline;
	}
	vk::Sampler GetSampler(bool linear, bool mipmapped, hikaru::Address u, hikaru::Address v);
	// point sampling, clamped or repeated
	vk::Sampler GetScreenSampler(bool repeat) const { return *screenSamplers[repeat]; }
	vk::PipelineLayout GetPipelineLayout() const { return *pipelineLayout; }
	vk::DescriptorSetLayout GetDescSetLayout() const { return *descSetLayout; }
	vk::PipelineLayout GetScreenPipelineLayout() const { return *screenPipelineLayout; }
	vk::DescriptorSetLayout GetScreenDescSetLayout() const { return *screenDescSetLayout; }

private:
	u32 hash(bool textured, const hikaru::Draw& draw) const
	{
		return (u32)textured | ((u32)draw.blend << 1) | ((u32)draw.cull << 4)
				| ((u32)draw.depthTest << 6) | ((u32)draw.depthWrite << 7);
	}
	void CreatePipeline(bool textured, const hikaru::Draw& draw);
	void CreateScreenPipeline(bool composite);

	HikaruShaderManager *shaderManager = nullptr;
	vk::RenderPass renderPass;
	vk::RenderPass compositePass;
	std::map<u32, vk::UniquePipeline> pipelines;
	vk::UniquePipeline blendPipeline;
	vk::UniquePipeline compositePipeline;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout descSetLayout;
	vk::UniquePipelineLayout screenPipelineLayout;
	vk::UniqueDescriptorSetLayout screenDescSetLayout;
	std::map<u32, vk::UniqueSampler> samplers;
	vk::UniqueSampler screenSamplers[2];
};

class HikaruDrawer : public BaseDrawer
{
public:
	void Init(CommandPool *commandPool);
	void Term();

	// Draw a frame at width x height pixels and submit it
	void Draw(const hikaru::DrawList& dl, u32 width, u32 height);
	bool PresentFrame();

private:
	struct HikaruTexture
	{
		Allocation allocation;
		vk::UniqueImage image;
		vk::UniqueImageView imageView;
		u32 width = 0;
		u32 height = 0;
		u32 levels = 0;
		u32 version = 0;
	};

	void initRenderTarget(u32 width, u32 height);
	void record(vk::CommandBuffer cmdBuffer, const hikaru::DrawList& dl);
	bool updateTexture(vk::CommandBuffer cmdBuffer, int id);
	vk::ImageView textureView(int id) const;
	void uploadTextures(vk::CommandBuffer cmdBuffer, const hikaru::DrawList& dl);
	void freeTexture(std::unique_ptr<HikaruTexture>& texture);
	void drawScreen(vk::CommandBuffer cmdBuffer, bool composite, const vk::ImageView views[3], const bool repeat[3],
			const HikaruScreenConstants& constants);
	vk::Rect2D scaleRect(int x0, int y0, int x1, int y1) const;

	HikaruShaderManager shaderManager;
	HikaruPipelineManager pipelineManager;
	DynamicDescSetAlloc descSetAlloc;
	DynamicDescSetAlloc screenDescSetAlloc;

	vk::UniqueRenderPass renderPass;		// 3D picture into a 3D buffer
	vk::UniqueRenderPass compositePass;		// final picture into an output image
	// The 3D buffers (0x781 selects the one drawn into); they persist from frame to frame
	std::vector<std::unique_ptr<FramebufferAttachment>> buffers;
	std::vector<vk::UniqueFramebuffer> bufferFramebuffers;
	std::unique_ptr<FramebufferAttachment> depthAttachment;
	bool buffersCleared = false;
	// The final pictures: one being drawn, one being presented
	std::vector<std::unique_ptr<FramebufferAttachment>> colorAttachments;
	std::vector<vk::UniqueFramebuffer> framebuffers;
	vk::Extent2D extent;
	int imageIndex = 0;
	bool frameRendered = false;
	float scaleX = 1.f;
	float scaleY = 1.f;

	std::vector<std::unique_ptr<HikaruTexture>> textures;	// by texture store id
};
