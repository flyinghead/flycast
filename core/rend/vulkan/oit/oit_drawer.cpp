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
#include "oit_drawer.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/sorter.h"

#include <algorithm>
#include <memory>

void OITDrawer::DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool autosort, Pass pass,
		const PolyParam& poly, u32 first, u32 count)
{
	static const float scopeColor[4] = { 0.25f, 0.50f, 0.25f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "DrawPoly(OIT)", scopeColor);

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

	bool twoVolumes = poly.tsp1.full != (u32)-1 || poly.tcw1.full != (u32)-1;

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

	OITDescriptorSets::PushConstants pushConstants = {
			{
				(float)scissorRect.offset.x,
				(float)scissorRect.offset.y,
				(float)scissorRect.offset.x + (float)scissorRect.extent.width,
				(float)scissorRect.offset.y + (float)scissorRect.extent.height
			},
			{ poly.tsp.SrcInstr, poly.tsp.DstInstr, 0, 0 },
			trilinearAlpha,
			palette_index,
	};
	if (twoVolumes)
	{
		pushConstants.blend_mode1 = { poly.tsp1.SrcInstr, poly.tsp1.DstInstr, 0, 0 };
		pushConstants.shading_instr0 = poly.tsp.ShadInstr;
		pushConstants.shading_instr1 = poly.tsp1.ShadInstr;
		pushConstants.fog_control0 = poly.tsp.FogCtrl;
		pushConstants.fog_control1 = poly.tsp1.FogCtrl;
		pushConstants.use_alpha0 = poly.tsp.UseAlpha;
		pushConstants.use_alpha1 = poly.tsp1.UseAlpha;
		pushConstants.ignore_tex_alpha0 = poly.tsp.IgnoreTexA;
		pushConstants.ignore_tex_alpha1 = poly.tsp1.IgnoreTexA;
	}
	cmdBuffer.pushConstants<OITDescriptorSets::PushConstants>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
	if (!poly.isNaomi2())
	{
		OITDescriptorSets::VtxPushConstants vtxPushConstants {};
		if (listType == ListType_Translucent) {
			u32 firstVertexIdx = pvrrc.idx[pvrrc.global_param_tr[0].first];
			vtxPushConstants.polyNumber = (int)((&poly - &pvrrc.global_param_tr[0]) << 17) - firstVertexIdx;
		};
		cmdBuffer.pushConstants<OITDescriptorSets::VtxPushConstants>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eVertex,
				sizeof(OITDescriptorSets::PushConstants), vtxPushConstants);
	}

	if (poly.pcw.Texture == 1 || poly.isNaomi2())
	{
		vk::DeviceSize offset = 0;
		u32 polyNumber = 0;
		if (poly.isNaomi2())
		{
			switch (listType)
			{
			case ListType_Opaque:
				offset = offsets.naomi2OpaqueOffset;
				polyNumber = &poly - &pvrrc.global_param_op[0];
				break;
			case ListType_Punch_Through:
				offset = offsets.naomi2PunchThroughOffset;
				polyNumber = &poly - &pvrrc.global_param_pt[0];
				break;
			case ListType_Translucent:
				offset = offsets.naomi2TranslucentOffset;
				polyNumber = &poly - &pvrrc.global_param_tr[0];
				break;
			}
		}
		descriptorSets.bindPerPolyDescriptorSets(cmdBuffer, poly, polyNumber, curMainBuffer, offset, offsets.lightsOffset,
				listType == ListType_Punch_Through);
	}

	vk::Pipeline pipeline = pipelineManager->GetPipeline(listType, autosort, poly, pass, gpuPalette);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

	cmdBuffer.drawIndexed(count, 1, first, 0, 0);
}

void OITDrawer::DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, Pass pass,
		const std::vector<PolyParam>& polys, u32 first, u32 last)
{
	if (first == last)
		return;

	static const float scopeColor[4] = { 0.50f, 0.25f, 0.50f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "DrawList(OIT)", scopeColor);

	const PolyParam *pp_end = polys.data() + last;
	for (const PolyParam *pp = &polys[first]; pp != pp_end; pp++)
		if (pp->count > 2)
			DrawPoly(cmdBuffer, listType, sortTriangles, pass, *pp, pp->first, pp->count);
}

template<bool Translucent>
void OITDrawer::DrawModifierVolumes(const vk::CommandBuffer& cmdBuffer, int first, int count, const ModifierVolumeParam *modVolParams)
{
	if (count == 0 || pvrrc.modtrig.empty() || !config::ModifierVolumes)
		return;

	static const float scopeColor[4] = { 0.75f, 0.25f, 0.25f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "DrawModVols(OIT)", scopeColor);

	cmdBuffer.bindVertexBuffers(0, curMainBuffer, offsets.modVolOffset);
	SetScissor(cmdBuffer, baseScissor);

	const ModifierVolumeParam *params = &modVolParams[first];

	int mod_base = -1;
	vk::Pipeline pipeline;

	for (int cmv = 0; cmv < count; cmv++)
	{
		const ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		verify(param.first >= 0 && param.first + param.count <= (u32)pvrrc.modtrig.size());

		if (mod_base == -1)
			mod_base = param.first;

		if (!param.isp.VolumeLast && mv_mode > 0)
		{
			// OR'ing (open volume or quad)
			if (Translucent)
				pipeline = pipelineManager->GetTrModifierVolumePipeline(ModVolMode::Or, param.isp.CullMode, param.isNaomi2());
			else
				pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Or, param.isp.CullMode, param.isNaomi2());
		}
		else
		{
			// XOR'ing (closed volume)
			if (Translucent)
				pipeline = pipelineManager->GetTrModifierVolumePipeline(ModVolMode::Xor, param.isp.CullMode, param.isNaomi2());
			else
				pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Xor, param.isp.CullMode, param.isNaomi2());
		}
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

		vk::DeviceSize uniformOffset = Translucent ? offsets.naomi2TrModVolOffset : offsets.naomi2ModVolOffset;
		descriptorSets.bindPerPolyDescriptorSets(cmdBuffer, param, first + cmv, curMainBuffer, uniformOffset);

		cmdBuffer.draw(param.count * 3, 1, param.first * 3, 0);

		if (mv_mode == 1 || mv_mode == 2)
		{
			//Sum the area
			if (Translucent)
			{
				vk::MemoryBarrier barrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
				cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
						vk::DependencyFlagBits::eByRegion, barrier, nullptr, nullptr);
				pipeline = pipelineManager->GetTrModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isp.CullMode, param.isNaomi2());
			}
			else
				pipeline = pipelineManager->GetModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isp.CullMode, param.isNaomi2());
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			cmdBuffer.draw((param.first + param.count - mod_base) * 3, 1, mod_base * 3, 0);

			mod_base = -1;
			if (Translucent)
			{
				vk::MemoryBarrier barrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
				cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
						vk::DependencyFlagBits::eByRegion, barrier, nullptr, nullptr);
			}
		}
	}
	cmdBuffer.bindVertexBuffers(0, curMainBuffer, {0});
}

void OITDrawer::UploadMainBuffer(const OITDescriptorSets::VertexShaderUniforms& vertexUniforms,
		const OITDescriptorSets::FragmentShaderUniforms& fragmentUniforms)
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

	// Translucent poly params
	std::vector<u32> trPolyParams(pvrrc.global_param_tr.size() * 2);
	if (pvrrc.global_param_tr.empty())
		trPolyParams.push_back(0);	// makes the validation layers happy
	else
	{
		const PolyParam *pp_end = &pvrrc.global_param_tr.back() + 1;
		const PolyParam *pp = &pvrrc.global_param_tr[0];
		for (int i = 0; pp != pp_end; i += 2, pp++)
		{
			trPolyParams[i] = (pp->tsp.full & 0xffff00c0) | ((pp->isp.full >> 16) & 0xe400) | ((pp->pcw.full >> 7) & 1);
			trPolyParams[i + 1] = pp->tsp1.full;
		}
	}
	offsets.polyParamsSize = trPolyParams.size() * 4;
	offsets.polyParamsOffset = packer.addStorage(trPolyParams.data(), offsets.polyParamsSize);

	std::vector<u8> n2uniforms;
	if (settings.platform.isNaomi2())
	{
		packNaomi2Uniforms(packer, offsets, n2uniforms, true);
		offsets.lightsOffset = packNaomi2Lights(packer);
	}

	BufferData *buffer = GetMainBuffer(packer.size());
	packer.upload(*buffer);
	curMainBuffer = buffer->buffer.get();
}

vk::Framebuffer OITTextureDrawer::getFramebuffer(int renderPass, int renderPassCount)
{
	if (renderPass < renderPassCount - 1)
	{
		framebufferIndex = (renderPassCount - renderPass) % 2;
		return *tempFramebuffers[framebufferIndex];
	}
	else {
		framebufferIndex = 0;
		return *framebuffer;
	}
}

bool OITDrawer::Draw(const Texture *fogTexture, const Texture *paletteTexture)
{
	vk::CommandBuffer cmdBuffer = NewFrame();

	static const float scopeColor[4] = { 0.75f, 0.75f, 0.75f, 1.0f };
	CommandBufferDebugScope _(cmdBuffer, "Draw(OIT)", scopeColor);

	if (needAttachmentTransition)
	{
		needAttachmentTransition = false;
		// Not convinced that this is really needed but it makes validation layers happy
		for (auto& attachment : colorAttachments)
			// FIXME should be eTransferSrcOptimal if fullFB (screen) or copy to vram (rtt) -> 1 validation error at startup
			setImageLayout(cmdBuffer, attachment->GetImage(), vk::Format::eR8G8B8A8Unorm, 1,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
		for (auto& attachment : depthAttachments)
			setImageLayout(cmdBuffer, attachment->GetImage(), GetContext()->GetDepthFormat(), 1,
					vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
	}

	OITDescriptorSets::VertexShaderUniforms vtxUniforms;
	vtxUniforms.ndcMat = matrices.GetNormalMatrix();

	OITDescriptorSets::FragmentShaderUniforms fragUniforms = MakeFragmentUniforms<OITDescriptorSets::FragmentShaderUniforms>();
	fragUniforms.shade_scale_factor = FPU_SHAD_SCALE.scale_factor / 256.f;
	// sizeof(Pixel) == 16
	fragUniforms.pixelBufferSize = std::min<u64>(config::PixelBufferSize, GetContext()->GetMaxMemoryAllocationSize()) / 16;
	fragUniforms.viewportWidth = maxWidth;
	dithering = config::EmulateFramebuffer && pvrrc.fb_W_CTRL.fb_dither && pvrrc.fb_W_CTRL.fb_packmode <= 3;
	if (dithering)
	{
		switch (pvrrc.fb_W_CTRL.fb_packmode)
		{
		case 0: // 0555 KRGB 16 bit
		case 3: // 1555 ARGB 16 bit
			fragUniforms.ditherColorMax[0] = fragUniforms.ditherColorMax[1] = fragUniforms.ditherColorMax[2] = 31.f;
			fragUniforms.ditherColorMax[3] = 255.f;
			break;
		case 1: // 565 RGB 16 bit
			fragUniforms.ditherColorMax[0] = fragUniforms.ditherColorMax[2] = 31.f;
			fragUniforms.ditherColorMax[1] = 63.f;
			fragUniforms.ditherColorMax[3] = 255.f;
			break;
		case 2: // 4444 ARGB 16 bit
			fragUniforms.ditherColorMax[0] = fragUniforms.ditherColorMax[1]
				= fragUniforms.ditherColorMax[2] = fragUniforms.ditherColorMax[3] = 15.f;
			break;
		default:
			break;
		}
	}

	currentScissor = vk::Rect2D();

	bool firstFrameAfterInit = oitBuffers->isFirstFrameAfterInit();
	oitBuffers->OnNewFrame(cmdBuffer);

	if (VulkanContext::Instance()->hasProvokingVertex())
	{
		// Pipelines are using VK_EXT_provoking_vertex, no need to
		// re-order vertices
	}
	else
	{
		setFirstProvokingVertex(pvrrc);
	}

	// Upload vertex and index buffers
	UploadMainBuffer(vtxUniforms, fragUniforms);

	quadBuffer->Update();

	// Update per-frame descriptor set and bind it
	descriptorSets.updateUniforms(curMainBuffer, (u32)offsets.vertexUniformOffset, (u32)offsets.fragmentUniformOffset,
			fogTexture->GetImageView(), (u32)offsets.polyParamsOffset,
			(u32)offsets.polyParamsSize, depthAttachments[0]->GetStencilView(),
			depthAttachments[0]->GetImageView(), paletteTexture->GetImageView(), oitBuffers);
	descriptorSets.bindPerFrameDescriptorSets(cmdBuffer);
	descriptorSets.updateColorInputDescSet(0, colorAttachments[0]->GetImageView());
	descriptorSets.updateColorInputDescSet(1, colorAttachments[1]->GetImageView());

	// Bind vertex and index buffers
	cmdBuffer.bindVertexBuffers(0, curMainBuffer, {0});
	cmdBuffer.bindIndexBuffer(curMainBuffer, offsets.indexOffset, vk::IndexType::eUint32);

	// Make sure to push constants even if not used
	OITDescriptorSets::PushConstants pushConstants = { };
	cmdBuffer.pushConstants<OITDescriptorSets::PushConstants>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
	OITDescriptorSets::VtxPushConstants vtxPushConstants = { };
	cmdBuffer.pushConstants<OITDescriptorSets::VtxPushConstants>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eVertex,
			sizeof(pushConstants), vtxPushConstants);

	const std::array<vk::ClearValue, 4> clear_colors = {
			pvrrc.isRTT ? vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}) : getBorderColor(),
			pvrrc.isRTT ? vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}) : getBorderColor(),
			vk::ClearDepthStencilValue{ 0.f, 0 },
			vk::ClearDepthStencilValue{ 0.f, 0 },
	};

	RenderPass previous_pass = {};
    for (int render_pass = 0; render_pass < (int)pvrrc.render_passes.size(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes[render_pass];

        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d TrMV %d autosort %d", render_pass + 1,
        		current_pass.op_count - previous_pass.op_count,
				current_pass.pt_count - previous_pass.pt_count,
				current_pass.tr_count - previous_pass.tr_count,
				current_pass.mvo_count - previous_pass.mvo_count,
				current_pass.mv_op_tr_shared ? current_pass.mvo_count - previous_pass.mvo_count : current_pass.mvo_tr_count - previous_pass.mvo_tr_count,
				current_pass.autosort);

        // Reset the pixel counter
    	oitBuffers->ResetPixelCounter(cmdBuffer);

    	const bool initialPass = render_pass == 0;
    	const bool finalPass = render_pass == (int)pvrrc.render_passes.size() - 1;

    	vk::Framebuffer targetFramebuffer = getFramebuffer(render_pass, pvrrc.render_passes.size());
    	cmdBuffer.beginRenderPass(
    			vk::RenderPassBeginInfo(pipelineManager->GetRenderPass(initialPass, finalPass, initialPass && pvrrc.clearFramebuffer),
    					targetFramebuffer, viewport, clear_colors),
    			vk::SubpassContents::eInline);

		// Depth + stencil subpass
		DrawList(cmdBuffer, ListType_Opaque, false, Pass::Depth, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count);
		DrawList(cmdBuffer, ListType_Punch_Through, false, Pass::Depth, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count);

		DrawModifierVolumes<false>(cmdBuffer, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count, pvrrc.global_param_mvo.data());

		// Color subpass
		cmdBuffer.nextSubpass(vk::SubpassContents::eInline);

		// OP + PT
		DrawList(cmdBuffer, ListType_Opaque, false, Pass::Color, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count);
		DrawList(cmdBuffer, ListType_Punch_Through, false, Pass::Color, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count);

		// TR
		if (firstFrameAfterInit)
		{
			// Clear abuffers
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager->GetClearPipeline());
			quadBuffer->Bind(cmdBuffer);
			quadBuffer->Draw(cmdBuffer);

			vk::MemoryBarrier memoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
			cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
					vk::DependencyFlagBits::eByRegion, memoryBarrier, nullptr, nullptr);
			cmdBuffer.bindVertexBuffers(0, curMainBuffer, {0});
			firstFrameAfterInit = false;
		}
		if (current_pass.autosort)
			DrawList(cmdBuffer, ListType_Translucent, true, Pass::OIT, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
		else
			DrawList(cmdBuffer, ListType_Translucent, false, Pass::Color, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);

		// Final subpass
		cmdBuffer.nextSubpass(vk::SubpassContents::eInline);
		// Bind the input attachment (OP+PT)
		descriptorSets.bindColorInputDescSet(cmdBuffer, 1 - getFramebufferIndex());

		if (initialPass && !pvrrc.isRTT && clearNeeded[getFramebufferIndex()])
		{
			clearNeeded[getFramebufferIndex()] = false;
			SetScissor(cmdBuffer, viewport);
			cmdBuffer.clearAttachments(vk::ClearAttachment(vk::ImageAspectFlagBits::eColor, 0, clear_colors[0]),
					vk::ClearRect(viewport, 0, 1));
		}

		// Tr modifier volumes
		if (GetContext()->GetVendorID() != VulkanContext::VENDOR_QUALCOMM)	// Adreno bug
		{
			if (current_pass.mv_op_tr_shared)
				DrawModifierVolumes<true>(cmdBuffer, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count, pvrrc.global_param_mvo.data());
			else
				DrawModifierVolumes<true>(cmdBuffer, previous_pass.mvo_tr_count, current_pass.mvo_tr_count - previous_pass.mvo_tr_count, pvrrc.global_param_mvo_tr.data());
		}

		SetScissor(cmdBuffer, viewport);
		vk::Pipeline pipeline = pipelineManager->GetFinalPipeline(dithering && finalPass);
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		quadBuffer->Bind(cmdBuffer);
		quadBuffer->Draw(cmdBuffer);

		if (!finalPass)
		{
	    	// Re-bind vertex and index buffers
	    	cmdBuffer.bindVertexBuffers(0, curMainBuffer, {0});
	    	cmdBuffer.bindIndexBuffer(curMainBuffer, offsets.indexOffset, vk::IndexType::eUint32);

			// Tr depth-only pass
			DrawList(cmdBuffer, ListType_Translucent, current_pass.autosort, Pass::Depth, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
		}

		cmdBuffer.endRenderPass();
		previous_pass = current_pass;
    }
    curMainBuffer = nullptr;

	return !pvrrc.isRTT;
}

void OITDrawer::MakeBuffers(int width, int height, vk::ImageUsageFlags colorUsage)
{
	oitBuffers->Init(width, height);

	if (width <= maxWidth && height <= maxHeight && colorUsage == currentBufferUsage)
		return;
	maxWidth = std::max(maxWidth, width);
	maxHeight = std::max(maxHeight, height);
	currentBufferUsage = colorUsage;

	for (auto& framebuffer : tempFramebuffers) {
		if (framebuffer)
			commandPool->addToFlight(new Deleter(std::move(framebuffer)));
	}

	vk::Device device = GetContext()->GetDevice();
	vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment
			| colorUsage;
	for (auto& attachment : colorAttachments)
	{
		if (attachment)
			commandPool->addToFlight(new Deleter(std::move(attachment)));
		attachment = std::make_unique<FramebufferAttachment>(
				GetContext()->GetPhysicalDevice(), device);
		attachment->Init(maxWidth, maxHeight, vk::Format::eR8G8B8A8Unorm, usage,
				"COLOR ATTACHMENT " + std::to_string(&attachment - &colorAttachments[0]));
	}

	for (auto& attachment : depthAttachments)
	{
		if (attachment)
			commandPool->addToFlight(new Deleter(std::move(attachment)));
		attachment = std::make_unique<FramebufferAttachment>(
				GetContext()->GetPhysicalDevice(), device);
		attachment->Init(maxWidth, maxHeight, GetContext()->GetDepthFormat(),
				vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
				"DEPTH ATTACHMENT" + std::to_string(&attachment - &depthAttachments[0]));
	}
	needAttachmentTransition = true;

	std::array<vk::ImageView, 4> attachments = {
			colorAttachments[0]->GetImageView(),
			colorAttachments[1]->GetImageView(),
			depthAttachments[0]->GetImageView(),
			depthAttachments[1]->GetImageView(),
	};
	vk::FramebufferCreateInfo createInfo(vk::FramebufferCreateFlags(), pipelineManager->GetRenderPass(true, true),
			attachments, maxWidth, maxHeight, 1);
	tempFramebuffers[0] = device.createFramebufferUnique(createInfo);
	attachments[0] = attachments[1];
	attachments[1] = colorAttachments[0]->GetImageView();
	tempFramebuffers[1] = device.createFramebufferUnique(createInfo);
}

vk::Framebuffer OITScreenDrawer::getFramebuffer(int renderPass, int renderPassCount)
{
	framebufferIndex = 1 - framebufferIndex;
	vk::Framebuffer framebuffer = tempFramebuffers[framebufferIndex].get();
	return framebuffer;
}

void OITScreenDrawer::MakeFramebuffers(const vk::Extent2D& viewport)
{
	this->viewport.offset.x = 0;
	this->viewport.offset.y = 0;
	this->viewport.extent = viewport;

	// make sure all attachments have the same dimensions
	maxWidth = 0;
	maxHeight = 0;
	MakeBuffers(viewport.width, viewport.height, config::EmulateFramebuffer ? vk::ImageUsageFlagBits::eTransferSrc : vk::ImageUsageFlagBits::eSampled);

	clearNeeded = { true, true };
}

vk::CommandBuffer OITTextureDrawer::NewFrame()
{
	DEBUG_LOG(RENDERER, "RenderToTexture packmode=%d stride=%d - %d x %d @ %06x", pvrrc.fb_W_CTRL.fb_packmode, pvrrc.fb_W_LINESTRIDE * 8,
			pvrrc.fb_X_CLIP.max + 1, pvrrc.fb_Y_CLIP.max + 1, pvrrc.fb_W_SOF1 & VRAM_MASK);
	NewImage();

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

	vk::CommandBuffer commandBuffer = commandPool->Allocate(true);
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	MakeBuffers(widthPow2, heightPow2, config::RenderToTextureBuffer ? vk::ImageUsageFlagBits::eTransferSrc : vk::ImageUsageFlagBits::eSampled);

	vk::ImageView colorImageView;
	vk::ImageLayout colorImageCurrentLayout;

	if (!config::RenderToTextureBuffer)
	{
		texture = textureCache->getRTTexture(textureAddr, pvrrc.fb_W_CTRL.fb_packmode, origWidth, origHeight);
		if (textureCache->IsInFlight(texture, false))
		{
			texture->readOnlyImageView = *texture->imageView;
			texture->deferDeleteResource(commandPool);
		}
		textureCache->SetInFlight(texture);

		constexpr vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
		if (!texture->image || texture->format != vk::Format::eR8G8B8A8Unorm
				|| texture->extent.width != widthPow2 || texture->extent.height != heightPow2
				|| (texture->usageFlags & imageUsage) != imageUsage)
		{
			texture->extent = vk::Extent2D(widthPow2, heightPow2);
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
		if (!colorAttachment || widthPow2 > colorAttachment->getExtent().width || heightPow2 > colorAttachment->getExtent().height)
		{
			if (!colorAttachment)
				colorAttachment = std::make_unique<FramebufferAttachment>(context->GetPhysicalDevice(), device);
			else
				GetContext()->WaitIdle();
			colorAttachment->Init(widthPow2, heightPow2, vk::Format::eR8G8B8A8Unorm,
					vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc,
					"RTT COLOR ATTACHMENT");
			colorImageCurrentLayout = vk::ImageLayout::eUndefined;
		}
		else
			colorImageCurrentLayout = vk::ImageLayout::eTransferSrcOptimal;
		colorImage = colorAttachment->GetImage();
		colorImageView = colorAttachment->GetImageView();
	}
	viewport.offset.x = 0;
	viewport.offset.y = 0;
	viewport.extent.width = widthPow2;
	viewport.extent.height = heightPow2;

	setImageLayout(commandBuffer, colorImage, vk::Format::eR8G8B8A8Unorm, 1, colorImageCurrentLayout, vk::ImageLayout::eColorAttachmentOptimal);

	std::array<vk::ImageView, 4> imageViews = {
		colorImageView,
		colorAttachments[1]->GetImageView(),
		depthAttachments[0]->GetImageView(),
		depthAttachments[1]->GetImageView(),
	};
	if (framebuffer)
		commandPool->addToFlight(new Deleter(std::move(framebuffer)));
	framebuffer = device.createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(),
			rttPipelineManager->GetRenderPass(true, true), imageViews, widthPow2, heightPow2, 1));

	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)upscaledWidth, (float)upscaledHeight, 1.0f, 0.0f));
	u32 minX = pvrrc.getFramebufferMinX() * upscaledWidth / origWidth;
	u32 minY = pvrrc.getFramebufferMinY() * upscaledHeight / origHeight;
	getRenderToTextureDimensions(minX, minY, widthPow2, heightPow2);
	baseScissor = vk::Rect2D(vk::Offset2D(minX, minY), vk::Extent2D(upscaledWidth, upscaledHeight));
	commandBuffer.setScissor(0, baseScissor);
	currentCommandBuffer = commandBuffer;

	return commandBuffer;
}

void OITTextureDrawer::EndFrame()
{
	u32 clippedWidth = pvrrc.getFramebufferWidth();
	u32 clippedHeight = pvrrc.getFramebufferHeight();

	if (config::RenderToTextureBuffer)
	{
		vk::BufferImageCopy copyRegion(0, clippedWidth, clippedHeight,
				vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0),
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

	colorImage = nullptr;
	currentCommandBuffer = nullptr;

	if (config::RenderToTextureBuffer)
	{
		commandPool->EndFrameAndWait();

		u16 *dst = (u16 *)&vram[textureAddr];

		PixelBuffer<u32> tmpBuf;
		tmpBuf.init(clippedWidth, clippedHeight);
		colorAttachment->GetBufferData()->download(clippedWidth * clippedHeight * 4, tmpBuf.data());
		WriteTextureToVRam(clippedWidth, clippedHeight, (u8 *)tmpBuf.data(), dst, pvrrc.fb_W_CTRL, pvrrc.fb_W_LINESTRIDE * 8);
	}
	else
	{
		commandPool->EndFrame();
		//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);

		texture->dirty = 0;
		texture->unprotectVRam();
	}
}

vk::CommandBuffer OITScreenDrawer::NewFrame()
{
	if (!frameStarted)
	{
		frameStarted = true;
		frameRendered = false;
		NewImage();
		currentCommandBuffer = commandPool->Allocate(true);
		currentCommandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));
	}
	matrices.CalcMatrices(&pvrrc, viewport.extent.width, viewport.extent.height);

	SetBaseScissor(viewport.extent);

	currentCommandBuffer.setScissor(0, baseScissor);
	currentCommandBuffer.setViewport(0, vk::Viewport((float)viewport.offset.x, (float)viewport.offset.y, (float)viewport.extent.width, (float)viewport.extent.height, 1.0f, 0.0f));

	return currentCommandBuffer;
}
