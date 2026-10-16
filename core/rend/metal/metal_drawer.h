/*
	Copyright 2025 flyinghead

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
#include "metal_apple.h"

#include "metal_texture.h"
#include "metal_shaders.h"
#include "metal_pipeline.h"
#include "metal_buffer.h"
#include "metal_commandpool.h"
#include "rend/texconv.h"
#include "rend/transform_matrix.h"
#include "rend/sorter.h"
#include "hw/pvr/pvr_mem.h"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

// Fragment push block for the stencil modifier-volume passes (ModVolShaderSource
// PushBlock, fragment buffer 1) and the OIT translucent modvol pass (buffer 5).
// 32 bytes, float4-aligned, so the MSL and C++ layouts agree byte for byte.
struct MetalModVolPushBlock
{
	float clipTest[4] {};   // x0, y0, x1, y1 in framebuffer pixels (Inside clip only)
	float shaderColor = 1.f; // Final pass darkening alpha
	int clipInside = 0;      // 1 = discard fragments inside clipTest
	float pad[2] {};

	/// Loads the per-volume tile clip. Returns true when the block changed and
	/// must be re-pushed to the encoder.
	bool setTileClip(TileClipping mode, const MTLScissorRect& rect)
	{
		const int inside = mode == TileClipping::Inside ? 1 : 0;
		const float next[4] = {
			(float)rect.x, (float)rect.y,
			(float)(rect.x + rect.width), (float)(rect.y + rect.height)
		};
		bool changed = inside != clipInside;
		if (inside)
			for (int i = 0; i < 4 && !changed; i++)
				changed = next[i] != clipTest[i];
		if (!changed)
			return false;
		clipInside = inside;
		if (inside)
			std::copy(next, next + 4, clipTest);
		return true;
	}
};
static_assert(sizeof(MetalModVolPushBlock) == 32, "must match the MSL PushBlock");

class MetalBaseDrawer
{
public:
	void SetCommandPool(MetalCommandPool *commandPool) { this->commandPool = commandPool; }
	// Commit 90301d5c3 removed the _pvrrc global: the renderer hands each
	// drawer the rend_context it is about to draw, exactly like Vulkan's
	// BaseDrawer::setRendContext.
	void setRendContext(rend_context *rendContext) {
		this->rendContext = rendContext;
	}

protected:
	TileClipping SetTileClip(id<MTLRenderCommandEncoder> encoder, u32 val, MTLScissorRect& clipRect);
	void SetBaseScissor(MTLViewport viewport);

	void SetScissor(id<MTLRenderCommandEncoder> encoder, const MTLScissorRect& scissor)
	{
		if (scissor.x != currentScissor.x ||
			scissor.y != currentScissor.y ||
			scissor.width != currentScissor.width ||
			scissor.height != currentScissor.height)
		{
			// Clamp scissor rect to base scissor bounds to prevent Metal validation errors
			MTLScissorRect clampedScissor = scissor;

			// First, clamp positions to be within base scissor bounds
			NSUInteger baseMaxX = baseScissor.x + baseScissor.width;
			NSUInteger baseMaxY = baseScissor.y + baseScissor.height;

			clampedScissor.x = std::max(clampedScissor.x, baseScissor.x);
			clampedScissor.y = std::max(clampedScissor.y, baseScissor.y);
			clampedScissor.x = std::min(clampedScissor.x, baseMaxX);
			clampedScissor.y = std::min(clampedScissor.y, baseMaxY);

			// Then clamp width/height to ensure they don't exceed bounds
			NSUInteger maxWidth = baseMaxX - clampedScissor.x;
			NSUInteger maxHeight = baseMaxY - clampedScissor.y;

			clampedScissor.width = std::min(clampedScissor.width, maxWidth);
			clampedScissor.height = std::min(clampedScissor.height, maxHeight);

			// Ensure minimum size (Metal requires non-zero dimensions)
			clampedScissor.width = std::max(clampedScissor.width, (NSUInteger)1);
			clampedScissor.height = std::max(clampedScissor.height, (NSUInteger)1);

			if (clampedScissor.x != scissor.x || clampedScissor.y != scissor.y ||
				clampedScissor.width != scissor.width || clampedScissor.height != scissor.height) {
				DEBUG_LOG(RENDERER, "Clamped scissor rect: requested=(%d,%d,%d,%d) -> clamped=(%d,%d,%d,%d), base=(%d,%d,%d,%d)",
						 (int)scissor.x, (int)scissor.y, (int)scissor.width, (int)scissor.height,
						 (int)clampedScissor.x, (int)clampedScissor.y, (int)clampedScissor.width, (int)clampedScissor.height,
						 (int)baseScissor.x, (int)baseScissor.y, (int)baseScissor.width, (int)baseScissor.height);
			}

			[encoder setScissorRect:clampedScissor];
			currentScissor = clampedScissor;
		}
	}

	MetalBufferData* GetMainBuffer(u32 size)
	{
		MetalBufferData *buffer;
		if (!mainBuffers.empty())
		{
			buffer = mainBuffers.back().release();
			mainBuffers.pop_back();
			if (buffer->bufferSize < size) {
				commandPool->addToFlight(new MetalDeleter(buffer));
				u32 newSize = (u32)buffer->bufferSize;
				while (newSize < size)
					newSize *= 2;
				INFO_LOG(RENDERER, "Increasing main buffer size %llu -> %d", (unsigned long long)buffer->bufferSize, newSize);

				buffer = new MetalBufferData(newSize);
			}
		}
		else
		{
			buffer = new MetalBufferData(std::max(512 * 1024u, size));
		}

		class BufferHolder : public MetalDeletable
		{
		public:
			BufferHolder(MetalBufferData *buffer, MetalBaseDrawer *drawer) : buffer(buffer), drawer(drawer) {}

			~BufferHolder() override {
				drawer->mainBuffers.emplace_back(buffer);
			}

		private:
			MetalBufferData *buffer;
			MetalBaseDrawer *drawer;
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
	void packNaomi2Uniforms(MetalBufferPacker& packer, Offsets& offsets, std::vector<u8>& n2uniforms, bool trModVolIncluded)
	{
		size_t n2UniformSize = sizeof(MetalN2VertexShaderUniforms) + MetalBufferPacker::align(sizeof(MetalN2VertexShaderUniforms), kConstantBufferAlign);
		int items = (int)(rendContext->global_param_op.size() + rendContext->global_param_pt.size() + rendContext->global_param_tr.size() + rendContext->global_param_mvo.size());
		if (trModVolIncluded)
			items += rendContext->global_param_mvo_tr.size();
		n2uniforms.resize(items * n2UniformSize);
		size_t bufIdx = 0;
		auto addUniform = [&](const PolyParam& pp, int polyNumber) {
			if (pp.isNaomi2())
			{
				MetalN2VertexShaderUniforms& uni = *(MetalN2VertexShaderUniforms *)&n2uniforms[bufIdx];
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
				addUniform(pp, (int)(((&pp - &rendContext->global_param_tr[0]) << 17) - firstVertexIdx));
		}
		size_t mvOffset = bufIdx;
		for (const ModifierVolumeParam& mvp : rendContext->global_param_mvo)
		{
			if (mvp.isNaomi2())
			{
				MetalN2VertexShaderUniforms& uni = *(MetalN2VertexShaderUniforms *)&n2uniforms[bufIdx];
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
					MetalN2VertexShaderUniforms& uni = *(MetalN2VertexShaderUniforms *)&n2uniforms[bufIdx];
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

	u64 packNaomi2Lights(MetalBufferPacker& packer)
	{
		u64 offset = -1;

		if (!rendContext->lightModels.empty())
		{
			offset = packer.addUniform(&rendContext->lightModels[0], rendContext->lightModels.size() * sizeof(decltype(rendContext->lightModels[0])));
		}

		return offset;
	}

	MTLCullMode toMetalCullMode(int cullMode) {
		return cullMode == 3 ? MTLCullModeBack
			   : cullMode == 2 ? MTLCullModeFront
			   : MTLCullModeNone;
	}

	// Framebuffer emulation: copies the rendered frame back into VRAM so
	// presentFramebuffer() can show it. Shared by both screen drawers, like
	// Vulkan's BaseDrawer::scaleAndWriteFramebuffer. Commits the current
	// frame's command buffer and waits for it (EndFrameAndWait).
	void scaleAndWriteFramebuffer(id<MTLTexture> finalTexture);

	void releaseReadbackTextures() {
		cachedScaledTexture = nil;
		cachedScaledWidth = cachedScaledHeight = 0;
		cachedReadableTexture = nil;
		cachedReadableWidth = cachedReadableHeight = 0;
	}

	MTLScissorRect baseScissor {};
	MTLScissorRect currentScissor {};
	TransformMatrix matrices { true };
	MetalCommandPool *commandPool = nullptr;
	std::vector<std::unique_ptr<MetalBufferData>> mainBuffers;
	rend_context *rendContext = nullptr;

private:
	// Framebuffer-emulation readback scratch; a member for the same reason as
	// MetalTextureDrawer::rttReadback -- scaleAndWriteFramebuffer() runs every
	// frame under EmulateFramebuffer and was allocating ~1.2MB per call.
	PixelBuffer<u32> fbReadback;

	// The two MTLTextures scaleAndWriteFramebuffer() needs, cached across
	// frames. Both were allocated per call, which is a good deal more expensive
	// than the malloc above. Safe to reuse: the only consumer is the blit that
	// commandPool->EndFrameAndWait() synchronizes before getBytes, so the GPU
	// is provably done with last frame's copy before this frame touches it.
	// Reallocated only when the surface dimensions change.
	id<MTLTexture> cachedScaledTexture = nil;
	u32 cachedScaledWidth = 0;
	u32 cachedScaledHeight = 0;
	id<MTLTexture> cachedReadableTexture = nil;
	u32 cachedReadableWidth = 0;
	u32 cachedReadableHeight = 0;
};

class MetalDrawer : public MetalBaseDrawer
{
public:
	virtual ~MetalDrawer() = default;

	bool Draw(const MetalTexture *fogTexture, const MetalTexture *paletteTexture);
	virtual void EndRenderPass() {
		renderPassStarted = false;
	}

	virtual void Term() {

	}

protected:
	virtual id<MTLRenderCommandEncoder> BeginRenderPass() = 0;
	void Init(MetalSamplers *samplers, MetalPipelineManager pipelineManager) {
		this->samplers = samplers;
		this->pipelineManager = std::make_unique<MetalPipelineManager>(pipelineManager);
	}

	int GetCurrentImage() const { return imageIndex; }

	id<MTLRenderCommandEncoder> currentEncoder = nil;
	MetalSamplers *samplers = nullptr;
	bool renderPassStarted = false;

private:
	void DrawPoly(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count);
	// False (and logs, rate-limited) when [first, first+count) runs past the idx
	// array uploaded this frame. See the definition for why.
	bool indexRangeValid(u64 first, u64 count, const char *site) const;

	// Redundant-state elimination for the per-polygon draw loop.
	//
	// DrawPoly unconditionally re-issued pipeline, depth-stencil, cull mode,
	// stencil reference, texture, sampler and push constants for EVERY polygon.
	// Metal encoder state is sticky, so setting an identical value again is
	// pure overhead — an ObjC message send plus driver-side encoding — and a
	// busy scene submits thousands of polys, most of which share state with
	// their neighbour because that is how the TA groups them.
	//
	// Output is bit-identical by construction: a set is skipped only when the
	// value already matches what this encoder was last given.
	//
	// Object pointers are compared by identity, which is sound because the
	// pipeline/sampler/depth-stencil managers hand back the same object for the
	// same key. Textures are only ever COMPARED, never dereferenced, and the
	// cache is invalidated at the start of every draw list — so a freed texture
	// cannot be mistaken for a live one at the same address across lists.
	//
	// MUST be invalidated whenever anything else touches the encoder, or a new
	// encoder begins. Call sites: DrawList, DrawSorted, DrawModVols.
	struct DrawStateCache
	{
		id<MTLRenderPipelineState> pipeline = nil;
		id<MTLDepthStencilState> depthStencil = nil;
		id<MTLTexture> fragTexture = nil;
		id<MTLSamplerState> fragSampler = nil;
		MTLCullMode cullMode = MTLCullModeNone;
		u32 stencilRef = 0;
		std::array<float, 8> pushConstants {};
		bool haveCullMode = false;
		bool haveStencilRef = false;
		bool havePushConstants = false;

		// Naomi2 only. The per-poly vertex-buffer binds at indices 1 (N2 vertex
		// uniforms) and 2 (light model) are the one pair of encoder sets that
		// genuinely varies per polygon -- but only on Naomi2, where the offset
		// is base + polyIndex * stride. On Dreamcast/Naomi1 both are constant,
		// so that path hoists the index-1 bind out of the loop entirely
		// (MetalDrawer::Draw) instead of caching it here.
		struct BufferBinding {
			id<MTLBuffer> buffer = nil;
			u64 offset = 0;
			bool operator!=(const BufferBinding& o) const { return buffer != o.buffer || offset != o.offset; }
		};
		BufferBinding n2Uniform;
		BufferBinding n2Lights;
		bool haveN2Uniform = false;
		bool haveN2Lights = false;

		void invalidate() { *this = DrawStateCache(); }
	};
	DrawStateCache drawState;
	void DrawSorted(id<MTLRenderCommandEncoder> encoder, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass);
	void DrawList(id<MTLRenderCommandEncoder> encoder, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last);
	void DrawModVols(id<MTLRenderCommandEncoder> encoder, int first, int count);
	void UploadMainBuffer(const MetalVertexShaderUniforms& vertexUniforms, const MetalFragmentShaderUniforms& fragmentUniforms);
	void AdvanceFrameBuffers() {
		if (!ring)
			return;
		ring->advance();
	}

	int imageIndex = 0;
	struct {
		u64 indexOffset = 0;
		u64 modVolOffset = 0;
		u64 vertexUniformOffset = 0;
		u64 fragmentUniformOffset = 0;
		u64 naomi2OpaqueOffset = 0;
		u64 naomi2PunchThroughOffset = 0;
		u64 naomi2TranslucentOffset = 0;
		u64 naomi2ModVolOffset = 0;
		u64 naomi2TrModVolOffset = 0;
		u64 lightsOffset = (u64)-1;  // Initialize to -1 to indicate unset
		// Element count of the idx array UploadMainBuffer packed this frame; the
		// bound every indexed draw is checked against (indexRangeValid).
		u64 indexCount = 0;
	} offsets;
	id<MTLBuffer> curMainBuffer = nil;
	std::unique_ptr<MetalRingBuffer> ring;
	u32 currentBaseOffset = 0;
	std::unique_ptr<MetalPipelineManager> pipelineManager = nullptr;
	bool dithering = false;
	/// True when Draw() already bound the index-1 vertex buffer for the whole
	/// frame, so DrawPoly must not re-bind it. Non-Naomi2 platforms only.
	bool hoistedVertexBind = false;
};

class MetalScreenDrawer : public MetalDrawer
{
public:
	~MetalScreenDrawer() override {
		cleanupFramebuffers();
	}
	void Init(MetalSamplers *samplers, MetalShaders *shaders, const MTLViewport& viewport);

	void EndRenderPass() override;
	bool PresentFrame()
	{
		EndRenderPass();
		if (!frameRendered)
			return false;
		frameRendered = false;
		MetalContext::Instance()->PresentFrame(framebuffers[GetCurrentImage()], viewport, aspectRatio);

		return true;
	}

protected:
	id<MTLRenderCommandEncoder> BeginRenderPass() override;

private:
	void cleanupFramebuffers() {
		releaseReadbackTextures();
		if (!framebuffers.empty() && commandPool) {
			commandPool->addToFlight(new MetalDeleter(std::move(framebuffers)));
		}
		framebuffers.clear();
		if (depthAttachment && commandPool) {
			class ResourceDeleter : public MetalDeletable {
			public:
				ResourceDeleter(id<MTLTexture> texture) { std::swap(this->texture, texture); }
				~ResourceDeleter() override {
					texture = nil;
				}
			private:
				id<MTLTexture> texture = nil;
			};
			commandPool->addToFlight(new ResourceDeleter(depthAttachment));
		}
		depthAttachment = nil;
		loadPassDescriptors.clear();
		clearPassDescriptors.clear();
		clearNeeded.clear();
	}
	std::vector<id<MTLTexture>> framebuffers;
	std::vector<MTLRenderPassDescriptor*> loadPassDescriptors;
	std::vector<MTLRenderPassDescriptor*> clearPassDescriptors;
	id<MTLTexture> depthAttachment;
	MTLViewport viewport;
	MetalShaders *shaderManager = nullptr;
	std::vector<bool> clearNeeded;
	bool frameRendered = false;
	float aspectRatio = 0.f;
	bool emulateFramebuffer = false;
};

class MetalTextureDrawer : public MetalDrawer
{
public:
	void Init(MetalSamplers *samplers, MetalShaders *shaders, MetalTextureCache *textureCache);

	void EndRenderPass() override;

protected:
	id<MTLRenderCommandEncoder> BeginRenderPass() override;

private:
	u32 textureAddr = 0;
	// RTT readback scratch. A member, not a per-call local: RTT-heavy scenes
	// (Sonic Adventure reflections, DOA2 intro) run this path several times a
	// frame, and a local meant a malloc/free per RTT. Grow-only, so it settles
	// at the largest RTT seen. Render-thread only; fully overwritten by
	// getBytes before WriteTextureToVRam reads it, so it carries no state.
	PixelBuffer<u32> rttReadback;

	MetalTexture *texture = nullptr;
	std::vector<id<MTLTexture>> framebuffers;
	MTLRenderPassDescriptor *rttPassDescriptor = nil;
	id<MTLTexture> colorAttachment;
	id<MTLTexture> depthAttachment;
	MetalTextureCache *textureCache = nullptr;
};
