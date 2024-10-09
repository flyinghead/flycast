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

#include <memory>

struct QuadVertex
{
	float x, y, z;
	float u, v;
};

vk::PipelineVertexInputStateCreateInfo GetQuadInputStateCreateInfo(bool uv);

class QuadBuffer
{
public:
	QuadBuffer()
	{
		buffer = std::make_unique<BufferData>(sizeof(QuadVertex) * 4, vk::BufferUsageFlagBits::eVertexBuffer);
	}

	void Bind(vk::CommandBuffer commandBuffer)
	{
		commandBuffer.bindVertexBuffers(0, buffer->buffer.get(), {0});
	}
	void Draw(vk::CommandBuffer commandBuffer)
	{
		commandBuffer.draw(4, 1, 0, 0);
	}

	void Update(QuadVertex vertices[4] = nullptr)
	{
		if (vertices == nullptr)
		{
			static QuadVertex defaultVtx[4]
			{
				{ -1.f, -1.f, 0.f, 0.f, 0.f },
				{  1.f, -1.f, 0.f, 1.f, 0.f },
				{ -1.f,  1.f, 0.f, 0.f, 1.f },
				{  1.f,  1.f, 0.f, 1.f, 1.f },
			};
			vertices = defaultVtx;
		};

		const size_t quadVerticesHash = hashQuadVertexData(vertices);;
		if (quadVerticesHash == lastUploadHash)
		{
			// data already uploaded
			return;
		}

		// new data to upload, update hash
		lastUploadHash = quadVerticesHash;
		buffer->upload(sizeof(QuadVertex) * 4, vertices);

	}
private:
	std::unique_ptr<BufferData> buffer;

	size_t hashQuadVertexData(QuadVertex vertices[4]) const;
	size_t lastUploadHash = 0;
};

class QuadPipeline
{
public:
	QuadPipeline(bool ignoreTexAlpha, bool rotate = false)
		: rotate(rotate), ignoreTexAlpha(ignoreTexAlpha) {}
	void Init(ShaderManager *shaderManager, vk::RenderPass renderPass, int subpass);
	void Init(const QuadPipeline& other) { Init(other.shaderManager, other.renderPass, other.subpass); }
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
	int subpass = 0;
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
	void Draw(vk::CommandBuffer commandBuffer, vk::ImageView imageView, QuadVertex vertices[4] = nullptr, bool nearestFilter = false, const float *color = nullptr);
private:
	QuadPipeline *pipeline = nullptr;
	std::unique_ptr<QuadBuffer> buffer;
	std::vector<vk::UniqueDescriptorSet> descriptorSets;
};
