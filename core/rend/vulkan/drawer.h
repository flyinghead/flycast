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
#include "rend/transform_matrix.h"
#include "vulkan.h"
#include "buffer.h"
#include "commandpool.h"
#include "pipeline.h"
#include "shaders.h"
#include "texture.h"

#include <memory>
#include <vector>
#include <glm/gtc/type_ptr.hpp>

class BaseDrawer
{
public:
	void SetCommandPool(CommandPool *commandPool) { this->commandPool = commandPool; }
	void setRendContext(rend_context *rendContext) {
		this->rendContext = rendContext;
	}

protected:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	TileClipping SetTileClip(vk::CommandBuffer cmdBuffer, u32 val, vk::Rect2D& clipRect);
	void SetBaseScissor(const vk::Extent2D& viewport = vk::Extent2D());
	void scaleAndWriteFramebuffer(vk::CommandBuffer commandBuffer, FramebufferAttachment *finalFB);

	void SetScissor(vk::CommandBuffer cmdBuffer, const vk::Rect2D& scissor)
	{
		if (scissor != currentScissor)
		{
			cmdBuffer.setScissor(0, scissor);
			currentScissor = scissor;
		}
	}

	BufferData* GetMainBuffer(u32 size, vk::BufferUsageFlags extraFlags = {})
	{
		const vk::BufferUsageFlags usageFlags
			{ vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eUniformBuffer | extraFlags };
		BufferData *buffer;
		if (!mainBuffers.empty())
		{
			buffer = mainBuffers.back().release();
			mainBuffers.pop_back();
			if (buffer->bufferSize < size)
			{
				u32 newSize = (u32)buffer->bufferSize;
				// FIXME vf4evob still complains about buffer in use after 2 frames. Due to swap chain size of 3
				commandPool->addToFlight(new Deleter(buffer));
				while (newSize < size)
					newSize *= 2;
				INFO_LOG(RENDERER, "Increasing main buffer size %zd -> %d", buffer->bufferSize, newSize);
				buffer = new BufferData(newSize, usageFlags);
			}
		}
		else {
			buffer = new BufferData(std::max(512 * 1024u, size), usageFlags);
		}

		class BufferHolder : public Deletable
		{
		public:
			BufferHolder(BufferData *buffer, BaseDrawer *drawer) : buffer(buffer), drawer(drawer) {}

			~BufferHolder() override {
				drawer->mainBuffers.emplace_back(buffer);
			}

		private:
			BufferData *buffer;
			BaseDrawer *drawer;
		};
		commandPool->addToFlight(new BufferHolder(buffer, this));

		return buffer;
	}

	template<typename T>
	T MakeFragmentUniforms()
	{
		T fragUniforms;

		//VERT and RAM fog color constants
		FOG_COL_VERT.getRGBColor(fragUniforms.sp_FOG_COL_VERT);
		FOG_COL_RAM.getRGBColor(fragUniforms.sp_FOG_COL_RAM);

		//Fog density constant
		fragUniforms.sp_FOG_DENSITY = FOG_DENSITY.get() * config::ExtraDepthScale;

		rendContext->fog_clamp_min.getRGBAColor(fragUniforms.colorClampMin);
		rendContext->fog_clamp_max.getRGBAColor(fragUniforms.colorClampMax);

		fragUniforms.cp_AlphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

		return fragUniforms;
	}

	template<typename Offsets>
	void packNaomi2Uniforms(BufferPacker& packer, Offsets& offsets, std::vector<u8>& n2uniforms, bool trModVolIncluded)
	{
		size_t n2UniformSize = sizeof(N2VertexShaderUniforms) + align(sizeof(N2VertexShaderUniforms), GetContext()->GetUniformBufferAlignment());
		int items = rendContext->global_param_op.size() + rendContext->global_param_pt.size() + rendContext->global_param_tr.size() + rendContext->global_param_mvo.size();
		if (trModVolIncluded)
			items += rendContext->global_param_mvo_tr.size();
		n2uniforms.resize(items * n2UniformSize);
		size_t bufIdx = 0;
		auto addUniform = [&](const PolyParam& pp, int polyNumber) {
			if (pp.isNaomi2())
			{
				N2VertexShaderUniforms& uni = *(N2VertexShaderUniforms *)&n2uniforms[bufIdx];
				memcpy(glm::value_ptr(uni.mvMat), rendContext->matrices[pp.mvMatrix].mat, sizeof(uni.mvMat));
				memcpy(glm::value_ptr(uni.normalMat), rendContext->matrices[pp.normalMatrix].mat, sizeof(uni.normalMat));
				memcpy(glm::value_ptr(uni.projMat), rendContext->matrices[pp.projMatrix].mat, sizeof(uni.projMat));
				uni.bumpMapping = pp.pcw.Texture == 1 && pp.tcw.PixelFmt == PixelBumpMap;
				uni.polyNumber = polyNumber;
				for (size_t i = 0; i < 2; i++)
				{
					uni.envMapping[i] = pp.envMapping[i];
					uni.glossCoef[i] = pp.glossCoef[i];
					uni.constantColor[i] = pp.constantColor[i];
				}
			}
			bufIdx += n2UniformSize;
		};
		for (const PolyParam& pp : rendContext->global_param_op)
			addUniform(pp, 0);
		size_t ptOffset = bufIdx;
		for (const PolyParam& pp : rendContext->global_param_pt)
			addUniform(pp, 0);
		size_t trOffset = bufIdx;
		if (!rendContext->global_param_tr.empty())
		{
			u32 firstVertexIdx = rendContext->idx[rendContext->global_param_tr[0].first];
			for (const PolyParam& pp : rendContext->global_param_tr)
				addUniform(pp, ((&pp - &rendContext->global_param_tr[0]) << 17) - firstVertexIdx);
		}
		size_t mvOffset = bufIdx;
		for (const ModifierVolumeParam& mvp : rendContext->global_param_mvo)
		{
			if (mvp.isNaomi2())
			{
				N2VertexShaderUniforms& uni = *(N2VertexShaderUniforms *)&n2uniforms[bufIdx];
				memcpy(glm::value_ptr(uni.mvMat), rendContext->matrices[mvp.mvMatrix].mat, sizeof(uni.mvMat));
				memcpy(glm::value_ptr(uni.projMat), rendContext->matrices[mvp.projMatrix].mat, sizeof(uni.projMat));
			}
			bufIdx += n2UniformSize;
		}
		size_t trMvOffset = bufIdx;
		if (trModVolIncluded)
			for (const ModifierVolumeParam& mvp : rendContext->global_param_mvo_tr)
			{
				if (mvp.isNaomi2())
				{
					N2VertexShaderUniforms& uni = *(N2VertexShaderUniforms *)&n2uniforms[bufIdx];
					memcpy(glm::value_ptr(uni.mvMat), rendContext->matrices[mvp.mvMatrix].mat, sizeof(uni.mvMat));
					memcpy(glm::value_ptr(uni.projMat), rendContext->matrices[mvp.projMatrix].mat, sizeof(uni.projMat));
				}
				bufIdx += n2UniformSize;
			}
		offsets.naomi2OpaqueOffset = packer.addUniform(n2uniforms.data(), bufIdx);
		offsets.naomi2PunchThroughOffset = offsets.naomi2OpaqueOffset + ptOffset;
		offsets.naomi2TranslucentOffset = offsets.naomi2OpaqueOffset + trOffset;
		offsets.naomi2ModVolOffset = offsets.naomi2OpaqueOffset + mvOffset;
		offsets.naomi2TrModVolOffset = offsets.naomi2OpaqueOffset + trMvOffset;
	}

	vk::DeviceSize packNaomi2Lights(BufferPacker& packer)
	{
		vk::DeviceSize offset = -1;

		size_t n2LightSize = sizeof(N2LightModel) + align(sizeof(N2LightModel), GetContext()->GetUniformBufferAlignment());
		if (n2LightSize == sizeof(N2LightModel) && !rendContext->lightModels.empty())
		{
			offset = packer.addUniform(&rendContext->lightModels[0], rendContext->lightModels.size() * sizeof(decltype(rendContext->lightModels[0])));
		}
		else
		{
			for (const N2LightModel& model : rendContext->lightModels)
			{
				vk::DeviceSize ioffset = packer.addUniform(&model, sizeof(N2LightModel));
				if (offset == (vk::DeviceSize)-1)
					offset = ioffset;
			}
		}

		return offset;
	}

	vk::Rect2D baseScissor;
	vk::Rect2D currentScissor;
	TransformMatrix matrices;
	CommandPool *commandPool = nullptr;
	std::vector<std::unique_ptr<BufferData>> mainBuffers;
	rend_context *rendContext = nullptr;
};

class Drawer;

class RenderDelegate
{
public:
	virtual ~RenderDelegate() = default;
	virtual void init(Drawer *drawer) {
		this->drawer = drawer;
	}
	virtual void term() {}
	virtual void createAttachments(int index, vk::ImageView imageView, vk::Image image) {}
	virtual vk::RenderPass getRenderPass(bool load) { return {}; }

	virtual void beginRender(vk::CommandBuffer cmdBuffer, int index) = 0;
	virtual void endRender(vk::CommandBuffer cmdBuffer) = 0;
	virtual bool beforeDrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles,
			const PolyParam& poly, u32 first, u32 count) {
		return true;
	}

protected:
	static VulkanContext *vkCtx() { return VulkanContext::Instance(); }

	Drawer *drawer = nullptr;
};

class Drawer : public BaseDrawer
{
public:
	virtual ~Drawer() = default;
	void Term();

	bool Draw(const Texture *fogTexture, const Texture *paletteTexture);
	virtual void EndRenderPass() {
		renderPassStarted = false;
	}
	vk::CommandBuffer GetCurrentCommandBuffer() const { return currentCommandBuffer; }
	vk::RenderPass GetRenderPass() const { return renderDelegate->getRenderPass(false); }

protected:
	virtual u32 GetSwapChainSize() { return GetContext()->GetSwapChainSize(); }
	virtual vk::CommandBuffer BeginRenderPass() = 0;
	void NewImage();
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager);

	int GetCurrentImage() const { return imageIndex; }

	std::unique_ptr<RenderDelegate> renderDelegate;
	vk::CommandBuffer currentCommandBuffer;
	SamplerManager *samplerManager = nullptr;
	bool renderPassStarted = false;
	std::vector<std::unique_ptr<FramebufferAttachment>> colorAttachments;
	std::unique_ptr<FramebufferAttachment> depthAttachment;
	vk::Extent2D viewport;
	vk::ImageView secAccumView {};

private:
	void SortTriangles();
	void DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
	void DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass);
	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last);
	void DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count);
	void UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms);

	int imageIndex = 0;
	struct {
		vk::DeviceSize indexOffset = 0;
		vk::DeviceSize modVolOffset = 0;
		vk::DeviceSize vertexUniformOffset = 0;
		vk::DeviceSize fragmentUniformOffset = 0;
		vk::DeviceSize naomi2OpaqueOffset = 0;
		vk::DeviceSize naomi2PunchThroughOffset = 0;
		vk::DeviceSize naomi2TranslucentOffset = 0;
		vk::DeviceSize naomi2ModVolOffset = 0;
		vk::DeviceSize naomi2TrModVolOffset = 0;
		vk::DeviceSize lightsOffset = 0;
	} offsets;
	DescriptorSets descriptorSets;
	vk::Buffer curMainBuffer;
	std::unique_ptr<PipelineManager> pipelineManager;
	bool perStripSorting = false;
	bool dithering = false;

	friend class ClassicRender;
	friend class DynamicRender;
	friend class ScreenClassicRender;
	friend class TextureClassicRender;
	friend class ScreenDynamicRender;
};

class ScreenDrawer : public Drawer
{
public:
	ScreenDrawer();
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager, const vk::Extent2D& viewport);

	void EndRenderPass() override;
	bool PresentFrame();

protected:
	vk::CommandBuffer BeginRenderPass() override;
	u32 GetSwapChainSize() override { return 2; }

private:
	void createAttachments(vk::CommandBuffer cmdBuffer);

	bool frameRendered = false;
	float aspectRatio = 0.f;
	bool emulateFramebuffer = false;
};

class TextureDrawer : public Drawer
{
public:
	TextureDrawer();
	void Init(SamplerManager *samplerManager, ShaderManager *shaderManager, TextureCache *textureCache);

	void EndRenderPass() override;

protected:
	vk::CommandBuffer BeginRenderPass() override;

private:
	u32 textureAddr = 0;
	Texture *texture = nullptr;
	TextureCache *textureCache = nullptr;
};

class ClassicRender : public RenderDelegate
{
public:
	void init(Drawer *drawer) override;
	void term() override;
	void createAttachments(int index, vk::ImageView imageView, vk::Image image) override;

	void beginRender(vk::CommandBuffer cmdBuffer, int index) override;
	void endRender(vk::CommandBuffer cmdBuffer) override;
	bool beforeDrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles,
			const PolyParam& poly, u32 first, u32 count) override;

protected:
	std::vector<vk::UniqueFramebuffer> framebuffers;
};

class ScreenClassicRender : public ClassicRender
{
public:
	void init(Drawer *drawer) override;
	void term() override;
	vk::RenderPass getRenderPass(bool load) override {
		return load ? *renderPassLoad : *renderPassClear;
	}

protected:
	vk::UniqueRenderPass renderPassLoad;
	vk::UniqueRenderPass renderPassClear;
};

class TextureClassicRender : public ClassicRender
{
public:
	void init(Drawer *drawer) override;
	void term() override;
	void createAttachments(int index, vk::ImageView imageView, vk::Image image) override;
	vk::RenderPass getRenderPass(bool load) override {
		return *rttRenderPass;
	}

private:
	bool renderToTextureBuffer = false;
	vk::UniqueRenderPass rttRenderPass;
};

class DynamicRender : public RenderDelegate
{
public:
	void term() override;
	void createAttachments(int index, vk::ImageView imageView, vk::Image image) override;

	void beginRender(vk::CommandBuffer cmdBuffer, int index) override;
	void endRender(vk::CommandBuffer cmdBuffer) override;
	bool beforeDrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles,
			const PolyParam& poly, u32 first, u32 count) override;

private:
	bool writingSecAccum = false;
	bool lastDstSelect = false;
	std::unique_ptr<FramebufferAttachment> secAccum;
	vk::ImageView renderTarget;
};

class ScreenDynamicRender : public DynamicRender
{
public:
	void init(Drawer *drawer) override;
	void beginRender(vk::CommandBuffer cmdBuffer, int index) override;
	void endRender(vk::CommandBuffer cmdBuffer) override;

private:
	FramebufferAttachment *renderAttachment = nullptr;
	bool emulateFramebuffer = false;
};

class TextureDynamicRender : public DynamicRender
{
public:
	void createAttachments(int index, vk::ImageView imageView, vk::Image image) override;
	void endRender(vk::CommandBuffer cmdBuffer) override;

private:
	vk::Image renderImage;
};

