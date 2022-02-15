/*
	Copyright 2021 flyinghead

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
#include "types.h"
#include "hw/pvr/Renderer_if.h"
#include <d3d11.h>
#include "../dx11context.h"
#include "../dx11_renderer.h"
#include "rend/transform_matrix.h"
#include "../dx11_quad.h"
#include "../dx11_texture.h"
#include "dx11_oitshaders.h"
#include "rend/sorter.h"
#include "../dx11_renderstate.h"
#include "dx11_oitbuffers.h"
#include "dx11_oitshaders.h"
#include "rend/tileclip.h"

const D3D11_INPUT_ELEMENT_DESC MainLayout[]
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(Vertex, x), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, spc), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(Vertex, u),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    2, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, col1), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR",    3, DXGI_FORMAT_B8G8R8A8_UNORM, 0, (UINT)offsetof(Vertex, spc1), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,   0, (UINT)offsetof(Vertex, u1),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

struct DX11OITRenderer : public DX11Renderer
{
	struct PixelPolyConstants
	{
		float clipTest[4];
		int blend_mode0[2];
		int blend_mode1[2];
		float paletteIndex;
		float trilinearAlpha;
		int pp_Number;

		// two volume mode
		int shading_instr0;
		int shading_instr1;
		int fog_control0;
		int fog_control1;
		int use_alpha0;
		int use_alpha1;
		int ignore_tex_alpha0;
		int ignore_tex_alpha1;
	};

	bool Init() override
	{
		if (!DX11Renderer::Init())
			return false;
		pxlPolyConstants.reset();
		D3D11_BUFFER_DESC desc{};
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		desc.ByteWidth = sizeof(PixelPolyConstants);
		desc.ByteWidth = (((desc.ByteWidth - 1) >> 4) + 1) << 4;
		bool success = SUCCEEDED(device->CreateBuffer(&desc, nullptr, &pxlPolyConstants.get()));

		shaders.init(device, theDX11Context.getCompiler());
		buffers.init(device, deviceContext);
		ComPtr<ID3DBlob> blob = shaders.getVertexShaderBlob();
		mainInputLayout.reset();
		return success && SUCCEEDED(device->CreateInputLayout(MainLayout, ARRAY_SIZE(MainLayout), blob->GetBufferPointer(), blob->GetBufferSize(), &mainInputLayout.get()));
	}

	void Resize(int w, int h) override {
		DX11Renderer::Resize(w, h);
		buffers.resize(w, h);

		// FIXME must be used by RTT too
		createTexAndRenderTarget(opaqueTex, opaqueRenderTarget, w, h);
		opaqueTextureView.reset();
		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		device->CreateShaderResourceView(opaqueTex, &viewDesc, &opaqueTextureView.get());

		// For depth pass. Use a 32-bit format for depth to avoid loss of precision
		createDepthTexAndView(depthStencilTex2, depthStencilView2, width, height, DXGI_FORMAT_R32G8X24_TYPELESS, D3D11_BIND_SHADER_RESOURCE);
		stencilView.reset();
		viewDesc.Format = DXGI_FORMAT_X32_TYPELESS_G8X24_UINT;
		device->CreateShaderResourceView(depthStencilTex2, &viewDesc, &stencilView.get());

		depthView.reset();
		viewDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		device->CreateShaderResourceView(depthStencilTex2, &viewDesc, &depthView.get());
	}

	void Term() override
	{
		opaqueTextureView.reset();
		opaqueRenderTarget.reset();
		opaqueTex.reset();
		shaders.term();
		buffers.term();
		DX11Renderer::Term();
	}

	template <u32 Type, bool SortingEnabled, DX11OITShaders::Pass pass>
	void setRenderState(const PolyParam *gp, int polyNumber)
	{
		ComPtr<ID3D11VertexShader> vertexShader = shaders.getVertexShader(gp->pcw.Gouraud);
		deviceContext->VSSetShader(vertexShader, nullptr, 0);

		PixelPolyConstants constants;
		if (gp->pcw.Texture && gp->tsp.FilterMode > 1 && Type != ListType_Punch_Through && gp->tcw.MipMapped == 1)
		{
			constants.trilinearAlpha = 0.25f * (gp->tsp.MipMapD & 0x3);
			if (gp->tsp.FilterMode == 2)
				// Trilinear pass A
				constants.trilinearAlpha = 1.f - constants.trilinearAlpha;
		}
		else
			constants.trilinearAlpha = 1.f;

		int clip_rect[4] = {};
		TileClipping clipmode = GetTileClip(gp->tileclip, matrices.GetViewportMatrix(), clip_rect);
		bool gpuPalette = gp->texture != nullptr ? gp->texture->gpuPalette : false;
		// Two volumes mode only supported for OP and PT
		bool two_volumes_mode = (gp->tsp1.full != (u32)-1) && Type != ListType_Translucent;
		bool useTexture;

		ComPtr<ID3D11PixelShader> pixelShader;
		if (pass == DX11OITShaders::Depth)
		{
			useTexture = Type == ListType_Punch_Through ? gp->pcw.Texture : false;
			pixelShader = shaders.getShader(
					useTexture,
					gp->tsp.UseAlpha,
					gp->tsp.IgnoreTexA,
					0,
					false,
					2,
					false,
					false,
					gpuPalette,
					gp->pcw.Gouraud,
					useTexture,
					clipmode == TileClipping::Inside,
					false,
					two_volumes_mode,
					pass);
		}
		else
		{
			bool color_clamp = gp->tsp.ColorClamp && (pvrrc.fog_clamp_min.full != 0 || pvrrc.fog_clamp_max.full != 0xffffffff);

			int fog_ctrl = config::Fog ? gp->tsp.FogCtrl : 2;
			useTexture = gp->pcw.Texture;

			pixelShader = shaders.getShader(
					useTexture,
					gp->tsp.UseAlpha,
					gp->tsp.IgnoreTexA,
					gp->tsp.ShadInstr,
					gp->pcw.Offset,
					fog_ctrl,
					gp->tcw.PixelFmt == PixelBumpMap,
					color_clamp,
					gpuPalette,
					gp->pcw.Gouraud,
					Type == ListType_Punch_Through,
					clipmode == TileClipping::Inside,
					gp->pcw.Texture && gp->tsp.FilterMode == 0 && !gp->tsp.ClampU && !gp->tsp.ClampV && !gp->tsp.FlipU && !gp->tsp.FlipV,
					two_volumes_mode,
					pass);

		}
		deviceContext->PSSetShader(pixelShader, nullptr, 0);

		if (gpuPalette)
		{
			if (gp->tcw.PixelFmt == PixelPal4)
				constants.paletteIndex = (float)(gp->tcw.PalSelect << 4);
			else
				constants.paletteIndex = (float)((gp->tcw.PalSelect >> 4) << 8);
		}

		if (clipmode == TileClipping::Outside)
		{
			RECT rect { clip_rect[0], clip_rect[1], clip_rect[0] + clip_rect[2], clip_rect[1] + clip_rect[3] };
			deviceContext->RSSetScissorRects(1, &rect);
		}
		else
		{
			deviceContext->RSSetScissorRects(1, &scissorRect);
			if (clipmode == TileClipping::Inside)
			{
				constants.clipTest[0] = (float)clip_rect[0];
				constants.clipTest[1] = (float)clip_rect[1];
				constants.clipTest[2] = (float)(clip_rect[0] + clip_rect[2]);
				constants.clipTest[3] = (float)(clip_rect[1] + clip_rect[3]);
			}
		}
		constants.pp_Number = polyNumber;
		constants.blend_mode0[0] = gp->tsp.SrcInstr;
		constants.blend_mode0[1] = gp->tsp.DstInstr;
		if (two_volumes_mode)
		{
			constants.blend_mode1[0] = gp->tsp1.SrcInstr;
			constants.blend_mode1[1] = gp->tsp1.DstInstr;
			constants.shading_instr0 = gp->tsp.ShadInstr;
			constants.shading_instr1 = gp->tsp1.ShadInstr;
			constants.fog_control0 = gp->tsp.FogCtrl;
			constants.fog_control1 = gp->tsp1.FogCtrl;
			constants.use_alpha0 = gp->tsp.UseAlpha;
			constants.use_alpha1 = gp->tsp1.UseAlpha;
			constants.ignore_tex_alpha0 = gp->tsp.IgnoreTexA;
			constants.ignore_tex_alpha1 = gp->tsp1.IgnoreTexA;
		}
		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		deviceContext->Map(pxlPolyConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, &constants, sizeof(constants));
		deviceContext->Unmap(pxlPolyConstants, 0);

		if (pass == DX11OITShaders::Color)
		{
			// Apparently punch-through polys support blending, or at least some combinations
			if (Type == ListType_Translucent || Type == ListType_Punch_Through)
				deviceContext->OMSetBlendState(blendStates.getState(true, gp->tsp.SrcInstr, gp->tsp.DstInstr), nullptr, 0xffffffff);
			else
				deviceContext->OMSetBlendState(blendStates.getState(false, gp->tsp.SrcInstr, gp->tsp.DstInstr), nullptr, 0xffffffff);
		}
		if (useTexture)
		{
			for (int i = 0; i < 2; i++)
			{
				DX11Texture *texture = (DX11Texture *)(i == 0 ? gp->texture : gp->texture1);
				if (texture == nullptr)
					continue;
				int slot = i == 0 ? 0 : 3;
				deviceContext->PSSetShaderResources(slot, 1, &texture->textureView.get());
				TSP tsp = i == 0 ? gp->tsp : gp->tsp1;
		        auto sampler = samplers->getSampler(tsp.FilterMode != 0 && !gpuPalette, tsp.ClampU, tsp.ClampV, tsp.FlipU, tsp.FlipV);
		        deviceContext->PSSetSamplers(slot, 1, &sampler.get());
			}
		}

		setCullMode(gp->isp.CullMode);

		//set Z mode, only if required
		int zfunc;
		if (Type == ListType_Punch_Through || (pass == DX11OITShaders::Depth && SortingEnabled))
			zfunc = 6; // GEQ
		else
			zfunc = gp->isp.DepthMode;

		bool zwriteEnable = false;
		if (pass == DX11OITShaders::Depth || pass == DX11OITShaders::Color)
		{
			// Z Write Disable seems to be ignored for punch-through.
			// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
			if (Type == ListType_Punch_Through)
				zwriteEnable = true;
			else
				zwriteEnable = !gp->isp.ZWriteDis;
		}
		bool needStencil = config::ModifierVolumes && pass == DX11OITShaders::Depth && Type != ListType_Translucent;
		const u32 stencil = (gp->pcw.Shadow != 0) ? 0x80 : 0;
		deviceContext->OMSetDepthStencilState(depthStencilStates.getState(true, zwriteEnable, zfunc, needStencil), stencil);
	}

	template <u32 Type, bool SortingEnabled, DX11OITShaders::Pass pass>
	void drawList(const List<PolyParam>& gply, int first, int count)
	{
		deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		PolyParam* params = &gply.head()[first];

		while (count-- > 0)
		{
			if (params->count > 2)
			{
				if ((Type == ListType_Opaque || (Type == ListType_Translucent && !SortingEnabled)) && params->isp.DepthMode == 0)
				{
					// depthFunc = never
					params++;
					continue;
				}
				setRenderState<Type, SortingEnabled, pass>(params, (int)(params - gply.head()));
				deviceContext->DrawIndexed(params->count, params->first, 0);
			}

			params++;
		}
	}

	template<bool Transparent>
	void drawModVols(int first, int count)
	{
		if (count == 0 || pvrrc.modtrig.used() == 0 || !config::ModifierVolumes)
			return;

		deviceContext->IASetInputLayout(modVolInputLayout);
	    unsigned int stride = 3 * sizeof(float);
	    unsigned int offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &modvolBuffer.get(), &stride, &offset);
		deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->VSSetShader(shaders.getMVVertexShader(), nullptr, 0);
		if (!Transparent)
			deviceContext->PSSetShader(shaders.getModVolShader(), nullptr, 0);
		deviceContext->RSSetScissorRects(1, &scissorRect);

		ModifierVolumeParam* params = Transparent ? &pvrrc.global_param_mvo_tr.head()[first] : &pvrrc.global_param_mvo.head()[first];
		int mod_base = -1;

		for (int cmv = 0; cmv < count; cmv++)
		{
			ModifierVolumeParam& param = params[cmv];

			u32 mv_mode = param.isp.DepthMode;

			if (mod_base == -1)
				mod_base = param.first;

			if (param.count > 0)
			{
				if (Transparent)
				{
					if (!param.isp.VolumeLast && mv_mode > 0)
						// OR'ing (open volume or quad)
						deviceContext->PSSetShader(shaders.getTrModVolShader(DepthStencilStates::Or), nullptr, 0);
					else
						// XOR'ing (closed volume)
						deviceContext->PSSetShader(shaders.getTrModVolShader(DepthStencilStates::Xor), nullptr, 0);
				}
				else
				{
					if (!param.isp.VolumeLast && mv_mode > 0)
						// OR'ing (open volume or quad)
						deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Or), 2);
					else
						// XOR'ing (closed volume)
						deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(DepthStencilStates::Xor), 0);
				}
				setCullMode(param.isp.CullMode);
				deviceContext->Draw(param.count * 3, param.first * 3);
			}

			if (mv_mode == 1 || mv_mode == 2)
			{
				// Sum the area
				if (Transparent)
					deviceContext->PSSetShader(shaders.getTrModVolShader(mv_mode == 1 ? DepthStencilStates::Inclusion : DepthStencilStates::Exclusion), nullptr, 0);
				else
					deviceContext->OMSetDepthStencilState(depthStencilStates.getMVState(mv_mode == 1 ? DepthStencilStates::Inclusion : DepthStencilStates::Exclusion), 1);
				deviceContext->Draw((param.first + param.count - mod_base) * 3, mod_base * 3);
				mod_base = -1;
			}
		}

		// Restore main input layout and vertex buffers
		deviceContext->IASetInputLayout(mainInputLayout);
	    stride = sizeof(Vertex);
	    offset = 0;
		deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);
		deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}

	void renderABuffer()
	{
		if (pvrrc.isRTT)
			deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &rttRenderTarget.get(), nullptr, 0, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);
		else
			deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &fbRenderTarget.get(), nullptr, 0, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);
		deviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
		deviceContext->PSSetShaderResources(0, 1, &opaqueTextureView.get());
        auto sampler = samplers->getSampler(false);
        deviceContext->PSSetSamplers(0, 1, &sampler.get());
		deviceContext->RSSetScissorRects(1, &scissorRect);
		deviceContext->OMSetDepthStencilState(depthStencilStates.getState(false, false, 0, false), 0);
		setCullMode(0);

		deviceContext->VSSetShader(shaders.getFinalVertexShader(), nullptr, 0);
		deviceContext->PSSetShader(shaders.getFinalShader(), nullptr, 0);

		deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		deviceContext->Draw(4, 0);
	}

	void drawStrips()
	{
		{
			// tr_poly_params
			std::vector<u32> trPolyParams(pvrrc.global_param_tr.used() * 2);
			const PolyParam *pp_end = pvrrc.global_param_tr.LastPtr(0);
			const PolyParam *pp = pvrrc.global_param_tr.head();
			for (int i = 0; pp != pp_end; i += 2, pp++)
			{
				trPolyParams[i] = (pp->tsp.full & 0xffff00c0) | ((pp->isp.full >> 16) & 0xe400) | ((pp->pcw.full >> 7) & 1);
				trPolyParams[i + 1] = pp->tsp1.full;
			}
			u32 newSize = (u32)(trPolyParams.size() * sizeof(u32));
			if (newSize > 0)
			{
				if (!trPolyParamsBuffer || trPolyParamsBufferSize < newSize)
				{
					trPolyParamsBufferView.reset();
					trPolyParamsBuffer.reset();
					D3D11_BUFFER_DESC desc{};
					desc.ByteWidth = newSize;
					desc.Usage = D3D11_USAGE_DYNAMIC;
					desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
					desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
					desc.StructureByteStride = sizeof(u32) * 2;	// sizeof(struct PolyParam)
					desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

					HRESULT hr = device->CreateBuffer(&desc, nullptr, &trPolyParamsBuffer.get());
					if (FAILED(hr))
						WARN_LOG(RENDERER, "TR poly params buffer creation failed");
					else
					{
						trPolyParamsBufferSize = newSize;
						D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
						viewDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
						viewDesc.Format = DXGI_FORMAT_UNKNOWN;
						viewDesc.Buffer.NumElements = desc.ByteWidth / desc.StructureByteStride;

						hr = device->CreateShaderResourceView(trPolyParamsBuffer, &viewDesc, &trPolyParamsBufferView.get());
						if (FAILED(hr))
							WARN_LOG(RENDERER, "TR poly params buffer view creation failed");
					}
				}
				D3D11_MAPPED_SUBRESOURCE mappedSubres;
				deviceContext->Map(trPolyParamsBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
				memcpy(mappedSubres.pData, trPolyParams.data(), newSize);
				deviceContext->Unmap(trPolyParamsBuffer, 0);

				deviceContext->PSSetShaderResources(5, 1, &trPolyParamsBufferView.get());
			}
		}

		buffers.bind();
		deviceContext->ClearDepthStencilView(depthTexView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);
		deviceContext->ClearDepthStencilView(depthStencilView2, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.f, 0);

		RenderPass previous_pass {};
		int render_pass_count = pvrrc.render_passes.used();
		for (int render_pass = 0; render_pass < render_pass_count; render_pass++)
		{
			const RenderPass& current_pass = pvrrc.render_passes.head()[render_pass];
			u32 op_count = current_pass.op_count - previous_pass.op_count;
			u32 pt_count = current_pass.pt_count - previous_pass.pt_count;
			u32 tr_count = current_pass.tr_count - previous_pass.tr_count;
			u32 mvo_count = current_pass.mvo_count - previous_pass.mvo_count;
			DEBUG_LOG(RENDERER, "Render pass %d OP %d PT %d TR %d MV %d Tr MV %d autosort %d", render_pass + 1,
					op_count, pt_count, tr_count, mvo_count, current_pass.mvo_tr_count - previous_pass.mvo_tr_count, current_pass.autosort);

			//
			// PASS 1: Geometry pass to update depth and stencil
			//
			// unbind depth/stencil
			ID3D11ShaderResourceView *p = nullptr;
		    deviceContext->PSSetShaderResources(4, 1, &p);
		    // disable color writes
			deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);
			deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &opaqueRenderTarget.get(), depthStencilView2, 0, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);

			drawList<ListType_Opaque, false, DX11OITShaders::Depth>(pvrrc.global_param_op, previous_pass.op_count, op_count);
			drawList<ListType_Punch_Through, false, DX11OITShaders::Depth>(pvrrc.global_param_pt, previous_pass.pt_count, pt_count);
			drawModVols<false>(previous_pass.mvo_count, mvo_count);

			//
			// PASS 2: Render OP and PT to opaque render target
			//
			deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &opaqueRenderTarget.get(), depthTexView, 0, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);
		    deviceContext->PSSetShaderResources(4, 1, &stencilView.get());

			drawList<ListType_Opaque, false, DX11OITShaders::Color>(pvrrc.global_param_op, previous_pass.op_count, op_count);
			drawList<ListType_Punch_Through, false, DX11OITShaders::Color>(pvrrc.global_param_pt, previous_pass.pt_count, pt_count);

			//
			// PASS 3: Render TR to a-buffers
			//
			if (current_pass.autosort)
			{
			    deviceContext->PSSetShaderResources(4, 1, &depthView.get());
			    // disable color writes
				deviceContext->OMSetBlendState(blendStates.getState(false, 0, 0, true), nullptr, 0xffffffff);
				drawList<ListType_Translucent, true, DX11OITShaders::OIT>(pvrrc.global_param_tr, previous_pass.tr_count, tr_count);
				if (render_pass < render_pass_count - 1)
				{
					//
					// PASS 3b: Geometry pass with TR to update the depth for the next TA render pass
					//
					ID3D11ShaderResourceView *p = nullptr;
				    deviceContext->PSSetShaderResources(4, 1, &p);
					deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(1, &opaqueRenderTarget.get(), depthTexView, 0, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, nullptr, nullptr);
					drawList<ListType_Translucent, true, DX11OITShaders::Depth>(pvrrc.global_param_tr, previous_pass.tr_count, tr_count);
				}
				ID3D11ShaderResourceView *p = nullptr;
			    deviceContext->PSSetShaderResources(4, 1, &p);
			    if (!theDX11Context.isIntel())
			    	// Intel Iris Plus 640 just crashes
			    	drawModVols<true>(previous_pass.mvo_tr_count, current_pass.mvo_tr_count - previous_pass.mvo_tr_count);
			}
			else
			{
				ID3D11ShaderResourceView *p = nullptr;
			    deviceContext->PSSetShaderResources(4, 1, &p);
				drawList<ListType_Translucent, false, DX11OITShaders::Color>(pvrrc.global_param_tr, previous_pass.tr_count, tr_count);
			}
			if (render_pass < render_pass_count - 1)
			{
				//
				// PASS 3c: Render a-buffer to temporary texture
				//
				renderABuffer();
			    deviceContext->PSSetShaderResources(0, 1, &p);

				// Clear the stencil from this pass
				deviceContext->ClearDepthStencilView(depthStencilView2, D3D11_CLEAR_STENCIL, 0.f, 0);
			}

			previous_pass = current_pass;
		}

		//
		// PASS 4: Render a-buffers to screen
		//
		renderABuffer();
	}

	bool Render() override
	{
		// Make sure to unbind the framebuffer view before setting it as render target
		ID3D11ShaderResourceView *p = nullptr;
	    deviceContext->PSSetShaderResources(0, 1, &p);
	    // To avoid DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET warnings
		deviceContext->OMSetRenderTargets(1, &fbRenderTarget.get(), depthTexView);
		configVertexShader();

		bool is_rtt = pvrrc.isRTT;
		u32 texAddress = FB_W_SOF1 & VRAM_MASK;

		deviceContext->IASetInputLayout(mainInputLayout);

		if (!pvrrc.isRenderFramebuffer)
		{
			uploadGeometryBuffers();

			updateFogTexture();
			updatePaletteTexture();

			setupPixelShaderConstants();

			drawStrips();
		}
		else
		{
			renderDCFramebuffer();
		}

		if (is_rtt)
		{
			readRttRenderTarget(texAddress);
		}
		else
		{
#ifndef LIBRETRO
			deviceContext->OMSetRenderTargets(1, &theDX11Context.getRenderTarget().get(), nullptr);
			renderFramebuffer();
			DrawOSD(false);
			theDX11Context.setFrameRendered();
#else
			theDX11Context.drawOverlay(width, height);
			ID3D11RenderTargetView *nullView = nullptr;
			deviceContext->OMSetRenderTargets(1, &nullView, nullptr);
			deviceContext->PSSetShaderResources(0, 1, &fbTextureView.get());
#endif
			frameRendered = true;
			frameRenderedOnce = true;
		}

		return !is_rtt;
	}

private:
	Buffers buffers;
	DX11OITShaders shaders;
	ComPtr<ID3D11Texture2D> opaqueTex;
	ComPtr<ID3D11RenderTargetView> opaqueRenderTarget;
	ComPtr<ID3D11ShaderResourceView> opaqueTextureView;
	ComPtr<ID3D11ShaderResourceView> stencilView;
	ComPtr<ID3D11ShaderResourceView> depthView;
	ComPtr<ID3D11Texture2D> depthStencilTex2;
	ComPtr<ID3D11DepthStencilView> depthStencilView2;
	ComPtr<ID3D11Buffer> trPolyParamsBuffer;
	u32 trPolyParamsBufferSize = 0;
	ComPtr<ID3D11ShaderResourceView> trPolyParamsBufferView;
};

Renderer *rend_OITDirectX11()
{
	return new DX11OITRenderer();
}
