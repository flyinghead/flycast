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
#include "hw/pvr/Renderer_if.h"
#include <d3d11.h>
#include "dx11context.h"
#include "rend/transform_matrix.h"
#include "dx11_quad.h"
#include "dx11_texture.h"
#include "dx11_shaders.h"
#include "rend/sorter.h"
#include "dx11_renderstate.h"

struct DX11Renderer : public Renderer
{
	bool Init() override;
	void Resize(int w, int h) override;
	void Term() override;
	bool Process(TA_context* ctx) override;
	bool Render() override;

	bool Present() override
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
		return true;
	}

	bool RenderLastFrame() override;
	void DrawOSD(bool clear_screen) override;
	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override;

private:
	void readDCFramebuffer();
	void renderDCFramebuffer();
	bool ensureBufferSize(ComPtr<ID3D11Buffer>& buffer, D3D11_BIND_FLAG bind, u32& currentSize, u32 minSize);
	void setProvokingVertices();
	void prepareRttRenderTarget(u32 texAddress);
	void readRttRenderTarget(u32 texAddress);
	void renderFramebuffer();
	void updateFogTexture();
	void updatePaletteTexture();
	void setBaseScissor();
	void drawStrips();
	template <u32 Type, bool SortingEnabled>
	void drawList(const List<PolyParam>& gply, int first, int count);
	template <u32 Type, bool SortingEnabled>
	void setRenderState(const PolyParam *gp);
	void sortTriangles(int first, int count);
	void drawSorted(bool multipass);
	void drawModVols(int first, int count);
	void setCullMode(int mode);
	void createDepthTexAndView(ComPtr<ID3D11Texture2D>& depthTex, ComPtr<ID3D11DepthStencilView>& depthTexView, int width, int height);
	void createTexAndRenderTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& renderTarget, int width, int height);

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;
	ComPtr<ID3D11Buffer> vertexBuffer;
	u32 vertexBufferSize = 0;
	ComPtr<ID3D11Buffer> modvolBuffer;
	u32 modvolBufferSize = 0;
	ComPtr<ID3D11Buffer> indexBuffer;
	u32 indexBufferSize = 0;
	ComPtr<ID3D11Buffer> sortedTriIndexBuffer;
	u32 sortedTriIndexBufferSize = 0;

	ComPtr<ID3D11Texture2D> fbTex;
	ComPtr<ID3D11RenderTargetView> fbRenderTarget;
	ComPtr<ID3D11ShaderResourceView> fbTextureView;
	ComPtr<ID3D11Texture2D> depthTex;
	ComPtr<ID3D11DepthStencilView> depthTexView;
	ComPtr<ID3D11Texture2D> dcfbTexture;
	ComPtr<ID3D11ShaderResourceView> dcfbTextureView;
	ComPtr<ID3D11Texture2D> paletteTexture;
	ComPtr<ID3D11ShaderResourceView> paletteTextureView;
	ComPtr<ID3D11Texture2D> fogTexture;
	ComPtr<ID3D11ShaderResourceView> fogTextureView;
	ComPtr<ID3D11Texture2D> rttTexture;
	ComPtr<ID3D11RenderTargetView> rttRenderTarget;
	ComPtr<ID3D11Texture2D> rttDepthTex;
	ComPtr<ID3D11DepthStencilView> rttDepthTexView;
	ComPtr<ID3D11Texture2D> whiteTexture;
	ComPtr<ID3D11ShaderResourceView> whiteTextureView;

	ComPtr<ID3D11RasterizerState> rasterCullNone, rasterCullFront, rasterCullBack;

	u32 width = 0;
	u32 height = 0;
	TransformMatrix<COORD_DIRECTX> matrices;
	DX11TextureCache texCache;
	DX11Shaders *shaders;
	Samplers *samplers;
	DepthStencilStates depthStencilStates;
	BlendStates blendStates;
	std::vector<SortTrigDrawParam> pidx_sort;
	std::unique_ptr<Quad> quad;
	ComPtr<ID3D11InputLayout> mainInputLayout;
	ComPtr<ID3D11InputLayout> modVolInputLayout;
	ComPtr<ID3D11Buffer> vtxConstants;
	ComPtr<ID3D11Buffer> pxlConstants;
	ComPtr<ID3D11Buffer> pxlPolyConstants;
	D3D11_RECT scissorRect{};
	bool scissorEnable = false;
	bool frameRendered = false;
	bool frameRenderedOnce = false;
};
