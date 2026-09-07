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

TileClipping BaseDrawer::SetTileClip(vk::CommandBuffer cmdBuffer, u32 val, vk::Rect2D& clipRect)
{
	Rect rect;
	TileClipping clipmode = matrices.getTileClip(val, rect);
	if (clipmode != TileClipping::Off)
	{
		clipRect.offset.x = rect.origin.x;
		clipRect.offset.y = rect.origin.y;
		clipRect.extent.width = rect.size.x;
		clipRect.extent.height = rect.size.y;
	}
	if (clipmode == TileClipping::Outside)
		SetScissor(cmdBuffer, clipRect);
	else
		SetScissor(cmdBuffer, baseScissor);

	return clipmode;
}

void BaseDrawer::SetBaseScissor(const vk::Extent2D& viewport) {
	Rect scissor = matrices.getBaseScissor();
	baseScissor = vk::Rect2D{ {scissor.origin.x, scissor.origin.y}, {(u32)scissor.size.x, (u32)scissor.size.y} };
}

void BaseDrawer::scaleAndWriteFramebuffer(vk::CommandBuffer commandBuffer, FramebufferAttachment *finalFB)
{
	static const float scopeColor[4] = { 0.25f, 0.25f, 0.25f, 0.25f };
	CommandBufferDebugScope _(commandBuffer, "scaleAndWriteFramebuffer", scopeColor);

	u32 width = rendContext->globClip.x;
	u32 height = rendContext->globClip.y;
	glm::ivec2 scaledSize;
	Rect finalClip;
	getWriteFBToVramParams(*rendContext, scaledSize, finalClip);

	FramebufferAttachment *scaledFB = nullptr;

	if (scaledSize.x != (int)width || scaledSize.y != (int)height)
	{
		const u32 scaledW = scaledSize.x;
		const u32 scaledH = scaledSize.y;

		scaledFB = new FramebufferAttachment(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice());
		scaledFB->Init(scaledW, scaledH, vk::Format::eR8G8B8A8Unorm, vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst,
				"SCALED FRAMEBUFFER");

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
	}

	vk::BufferImageCopy copyRegion(0, width, height, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0),
			vk::Extent3D(width, height, 1));
	commandBuffer.copyImageToBuffer(finalFB->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
			*finalFB->GetBufferData()->buffer, copyRegion);

	vk::BufferMemoryBarrier bufferMemoryBarrier(
			vk::AccessFlagBits::eTransferWrite,
			vk::AccessFlagBits::eHostRead,
			vk::QueueFamilyIgnored,
			vk::QueueFamilyIgnored,
			*finalFB->GetBufferData()->buffer,
			0,
			vk::WholeSize);
	commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
					vk::PipelineStageFlagBits::eHost, {}, nullptr, bufferMemoryBarrier, nullptr);

	commandBuffer.end();
	commandPool->EndFrameAndWait();

	PixelBuffer<u32> tmpBuf;
	tmpBuf.init(width, height);
	finalFB->GetBufferData()->download(width * height * 4, tmpBuf.data());

	WriteFramebuffer(width, height, (u8 *)tmpBuf.data(), rendContext->fb_W_SOF1 & VRAM_MASK,
			rendContext->fb_W_CTRL, rendContext->fb_W_LINESTRIDE * 8, finalClip);

	delete scaledFB;
}

void Drawer::Init(SamplerManager *samplerManager, ShaderManager *shaderManager)
{
	this->samplerManager = samplerManager;
	if (!pipelineManager)
		pipelineManager = std::make_unique<PipelineManager>();
	pipelineManager->Init(shaderManager);

	descriptorSets.init(samplerManager, pipelineManager->GetPipelineLayout(), pipelineManager->GetPerFrameDSLayout(), pipelineManager->GetPerPolyDSLayout());
	renderDelegate->init(this);
}

void Drawer::Term()
{
	renderDelegate->term();
	colorAttachments.clear();
	depthAttachment.reset();
	descriptorSets.term();
	mainBuffers.clear();
	pipelineManager.reset();
}

void Drawer::NewImage()
{
	descriptorSets.nextFrame();
	imageIndex = (imageIndex + 1) % GetSwapChainSize();
	if (perStripSorting != config::PerStripSorting)
	{
		perStripSorting = config::PerStripSorting;
		pipelineManager->Reset();
	}
}

void Drawer::DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count)
{
	if (!renderDelegate->beforeDrawPoly(cmdBuffer, listType, sortTriangles, poly, first, count))
		return;
	static const float scopeColor[4] = { 0.25f, 0.50f, 0.25f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "DrawPoly", scopeColor);

	vk::Rect2D scissorRect;
	TileClipping tileClip = SetTileClip(cmdBuffer, poly.tileclip, scissorRect);

	float trilinearAlpha = 1.f;
	bool trilinearAlphaEnabled = false;
	if (poly.tsp.FilterMode > 1 && poly.pcw.Texture && listType != ListType_Punch_Through && poly.tcw.MipMapped == 1)
	{
		trilinearAlpha = 0.25f * (poly.tsp.MipMapD & 0x3);
		if (poly.tsp.FilterMode == 2)
			// Trilinear pass A
			trilinearAlpha = 1.f - trilinearAlpha;
		trilinearAlphaEnabled = true;
	}
	int gpuPalette = poly.texture == nullptr || !poly.texture->gpuPalette ? 0
			: poly.tsp.FilterMode + 1;
	float palette_index = 0.f;
	if (gpuPalette != 0)
	{
		if (config::TextureFiltering == 1)
			gpuPalette = 1; // force nearest
		else if (config::TextureFiltering == 2)
			gpuPalette = 2; // force linear
		if (poly.tcw.PixelFmt == PixelPal4)
			palette_index = float(poly.tcw.PalSelect << 4) / 1023.f;
		else
			palette_index = float((poly.tcw.PalSelect >> 4) << 8) / 1023.f;
	}

	if (tileClip == TileClipping::Inside || trilinearAlphaEnabled || gpuPalette != 0)
	{
		const std::array<float, 6> pushConstants = {
				(float)scissorRect.offset.x,
				(float)scissorRect.offset.y,
				(float)scissorRect.offset.x + (float)scissorRect.extent.width,
				(float)scissorRect.offset.y + (float)scissorRect.extent.height,
				trilinearAlpha,
				palette_index
		};
		cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
	}

	vk::Pipeline pipeline = pipelineManager->GetPipeline(listType, sortTriangles, poly, gpuPalette, dithering);
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
				index = &poly - &rendContext->global_param_op[0];
				break;
			case ListType_Punch_Through:
				offset = offsets.naomi2PunchThroughOffset;
				index = &poly - &rendContext->global_param_pt[0];
				break;
			case ListType_Translucent:
				offset = offsets.naomi2TranslucentOffset;
				index = &poly - &rendContext->global_param_tr[0];
				break;
			}
		}
		descriptorSets.bindPerPolyDescriptorSets(cmdBuffer, poly, index, curMainBuffer, offset, offsets.lightsOffset,
				listType == ListType_Punch_Through);
	}
	cmdBuffer.drawIndexed(count, 1, first, 0, 0);
}

void Drawer::DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortedTriangle>& polys, u32 first, u32 last, bool multipass)
{
	if (first == last)
		return;

	static const float scopeColor[4] = { 0.25f, 0.50f, 0.50f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "DrawSorted", scopeColor);

	for (u32 idx = first; idx < last; idx++)
		DrawPoly(cmdBuffer, ListType_Translucent, true, rendContext->global_param_tr[polys[idx].polyIndex], polys[idx].first, polys[idx].count);
	if (multipass && config::TranslucentPolygonDepthMask)
	{
		// Write to the depth buffer now. The next render pass might need it. (Cosmic Smash)
		for (u32 idx = first; idx < last; idx++)
		{
			const SortedTriangle& param = polys[idx];
			const PolyParam& polyParam = rendContext->global_param_tr[param.polyIndex];
			if (polyParam.isp.ZWriteDis)
				continue;
			vk::Pipeline pipeline = pipelineManager->GetDepthPassPipeline(polyParam.isp.CullMode, polyParam.isNaomi2());
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			vk::Rect2D scissorRect;
			SetTileClip(cmdBuffer, polyParam.tileclip, scissorRect);
			cmdBuffer.drawIndexed(param.count, 1, param.first, 0, 0);
		}
	}
}

void Drawer::DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const std::vector<PolyParam>& polys, u32 first, u32 last)
{
	if (first == last)
		return;

	static const float scopeColor[4] = { 0.50f, 0.25f, 0.50f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "DrawList", scopeColor);

	const PolyParam *pp_end = polys.data() + last;
	for (const PolyParam *pp = &polys[first]; pp != pp_end; pp++)
		if (pp->count > 2)
			DrawPoly(cmdBuffer, listType, sortTriangles, *pp, pp->first, pp->count);
}

void Drawer::DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count)
{
	if (count == 0 || rendContext->modtrig.empty() || !config::ModifierVolumes)
		return;

	static const float scopeColor[4] = { 0.75f, 0.25f, 0.25f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "DrawModVols", scopeColor);

	cmdBuffer.bindVertexBuffers(0, curMainBuffer, offsets.modVolOffset);

	ModifierVolumeParam* params = &rendContext->global_param_mvo[first];

	int mod_base = -1;
	vk::Pipeline pipeline;
	vk::Rect2D scissorRect;

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
		descriptorSets.bindPerPolyDescriptorSets(cmdBuffer, param, first + cmv, curMainBuffer, offsets.naomi2ModVolOffset);
		SetTileClip(cmdBuffer, param.tileclip, scissorRect);
		// TODO inside clipping

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
	cmdBuffer.bindVertexBuffers(0, curMainBuffer, {0});
	SetTileClip(cmdBuffer, 0, scissorRect);

	const std::array<float, 6> pushConstants = { 1 - FPU_SHAD_SCALE.scale_factor / 256.f, 0, 0, 0, 0, 0 };
	cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

	pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Final, 0, false);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	cmdBuffer.drawIndexed(4, 1, 0, 0, 0);
}

void Drawer::UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms)
{
	BufferPacker packer;

	// Vertex
	packer.add(rendContext->verts.data(), rendContext->verts.size() * sizeof(decltype(*rendContext->verts.data())));
	// Modifier Volumes
	offsets.modVolOffset = packer.add(rendContext->modtrig.data(), rendContext->modtrig.size() * sizeof(decltype(*rendContext->modtrig.data())));
	// Index
	offsets.indexOffset = packer.add(rendContext->idx.data(), rendContext->idx.size() * sizeof(decltype(*rendContext->idx.data())));
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
	curMainBuffer = buffer->buffer.get();
}

bool Drawer::Draw(const Texture *fogTexture, const Texture *paletteTexture)
{
	FragmentShaderUniforms fragUniforms = MakeFragmentUniforms<FragmentShaderUniforms>();
	dithering = config::EmulateFramebuffer && rendContext->fb_W_CTRL.fb_dither && rendContext->fb_W_CTRL.fb_packmode <= 3;
	if (dithering)
	{
		switch (rendContext->fb_W_CTRL.fb_packmode)
		{
		case 0: // 0555 KRGB 16 bit
		case 3: // 1555 ARGB 16 bit
			fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[1] = fragUniforms.ditherDivisor[2] = 2.f;
			break;
		case 1: // 565 RGB 16 bit
			fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[2] = 2.f;
			fragUniforms.ditherDivisor[1] = 4.f;
			break;
		case 2: // 4444 ARGB 16 bit
			fragUniforms.ditherDivisor[0] = fragUniforms.ditherDivisor[1] = fragUniforms.ditherDivisor[2] = 1.f;
			break;
		default:
			break;
		}
		fragUniforms.ditherDivisor[3] = 1.f;
	}

	currentScissor = vk::Rect2D();

	vk::CommandBuffer cmdBuffer = BeginRenderPass();

	static const float scopeColor[4] = { 0.75f, 0.75f, 0.75f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "Draw", scopeColor);

	if (VulkanContext::Instance()->hasProvokingVertex())
	{
		// Pipelines are using VK_EXT_provoking_vertex, no need to
		// re-order vertices
	}
	else
	{
		setFirstProvokingVertex(*rendContext);
	}

	// Upload vertex and index buffers
	VertexShaderUniforms vtxUniforms;
	vtxUniforms.ndcMat = matrices.GetNormalMatrix();

	UploadMainBuffer(vtxUniforms, fragUniforms);

	// Update per-frame descriptor set and bind it
	descriptorSets.updateUniforms(curMainBuffer, (u32)offsets.vertexUniformOffset, (u32)offsets.fragmentUniformOffset,
			fogTexture->GetImageView(), paletteTexture->GetImageView(), secAccumView);
	descriptorSets.bindPerFrameDescriptorSets(cmdBuffer);

	// Bind vertex and index buffers
	cmdBuffer.bindVertexBuffers(0, curMainBuffer, {0});
	cmdBuffer.bindIndexBuffer(curMainBuffer, offsets.indexOffset, vk::IndexType::eUint32);

	// Make sure to push constants even if not used
	const std::array<float, 6> pushConstants = { 0, 0, 0, 0, 0, 0 };
	cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

	RenderPass previous_pass{};
    for (int render_pass = 0; render_pass < (int)rendContext->render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = rendContext->render_passes[render_pass];

        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d autosort %d", render_pass + 1,
        		current_pass.op_count - previous_pass.op_count,
				current_pass.pt_count - previous_pass.pt_count,
				current_pass.tr_count - previous_pass.tr_count,
				current_pass.mvo_count - previous_pass.mvo_count, current_pass.autosort);
		DrawList(cmdBuffer, ListType_Opaque, false, rendContext->global_param_op, previous_pass.op_count, current_pass.op_count);
		DrawList(cmdBuffer, ListType_Punch_Through, false, rendContext->global_param_pt, previous_pass.pt_count, current_pass.pt_count);
		DrawModVols(cmdBuffer, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);
		if (current_pass.autosort)
        {
			if (!config::PerStripSorting)
				DrawSorted(cmdBuffer, rendContext->sortedTriangles, previous_pass.sorted_tr_count, current_pass.sorted_tr_count, render_pass + 1 < (int)rendContext->render_passes.size());
			else
				DrawList(cmdBuffer, ListType_Translucent, true, rendContext->global_param_tr, previous_pass.tr_count, current_pass.tr_count);
        }
		else
			DrawList(cmdBuffer, ListType_Translucent, false, rendContext->global_param_tr, previous_pass.tr_count, current_pass.tr_count);
		previous_pass = current_pass;
    }
    curMainBuffer = nullptr;

	return !rendContext->isRTT;
}

TextureDrawer::TextureDrawer()
{
	if (GetContext()->supportsDynamicLocalRead())
		renderDelegate = std::make_unique<TextureDynamicRender>();
	else
		renderDelegate = std::make_unique<TextureClassicRender>();
}

void TextureDrawer::Init(SamplerManager *samplerManager, ShaderManager *shaderManager, TextureCache *textureCache)
{
	Drawer::Init(samplerManager, shaderManager);
	this->textureCache = textureCache;
}

vk::CommandBuffer TextureDrawer::BeginRenderPass()
{
	DEBUG_LOG(RENDERER, "RenderToTexture packmode=%d stride=%d - %d x %d @ %06x", rendContext->fb_W_CTRL.fb_packmode, rendContext->fb_W_LINESTRIDE * 8,
			rendContext->fbClip.size.x, rendContext->fbClip.size.y, rendContext->fb_W_SOF1 & VRAM_MASK);

	textureAddr = rendContext->fb_W_SOF1 & VRAM_MASK;
	u32 width = rendContext->framebufferWidth;
	u32 height = rendContext->framebufferHeight;
	matrices.CalcMatrices(rendContext, width, height);

	VulkanContext *context = GetContext();
	vk::Device device = context->GetDevice();

	NewImage();
	vk::CommandBuffer commandBuffer = commandPool->Allocate(true);
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	if (!depthAttachment || width > depthAttachment->getExtent().width || height > depthAttachment->getExtent().height)
	{
		if (!depthAttachment)
			depthAttachment = std::make_unique<FramebufferAttachment>(context->GetPhysicalDevice(), device);
		else
			GetContext()->WaitIdle();
		depthAttachment->Init(width, height, GetContext()->GetDepthFormat(),
				vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
				"RTT DEPTH ATTACHMENT");
	}
	vk::Image colorImage;
	vk::ImageView colorImageView;
	vk::ImageLayout colorImageCurrentLayout;

	if (!config::RenderToTextureBuffer)
	{
		int wpo2, hpo2;
		getPvrFramebufferSize(*rendContext, wpo2, hpo2);
		texture = textureCache->getRTTexture(textureAddr, rendContext->fb_W_CTRL.fb_packmode, wpo2, hpo2);
		if (textureCache->IsInFlight(texture, false))
		{
			texture->readOnlyImageView = *texture->imageView;
			texture->deferDeleteResource(commandPool);
		}
		textureCache->SetInFlight(texture);

		constexpr vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
		if (!texture->image || texture->format != vk::Format::eR8G8B8A8Unorm
				|| texture->extent.width != width || texture->extent.height != height
				|| (texture->usageFlags & imageUsage) != imageUsage)
		{
			texture->extent = vk::Extent2D(width, height);
			texture->format = vk::Format::eR8G8B8A8Unorm;
			texture->needsStaging = true;
			texture->CreateImage(vk::ImageTiling::eOptimal, imageUsage,
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
		if (colorAttachments.empty() || width > colorAttachments[0]->getExtent().width || height > colorAttachments[0]->getExtent().height)
		{
			if (colorAttachments.empty())
				colorAttachments.push_back(std::make_unique<FramebufferAttachment>(context->GetPhysicalDevice(), device));
			else
				GetContext()->WaitIdle();
			colorAttachments[0]->Init(width, height, vk::Format::eR8G8B8A8Unorm,
					vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
					"RTT COLOR ATTACHMENT");
			colorImageCurrentLayout = vk::ImageLayout::eUndefined;
		}
		else {
			colorImageCurrentLayout = vk::ImageLayout::eTransferSrcOptimal;
		}
		colorImage = colorAttachments[0]->GetImage();
		colorImageView = colorAttachments[0]->GetImageView();
	}
	this->viewport = vk::Extent2D(width, height);

	setImageLayout(commandBuffer, colorImage, vk::Format::eR8G8B8A8Unorm, 1, colorImageCurrentLayout, vk::ImageLayout::eColorAttachmentOptimal);

	renderDelegate->createAttachments(0, colorImageView, colorImage);
	renderDelegate->beginRender(commandBuffer, 0);
	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)viewport.width, (float)viewport.height, 1.0f, 0.0f));
	Rect scissor = matrices.getBaseScissor();
	baseScissor = vk::Rect2D(vk::Offset2D(scissor.origin.x, scissor.origin.y), vk::Extent2D(scissor.size.x, scissor.size.y));
	commandBuffer.setScissor(0, baseScissor);
	currentCommandBuffer = commandBuffer;

	return commandBuffer;
}

void TextureDrawer::EndRenderPass()
{
	renderDelegate->endRender(currentCommandBuffer);

	u32 fbw = rendContext->framebufferWidth;
	u32 fbh = rendContext->framebufferHeight;

	if (config::RenderToTextureBuffer)
	{
		vk::BufferImageCopy copyRegion(0, fbw, fbh,
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0),
				vk::Extent3D(fbw, fbh, 1));
		currentCommandBuffer.copyImageToBuffer(colorAttachments[0]->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
				*colorAttachments[0]->GetBufferData()->buffer, copyRegion);

		vk::BufferMemoryBarrier bufferMemoryBarrier(
				vk::AccessFlagBits::eTransferWrite,
				vk::AccessFlagBits::eHostRead,
				vk::QueueFamilyIgnored,
				vk::QueueFamilyIgnored,
				*colorAttachments[0]->GetBufferData()->buffer,
				0,
				vk::WholeSize);
		currentCommandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer,
						vk::PipelineStageFlagBits::eHost, {}, nullptr, bufferMemoryBarrier, nullptr);
	}
	currentCommandBuffer.end();

	currentCommandBuffer = nullptr;

	if (config::RenderToTextureBuffer)
	{
		commandPool->EndFrameAndWait();

		u16 *dst = (u16 *)&vram[textureAddr];

		PixelBuffer<u32> tmpBuf;
		tmpBuf.init(fbw, fbh);
		colorAttachments[0]->GetBufferData()->download(fbw * fbh * 4, tmpBuf.data());
		WriteTextureToVRam(fbw, fbh, (u8 *)tmpBuf.data(), dst, rendContext->fb_W_CTRL, rendContext->fb_W_LINESTRIDE * 8, rendContext->fbClip);
	}
	else
	{
		commandPool->EndFrame();
		//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);

		texture->dirty = 0;
		texture->unprotectVRam();
	}
	Drawer::EndRenderPass();
}

ScreenDrawer::ScreenDrawer()
{
	if (GetContext()->supportsDynamicLocalRead())
		renderDelegate = std::make_unique<ScreenDynamicRender>();
	else
		renderDelegate = std::make_unique<ScreenClassicRender>();
}

void ScreenDrawer::Init(SamplerManager *samplerManager, ShaderManager *shaderManager, const vk::Extent2D& viewport)
{
	emulateFramebuffer = config::EmulateFramebuffer;
	if (this->viewport != viewport)
	{
		if (!colorAttachments.empty())
			commandPool->addToFlight(new Deleter(std::move(colorAttachments)));
		if (depthAttachment)
			commandPool->addToFlight(new Deleter(depthAttachment.release()));
	}
	this->viewport = viewport;
	frameRendered = false;

	Drawer::Init(samplerManager, shaderManager);
}

void ScreenDrawer::createAttachments(vk::CommandBuffer cmdBuffer)
{
	if (!depthAttachment)
	{
		depthAttachment = std::make_unique<FramebufferAttachment>(
				GetContext()->GetPhysicalDevice(), GetContext()->GetDevice());
		depthAttachment->Init(viewport.width, viewport.height, GetContext()->GetDepthFormat(),
				vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eTransientAttachment,
				"DEPTH ATTACHMENT");
	}

	while (colorAttachments.size() < GetSwapChainSize())
	{
		colorAttachments.push_back(std::make_unique<FramebufferAttachment>(
				GetContext()->GetPhysicalDevice(), GetContext()->GetDevice()));
		vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment;
		if (config::EmulateFramebuffer)
			usage |= vk::ImageUsageFlagBits::eTransferSrc;
		else
			usage |= vk::ImageUsageFlagBits::eSampled;
		colorAttachments.back()->Init(viewport.width, viewport.height, vk::Format::eR8G8B8A8Unorm, usage,
				"COLOR ATTACHMENT " + std::to_string(colorAttachments.size() - 1));
		setImageLayout(cmdBuffer, colorAttachments.back()->GetImage(), vk::Format::eR8G8B8A8Unorm,
				1, vk::ImageLayout::eUndefined,
				emulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal);
	}
	renderDelegate->createAttachments(GetCurrentImage(), colorAttachments[GetCurrentImage()]->GetImageView(),
			colorAttachments[GetCurrentImage()]->GetImage());
}

vk::CommandBuffer ScreenDrawer::BeginRenderPass()
{
	if (!renderPassStarted)
	{
		NewImage();
		frameRendered = false;
		vk::CommandBuffer commandBuffer = commandPool->Allocate(true);
		commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
		createAttachments(commandBuffer);

		renderDelegate->beginRender(commandBuffer, GetCurrentImage());
		currentCommandBuffer = commandBuffer;
		renderPassStarted = true;
	}
	currentCommandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)viewport.width, (float)viewport.height, 1.0f, 0.0f));

	matrices.CalcMatrices(rendContext, viewport.width, viewport.height);

	SetBaseScissor(viewport);
	currentCommandBuffer.setScissor(0, baseScissor);

	return currentCommandBuffer;
}

void ScreenDrawer::EndRenderPass()
{
	if (!renderPassStarted)
		return;
	renderDelegate->endRender(currentCommandBuffer);
	FramebufferAttachment *colorAttachment = nullptr;
	if (GetCurrentImage() < (int)colorAttachments.size())
		colorAttachment = colorAttachments[GetCurrentImage()].get();
	if (colorAttachment != nullptr)
	{
		if (emulateFramebuffer)
		{
			scaleAndWriteFramebuffer(currentCommandBuffer, colorAttachments[GetCurrentImage()].get());
		}
		else
		{
			currentCommandBuffer.end();
			commandPool->EndFrame();
			aspectRatio = getOutputFramebufferAspectRatio();
		}
		frameRendered = true;
	}
	else {
		currentCommandBuffer.end();
		commandPool->EndFrame();
	}
	currentCommandBuffer = nullptr;
	Drawer::EndRenderPass();
}

bool ScreenDrawer::PresentFrame()
{
	EndRenderPass();
	if (!frameRendered)
		return false;
	frameRendered = false;
	GetContext()->PresentFrame(colorAttachments[GetCurrentImage()]->GetImage(),
			colorAttachments[GetCurrentImage()]->GetImageView(), viewport, aspectRatio);

	return true;
}

void ClassicRender::init(Drawer *drawer)
{
	RenderDelegate::init(drawer);
	if (!framebuffers.empty()) {
		verify(drawer->commandPool != nullptr);
		drawer->commandPool->addToFlight(new Deleter(std::move(framebuffers)));
	}
}

void ClassicRender::term()
{
	framebuffers.clear();
	RenderDelegate::term();
}

void ClassicRender::createAttachments(int index, vk::ImageView imageView, vk::Image image)
{
	std::array<vk::ImageView, 2> attachments = {
			imageView,
			drawer->depthAttachment->GetImageView(),
	};
	if ((int)framebuffers.size() < index + 1)
		framebuffers.resize(index + 1);
	if (!framebuffers[index])
	{
		vk::FramebufferCreateInfo createInfo(vk::FramebufferCreateFlags(), getRenderPass(false),
				attachments, drawer->viewport.width, drawer->viewport.height, 1);
		framebuffers[index] = vkCtx()->GetDevice().createFramebufferUnique(createInfo);
	}
}

void ClassicRender::beginRender(vk::CommandBuffer cmdBuffer, int index)
{
	vk::RenderPass renderPass = getRenderPass(!drawer->rendContext->clearFramebuffer);
	const std::array<vk::ClearValue, 2> clear_colors = { vk::ClearColorValue(std::array<float, 4> { 0.f, 0.f, 0.f, 1.f }), vk::ClearDepthStencilValue { 0.f, 0 } };
	cmdBuffer.beginRenderPass(vk::RenderPassBeginInfo(renderPass, *framebuffers[index],
			vk::Rect2D( { 0, 0 }, drawer->viewport), clear_colors), vk::SubpassContents::eInline);
}

void ClassicRender::endRender(vk::CommandBuffer cmdBuffer) {
	cmdBuffer.endRenderPass();
}

bool ClassicRender::beforeDrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles,
		const PolyParam& poly, u32 first, u32 count)
{
	return poly.tsp.DstSelect != 1 && poly.tsp.SrcSelect != 1;
}

void ScreenClassicRender::init(Drawer *drawer)
{
	ClassicRender::init(drawer);
	std::array<vk::AttachmentDescription, 2> attachmentDescriptions = {
			// Color attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eLoad, vk::AttachmentStoreOp::eStore,
					vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					config::EmulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal,
					config::EmulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal),
			// Depth attachment
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vkCtx()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
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

	vk::SubpassDependency dependency(0, vk::SubpassExternal, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
			vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead,vk::DependencyFlagBits::eByRegion);

	renderPassLoad = vkCtx()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
			attachmentDescriptions,
			subpass,
			dependency));

	attachmentDescriptions[0].loadOp = vk::AttachmentLoadOp::eClear;
	renderPassClear = vkCtx()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(),
			attachmentDescriptions,
			subpass,
			dependency));
	drawer->pipelineManager->setRenderPass(*renderPassClear);
}

void ScreenClassicRender::term()
{
	renderPassLoad.reset();
	renderPassClear.reset();
	ClassicRender::term();
}

void TextureClassicRender::init(Drawer *drawer)
{
	ClassicRender::init(drawer);
	// RTT render pass
	renderToTextureBuffer = config::RenderToTextureBuffer;
	vk::AttachmentDescription attachmentDescriptions[] = {
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vk::Format::eR8G8B8A8Unorm, vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore, vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eColorAttachmentOptimal,
					renderToTextureBuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal),
			vk::AttachmentDescription(vk::AttachmentDescriptionFlags(), vkCtx()->GetDepthFormat(), vk::SampleCountFlagBits::e1,
					vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare, vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eDontCare,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal),
	};
	vk::AttachmentReference colorReference(0, vk::ImageLayout::eColorAttachmentOptimal);
	vk::AttachmentReference depthReference(1, vk::ImageLayout::eDepthStencilAttachmentOptimal);
	vk::SubpassDescription subpass(vk::SubpassDescriptionFlags(), vk::PipelineBindPoint::eGraphics, nullptr, colorReference, nullptr, &depthReference);
	vk::SubpassDependency dependencies[] {
		vk::SubpassDependency(vk::SubpassExternal, 0, vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eColorAttachmentOutput,
				vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eColorAttachmentWrite),
		vk::SubpassDependency(0, vk::SubpassExternal, vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eFragmentShader,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eShaderRead),
	};
	vk::SubpassDependency vramWriteDeps[] {
		vk::SubpassDependency(0, vk::SubpassExternal,
				vk::PipelineStageFlagBits::eColorAttachmentOutput, vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eHost,
				vk::AccessFlagBits::eColorAttachmentWrite, vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eHostRead),
	};

	rttRenderPass = vkCtx()->GetDevice().createRenderPassUnique(vk::RenderPassCreateInfo(vk::RenderPassCreateFlags(), 2, attachmentDescriptions,
			1, &subpass, renderToTextureBuffer ? std::size(vramWriteDeps) : std::size(dependencies), renderToTextureBuffer ? vramWriteDeps : dependencies));
	drawer->pipelineManager->setRenderPass(*rttRenderPass);
}

void TextureClassicRender::term() {
	rttRenderPass.reset();
	ClassicRender::term();
}

void TextureClassicRender::createAttachments(int index, vk::ImageView imageView, vk::Image image)
{
	if (renderToTextureBuffer != config::RenderToTextureBuffer)
	{
		renderToTextureBuffer = config::RenderToTextureBuffer;
		// Recreate render pass, pipelines and delete framebuffers
		init(drawer);
	}
	else {
		// delete framebuffers only
		ClassicRender::init(drawer);
	}
	if (framebuffers.size() > (unsigned)index)
		framebuffers[index].reset();
	ClassicRender::createAttachments(index, imageView, image);
}

void DynamicRender::term() {
	secAccum.reset();
	RenderDelegate::term();
}

void DynamicRender::createAttachments(int index, vk::ImageView imageView, vk::Image image) {
	renderTarget = imageView;
}

void DynamicRender::beginRender(vk::CommandBuffer cmdBuffer, int index)
{
	if (secAccum && (drawer->viewport.width > secAccum->getExtent().width
			|| drawer->viewport.height > secAccum->getExtent().height)) {
		drawer->commandPool->addToFlight(new Deleter(secAccum.release()));
	}
	if (!secAccum)
	{
		secAccum = std::make_unique<FramebufferAttachment>(vkCtx()->GetPhysicalDevice(),
				vkCtx()->GetDevice());
		secAccum->Init(drawer->viewport.width, drawer->viewport.height, vk::Format::eR8G8B8A8Unorm,
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransientAttachment
					| vk::ImageUsageFlagBits::eInputAttachment,
				"2ND ACCUM");
		drawer->secAccumView = secAccum->GetImageView();
	}
	vk::RenderingAttachmentInfoKHR colorAttachments[2] {};
	colorAttachments[0].imageView = renderTarget;
	colorAttachments[0].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
	if (drawer->rendContext->clearFramebuffer) {
		colorAttachments[0].loadOp = vk::AttachmentLoadOp::eClear;
		colorAttachments[0].clearValue = vk::ClearColorValue(std::array<float, 4> { 0.f, 0.f, 0.f, 1.f });
	}
	else {
		colorAttachments[0].loadOp = vk::AttachmentLoadOp::eLoad;
	}
	colorAttachments[0].storeOp = vk::AttachmentStoreOp::eStore;

	colorAttachments[1].imageView = this->secAccum->GetImageView();
	colorAttachments[1].imageLayout = vk::ImageLayout::eRenderingLocalReadKHR;
	colorAttachments[1].loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachments[1].storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachments[1].clearValue.color = vk::ClearColorValue{};

	vk::RenderingAttachmentInfoKHR depthAttachment {};
	depthAttachment.imageView = drawer->depthAttachment->GetImageView();
	depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.clearValue.depthStencil = vk::ClearDepthStencilValue{ 0.f, 0 };

	vk::RenderingInfoKHR renderingInfo {};
	renderingInfo.renderArea = vk::Rect2D({ 0, 0 }, drawer->viewport);
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 2;

	renderingInfo.pColorAttachments = colorAttachments;
	renderingInfo.pDepthAttachment = &depthAttachment;
	renderingInfo.pStencilAttachment = &depthAttachment;

	cmdBuffer.beginRenderingKHR(&renderingInfo);

	const u32 indexes[] { 0, vk::AttachmentUnused };
	const vk::RenderingAttachmentLocationInfoKHR rali { indexes };
	cmdBuffer.setRenderingAttachmentLocationsKHR(&rali);
	const u32 inputIndexes[] { vk::AttachmentUnused, 1 };
	const vk::RenderingInputAttachmentIndexInfoKHR riai { inputIndexes };
	cmdBuffer.setRenderingInputAttachmentIndicesKHR(&riai);

	writingSecAccum = false;
	lastDstSelect = false;

}

void DynamicRender::endRender(vk::CommandBuffer cmdBuffer) {
	cmdBuffer.endRenderingKHR();
}

bool DynamicRender::beforeDrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles,
		const PolyParam& poly, u32 first, u32 count)
{
	if (poly.tsp.DstSelect == 1)
	{
		if (!writingSecAccum)
		{
			writingSecAccum = true;
			const vk::MemoryBarrier memBarrier { vk::AccessFlagBits::eInputAttachmentRead,
				vk::AccessFlagBits::eColorAttachmentWrite };
			cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader,
					vk::PipelineStageFlagBits::eColorAttachmentOutput,
					vk::DependencyFlagBits::eByRegion, memBarrier, nullptr, nullptr);
		}
		if (!lastDstSelect)
		{
			const u32 indexes[] { vk::AttachmentUnused, 0 };
			const vk::RenderingAttachmentLocationInfoKHR rali { indexes };
			cmdBuffer.setRenderingAttachmentLocationsKHR(&rali);
			const u32 inputIndexes[] { vk::AttachmentUnused, vk::AttachmentUnused };
			const vk::RenderingInputAttachmentIndexInfoKHR riai { inputIndexes };
			cmdBuffer.setRenderingInputAttachmentIndicesKHR(&riai);
			lastDstSelect = true;
		}
	}
	else
	{
		if (poly.tsp.SrcSelect == 1 && writingSecAccum)
		{
			writingSecAccum = false;
			const vk::MemoryBarrier memBarrier { vk::AccessFlagBits::eColorAttachmentWrite,
				vk::AccessFlagBits::eInputAttachmentRead };
			cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
					vk::PipelineStageFlagBits::eFragmentShader,
					vk::DependencyFlagBits::eByRegion, memBarrier, nullptr, nullptr);
		}
		if (lastDstSelect)
		{
			const u32 indexes[] { 0, vk::AttachmentUnused };
			const vk::RenderingAttachmentLocationInfoKHR rali { indexes };
			cmdBuffer.setRenderingAttachmentLocationsKHR(&rali);
			const u32 inputIndexes[] { vk::AttachmentUnused, 1 };
			const vk::RenderingInputAttachmentIndexInfoKHR riai { inputIndexes };
			cmdBuffer.setRenderingInputAttachmentIndicesKHR(&riai);
			lastDstSelect = false;
		}
	}
	return true;
}

void ScreenDynamicRender::init(Drawer *drawer)
{
	DynamicRender::init(drawer);
	emulateFramebuffer = config::EmulateFramebuffer;
	renderAttachment = nullptr;
}

void ScreenDynamicRender::beginRender(vk::CommandBuffer cmdBuffer, int index)
{
	renderAttachment = drawer->colorAttachments[index].get();
	setImageLayout(cmdBuffer, renderAttachment->GetImage(), vk::Format::eR8G8B8A8Unorm, 1,
			emulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageLayout::eColorAttachmentOptimal);
	DynamicRender::beginRender(cmdBuffer, index);
}

void ScreenDynamicRender::endRender(vk::CommandBuffer cmdBuffer)
{
	DynamicRender::endRender(cmdBuffer);
	if (renderAttachment == nullptr)
		return;
	setImageLayout(cmdBuffer, renderAttachment->GetImage(), vk::Format::eR8G8B8A8Unorm, 1,
			vk::ImageLayout::eColorAttachmentOptimal,
			emulateFramebuffer ? vk::ImageLayout::eTransferSrcOptimal : vk::ImageLayout::eShaderReadOnlyOptimal);
	renderAttachment = nullptr;
}

void TextureDynamicRender::createAttachments(int index, vk::ImageView imageView, vk::Image image)
{
	DynamicRender::createAttachments(index, imageView, image);
	renderImage = image;
}

void TextureDynamicRender::endRender(vk::CommandBuffer cmdBuffer)
{
	DynamicRender::endRender(cmdBuffer);
	if (renderImage == nullptr)
		return;
	setImageLayout(cmdBuffer, renderImage, vk::Format::eR8G8B8A8Unorm, 1,
			vk::ImageLayout::eColorAttachmentOptimal,
			config::RenderToTextureBuffer ? vk::ImageLayout::eTransferSrcOptimal
					: vk::ImageLayout::eShaderReadOnlyOptimal);
	renderImage = nullptr;
}
