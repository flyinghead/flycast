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
#pragma once
#include "types.h"
#include <array>
#include "hw/pvr/Renderer_if.h"
#include <d3d9.h>
#include "dxcontext.h"
#include "rend/transform_matrix.h"
#include "d3d_texture.h"
#include "d3d_shaders.h"
#include "rend/sorter.h"

class RenderStateCache
{
	IDirect3DDevice9 *device = nullptr;
	std::array<DWORD, 210> renderState;
	std::array<DWORD, 14> sampler0State;
	IDirect3DVertexShader9 *vertexShader = nullptr;
	IDirect3DPixelShader9 *pixelShader = nullptr;
	IDirect3DBaseTexture9 *texture = nullptr;

public:
	void setDevice(IDirect3DDevice9 *device) {
		this->device = device;
		reset();
	}

	void reset() {
		renderState.fill(0xfefefefe);
		sampler0State.fill(0xfefefefe);
		vertexShader = nullptr;
		pixelShader = nullptr;
		texture = nullptr;
	}

	HRESULT SetRenderState(D3DRENDERSTATETYPE state, DWORD value)
	{
		if ((u32)state < renderState.size())
		{
			if (renderState[state] == value)
				return S_OK;
			renderState[state] = value;
		}
		return device->SetRenderState(state, value);
	}
	HRESULT SetSamplerState(DWORD sampler, D3DSAMPLERSTATETYPE type, DWORD value)
	{
		if (sampler == 0 && (u32)type < sampler0State.size())
		{
			if (sampler0State[type] == value)
				return S_OK;
			sampler0State[type] = value;
		}
		return device->SetSamplerState(sampler, type, value);
	}
	HRESULT SetVertexShader(IDirect3DVertexShader9 *pShader)
	{
		if (pShader == vertexShader)
			return S_OK;
		vertexShader = pShader;
		return device->SetVertexShader(pShader);
	}
	HRESULT SetPixelShader(IDirect3DPixelShader9 *pShader)
	{
		if (pShader == pixelShader)
			return S_OK;
		pixelShader = pShader;
		return device->SetPixelShader(pShader);
	}
	HRESULT SetTexture(DWORD stage, IDirect3DBaseTexture9 *pTexture)
	{
		if (stage == 0)
		{
			if (pTexture == texture)
				return S_OK;
			texture = pTexture;
		}
		return device->SetTexture(stage, pTexture);
	}
};

struct D3DRenderer : public Renderer
{
	bool Init() override;
	void Resize(int w, int h) override;
	void Term() override;
	bool Process(TA_context* ctx) override;
	bool Render() override;
	bool RenderLastFrame() override;
	bool Present() override
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
		return true;
	}
	void DrawOSD(bool clear_screen) override;
	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override;
	void preReset();
	void postReset();

private:
	enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, ModeCount };

	void drawStrips();
	template <u32 Type, bool SortingEnabled>
	void drawList(const List<PolyParam>& gply, int first, int count);
	template <u32 Type, bool SortingEnabled>
	void setGPState(const PolyParam *gp);
	bool ensureVertexBufferSize(ComPtr<IDirect3DVertexBuffer9>& buffer, u32& currentSize, u32 minSize);
	bool ensureIndexBufferSize(ComPtr<IDirect3DIndexBuffer9>& buffer, u32& currentSize, u32 minSize);
	void updatePaletteTexture();
	void updateFogTexture();
	void renderFramebuffer();
	void readDCFramebuffer();
	void renderDCFramebuffer();
	void sortTriangles(int first, int count);
	void drawSorted(bool multipass);
	void setMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc);
	void drawModVols(int first, int count);
	void setProvokingVertices();
	void setTexMode(D3DSAMPLERSTATETYPE state, u32 clamp, u32 mirror);
	void setBaseScissor();
	void prepareRttRenderTarget(u32 texAddress);

	void readRttRenderTarget(u32 texAddress);

	RenderStateCache devCache;
	ComPtr<IDirect3DDevice9> device;
	ComPtr<IDirect3DVertexBuffer9> vertexBuffer;
	u32 vertexBufferSize = 0;
	ComPtr<IDirect3DVertexBuffer9> modvolBuffer;
	u32 modvolBufferSize = 0;
	ComPtr<IDirect3DIndexBuffer9> indexBuffer;
	u32 indexBufferSize = 0;
	ComPtr<IDirect3DIndexBuffer9> sortedTriIndexBuffer;
	u32 sortedTriIndexBufferSize = 0;
	ComPtr<IDirect3DVertexDeclaration9> mainVtxDecl;
	ComPtr<IDirect3DVertexDeclaration9> modVolVtxDecl;

	ComPtr<IDirect3DTexture9> framebufferTexture;
	ComPtr<IDirect3DSurface9> framebufferSurface;
	ComPtr<IDirect3DSurface9> backbuffer;
	ComPtr<IDirect3DTexture9> paletteTexture;
	ComPtr<IDirect3DTexture9> fogTexture;
	ComPtr<IDirect3DTexture9> dcfbTexture;
	ComPtr<IDirect3DSurface9> dcfbSurface;
	ComPtr<IDirect3DTexture9> rttTexture;
	ComPtr<IDirect3DSurface9> rttSurface;
	ComPtr<IDirect3DSurface9> depthSurface;

	u32 width = 0;
	u32 height = 0;
	TransformMatrix<COORD_DIRECTX> matrices;
	D3DTextureCache texCache;
	std::vector<SortTrigDrawParam> pidx_sort;
	D3DShaders shaders;
	RECT scissorRect{};
	bool scissorEnable = false;
	bool resetting = false;
	bool frameRendered = false;
	bool frameRenderedOnce = false;
};

