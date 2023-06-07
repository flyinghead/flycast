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
#include "drawer.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/sorter.h"

TileClipping BaseDrawer::SetTileClip(u32 val, vk::Rect2D& clipRect)
{
	int rect[4] = {};
	TileClipping clipmode = ::GetTileClip(val, matrices.GetViewportMatrix(), rect);
	if (clipmode != TileClipping::Off)
	{
		clipRect.offset.x = rect[0];
		clipRect.offset.y = rect[1];
		clipRect.extent.width = rect[2];
		clipRect.extent.height = rect[3];
	}

	return clipmode;
}

void BaseDrawer::SetBaseScissor(const vk::Extent2D& viewport)
{
	bool wide_screen_on = config::Widescreen
			&& !matrices.IsClipped() && !config::Rotate90 && !config::EmulateFramebuffer;
	if (!wide_screen_on)
	{
		float width;
		float height;
		float min_x;
		float min_y;
		glm::vec4 clip_min(pvrrc.fb_X_CLIP.min, pvrrc.fb_Y_CLIP.min, 0, 1);
		glm::vec4 clip_dim(pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1,
		pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1, 0, 0);
		clip_min = matrices.GetScissorMatrix() * clip_min;
		clip_dim = matrices.GetScissorMatrix() * clip_dim;

		min_x = clip_min[0];
		min_y = clip_min[1];
		width = clip_dim[0];
		height = clip_dim[1];
		if (width < 0)
		{
			min_x += width;
			width = -width;
		}
		if (height < 0)
		{
			min_y += height;
			height = -height;
		}

		baseScissor = vk::Rect2D(
				vk::Offset2D((u32) std::max(lroundf(min_x), 0L),
						(u32) std::max(lroundf(min_y), 0L)),
				vk::Extent2D((u32) std::max(lroundf(width), 0L),
						(u32) std::max(lroundf(height), 0L)));
	}
	else
	{
		baseScissor = { 0, 0, (u32)viewport.width, (u32)viewport.height };
	}
}

void BaseDrawer::scaleAndWriteFramebuffer(vk::CommandBuffer commandBuffer, FramebufferAttachment *finalFB)
{
	u32 width = (pvrrc.ta_GLOB_TILE_CLIP.tile_x_num + 1) * 32;
	u32 height = (pvrrc.ta_GLOB_TILE_CLIP.tile_y_num + 1) * 32;

	float xscale = pvrrc.scaler_ctl.hscale == 1 ? 0.5f : 1.f;
	float yscale = 1024.f / pvrrc.scaler_ctl.vscalefactor;
	if (std::abs(yscale - 1.f) < 0.01f)
		yscale = 1.f;

	FramebufferAttachment *scaledFB = nullptr;
	FB_X_CLIP_type xClip = pvrrc.fb_X_CLIP;
	FB_Y_CLIP_type yClip = pvrrc.fb_Y_CLIP;

	if (xscale != 1.f || yscale != 1.f)
	{
		u32 scaledW = width * xscale;
		u32 scaledH = height * yscale;

		scaledFB = new FramebufferAttachment(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice());
		scaledFB->Init(scaledW, scaledH, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst);

		setImageLayout(commandBuffer, scaledFB->GetImage(), vk::Format::eR8G8B8A8Unorm, 1, vk::ImageLayout::eUndefined,
				vk::ImageLayout::eTransferDstOptimal);

		vk::ImageBlit imageBlit;
		imageBlit.setSrcOffsets({ vk::Offset3D(0, 0, 0), vk::Offset3D(width, height, 1) });
		imageBlit.setSrcSubresource(vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1));
		imageBlit.setDstOffsets({ vk::Offset3D(0, 0, 0), vk::Offset3D(scaledW, scaledH, 1) });
		imageBlit.setDstSubresource(imageBlit.srcSubresource);
		commandBuffer.blitImage(finalFB->GetImage(), vk::ImageLayout::eTransferSrcOptimal, scaledFB->GetImage(), vk::ImageLayout::eTransferDstOptimal,
				1, &imageBlit, vk::Filter::eLinear);

		setImageLayout(commandBuffer, scaledFB->GetImage(), vk::Format::eR8G8B8A8Unorm, 1, vk::ImageLayout::eTransferDstOptimal,
				vk::ImageLayout::eTransferSrcOptimal);

		finalFB = scaledFB;
		width = scaledW;
		height = scaledH;
		// FB_Y_CLIP is applied before vscalefactor if > 1, so it must be scaled here
		if (yscale > 1) {
			yClip.min = std::round(yClip.min * yscale);
			yClip.max = std::round(yClip.max * yscale);
		}
	}

	vk::BufferImageCopy copyRegion(0, width, height, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0),
			vk::Extent3D(width, height, 1));
	commandBuffer.copyImageToBuffer(finalFB->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
			*finalFB->GetBufferData()->buffer, copyRegion);

	vk::BufferMemoryBarrier bufferMemoryBarrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eHostRead,
			VK_QUEUE_FAMILY_IGNORED,
			VK_QUEUE_FAMILY_IGNORED,
			*finalFB->GetBufferData()->buffer,
			0,
			VK_WHOLE_SIZE);
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eHost, {}, nullptr, bufferMemoryBarrier, nullptr);

	commandBuffer.end();
	commandPool->EndFrame();

	vk::Fence fence = commandPool->GetCurrentFence();
	GetContext()->GetDevice().waitForFences(1, &fence, true, UINT64_MAX);
	PixelBuffer<u32> tmpBuf;
	tmpBuf.init(width, height);
	finalFB->GetBufferData()->download(width * height * 4, tmpBuf.data());

	xClip.min = std::min(xClip.min, width - 1);
	xClip.max = std::min(xClip.max, width - 1);
	yClip.min = std::min(yClip.min, height - 1);
	yClip.max = std::min(yClip.max, height - 1);
	WriteFramebuffer(width, height, (u8 *)tmpBuf.data(), pvrrc.fb_W_SOF1 & VRAM_MASK,
			pvrrc.fb_W_CTRL, pvrrc.fb_W_LINESTRIDE * 8, xClip, yClip);

	delete scaledFB;
}

void Drawer::DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count)
{
	vk::Rect2D scissorRect;
	TileClipping tileClip = SetTileClip(poly.tileclip, scissorRect);
	if (tileClip == TileClipping::Outside)
		SetScissor(cmdBuffer, scissorRect);
	else
		SetScissor(cmdBuffer, baseScissor);

	float trilinearAlpha = 1.f;
	if (poly.tsp.FilterMode > 1 && poly.pcw.Texture && listType != ListType_Punch_Through && poly.tcw.MipMapped == 1)
	{
		trilinearAlpha = 0.25f * (poly.tsp.MipMapD & 0x3);
		if (poly.tsp.FilterMode == 2)
			// Trilinear pass A
			trilinearAlpha = 1.f - trilinearAlpha;
	}
	bool gpuPalette = poly.texture != nullptr ? poly.texture->gpuPalette : false;
	float palette_index = 0.f;
	if (gpuPalette)
	{
		if (poly.tcw.PixelFmt == PixelPal4)
			palette_index = float(poly.tcw.PalSelect << 4) / 1023.f;
		else
			palette_index = float((poly.tcw.PalSelect >> 4) << 8) / 1023.f;
	}

	if (tileClip == TileClipping::Inside || trilinearAlpha != 1.f || gpuPalette)
	{
		std::array<float, 6> pushConstants = {
				(float)scissorRect.offset.x,
				(float)scissorRect.offset.y,
				(float)scissorRect.offset.x + (float)scissorRect.extent.width,
				(float)scissorRect.offset.y + (float)scissorRect.extent.height,
				trilinearAlpha,
				palette_index
		};
		cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
	}

	vk::Pipeline pipeline = pipelineManager->GetPipeline(listType, sortTriangles, poly, gpuPalette);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	if (poly.pcw.Texture || poly.isNaomi2())
	{
		vk::DeviceSize offset = 0;
		u32 index = 0;
		if (poly.isNaomi2())
		{
			switch (listType)
			{
			case ListType_Opaque:
				offset = offsets.naomi2OpaqueOffset;
				index = &poly - &pvrrc.global_param_op[0];
				break;
			case ListType_Punch_Through:
				offset = offsets.naomi2PunchThroughOffset;
				index = &poly - &pvrrc.global_param_pt[0];
				break;
			case ListType_Translucent:
				offset = offsets.naomi2TranslucentOffset;
				index = &poly - &pvrrc.global_param_tr[0];
				break;
			}
		}
		descriptorSets.bindPerPolyDescriptorSets(cmdBuffer, poly, index, *GetMainBuffer(0)->buffer, offset, offsets.lightsOffset);
	}
	cmdBuffer.drawIndexed(count, 1, first, 0, 0);
}

void Drawer::DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass)
{
	if (first == last)
		return;
	for (u32 idx = first; idx < last; idx++)
		DrawPoly(cmdBuffer, ListType_Translucent, true, pvrrc.global_param_tr[polys[idx].polyIndex], polys[idx].first, polys[idx].count);
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		for (u32 idx = first; idx < last; idx++)
		{
			const SortedTriangle& param = polys[idx];
			const PolyParam& polyParam = pvrrc.global_param_tr[param.polyIndex];
			if (polyParam.isp.ZWriteDis)
				continue;
			vk::Pipeline pipeline = pipelineManager->GetDepthPassPipeline(polyParam.isp.CullMode, polyParam.isNaomi2());
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			vk::Rect2D scissorRect;
			TileClipping tileClip = SetTileClip(polyParam.tileclip, scissorRect);
			if (tileClip == TileClipping::Outside)
				SetScissor(cmdBuffer, scissorRect);
			else
				SetScissor(cmdBuffer, baseScissor);
			cmdBuffer.drawIndexed(param.count, 1, pvrrc.idx.size() + param.first, 0, 0);
		}
	}
}

void Drawer::DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last)
{
	if (first == last)
		return;
	const PolyParam *pp_end = polys.data() + last;
	for (const PolyParam *pp = &polys[first]; pp != pp_end; pp++)
		if (pp->count > 2)
			DrawPoly(cmdBuffer, listType, sortTriangles, *pp, pp->first, pp->count);
}

void Drawer::DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count)
{
	if (count == 0 || pvrrc.modtrig.empty() || !config::ModifierVolumes)
		return;

	vk::Buffer buffer = GetMainBuffer(0)->buffer.get();
	cmdBuffer.bindVertexBuffers(0, buffer, offsets.modVolOffset);
	SetScissor(cmdBuffer, baseScissor);

	ModifierVolumeParam* params = &pvrrc.global_param_mvo[first];

	int mod_base = -1;
	vk::Pipeline pipeline;

	for (int cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (!param.isp.VolumeLast && mv_mode > 0)
			pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Or, param.isp.CullMode, param.isNaomi2());	// OR'ing (open volume or quad)
		else
			pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Xor, param.isp.CullMode, param.isNaomi2());	// XOR'ing (closed volume)

		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		descriptorSets.bindPerPolyDescriptorSets(cmdBuffer, param, first + cmv, *GetMainBuffer(0)->buffer, offsets.naomi2ModVolOffset);

		cmdBuffer.draw(param.count * 3, 1, param.first * 3, 0);

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			pipeline = pipelineManager->GetModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isp.CullMode, param.isNaomi2());
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			cmdBuffer.draw((param.first + param.count - mod_base) * 3, 1, mod_base * 3, 0);
			mod_base = -1;
		}
	}
	cmdBuffer.bindVertexBuffers(0, buffer, {0});

	std::array<float, 5> pushConstants = { 1 - FPU_SHAD_SCALE.scale_factor / 256.f, 0, 0, 0, 0 };
	cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

	pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Final, 0, false);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	cmdBuffer.drawIndexed(4, 1, 0, 0, 0);
}

void Drawer::UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms)
{
	BufferPacker packer;

	// Vertex
	packer.add(pvrrc.verts.data(), pvrrc.verts.size() * sizeof(decltype(*pvrrc.verts.data())));
	// Modifier Volumes
	offsets.modVolOffset = packer.add(pvrrc.modtrig.data(), pvrrc.modtrig.size() * sizeof(decltype(*pvrrc.modtrig.data())));
	// Index
	offsets.indexOffset = packer.add(pvrrc.idx.data(), pvrrc.idx.size() * sizeof(decltype(*pvrrc.idx.data())));
	// Uniform buffers
	offsets.vertexUniformOffset = packer.addUniform(&vertexUniforms, sizeof(vertexUniforms));
	offsets.fragmentUniformOffset = packer.addUniform(&fragmentUniforms, sizeof(fragmentUniforms));

	std::vector<u8> n2uniforms;
	if (settings.platform.isNaomi2())
	{
		packNaomi2Uniforms(packer, offsets, n2uniforms, false);
		offsets.lightsOffset = packNaomi2Lights(packer);
	}

	BufferData *buffer = GetMainBuffer(packer.size());
	packer.upload(*buffer);
}

bool Drawer::Draw(const Texture *fogTexture, const Texture *paletteTexture)
{
	FragmentShaderUniforms fragUniforms = MakeFragmentUniforms<FragmentShaderUniforms>();

	currentScissor = vk::Rect2D();

	vk::CommandBuffer cmdBuffer = BeginRenderPass();
	if (!pvrrc.isRTT && (FB_R_CTRL.fb_enable == 0 || VO_CONTROL.blank_video == 1))
	{
		// Video output disabled
		return true;
	}

	setFirstProvokingVertex(pvrrc);

	// Upload vertex and index buffers
	VertexShaderUniforms vtxUniforms;
	vtxUniforms.ndcMat = matrices.GetNormalMatrix();

	UploadMainBuffer(vtxUniforms, fragUniforms);

	// Update per-frame descriptor set and bind it
	descriptorSets.updateUniforms(GetMainBuffer(0)->buffer.get(), (u32)offsets.vertexUniformOffset, (u32)offsets.fragmentUniformOffset,
			fogTexture->GetImageView(), paletteTexture->GetImageView());
	descriptorSets.bindPerFrameDescriptorSets(cmdBuffer);

	// Bind vertex and index buffers
	const vk::Buffer buffer = GetMainBuffer(0)->buffer.get();
	cmdBuffer.bindVertexBuffers(0, buffer, {0});
	cmdBuffer.bindIndexBuffer(buffer, offsets.indexOffset, vk::IndexType::eUint32);

	// Make sure to push constants even if not used
	std::array<float, 5> pushConstants = { 0, 0, 0, 0, 0 };
	cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

	RenderPass previous_pass{};
    for (int render_pass = 0; render_pass < (int)pvrrc.render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes[render_pass];

        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
        		current_pass.op_count - previous_pass.op_count,
				current_pass.pt_count - previous_pass.pt_count,
				current_pass.tr_count - previous_pass.tr_count,
				current_pass.mvo_count - previous_pass.mvo_count, current_pass.autosort);
		DrawList(cmdBuffer, ListType_Opaque, false, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count);
		DrawList(cmdBuffer, ListType_Punch_Through, false, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count);
		DrawModVols(cmdBuffer, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);
		if (current_pass.autosort)
        {
			if (!config::PerStripSorting)
				DrawSorted(cmdBuffer, pvrrc.sortedTriangles, previous_pass.sorted_tr_count, current_pass.sorted_tr_count, render_pass + 1 < (int)pvrrc.render_passes.size());
			else
				DrawList(cmdBuffer, ListType_Translucent, true, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
        }
		else
			DrawList(cmdBuffer, ListType_Translucent, false, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
		previous_pass = current_pass;
    }

	return !pvrrc.isRTT;
}

void TextureDrawer::Init(SamplerManager *samplerManager, ShaderManager *shaderManager, TextureCache *textureCache)
{
	if (!rttPipelineManager)
		rttPipelineManager = std::make_unique<RttPipelineManager>();
	rttPipelineManager->Init(shaderManager);
	Drawer::Init(samplerManager, rttPipelineManager.get());

	this->textureCache = textureCache;
}

vk::CommandBuffer TextureDrawer::BeginRenderPass()
{
	DEBUG_LOG(RENDERER, "RenderToTexture packmode=%d stride=%d - %d x %d @ %06x", pvrrc.fb_W_CTRL.fb_packmode, pvrrc.fb_W_LINESTRIDE * 8,
			pvrrc.fb_X_CLIP.max + 1, pvrrc.fb_Y_CLIP.max + 1, pvrrc.fb_W_SOF1 & VRAM_MASK);
	matrices.CalcMatrices(&pvrrc);

	textureAddr = pvrrc.fb_W_SOF1 & VRAM_MASK;
	u32 origWidth = pvrrc.getFramebufferWidth();
	u32 origHeight = pvrrc.getFramebufferHeight();
	u32 upscaledWidth = origWidth;
	u32 upscaledHeight = origHeight;
	u32 widthPow2;
	u32 heightPow2;
	getRenderToTextureDimensions(upscaledWidth, upscaledHeight, widthPow2, heightPow2);

	rttPipelineManager->CheckSettingsChange();
	VulkanContext *context = GetContext();
	vk::Device device = context->GetDevice();

	NewImage();
	vk::CommandBuffer commandBuffer = commandPool->Allocate();
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	if (!depthAttachment || widthPow2 > depthAttachment->getExtent().width || heightPow2 > depthAttachment->getExtent().height)
	{
		if (!depthAttachment)
			depthAttachment = std::make_unique<FramebufferAttachment>(context->GetPhysicalDevice(), device);
		else
			GetContext()->WaitIdle();
		depthAttachment->Init(widthPow2, heightPow2, GetContext()->GetDepthFormat(),
				vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment);
	}
	vk::Image colorImage;
	vk::ImageView colorImageView;
	vk::ImageLayout colorImageCurrentLayout;

	if (!config::RenderToTextureBuffer)
	{
		texture = textureCache->getRTTexture(textureAddr, pvrrc.fb_W_CTRL.fb_packmode, origWidth, origHeight);
		if (textureCache->IsInFlight(texture))
		{
			texture->readOnlyImageView = *texture->imageView;
			textureCache->DestroyLater(texture);
		}
		textureCache->SetInFlight(texture);

		if (texture->format != vk::Format::eR8G8B8A8Unorm || texture->extent.width != widthPow2 || texture->extent.height != heightPow2)
		{
			texture->extent = vk::Extent2D(widthPow2, heightPow2);
			texture->format = vk::Format::eR8G8B8A8Unorm;
			texture->needsStaging = true;
			texture->CreateImage(vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
					vk::ImageLayout::eUndefined, vk::ImageAspectFlagBits::eColor);
			colorImageCurrentLayout = vk::ImageLayout::eUndefined;
		}
		else
		{
			colorImageCurrentLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
		}
		colorImage = *texture->image;
		colorImageView = texture->GetImageView();
	}
	else
	{
		if (!colorAttachment || widthPow2 > colorAttachment->getExtent().width || heightPow2 > colorAttachment->getExtent().height)
		{
			if (!colorAttachment)
				colorAttachment = std::make_unique<FramebufferAttachment>(context->GetPhysicalDevice(), device);
			else
				GetContext()->WaitIdle();
			colorAttachment->Init(widthPow2, heightPow2, vk::Format::eR8G8B8A8Unorm,
					vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc);
			colorImageCurrentLayout = vk::ImageLayout::eUndefined;
		}
		else
			colorImageCurrentLayout = vk::ImageLayout::eTransferSrcOptimal;
		colorImage = colorAttachment->GetImage();
		colorImageView = colorAttachment->GetImageView();
	}
	width = widthPow2;
	height = heightPow2;

	setImageLayout(commandBuffer, colorImage, vk::Format::eR8G8B8A8Unorm, 1, colorImageCurrentLayout, vk::ImageLayout::eColorAttachmentOptimal);

	std::array<vk::ImageView, 2> imageViews = {
		colorImageView,
		depthAttachment->GetImageView(),
	};
	framebuffers.resize(GetContext()->GetSwapChainSize());
	framebuffers[GetCurrentImage()] = device.createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(),
			rttPipelineManager->GetRenderPass(), imageViews, widthPow2, heightPow2, 1));

	const std::array<vk::ClearValue, 2> clear_colors = { vk::ClearColorValue(std::array<float, 4> { 0.f, 0.f, 0.f, 1.f }), vk::ClearDepthStencilValue { 0.f, 0 } };
	commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(rttPipelineManager->GetRenderPass(),	*framebuffers[GetCurrentImage()],
			vk::Rect2D( { 0, 0 }, { width, height }), clear_colors), vk::SubpassContents::eInline);
	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)upscaledWidth, (float)upscaledHeight, 1.0f, 0.0f));
	u32 minX = pvrrc.fb_X_CLIP.min;
	u32 minY = pvrrc.fb_Y_CLIP.min;
	getRenderToTextureDimensions(minX, minY, widthPow2, heightPow2);
	baseScissor = vk::Rect2D(vk::Offset2D(minX, minY), vk::Extent2D(upscaledWidth, upscaledHeight));
	commandBuffer.setScissor(0, baseScissor);
	currentCommandBuffer = commandBuffer;

	return commandBuffer;
}

void TextureDrawer::EndRenderPass()
{
	currentCommandBuffer.endRenderPass();

	u32 clippedWidth = pvrrc.getFramebufferWidth();
	u32 clippedHeight = pvrrc.getFramebufferHeight();

	if (config::RenderToTextureBuffer)
	{
		vk::BufferImageCopy copyRegion(0, clippedWidth, clippedHeight, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0),
				vk::Extent3D(clippedWidth, clippedHeight, 1));
		currentCommandBuffer.copyImageToBuffer(colorAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
				*colorAttachment->GetBufferData()->buffer, copyRegion);

		vk::BufferMemoryBarrier bufferMemoryBarrier(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eHostRead,
				VK_QUEUE_FAMILY_IGNORED,
				VK_QUEUE_FAMILY_IGNORED,
				*colorAttachment->GetBufferData()->buffer,
				0,
				VK_WHOLE_SIZE);
		currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
						vk::PipelineStageFlagBits::eHost, {}, nullptr, bufferMemoryBarrier, nullptr);
	}
	currentCommandBuffer.end();

	currentCommandBuffer = nullptr;
	commandPool->EndFrame();

	if (config::RenderToTextureBuffer)
	{
		vk::Fence fence = commandPool->GetCurrentFence();
		GetContext()->GetDevice().waitForFences(fence, true, UINT64_MAX);

		u16 *dst = (u16 *)&vram[textureAddr];

		PixelBuffer<u32> tmpBuf;
		tmpBuf.init(clippedWidth, clippedHeight);
		colorAttachment->GetBufferData()->download(clippedWidth * clippedHeight * 4, tmpBuf.data());
		WriteTextureToVRam(clippedWidth, clippedHeight, (u8 *)tmpBuf.data(), dst, pvrrc.fb_W_CTRL, pvrrc.fb_W_LINESTRIDE * 8);
	}
	else
	{
		//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);

		texture->dirty = 0;
		texture->unprotectVRam();
	}
	Drawer::EndRenderPass();
}

void ScreenDrawer::Init(SamplerManager *samplerManager, ShaderManager *shaderManager, const vk::Extent2D& viewport)
{
	this->shaderManager = shaderManager;
	if (this->viewport != viewport)
	{
		framebuffers.clear();
		colorAttachments.clear();
		depthAttachment.reset();
		transitionNeeded.clear();
		clearNeeded.clear();
		frameRendered = false;
	}
	this->viewport = viewport;
	if (!depthAttachment)
	{
		depthAttachment = std::make_unique<FramebufferAttachment>(
				GetContext()->GetPhysicalDevice(), GetContext()->GetDevice());
		depthAttachment->Init(viewport.width, viewport.height, GetContext()->GetDepthFormat(),
				vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment);
	}

	if (!renderPassLoad)
	{
		std::array<vk::AttachmentDescription, 2> attachmentDescriptions = {
				// Color attachment
				vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
						vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore,
						vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
						config::EmulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal,
						config::EmulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal),
				// Depth attachment
				vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), GetContext()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
						vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
						vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
						vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal),
		};
		vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
		vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);

		vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics,
						nullptr,
						colorReference,
						nullptr,
						&depthReference);

		vk::SubpassDependency dependency(0, VK_SUBPASS_EXTERNAL, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead,vk::DependencyFlagBits::eByRegion);

		renderPassLoad = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
				attachmentDescriptions,
				subpass,
				dependency));

		attachmentDescriptions[0].loadOp = vk::AttachmentLoadOp::eClear;
		renderPassClear = GetContext()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
				attachmentDescriptions,
				subpass,
				dependency));
	}
	size_t size = GetSwapChainSize();
	if (colorAttachments.size() > size)
	{
		colorAttachments.resize(size);
		framebuffers.resize(size);
		transitionNeeded.resize(size);
		clearNeeded.resize(size);
	}
	else
	{
		std::array<vk::ImageView, 2> attachments = {
				nullptr,
				depthAttachment->GetImageView(),
		};
		while (colorAttachments.size() < size)
		{
			colorAttachments.push_back(std::make_unique<FramebufferAttachment>(
					GetContext()->GetPhysicalDevice(), GetContext()->GetDevice()));
			vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment;
			if (config::EmulateFramebuffer)
				usage |= vk::ImageUsageFlagBits::eTransferSrc;
			else
				usage |= vk::ImageUsageFlagBits::eSampled;
			colorAttachments.back()->Init(viewport.width, viewport.height, vk::Format::eR8G8B8A8Unorm, usage);
			attachments[0] = colorAttachments.back()->GetImageView();
			vk::FramebufferCreateInfo createInfo(vk::FramebufferCreateFlags(), *renderPassLoad,
					attachments, viewport.width, viewport.height, 1);
			framebuffers.push_back(GetContext()->GetDevice().createFramebufferUnique(createInfo));
			transitionNeeded.push_back(true);
			clearNeeded.push_back(true);
		}
	}
	frameRendered = false;

	if (!screenPipelineManager)
		screenPipelineManager = std::make_unique<PipelineManager>();
	screenPipelineManager->Init(shaderManager, *renderPassLoad);
	Drawer::Init(samplerManager, screenPipelineManager.get());
}

vk::CommandBuffer ScreenDrawer::BeginRenderPass()
{
	NewImage();
	vk::CommandBuffer commandBuffer = commandPool->Allocate();
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	if (transitionNeeded[GetCurrentImage()])
	{
		setImageLayout(commandBuffer, colorAttachments[GetCurrentImage()]->GetImage(), vk::Format::eR8G8B8A8Unorm,
				1, vk::ImageLayout::eUndefined,
				config::EmulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal);
		transitionNeeded[GetCurrentImage()] = false;
	}

	vk::RenderPass renderPass = clearNeeded[GetCurrentImage()] ? *renderPassClear : *renderPassLoad;
	clearNeeded[GetCurrentImage()] = false;
	const std::array<vk::ClearValue, 2> clear_colors = { vk::ClearColorValue(std::array<float, 4> { 0.f, 0.f, 0.f, 1.f }), vk::ClearDepthStencilValue { 0.f, 0 } };
	commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(renderPass, *framebuffers[GetCurrentImage()],
			vk::Rect2D( { 0, 0 }, viewport), clear_colors), vk::SubpassContents::eInline);
	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)viewport.width, (float)viewport.height, 1.0f, 0.0f));

	matrices.CalcMatrices(&pvrrc, viewport.width, viewport.height);

	SetBaseScissor(viewport);
	commandBuffer.setScissor(0, baseScissor);
	currentCommandBuffer = commandBuffer;

	return commandBuffer;
}

void ScreenDrawer::EndRenderPass()
{
	currentCommandBuffer.endRenderPass();
	if (config::EmulateFramebuffer)
	{
		scaleAndWriteFramebuffer(currentCommandBuffer, colorAttachments[GetCurrentImage()].get());
	}
	else
	{
		currentCommandBuffer.end();
		commandPool->EndFrame();
		aspectRatio = getOutputFramebufferAspectRatio();
	}
	currentCommandBuffer = nullptr;
	Drawer::EndRenderPass();
	frameRendered = true;
}
