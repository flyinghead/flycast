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

#include <algorithm>

void OITDrawer::DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool autosort, Pass pass,
		const PolyParam& poly, u32 first, u32 count)
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

	bool twoVolumes = poly.tsp1.full != (u32)-1 || poly.tcw1.full != (u32)-1;

	bool gpuPalette = poly.texture != nullptr ? poly.texture->gpuPalette : false;

	float palette_index = 0.f;
	if (poly.tcw.PixelFmt == PixelPal4)
		palette_index = float(poly.tcw.PalSelect << 4) / 1023.f;
	else
		palette_index = float((poly.tcw.PalSelect >> 4) << 8) / 1023.f;

	OITDescriptorSets::PushConstants pushConstants = {
			{
				(float)scissorRect.offset.x,
				(float)scissorRect.offset.y,
				(float)scissorRect.offset.x + (float)scissorRect.extent.width,
				(float)scissorRect.offset.y + (float)scissorRect.extent.height
			},
			{ poly.tsp.SrcInstr, poly.tsp.DstInstr, 0, 0 },
			trilinearAlpha,
			listType == ListType_Translucent ? (int)(&poly - pvrrc.global_param_tr.head()) : 0,
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

	bool needTexture = poly.pcw.Texture;
	if (needTexture)
		GetCurrentDescSet().SetTexture((Texture *)poly.texture, poly.tsp, (Texture *)poly.texture1, poly.tsp1);

	vk::Pipeline pipeline = pipelineManager->GetPipeline(listType, autosort, poly, pass, gpuPalette);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	if (needTexture)
		GetCurrentDescSet().BindPerPolyDescriptorSets(cmdBuffer, (Texture *)poly.texture, poly.tsp, (Texture *)poly.texture1, poly.tsp1);

	cmdBuffer.drawIndexed(count, 1, first, 0, 0);
}

void OITDrawer::DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, Pass pass,
		const List<PolyParam>& polys, u32 first, u32 last)
{
	const PolyParam *pp_end = polys.head() + last;
	for (const PolyParam *pp = polys.head() + first; pp != pp_end; pp++)
		if (pp->count > 2)
			DrawPoly(cmdBuffer, listType, sortTriangles, pass, *pp, pp->first, pp->count);
}

template<bool Translucent>
void OITDrawer::DrawModifierVolumes(const vk::CommandBuffer& cmdBuffer, int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0 || !config::ModifierVolumes)
		return;

	vk::Buffer buffer = GetMainBuffer(0)->buffer.get();
	cmdBuffer.bindVertexBuffers(0, 1, &buffer, &offsets.modVolOffset);
	SetScissor(cmdBuffer, baseScissor);

	ModifierVolumeParam* params = Translucent ? &pvrrc.global_param_mvo_tr.head()[first] : &pvrrc.global_param_mvo.head()[first];

	int mod_base = -1;
	vk::Pipeline pipeline;

	for (int cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		verify(param.first >= 0 && param.first + param.count <= (u32)pvrrc.modtrig.used());

		if (mod_base == -1)
			mod_base = param.first;

		if (!param.isp.VolumeLast && mv_mode > 0)
		{
			// OR'ing (open volume or quad)
			if (Translucent)
				pipeline = pipelineManager->GetTrModifierVolumePipeline(ModVolMode::Or, param.isp.CullMode);
			else
				pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Or, param.isp.CullMode);
		}
		else
		{
			// XOR'ing (closed volume)
			if (Translucent)
				pipeline = pipelineManager->GetTrModifierVolumePipeline(ModVolMode::Xor, param.isp.CullMode);
			else
				pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Xor, param.isp.CullMode);
		}
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		cmdBuffer.draw(param.count * 3, 1, param.first * 3, 0);

		if (mv_mode == 1 || mv_mode == 2)
		{
			//Sum the area
			if (Translucent)
			{
				vk::MemoryBarrier barrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
				cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
						vk::DependencyFlagBits::eByRegion, barrier, nullptr, nullptr);
				pipeline = pipelineManager->GetTrModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isp.CullMode);
			}
			else
				pipeline = pipelineManager->GetModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion, param.isp.CullMode);
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
	const vk::DeviceSize offset = 0;
	cmdBuffer.bindVertexBuffers(0, 1, &buffer, &offset);
}

void OITDrawer::UploadMainBuffer(const OITDescriptorSets::VertexShaderUniforms& vertexUniforms,
		const OITDescriptorSets::FragmentShaderUniforms& fragmentUniforms)
{
	using VertexShaderUniforms = OITDescriptorSets::VertexShaderUniforms;
	using FragmentShaderUniforms = OITDescriptorSets::FragmentShaderUniforms;

	// TODO Put this logic in an allocator
	std::vector<const void *> chunks;
	std::vector<u32> chunkSizes;

	// Vertex
	chunks.push_back(pvrrc.verts.head());
	chunkSizes.push_back(pvrrc.verts.bytes());

	u32 padding = align(pvrrc.verts.bytes(), 4);
	offsets.modVolOffset = pvrrc.verts.bytes() + padding;
	chunks.push_back(nullptr);
	chunkSizes.push_back(padding);

	// Modifier Volumes
	chunks.push_back(pvrrc.modtrig.head());
	chunkSizes.push_back(pvrrc.modtrig.bytes());
	padding = align(offsets.modVolOffset + pvrrc.modtrig.bytes(), 4);
	offsets.indexOffset = offsets.modVolOffset + pvrrc.modtrig.bytes() + padding;
	chunks.push_back(nullptr);
	chunkSizes.push_back(padding);

	// Index
	chunks.push_back(pvrrc.idx.head());
	chunkSizes.push_back(pvrrc.idx.bytes());

	// Uniform buffers
	u32 indexSize = pvrrc.idx.bytes();
	padding = align(offsets.indexOffset + indexSize, std::max(4, (int)GetContext()->GetUniformBufferAlignment()));
	offsets.vertexUniformOffset = offsets.indexOffset + indexSize + padding;
	chunks.push_back(nullptr);
	chunkSizes.push_back(padding);

	chunks.push_back(&vertexUniforms);
	chunkSizes.push_back(sizeof(vertexUniforms));
	padding = align(offsets.vertexUniformOffset + sizeof(VertexShaderUniforms), std::max(4, (int)GetContext()->GetUniformBufferAlignment()));
	offsets.fragmentUniformOffset = offsets.vertexUniformOffset + sizeof(VertexShaderUniforms) + padding;
	chunks.push_back(nullptr);
	chunkSizes.push_back(padding);

	chunks.push_back(&fragmentUniforms);
	chunkSizes.push_back(sizeof(fragmentUniforms));

	// Translucent poly params
	padding = align(offsets.fragmentUniformOffset + sizeof(FragmentShaderUniforms), std::max(4, (int)GetContext()->GetStorageBufferAlignment()));
	offsets.polyParamsOffset = offsets.fragmentUniformOffset + sizeof(FragmentShaderUniforms) + padding;
	chunks.push_back(nullptr);
	chunkSizes.push_back(padding);

	std::vector<u32> trPolyParams(pvrrc.global_param_tr.used() * 2);
	if (pvrrc.global_param_tr.used() == 0)
		trPolyParams.push_back(0);	// makes the validation layers happy
	else
	{
		const PolyParam *pp_end = pvrrc.global_param_tr.LastPtr(0);
		const PolyParam *pp = pvrrc.global_param_tr.head();
		for (int i = 0; pp != pp_end; i += 2, pp++)
		{
			trPolyParams[i] = (pp->tsp.full & 0xffff00c0) | ((pp->isp.full >> 16) & 0xe400) | ((pp->pcw.full >> 7) & 1);
			trPolyParams[i + 1] = pp->tsp1.full;
		}
	}
	offsets.polyParamsSize = trPolyParams.size() * 4;
	chunks.push_back(trPolyParams.data());
	chunkSizes.push_back((u32)offsets.polyParamsSize);
	u32 totalSize = (u32)(offsets.polyParamsOffset + offsets.polyParamsSize);

	BufferData *buffer = GetMainBuffer(totalSize);
	buffer->upload(chunks.size(), &chunkSizes[0], &chunks[0]);
}

bool OITDrawer::Draw(const Texture *fogTexture, const Texture *paletteTexture)
{
	vk::CommandBuffer cmdBuffer = NewFrame();

	if (needDepthTransition)
	{
		needDepthTransition = false;
		// Not convinced that this is really needed but it makes validation layers happy
		for (auto& attachment : depthAttachments)
			setImageLayout(cmdBuffer, attachment->GetImage(), GetContext()->GetDepthFormat(), 1, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilReadOnlyOptimal);
	}

	OITDescriptorSets::VertexShaderUniforms vtxUniforms;
	vtxUniforms.normal_matrix = matrices.GetNormalMatrix();

	OITDescriptorSets::FragmentShaderUniforms fragUniforms = MakeFragmentUniforms<OITDescriptorSets::FragmentShaderUniforms>();
	fragUniforms.shade_scale_factor = FPU_SHAD_SCALE.scale_factor / 256.f;
	// sizeof(Pixel) == 16
	fragUniforms.pixelBufferSize = std::min<u64>(config::PixelBufferSize, GetContext()->GetMaxMemoryAllocationSize()) / 16;
	fragUniforms.viewportWidth = maxWidth;

	currentScissor = vk::Rect2D();

	oitBuffers->OnNewFrame(cmdBuffer);

	SetProvokingVertices();

	// Upload vertex and index buffers
	UploadMainBuffer(vtxUniforms, fragUniforms);

	quadBuffer->Update();

	// Update per-frame descriptor set and bind it
	const vk::Buffer mainBuffer = GetMainBuffer(0)->buffer.get();
	GetCurrentDescSet().UpdateUniforms(mainBuffer, (u32)offsets.vertexUniformOffset, (u32)offsets.fragmentUniformOffset,
			fogTexture->GetImageView(), (u32)offsets.polyParamsOffset,
			(u32)offsets.polyParamsSize, depthAttachments[0]->GetStencilView(),
			depthAttachments[0]->GetImageView(), paletteTexture->GetImageView());
	GetCurrentDescSet().BindPerFrameDescriptorSets(cmdBuffer);
	GetCurrentDescSet().UpdateColorInputDescSet(0, colorAttachments[0]->GetImageView());
	GetCurrentDescSet().UpdateColorInputDescSet(1, colorAttachments[1]->GetImageView());
	oitBuffers->BindDescriptorSet(cmdBuffer, pipelineManager->GetPipelineLayout(), 3);

	// Bind vertex and index buffers
	const vk::DeviceSize zeroOffset[] = { 0 };
	cmdBuffer.bindVertexBuffers(0, 1, &mainBuffer, zeroOffset);
	cmdBuffer.bindIndexBuffer(mainBuffer, offsets.indexOffset, vk::IndexType::eUint32);

	// Make sure to push constants even if not used
	OITDescriptorSets::PushConstants pushConstants = { };
	cmdBuffer.pushConstants<OITDescriptorSets::PushConstants>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

	const std::array<vk::ClearValue, 4> clear_colors = {
			pvrrc.isRTT ? vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}) : getBorderColor(),
			pvrrc.isRTT ? vk::ClearColorValue(std::array<float, 4>{0.f, 0.f, 0.f, 1.f}) : getBorderColor(),
			vk::ClearDepthStencilValue{ 0.f, 0 },
			vk::ClearDepthStencilValue{ 0.f, 0 },
	};

	RenderPass previous_pass = {};
    for (int render_pass = 0; render_pass < pvrrc.render_passes.used(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];

        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d TrMV %d autosort %d", render_pass + 1,
        		current_pass.op_count - previous_pass.op_count,
				current_pass.pt_count - previous_pass.pt_count,
				current_pass.tr_count - previous_pass.tr_count,
				current_pass.mvo_count - previous_pass.mvo_count,
				current_pass.mvo_tr_count - previous_pass.mvo_tr_count, current_pass.autosort);

        // Reset the pixel counter
    	oitBuffers->ResetPixelCounter(cmdBuffer);

    	const bool initialPass = render_pass == 0;
    	const bool finalPass = render_pass == pvrrc.render_passes.used() - 1;

    	vk::Framebuffer targetFramebuffer;
    	if (!finalPass)
    		targetFramebuffer = *tempFramebuffers[(pvrrc.render_passes.used() - 1 - render_pass) % 2];
    	else
    		targetFramebuffer = GetFinalFramebuffer();
    	cmdBuffer.beginRenderPass(
    			vk::RenderPassBeginInfo(pipelineManager->GetRenderPass(initialPass, finalPass),
    					targetFramebuffer, viewport, clear_colors.size(), clear_colors.data()),
    			vk::SubpassContents::eInline);

    	if (!pvrrc.isRTT && (FB_R_CTRL.fb_enable == 0 || VO_CONTROL.blank_video == 1))
    	{
    		// Video output disabled
			cmdBuffer.nextSubpass(vk::SubpassContents::eInline);
    	}
    	else
    	{
			// Depth + stencil subpass
			DrawList(cmdBuffer, ListType_Opaque, false, Pass::Depth, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count);
			DrawList(cmdBuffer, ListType_Punch_Through, false, Pass::Depth, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count);

			DrawModifierVolumes<false>(cmdBuffer, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);

			// Color subpass
			cmdBuffer.nextSubpass(vk::SubpassContents::eInline);

			// OP + PT
			DrawList(cmdBuffer, ListType_Opaque, false, Pass::Color, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count);
			DrawList(cmdBuffer, ListType_Punch_Through, false, Pass::Color, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count);

			// TR
			if (current_pass.autosort)
			{
				if (!oitBuffers->isFirstFrameAfterInit())
					DrawList(cmdBuffer, ListType_Translucent, true, Pass::OIT, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
			}
			else
				DrawList(cmdBuffer, ListType_Translucent, false, Pass::Color, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);
    	}

		// Final subpass
		cmdBuffer.nextSubpass(vk::SubpassContents::eInline);
		GetCurrentDescSet().BindColorInputDescSet(cmdBuffer, (pvrrc.render_passes.used() - 1 - render_pass) % 2);

		if (initialPass && !pvrrc.isRTT && clearNeeded[GetCurrentImage()])
		{
			clearNeeded[GetCurrentImage()] = false;
			SetScissor(cmdBuffer, viewport);
			cmdBuffer.clearAttachments(vk::ClearAttachment(vk::ImageAspectFlagBits::eColor, 0, clear_colors[0]),
					vk::ClearRect(viewport, 0, 1));
		}
		SetScissor(cmdBuffer, baseScissor);

		if (!oitBuffers->isFirstFrameAfterInit())
		{
			// Tr modifier volumes
			if (GetContext()->GetVendorID() != VENDOR_QUALCOMM)	// Adreno bug
				DrawModifierVolumes<true>(cmdBuffer, previous_pass.mvo_tr_count, current_pass.mvo_tr_count - previous_pass.mvo_tr_count);

			vk::Pipeline pipeline = pipelineManager->GetFinalPipeline();
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			quadBuffer->Bind(cmdBuffer);
			quadBuffer->Draw(cmdBuffer);
		}

		// Clear
		vk::MemoryBarrier memoryBarrier(vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite);
		cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
				vk::DependencyFlagBits::eByRegion, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
		vk::Pipeline pipeline = pipelineManager->GetClearPipeline();
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		quadBuffer->Bind(cmdBuffer);
		quadBuffer->Draw(cmdBuffer);

		if (oitBuffers->isFirstFrameAfterInit())
		{
			// missing the transparent stuff on the first frame cuz I'm lazy
			vk::MemoryBarrier memoryBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);
			cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eFragmentShader, vk::PipelineStageFlagBits::eFragmentShader,
					vk::DependencyFlagBits::eByRegion, 1, &memoryBarrier, 0, nullptr, 0, nullptr);
			pipeline = pipelineManager->GetFinalPipeline();
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			quadBuffer->Bind(cmdBuffer);
			quadBuffer->Draw(cmdBuffer);
		}

		if (!finalPass)
		{
	    	// Re-bind vertex and index buffers
	    	cmdBuffer.bindVertexBuffers(0, 1, &mainBuffer, zeroOffset);
	    	cmdBuffer.bindIndexBuffer(mainBuffer, offsets.indexOffset, vk::IndexType::eUint32);

			// Tr depth-only pass
			DrawList(cmdBuffer, ListType_Translucent, current_pass.autosort, Pass::Depth, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count);

			cmdBuffer.endRenderPass();
		}

		previous_pass = current_pass;
    }

	return !pvrrc.isRTT;
}

void OITDrawer::MakeBuffers(int width, int height)
{
	oitBuffers->Init(width, height);

	if (width <= maxWidth && height <= maxHeight)
		return;
	maxWidth = std::max(maxWidth, width);
	maxHeight = std::max(maxHeight, height);

	GetContext()->WaitIdle();
	for (auto& attachment : colorAttachments)
	{
		attachment.reset();
		attachment = std::unique_ptr<FramebufferAttachment>(
				new FramebufferAttachment(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice()));
		attachment->Init(maxWidth, maxHeight, GetColorFormat(),
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);
	}

	for (auto& attachment : depthAttachments)
	{
		attachment.reset();
		attachment = std::unique_ptr<FramebufferAttachment>(
				new FramebufferAttachment(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice()));
		attachment->Init(maxWidth, maxHeight, GetContext()->GetDepthFormat(),
				vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment);
	}
	needDepthTransition = true;

	vk::ImageView attachments[] = {
			colorAttachments[1]->GetImageView(),
			colorAttachments[0]->GetImageView(),
			depthAttachments[0]->GetImageView(),
			depthAttachments[1]->GetImageView(),
	};
	vk::FramebufferCreateInfo createInfo(vk::FramebufferCreateFlags(), pipelineManager->GetRenderPass(true, true),
			ARRAY_SIZE(attachments), attachments, width, height, 1);
	tempFramebuffers[0] = GetContext()->GetDevice().createFramebufferUnique(createInfo);
	attachments[0] = attachments[1];
	attachments[1] = colorAttachments[1]->GetImageView();
	tempFramebuffers[1] = GetContext()->GetDevice().createFramebufferUnique(createInfo);
}

void OITScreenDrawer::MakeFramebuffers(const vk::Extent2D& viewport)
{
	this->viewport.offset.x = 0;
	this->viewport.offset.y = 0;
	this->viewport.extent = viewport;

	MakeBuffers(viewport.width, viewport.height);
	framebuffers.clear();
	finalColorAttachments.clear();
	transitionNeeded.clear();
	clearNeeded.clear();
	while (finalColorAttachments.size() < GetContext()->GetSwapChainSize())
	{
		finalColorAttachments.push_back(std::unique_ptr<FramebufferAttachment>(
				new FramebufferAttachment(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice())));
		finalColorAttachments.back()->Init(viewport.width, viewport.height, GetContext()->GetColorFormat(),
				vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled);
		vk::ImageView attachments[] = {
				finalColorAttachments.back()->GetImageView(),
				colorAttachments[0]->GetImageView(),
				depthAttachments[0]->GetImageView(),
				depthAttachments[1]->GetImageView(),
		};
		vk::FramebufferCreateInfo createInfo(vk::FramebufferCreateFlags(), screenPipelineManager->GetRenderPass(true, true),
				ARRAY_SIZE(attachments), attachments, viewport.width, viewport.height, 1);
		framebuffers.push_back(GetContext()->GetDevice().createFramebufferUnique(createInfo));
		transitionNeeded.push_back(true);
		clearNeeded.push_back(true);
	}
}

vk::CommandBuffer OITTextureDrawer::NewFrame()
{
	DEBUG_LOG(RENDERER, "RenderToTexture packmode=%d stride=%d - %d x %d @ %06x", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
			pvrrc.fb_X_CLIP.max + 1, pvrrc.fb_Y_CLIP.max + 1, FB_W_SOF1 & VRAM_MASK);
	NewImage();

	matrices.CalcMatrices(&pvrrc);

	textureAddr = FB_W_SOF1 & VRAM_MASK;
	u32 origWidth = pvrrc.fb_X_CLIP.max + 1;
	u32 origHeight = pvrrc.fb_Y_CLIP.max + 1;
	float upscale = 1.f;
	if (!config::RenderToTextureBuffer)
		upscale = config::RenderResolution / 480.f;
	u32 heightPow2 = 8;
	while (heightPow2 < origHeight)
		heightPow2 *= 2;
	u32 widthPow2 = 8;
	while (widthPow2 < origWidth)
		widthPow2 *= 2;
	u32 upscaledWidth = origWidth * upscale;
	u32 upscaledHeight = origHeight * upscale;
	widthPow2 *= upscale;
	heightPow2 *= upscale;

	rttPipelineManager->CheckSettingsChange();
	VulkanContext *context = GetContext();
	vk::Device device = context->GetDevice();

	vk::CommandBuffer commandBuffer = commandPool->Allocate();
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	MakeBuffers(widthPow2, heightPow2);

	vk::ImageView colorImageView;
	vk::ImageLayout colorImageCurrentLayout;

	if (!config::RenderToTextureBuffer)
	{
		// TexAddr : fb_rtt.TexAddr, Reserved : 0, StrideSel : 0, ScanOrder : 1
		TCW tcw = { { textureAddr >> 3, 0, 0, 1 } };
		switch (FB_W_CTRL.fb_packmode) {
		case 0:
		case 3:
			tcw.PixelFmt = Pixel1555;
			break;
		case 1:
			tcw.PixelFmt = Pixel565;
			break;
		case 2:
			tcw.PixelFmt = Pixel4444;
			break;
		}

		TSP tsp = {};
		for (tsp.TexU = 0; tsp.TexU <= 7 && (8u << tsp.TexU) < origWidth; tsp.TexU++);
		for (tsp.TexV = 0; tsp.TexV <= 7 && (8u << tsp.TexV) < origHeight; tsp.TexV++);

		texture = textureCache->getTextureCacheData(tsp, tcw);
		if (texture->IsNew())
		{
			texture->Create();
			texture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			texture->SetDevice(device);
		}
		else if (textureCache->IsInFlight(texture))
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
				colorAttachment = std::unique_ptr<FramebufferAttachment>(new FramebufferAttachment(context->GetPhysicalDevice(), device));
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
	viewport.offset.x = 0;
	viewport.offset.y = 0;
	viewport.extent.width = widthPow2;
	viewport.extent.height = heightPow2;

	setImageLayout(commandBuffer, colorImage, vk::Format::eR8G8B8A8Unorm, 1, colorImageCurrentLayout, vk::ImageLayout::eColorAttachmentOptimal);

	vk::ImageView imageViews[] = {
		colorImageView,
		colorAttachments[0]->GetImageView(),
		depthAttachments[0]->GetImageView(),
		depthAttachments[1]->GetImageView(),
	};
	framebuffers.resize(GetContext()->GetSwapChainSize());
	framebuffers[GetCurrentImage()] = device.createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(),
			rttPipelineManager->GetRenderPass(true, true), ARRAY_SIZE(imageViews), imageViews, widthPow2, heightPow2, 1));

	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)upscaledWidth, (float)upscaledHeight, 1.0f, 0.0f));
	baseScissor = vk::Rect2D(vk::Offset2D(pvrrc.fb_X_CLIP.min * upscale, pvrrc.fb_Y_CLIP.min * upscale),
			vk::Extent2D(upscaledWidth, upscaledHeight));
	commandBuffer.setScissor(0, baseScissor);
	currentCommandBuffer = commandBuffer;

	return commandBuffer;
}

void OITTextureDrawer::EndFrame()
{
	currentCommandBuffer.endRenderPass();

	u32 clippedWidth = pvrrc.fb_X_CLIP.max + 1;
	u32 clippedHeight = pvrrc.fb_Y_CLIP.max + 1;

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
	commandPool->EndFrame();

	if (config::RenderToTextureBuffer)
	{
		vk::Fence fence = commandPool->GetCurrentFence();
		GetContext()->GetDevice().waitForFences(1, &fence, true, UINT64_MAX);

		u16 *dst = (u16 *)&vram[textureAddr];

		PixelBuffer<u32> tmpBuf;
		tmpBuf.init(clippedWidth, clippedHeight);
		colorAttachment->GetBufferData()->download(clippedWidth * clippedHeight * 4, tmpBuf.data());
		WriteTextureToVRam(clippedWidth, clippedHeight, (u8 *)tmpBuf.data(), dst);
	}
	else
	{
		//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);

		texture->dirty = 0;
		libCore_vramlock_Lock(texture->sa_tex, texture->sa + texture->size - 1, texture);
	}
	OITDrawer::EndFrame();
}

vk::CommandBuffer OITScreenDrawer::NewFrame()
{
	vk::CommandBuffer commandBuffer = commandPool->Allocate();
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	if (transitionNeeded[GetCurrentImage()])
	{
		setImageLayout(commandBuffer, finalColorAttachments[GetCurrentImage()]->GetImage(), GetColorFormat(), 1, vk::ImageLayout::eUndefined, vk::ImageLayout::eShaderReadOnlyOptimal);
		transitionNeeded[GetCurrentImage()] = false;
	}
	matrices.CalcMatrices(&pvrrc, viewport.extent.width, viewport.extent.height);

	SetBaseScissor(viewport.extent);

	commandBuffer.setScissor(0, baseScissor);
	commandBuffer.setViewport(0, vk::Viewport((float)viewport.offset.x, (float)viewport.offset.y, (float)viewport.extent.width, (float)viewport.extent.height, 1.0f, 0.0f));
	currentCommandBuffer = commandBuffer;

	return commandBuffer;
}
