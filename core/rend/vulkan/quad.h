/*
    Created on: Nov 7, 2019

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
#include "vulkan.h"
#include "buffer.h"
#include "shaders.h"

struct QuadVertex
{
	f32 pos[3];
	f32 uv[2];
};

vk::PipelineVertexInputStateCreateInfo GetQuadInputStateCreateInfo(bool uv);

class QuadBuffer
{
public:
	QuadBuffer()
	{
		buffer = std::unique_ptr<BufferData>(new BufferData(sizeof(QuadVertex) * 4, vk::BufferUsageFlagBits::eVertexBuffer));
	}

	void Bind(vk::CommandBuffer commandBuffer)
	{
		const vk::DeviceSize zero = 0;
		commandBuffer.bindVertexBuffers(0, 1, &buffer->buffer.get(), &zero);
	}
	void Draw(vk::CommandBuffer commandBuffer)
	{
		commandBuffer.draw(4, 1, 0, 0);
	}

	void Update(QuadVertex vertices[] = nullptr)
	{
		if (vertices == nullptr)
		{
			static QuadVertex defaultVtx[] = {
				{ { -1.f, -1.f, 0.f }, { 0.f, 0.f } },
				{ {  1.f, -1.f, 0.f }, { 1.f, 0.f } },
				{ { -1.f,  1.f, 0.f }, { 0.f, 1.f } },
				{ {  1.f,  1.f, 0.f }, { 1.f, 1.f } },
			};
			vertices = defaultVtx;
		};
		buffer->upload(sizeof(QuadVertex) * 4, vertices);
	}
private:
	std::unique_ptr<BufferData> buffer;
};

class QuadPipeline
{
public:
	QuadPipeline(bool ignoreTexAlpha, bool rotate = false)
		: rotate(rotate), ignoreTexAlpha(ignoreTexAlpha) {}
	void Init(ShaderManager *shaderManager, vk::RenderPass renderPass);
	void Init(const QuadPipeline& other) { Init(other.shaderManager, other.renderPass); }
	void Term() {
		pipeline.reset();
		linearSampler.reset();
		nearestSampler.reset();
		pipelineLayout.reset();
		descSetLayout.reset();
	}
	void BindPipeline(vk::CommandBuffer commandBuffer) { commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, GetPipeline()); }
	vk::DescriptorSetLayout GetDescSetLayout() const { return *descSetLayout; }
	vk::PipelineLayout GetPipelineLayout() const { return *pipelineLayout; }
	vk::Sampler GetLinearSampler() const { return *linearSampler; }
	vk::Sampler GetNearestSampler() const { return *nearestSampler; }

private:
	vk::Pipeline GetPipeline()
	{
		if (!pipeline)
			CreatePipeline();
		return *pipeline;
	}
	void CreatePipeline();

	vk::RenderPass renderPass;
	vk::UniquePipeline pipeline;
	vk::UniqueSampler linearSampler;
	vk::UniqueSampler nearestSampler;
	vk::UniquePipelineLayout pipelineLayout;
	vk::UniqueDescriptorSetLayout descSetLayout;
	ShaderManager *shaderManager = nullptr;
	bool rotate;
	bool ignoreTexAlpha;
};

class QuadDrawer
{
public:
	QuadDrawer() = default;
	QuadDrawer(QuadDrawer &&) = default;
	QuadDrawer(const QuadDrawer &) = delete;
	QuadDrawer& operator=(QuadDrawer &&) = default;
	QuadDrawer& operator=(const QuadDrawer &) = delete;

	void Init(QuadPipeline *pipeline);
	void Draw(vk::CommandBuffer commandBuffer, vk::ImageView imageView, QuadVertex vertices[] = nullptr, bool nearestFilter = false, const float *color = nullptr);
private:
	QuadPipeline *pipeline = nullptr;
	std::unique_ptr<QuadBuffer> buffer;
	std::vector<vk::UniqueDescriptorSet> descriptorSets;
};
