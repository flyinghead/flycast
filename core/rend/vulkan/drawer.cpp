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
#include <math.h>
#include "drawer.h"
#include "hw/pvr/pvr_mem.h"

void Drawer::SortTriangles()
{
	sortedPolys.resize(pvrrc.render_passes.used());
	sortedIndexes.resize(pvrrc.render_passes.used());
	sortedIndexCount = 0;
	RenderPass previousPass = {};

	for (int render_pass = 0; render_pass < pvrrc.render_passes.used(); render_pass++)
	{
		const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];
		sortedIndexes[render_pass].clear();
		if (current_pass.autosort)
		{
			GenSorted(previousPass.tr_count, current_pass.tr_count - previousPass.tr_count, sortedPolys[render_pass], sortedIndexes[render_pass]);
			for (auto& poly : sortedPolys[render_pass])
				poly.first += sortedIndexCount;
			sortedIndexCount += sortedIndexes[render_pass].size();
		}
		else
			sortedPolys[render_pass].clear();
		previousPass = current_pass;
	}
}

// FIXME Code dup
TileClipping Drawer::SetTileClip(u32 val, vk::Rect2D& clipRect)
{
	if (!settings.rend.Clipping)
		return TileClipping::Off;

	u32 clipmode = val >> 28;
	if (clipmode < 2)
		return TileClipping::Off;	//always passes

	TileClipping tileClippingMode;
	if (clipmode & 1)
		tileClippingMode = TileClipping::Inside;   //render stuff outside the region
	else
		tileClippingMode = TileClipping::Outside;  //render stuff inside the region

	float csx = 0, csy = 0, cex = 0, cey = 0;


	csx = (float)(val & 63);
	cex = (float)((val >> 6) & 63);
	csy = (float)((val >> 12) & 31);
	cey = (float)((val >> 17) & 31);
	csx = csx * 32;
	cex = cex * 32 + 32;
	csy = csy * 32;
	cey = cey * 32 + 32;

	if (csx <= 0 && csy <= 0 && cex >= 640 && cey >= 480)
		return TileClipping::Off;

	if (tileClippingMode != TileClipping::Off)
	{
		if (!pvrrc.isRTT)
		{
			glm::vec4 clip_start(csx, csy, 0, 1);
			glm::vec4 clip_end(cex, cey, 0, 1);
			clip_start = matrices.GetViewportMatrix() * clip_start;
			clip_end = matrices.GetViewportMatrix() * clip_end;

			csx = clip_start[0];
			csy = clip_start[1];
			cey = clip_end[1];
			cex = clip_end[0];
		}
		else if (!settings.rend.RenderToTextureBuffer)
		{
			csx *= settings.rend.RenderToTextureUpscale;
			csy *= settings.rend.RenderToTextureUpscale;
			cex *= settings.rend.RenderToTextureUpscale;
			cey *= settings.rend.RenderToTextureUpscale;
		}
		clipRect = vk::Rect2D(vk::Offset2D(std::max(0, (int)lroundf(csx)), std::max(0, (int)lroundf(csy))),
				vk::Extent2D(std::max(0, (int)lroundf(cex - csx)), std::max(0, (int)lroundf(cey - csy))));
	}

	return tileClippingMode;
}

void Drawer::DrawPoly(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const PolyParam& poly, u32 first, u32 count)
{
	vk::Rect2D scissorRect;
	TileClipping tileClip = SetTileClip(poly.tileclip, scissorRect);
	if (tileClip == TileClipping::Off || tileClip == TileClipping::Inside)
		scissorRect = baseScissor;
	SetScissor(cmdBuffer, scissorRect);

	float trilinearAlpha = 1.f;
	if (poly.pcw.Texture && poly.tsp.FilterMode > 1 && listType != ListType_Punch_Through)
	{
		trilinearAlpha = 0.25 * (poly.tsp.MipMapD & 0x3);
		if (poly.tsp.FilterMode == 2)
			// Trilinear pass A
			trilinearAlpha = 1.0 - trilinearAlpha;
	}

	if (tileClip == TileClipping::Inside || trilinearAlpha != 1.f)
	{
		std::array<float, 5> pushConstants = { (float)scissorRect.offset.x, (float)scissorRect.offset.y,
				(float)scissorRect.extent.width, (float)scissorRect.extent.height, trilinearAlpha };
		cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);
	}

	if (poly.pcw.Texture)
		GetCurrentDescSet().SetTexture(poly.texid, poly.tsp);

	vk::Pipeline pipeline = pipelineManager->GetPipeline(listType, sortTriangles, poly);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	if (poly.pcw.Texture)
		GetCurrentDescSet().BindPerPolyDescriptorSets(cmdBuffer, poly.texid, poly.tsp);

	cmdBuffer.drawIndexed(count, 1, first, 0, 0);
}

void Drawer::DrawSorted(const vk::CommandBuffer& cmdBuffer, const std::vector<SortTrigDrawParam>& polys)
{
	for (const SortTrigDrawParam& param : polys)
	{
		DrawPoly(cmdBuffer, ListType_Translucent, true, *param.ppid, pvrrc.idx.used() + param.first, param.count);
	}
}

void Drawer::DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const List<PolyParam>& polys, u32 first, u32 count)
{
	for (u32 i = first; i < count; i++)
	{
		const PolyParam &pp = polys.head()[i];
		DrawPoly(cmdBuffer, listType, sortTriangles, pp, pp.first, pp.count);
	}
}

void Drawer::DrawModVols(const vk::CommandBuffer& cmdBuffer, int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0)
		return;

	vk::Buffer buffer = GetMainBuffer(0)->buffer.get();
	cmdBuffer.bindVertexBuffers(0, 1, &buffer, &offsets.modVolOffset);

	ModifierVolumeParam* params = &pvrrc.global_param_mvo.head()[first];

	int mod_base = -1;
	vk::Pipeline pipeline;

	for (u32 cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		if (mod_base == -1)
			mod_base = param.first;

		if (!param.isp.VolumeLast && mv_mode > 0)
			pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Or);	// OR'ing (open volume or quad)
		else
			pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Xor);	// XOR'ing (closed volume)
		cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
		cmdBuffer.draw(param.count * 3, 1, param.first * 3, 0);

		if (mv_mode == 1 || mv_mode == 2)
		{
			// Sum the area
			pipeline = pipelineManager->GetModifierVolumePipeline(mv_mode == 1 ? ModVolMode::Inclusion : ModVolMode::Exclusion);
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			cmdBuffer.draw((param.first + param.count - mod_base) * 3, 1, mod_base * 3, 0);
			mod_base = -1;
		}
	}
	const vk::DeviceSize offset = 0;
	cmdBuffer.bindVertexBuffers(0, 1, &buffer, &offset);

	std::array<float, 5> pushConstants = { 1 - FPU_SHAD_SCALE.scale_factor / 256.f, 0, 0, 0, 0 };
	cmdBuffer.pushConstants<float>(pipelineManager->GetPipelineLayout(), vk::ShaderStageFlagBits::eFragment, 0, pushConstants);

	pipeline = pipelineManager->GetModifierVolumePipeline(ModVolMode::Final);
	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
	cmdBuffer.drawIndexed(4, 1, 0, 0, 0);
}

void Drawer::UploadMainBuffer(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms)
{
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
	for (const std::vector<u32>& idx : sortedIndexes)
	{
		if (!idx.empty())
		{
			chunks.push_back(&idx[0]);
			chunkSizes.push_back(idx.size() * sizeof(u32));
		}
	}
	// Uniform buffers
	u32 indexSize = pvrrc.idx.bytes() + sortedIndexCount * sizeof(u32);
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
	u32 totalSize = offsets.fragmentUniformOffset + sizeof(FragmentShaderUniforms);

	BufferData *buffer = GetMainBuffer(totalSize);
	buffer->upload(GetContext()->GetDevice(), chunks.size(), &chunkSizes[0], &chunks[0]);
}

bool Drawer::Draw(const Texture *fogTexture)
{
	extern bool fog_needs_update;

	VertexShaderUniforms vtxUniforms;
	vtxUniforms.normal_matrix = matrices.GetNormalMatrix();

	FragmentShaderUniforms fragUniforms;
	fragUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

	//VERT and RAM fog color constants
	u8* fog_colvert_bgra=(u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra=(u8*)&FOG_COL_RAM;
	fragUniforms.sp_FOG_COL_VERT[0]=fog_colvert_bgra[2]/255.0f;
	fragUniforms.sp_FOG_COL_VERT[1]=fog_colvert_bgra[1]/255.0f;
	fragUniforms.sp_FOG_COL_VERT[2]=fog_colvert_bgra[0]/255.0f;

	fragUniforms.sp_FOG_COL_RAM[0]=fog_colram_bgra [2]/255.0f;
	fragUniforms.sp_FOG_COL_RAM[1]=fog_colram_bgra [1]/255.0f;
	fragUniforms.sp_FOG_COL_RAM[2]=fog_colram_bgra [0]/255.0f;

	//Fog density constant
	u8* fog_density=(u8*)&FOG_DENSITY;
	float fog_den_mant=fog_density[1]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp=(s8)fog_density[0];
	fragUniforms.sp_FOG_DENSITY = fog_den_mant * powf(2.0f, fog_den_exp);

	fragUniforms.colorClampMin[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	fragUniforms.colorClampMin[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	fragUniforms.colorClampMin[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	fragUniforms.colorClampMin[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;

	fragUniforms.colorClampMax[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	fragUniforms.colorClampMax[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	fragUniforms.colorClampMax[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	fragUniforms.colorClampMax[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;

	fragUniforms.cp_AlphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

	SortTriangles();
	currentScissor = vk::Rect2D();

	vk::CommandBuffer cmdBuffer = BeginRenderPass();

	// Upload vertex and index buffers
	UploadMainBuffer(vtxUniforms, fragUniforms);

	// Update per-frame descriptor set and bind it
	GetCurrentDescSet().UpdateUniforms(GetMainBuffer(0)->buffer.get(), offsets.vertexUniformOffset, offsets.fragmentUniformOffset, fogTexture->GetImageView());
	GetCurrentDescSet().BindPerFrameDescriptorSets(cmdBuffer);
	// Reset per-poly descriptor set pool
	GetCurrentDescSet().Reset();

	// Bind vertex and index buffers
	const vk::DeviceSize zeroOffset[] = { 0 };
	const vk::Buffer buffer = GetMainBuffer(0)->buffer.get();
	cmdBuffer.bindVertexBuffers(0, 1, &buffer, zeroOffset);
	cmdBuffer.bindIndexBuffer(buffer, offsets.indexOffset, vk::IndexType::eUint32);

	RenderPass previous_pass = {};
    for (int render_pass = 0; render_pass < pvrrc.render_passes.used(); render_pass++)
    {
        const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];

        DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d", render_pass + 1,
        		current_pass.op_count - previous_pass.op_count,
				current_pass.pt_count - previous_pass.pt_count,
				current_pass.tr_count - previous_pass.tr_count,
				current_pass.mvo_count - previous_pass.mvo_count);
		DrawList(cmdBuffer, ListType_Opaque, false, pvrrc.global_param_op, previous_pass.op_count, current_pass.op_count - previous_pass.op_count);
		DrawList(cmdBuffer, ListType_Punch_Through, false, pvrrc.global_param_pt, previous_pass.pt_count, current_pass.pt_count - previous_pass.pt_count);
		DrawModVols(cmdBuffer, previous_pass.mvo_count, current_pass.mvo_count - previous_pass.mvo_count);
		if (current_pass.autosort)
        {
			if (!settings.rend.PerStripSorting)
			{
				DrawSorted(cmdBuffer, sortedPolys[render_pass]);
			}
			else
			{
				SortPParams(previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
				DrawList(cmdBuffer, ListType_Translucent, true, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
			}
        }
		else
			DrawList(cmdBuffer, ListType_Translucent, false, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
		previous_pass = current_pass;
    }

	return !pvrrc.isRTT;
}

vk::CommandBuffer TextureDrawer::BeginRenderPass()
{
	DEBUG_LOG(RENDERER, "RenderToTexture packmode=%d stride=%d - %d,%d -> %d,%d", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
			FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
	matrices.CalcMatrices(&pvrrc);

	textureAddr = FB_W_SOF1 & VRAM_MASK;
	u32 origWidth = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
	u32 origHeight = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
	u32 upscaledWidth = origWidth;
	u32 upscaledHeight = origHeight;
	int heightPow2 = 2;
	while (heightPow2 < upscaledHeight)
		heightPow2 *= 2;
	int widthPow2 = 2;
	while (widthPow2 < upscaledWidth)
		widthPow2 *= 2;

	if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
	{
		upscaledWidth *= settings.rend.RenderToTextureUpscale;
		upscaledHeight *= settings.rend.RenderToTextureUpscale;
		widthPow2 *= settings.rend.RenderToTextureUpscale;
		heightPow2 *= settings.rend.RenderToTextureUpscale;
	}

	static_cast<RttPipelineManager*>(pipelineManager)->CheckSettingsChange();
	VulkanContext *context = GetContext();
	vk::Device device = context->GetDevice();

	vk::CommandBuffer commandBuffer = commandPool->Allocate();
	commandBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

	if (widthPow2 != this->width || heightPow2 != this->height || !depthAttachment)
	{
		if (!depthAttachment)
			depthAttachment = std::unique_ptr<FramebufferAttachment>(new FramebufferAttachment(context->GetPhysicalDevice(), device));
		depthAttachment->Init(widthPow2, heightPow2, GetContext()->GetDepthFormat());
	}
	vk::ImageView colorImageView;
	vk::ImageLayout colorImageCurrentLayout;

	if (!settings.rend.RenderToTextureBuffer)
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

		TSP tsp = { 0 };
		for (tsp.TexU = 0; tsp.TexU <= 7 && (8 << tsp.TexU) < origWidth; tsp.TexU++);
		for (tsp.TexV = 0; tsp.TexV <= 7 && (8 << tsp.TexV) < origHeight; tsp.TexV++);

		texture = textureCache->getTextureCacheData(tsp, tcw);
		if (texture->IsNew())
		{
			texture->Create();
			texture->SetAllocator(texAllocator);
			texture->SetPhysicalDevice(GetContext()->GetPhysicalDevice());
			texture->SetDevice(device);
		}
		if (texture->format != vk::Format::eR8G8B8A8Unorm)
		{
			texture->extent = vk::Extent2D(widthPow2, heightPow2);
			texture->format = vk::Format::eR8G8B8A8Unorm;
			texture->CreateImage(vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled,
					vk::ImageLayout::eUndefined, vk::MemoryPropertyFlags(), vk::ImageAspectFlagBits::eColor);
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
		if (widthPow2 != this->width || heightPow2 != this->height || !colorAttachment)
		{
			if (!colorAttachment)
			{
				colorAttachment = std::unique_ptr<FramebufferAttachment>(new FramebufferAttachment(context->GetPhysicalDevice(),
						device));
			}
			colorAttachment->Init(widthPow2, heightPow2, vk::Format::eR8G8B8A8Unorm);
		}
		colorImage = *colorAttachment->GetImage();
		colorImageView = *colorAttachment->GetImageView();
		colorImageCurrentLayout = vk::ImageLayout::eUndefined;
	}
	width = widthPow2;
	height = heightPow2;

	setImageLayout(commandBuffer, colorImage, vk::Format::eR8G8B8A8Unorm, 1, colorImageCurrentLayout, vk::ImageLayout::eColorAttachmentOptimal);

	vk::ImageView imageViews[] = {
		colorImageView,
		*depthAttachment->GetImageView(),
	};
	framebuffer = device.createFramebufferUnique(vk::FramebufferCreateInfo(vk::FramebufferCreateFlags(),
			pipelineManager->GetRenderPass(), ARRAY_SIZE(imageViews), imageViews, widthPow2, heightPow2, 1));

	const vk::ClearValue clear_colors[] = { vk::ClearColorValue(std::array<float, 4> { 0.f, 0.f, 0.f, 1.f }), vk::ClearDepthStencilValue { 0.f, 0 } };
	commandBuffer.beginRenderPass(vk::RenderPassBeginInfo(pipelineManager->GetRenderPass(),	*framebuffer,
			vk::Rect2D( { 0, 0 }, { width, height }), 2, clear_colors), vk::SubpassContents::eInline);
	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)upscaledWidth, (float)upscaledHeight, 1.0f, 0.0f));
	baseScissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(upscaledWidth, upscaledHeight));
	commandBuffer.setScissor(0, baseScissor);
	currentCommandBuffer = commandBuffer;

	return commandBuffer;
}

void TextureDrawer::EndRenderPass()
{
	currentCommandBuffer.endRenderPass();

	if (settings.rend.RenderToTextureBuffer)
	{
		vk::BufferImageCopy copyRegion(0, width, height, vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, 0, 1), vk::Offset3D(0, 0, 0),
				vk::Extent3D(vk::Extent2D(width, height), 1));
		currentCommandBuffer.copyImageToBuffer(*colorAttachment->GetImage(), vk::ImageLayout::eTransferSrcOptimal,
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

	GetContext()->GetGraphicsQueue().submit(vk::SubmitInfo(0, nullptr, nullptr, 1, &currentCommandBuffer),
			settings.rend.RenderToTextureBuffer ? *fence : nullptr);
	colorImage = nullptr;
	currentCommandBuffer = nullptr;
	commandPool->EndFrame();



	if (settings.rend.RenderToTextureBuffer)
	{
		GetContext()->GetDevice().waitForFences(1, &fence.get(), true, UINT64_MAX);
		GetContext()->GetDevice().resetFences(1, &fence.get());

		u16 *dst = (u16 *)&vram[textureAddr];

		PixelBuffer<u32> tmpBuf;
		tmpBuf.init(width, height);
		colorAttachment->GetBufferData()->download(GetContext()->GetDevice(), width * height * 4, tmpBuf.data());
		WriteTextureToVRam(width, height, (u8 *)tmpBuf.data(), dst);

		return;
	}
	//memset(&vram[fb_rtt.TexAddr << 3], '\0', size);

	texture->dirty = 0;
	if (texture->lock_block == NULL)
		texture->lock_block = libCore_vramlock_Lock(texture->sa_tex, texture->sa + texture->size - 1, texture);
}

vk::CommandBuffer ScreenDrawer::BeginRenderPass()
{
	GetContext()->NewFrame();
	GetContext()->BeginRenderPass();
	vk::CommandBuffer commandBuffer = GetContext()->GetCurrentCommandBuffer();
	commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, (float)screen_width, (float)screen_height, 1.0f, 0.0f));

	matrices.CalcMatrices(&pvrrc);

	bool wide_screen_on = settings.rend.WideScreen && !pvrrc.isRenderFramebuffer && !matrices.IsClipped();

	if (!wide_screen_on)
	{
		float width;
		float height;
		float min_x;
		float min_y;

		if (pvrrc.isRenderFramebuffer)
		{
			width = 640;
			height = 480;
			min_x = 0;
			min_y = 0;
		}
		else
		{
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
		}

		baseScissor = vk::Rect2D(
				vk::Offset2D((u32)std::max(lroundf(min_x), 0L), (u32)std::max(lroundf(min_y), 0L)),
				vk::Extent2D((u32)std::max(lroundf(width), 0L), (u32)std::max(lroundf(height), 0L)));
	}
	else
	{
		baseScissor = vk::Rect2D(vk::Offset2D(0, 0), vk::Extent2D(screen_width, screen_height));
	}

	commandBuffer.setScissor(0, baseScissor);
	return commandBuffer;
}
