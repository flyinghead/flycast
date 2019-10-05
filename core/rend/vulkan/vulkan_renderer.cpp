/*
    Created on: Oct 2, 2019

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
#include <memory>
#include <math.h>
#include "vulkan.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/pvr/ta_ctx.h"
#include "../gui.h"
#include "rend/sorter.h"
#include "buffer.h"
#include "pipeline.h"
#include "shaders.h"
#include "texture.h"
#include "utils.h"

extern bool ProcessFrame(TA_context* ctx);

class VulkanRenderer : public Renderer
{
public:
	bool Init() override
	{
		printf("VulkanRenderer::Init\n");
		InitUniforms();

		pipelineManager.Init();

		return true;
	}

	void Resize(int w, int h) override
	{
	}

	void Term() override
	{
		printf("VulkanRenderer::Term\n");
		GetContext()->WaitIdle();
		killtex();
		glslang::FinalizeProcess();

	}

	bool Process(TA_context* ctx) override
	{
		GetContext()->NewFrame();
		if (ProcessFrame(ctx))
			return true;

		// FIXME
		GetContext()->BeginRenderPass();
		GetContext()->EndFrame();
		GetContext()->Present();
		return false;
	}

	void DrawOSD(bool clear_screen) override
	{
// TODO		gui_display_osd();
	}

	bool Render() override
	{
		printf("VulkanRenderer::Render\n");
		extern float fb_scale_x, fb_scale_y;
		extern bool fog_needs_update;

		bool is_rtt = pvrrc.isRTT;
		float dc_width = 640;
		float dc_height = 480;

		if (is_rtt)
		{
			dc_width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
			dc_height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
		}

		float scale_x = 1;
		float scale_y = 1;

		float scissoring_scale_x = 1;

		if (!is_rtt && !pvrrc.isRenderFramebuffer)
		{
			scale_x = fb_scale_x;
			scale_y = fb_scale_y;
			if (SCALER_CTL.interlace == 0 && SCALER_CTL.vscalefactor > 0x400)
				scale_y *= roundf((float)SCALER_CTL.vscalefactor / 0x400);

			//work out scaling parameters !
			//Pixel doubling is on VO, so it does not affect any pixel operations
			//A second scaling is used here for scissoring
			if (VO_CONTROL.pixel_double)
			{
				scissoring_scale_x = 0.5f;
				scale_x *= 0.5f;
			}

			if (SCALER_CTL.hscale)
			{
	            scissoring_scale_x /= 2;
				scale_x*=2;
			}
		}

		dc_width  *= scale_x;
		dc_height *= scale_y;

		float screen_stretching = settings.rend.ScreenStretching / 100.f;
		float screen_scaling = settings.rend.ScreenScaling / 100.f;

		float dc2s_scale_h;
		float ds2s_offs_x;

		VertexShaderUniforms vtxUniforms;
		if (is_rtt)
		{
			vtxUniforms.scale[0] = 2.0f / dc_width;
			vtxUniforms.scale[1] = 2.0f / dc_height;	// FIXME CT2 needs 480 here instead of dc_height=512
			vtxUniforms.scale[2] = 1;
			vtxUniforms.scale[3] = 1;
		}
		else
		{
			if (settings.rend.Rotate90)
			{
				dc2s_scale_h = screen_height / 640.0f;
				ds2s_offs_x =  (screen_width - dc2s_scale_h * 480.0f * screen_stretching) / 2;
				vtxUniforms.scale[0] = -2.0f / (screen_width / dc2s_scale_h * scale_x) * screen_stretching;
				vtxUniforms.scale[1] = 2.0f / dc_width;
				vtxUniforms.scale[2] = 1 - 2 * ds2s_offs_x / screen_width;
				vtxUniforms.scale[3] = 1;
			}
			else
			{
				dc2s_scale_h = screen_height / 480.0f;
				ds2s_offs_x =  (screen_width - dc2s_scale_h * 640.0f * screen_stretching) / 2;
				vtxUniforms.scale[0] = 2.0f / (screen_width / dc2s_scale_h * scale_x) * screen_stretching;
				vtxUniforms.scale[1] = 2.0f / dc_height;
				vtxUniforms.scale[2] = 1 - 2 * ds2s_offs_x / screen_width;
				vtxUniforms.scale[3] = 1;
			}
			//-1 -> too much to left
		}
		vtxUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

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

		fragUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
		fragUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
		fragUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
		fragUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;

		fragUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
		fragUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
		fragUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
		fragUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;

		if (fog_needs_update && settings.rend.Fog)
		{
			fog_needs_update = false;
			// TODO UpdateFogTexture((u8 *)FOG_TABLE, GL_TEXTURE1, gl.fog_image_format);
		}
		fragUniforms.cp_AlphaTestValue = (PT_ALPHA_REF & 0xFF) / 255.0f;

		ModVolShaderUniforms modVolUniforms;
		modVolUniforms.sp_ShaderColor = 1 - FPU_SHAD_SCALE.scale_factor / 256.f;

		UploadUniforms(vtxUniforms, fragUniforms, modVolUniforms);
		// TODO separate between per-frame and per-poly uniforms (only clip text and trilinear alpha are per-poly)
		// TODO this upload should go in a cmd buffer?

		GetContext()->BeginRenderPass();
		vk::CommandBuffer cmdBuffer = GetContext()->GetCurrentCommandBuffer();

		// Upload vertex and index buffers
		CheckVertexIndexBuffers(pvrrc.verts.bytes(), pvrrc.idx.bytes());
		if (pvrrc.verts.bytes() > 0)
			vertexBuffers[GetCurrentImage()]->upload(GetContext()->GetDevice().get(), pvrrc.verts.bytes(), pvrrc.verts.head());
		if (pvrrc.idx.bytes() > 0)
			indexBuffers[GetCurrentImage()]->upload(GetContext()->GetDevice().get(), pvrrc.idx.bytes(), pvrrc.idx.head());

		// Update per-frame descriptor set and bind it
		pipelineManager.GetDescriptorSets().UpdateUniforms(*vertexUniformBuffer, *fragmentUniformBuffer);
		pipelineManager.GetDescriptorSets().BindPerFrameDescriptorSets(cmdBuffer);
		// Reset per-poly descriptor set pool
		pipelineManager.GetDescriptorSets().Reset();

		// Bind vertex and index buffers
		const vk::DeviceSize offsets[] = { 0 };
		cmdBuffer.bindVertexBuffers(0, 1, &vertexBuffers[GetCurrentImage()]->buffer.get(), offsets);
		cmdBuffer.bindIndexBuffer(*indexBuffers[GetCurrentImage()]->buffer, 0, vk::IndexType::eUint32);

		cmdBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(GetContext()->GetViewPort().width),
				static_cast<float>(GetContext()->GetViewPort().width), 0.0f, 1.0f));
		cmdBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), GetContext()->GetViewPort()));

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
			if (current_pass.autosort)
            {
// TODO
//				if (!settings.rend.PerStripSorting)
//				{
//					//SortTriangles(previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
//					//DrawSorted(render_pass < pvrrc.render_passes.used() - 1);
//				}
//				else
				{
					SortPParams(previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
					DrawList(cmdBuffer, ListType_Translucent, true, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
				}
            }
			else
				DrawList(cmdBuffer, ListType_Translucent, false, pvrrc.global_param_tr, previous_pass.tr_count, current_pass.tr_count - previous_pass.tr_count);
			previous_pass = current_pass;
	    }

		GetContext()->EndFrame();

		return !is_rtt;
	}

	void Present() override
	{
		GetContext()->Present();
	}

	virtual u64 GetTexture(TSP tsp, TCW tcw) override
	{
		Texture* tf = static_cast<Texture*>(getTextureCacheData(tsp, tcw, [](){
			return (BaseTextureCacheData *)new Texture(VulkanContext::Instance()->GetPhysicalDevice(), *VulkanContext::Instance()->GetDevice());
		}));

		if (tf->IsNew())
			tf->Create();

		//update if needed
		if (tf->NeedsUpdate())
			tf->Update();
		else
			tf->CheckCustomTexture();

		return tf->GetIntId();
	}

private:
	VulkanContext *GetContext() const { return VulkanContext::Instance(); }
	int GetCurrentImage() const { return GetContext()->GetCurrentImageIndex(); }

	void DrawList(const vk::CommandBuffer& cmdBuffer, u32 listType, bool sortTriangles, const List<PolyParam>& polys, u32 first, u32 count)
	{
		for (u32 i = first; i < count; i++)
		{
			const PolyParam &pp = polys.head()[i];
			if (pp.pcw.Texture)
				pipelineManager.GetDescriptorSets().SetTexture(pp.texid, pp.tsp);

			vk::Pipeline pipeline = pipelineManager.GetPipeline(listType, sortTriangles, pp);
			cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			if (pp.pcw.Texture)
				pipelineManager.GetDescriptorSets().BindPerPolyDescriptorSets(cmdBuffer, pp.texid, pp.tsp);

			cmdBuffer.drawIndexed(pp.count, 1, pp.first, 0, 0);
		}
	}

	void InitUniforms()
	{
		vertexUniformBuffer = GetContext()->GetDevice()->createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(),
				sizeof(VertexShaderUniforms), vk::BufferUsageFlagBits::eUniformBuffer));
		vk::MemoryRequirements memRequirements = GetContext()->GetDevice()->getBufferMemoryRequirements(vertexUniformBuffer.get());
		vertexUniformMemSize = memRequirements.size;
		u32 typeIndex = findMemoryType(GetContext()->GetPhysicalDevice().getMemoryProperties(), memRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		vertexUniformMemory = GetContext()->GetDevice()->allocateMemoryUnique(vk::MemoryAllocateInfo(vertexUniformMemSize, typeIndex));
		GetContext()->GetDevice()->bindBufferMemory(vertexUniformBuffer.get(), vertexUniformMemory.get(), 0);

		fragmentUniformBuffer = GetContext()->GetDevice()->createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(),
				sizeof(FragmentShaderUniforms), vk::BufferUsageFlagBits::eUniformBuffer));
		memRequirements = GetContext()->GetDevice()->getBufferMemoryRequirements(fragmentUniformBuffer.get());
		fragmentUniformsMemSize = memRequirements.size;
		typeIndex = findMemoryType(GetContext()->GetPhysicalDevice().getMemoryProperties(), memRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		fragmentUniformMemory = GetContext()->GetDevice()->allocateMemoryUnique(vk::MemoryAllocateInfo(fragmentUniformsMemSize, typeIndex));
		GetContext()->GetDevice()->bindBufferMemory(fragmentUniformBuffer.get(), fragmentUniformMemory.get(), 0);

		modVolUniformBuffer = GetContext()->GetDevice()->createBufferUnique(vk::BufferCreateInfo(vk::BufferCreateFlags(),
				sizeof(ModVolShaderUniforms), vk::BufferUsageFlagBits::eUniformBuffer));
		memRequirements = GetContext()->GetDevice()->getBufferMemoryRequirements(modVolUniformBuffer.get());
		modVolUniformsMemSize = memRequirements.size;
		typeIndex = findMemoryType(GetContext()->GetPhysicalDevice().getMemoryProperties(), memRequirements.memoryTypeBits,
				vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		modVolUniformMemory = GetContext()->GetDevice()->allocateMemoryUnique(vk::MemoryAllocateInfo(modVolUniformsMemSize, typeIndex));
		GetContext()->GetDevice()->bindBufferMemory(modVolUniformBuffer.get(), modVolUniformMemory.get(), 0);
	}

	void UploadUniforms(const VertexShaderUniforms& vertexUniforms, const FragmentShaderUniforms& fragmentUniforms, const ModVolShaderUniforms& modVolUniforms)
	{
		uint8_t* pData = static_cast<uint8_t*>(GetContext()->GetDevice()->mapMemory(vertexUniformMemory.get(), 0, vertexUniformMemSize));
		memcpy(pData, &vertexUniforms, sizeof(vertexUniforms));
		GetContext()->GetDevice()->unmapMemory(vertexUniformMemory.get());

		pData = static_cast<uint8_t*>(GetContext()->GetDevice()->mapMemory(fragmentUniformMemory.get(), 0, fragmentUniformsMemSize));
		memcpy(pData, &fragmentUniforms, sizeof(fragmentUniforms));
		GetContext()->GetDevice()->unmapMemory(fragmentUniformMemory.get());

		pData = static_cast<uint8_t*>(GetContext()->GetDevice()->mapMemory(modVolUniformMemory.get(), 0, modVolUniformsMemSize));
		memcpy(pData, &modVolUniforms, sizeof(modVolUniforms));
		GetContext()->GetDevice()->unmapMemory(modVolUniformMemory.get());
	}

	void CheckVertexIndexBuffers(u32 vertexSize, u32 indexSize)
	{
		if (vertexBuffers.empty())
		{
			for (int i = 0; i < GetContext()->GetSwapChainSize(); i++)
				vertexBuffers.push_back(std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice().get(), vertexSize,
						vk::BufferUsageFlagBits::eVertexBuffer)));
		}
		else if (vertexBuffers[GetCurrentImage()]->m_size < vertexSize)
		{
			INFO_LOG(RENDERER, "Increasing vertex buffer size %d -> %d", (u32)vertexBuffers[GetCurrentImage()]->m_size, vertexSize);
			vertexBuffers[GetCurrentImage()] = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice().get(), vertexSize,
					vk::BufferUsageFlagBits::eVertexBuffer));
		}
		if (indexBuffers.empty())
		{
			for (int i = 0; i < GetContext()->GetSwapChainSize(); i++)
				indexBuffers.push_back(std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice().get(), indexSize,
						vk::BufferUsageFlagBits::eIndexBuffer)));
		}
		else if (indexBuffers[GetCurrentImage()]->m_size < indexSize)
		{
			INFO_LOG(RENDERER, "Increasing index buffer size %d -> %d", (u32)indexBuffers[GetCurrentImage()]->m_size, indexSize);
			indexBuffers[GetCurrentImage()] = std::unique_ptr<BufferData>(new BufferData(GetContext()->GetPhysicalDevice(), GetContext()->GetDevice().get(), indexSize,
					vk::BufferUsageFlagBits::eIndexBuffer));
		}
	}

	// Uniforms
	vk::UniqueBuffer vertexUniformBuffer;
	vk::UniqueBuffer fragmentUniformBuffer;
	vk::UniqueBuffer modVolUniformBuffer;
	vk::UniqueDeviceMemory vertexUniformMemory;
	vk::UniqueDeviceMemory fragmentUniformMemory;
	vk::UniqueDeviceMemory modVolUniformMemory;
	vk::DeviceSize vertexUniformMemSize;
	vk::DeviceSize fragmentUniformsMemSize;
	vk::DeviceSize modVolUniformsMemSize;

	// Buffers
	std::vector<std::unique_ptr<BufferData>> vertexBuffers;
	std::vector<std::unique_ptr<BufferData>> indexBuffers;

	ShaderManager shaderManager;
	PipelineManager pipelineManager;
};

Renderer* rend_Vulkan()
{
	return new VulkanRenderer();
}
