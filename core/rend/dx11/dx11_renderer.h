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
#include "dx11_renderstate.h"
#include "dx11_naomi2.h"
#ifndef LIBRETRO
#include "dx11_driver.h"
#endif

struct DX11Renderer : public Renderer
{
	bool Init() override;
	void Term() override;
	void Process(TA_context* ctx) override;
	bool Render() override;
	void RenderFramebuffer(const FramebufferInfo& info) override;

	bool Present() override
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
#ifndef LIBRETRO
		imguiDriver->setFrameRendered();
#endif
		return true;
	}

	bool RenderLastFrame() override;
	void DrawOSD(bool clear_screen) override;
	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) override;

protected:
	struct VertexConstants
	{
	    float transMatrix[4][4];
	    float leftPlane[4];
	    float topPlane[4];
	    float rightPlane[4];
	    float bottomPlane[4];
	};

	struct PixelConstants
	{
		float colorClampMin[4];
		float colorClampMax[4];
		float fog_col_vert[4];
		float fog_col_ram[4];
		float ditherColorMax[4];
		float fogDensity;
		float shadowScale;
		float alphaTestValue;
	};

	struct PixelPolyConstants
	{
		float clipTest[4];
		float paletteIndex;
		float trilinearAlpha;
	};

	virtual void resize(int w, int h);
	bool ensureBufferSize(ComPtr<ID3D11Buffer>& buffer, D3D11_BIND_FLAG bind, u32& currentSize, u32 minSize);
	void createDepthTexAndView(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11DepthStencilView>& view, int width, int height, DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT, UINT bindFlags = 0);
	void createTexAndRenderTarget(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11RenderTargetView>& renderTarget, int width, int height);
	void configVertexShader();
	void uploadGeometryBuffers();
	void setupPixelShaderConstants();
	void updateFogTexture();
	void updatePaletteTexture();
	void readRttRenderTarget(u32 texAddress);
	void displayFramebuffer();
	void setCullMode(int mode);
	virtual void setRTTSize(int width, int height) {}
	void writeFramebufferToVRAM();
	void renderVideoRouting();

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;
	ComPtr<ID3D11Texture2D> depthTex;
	ComPtr<ID3D11DepthStencilView> depthTexView;
	ComPtr<ID3D11InputLayout> mainInputLayout;
	ComPtr<ID3D11InputLayout> modVolInputLayout;
	ComPtr<ID3D11Buffer> pxlPolyConstants;
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> indexBuffer;
	ComPtr<ID3D11Buffer> modvolBuffer;
	ComPtr<ID3D11RenderTargetView> fbRenderTarget;
	ComPtr<ID3D11RenderTargetView> rttRenderTarget;
	ComPtr<ID3D11ShaderResourceView> fbTextureView;

	BlendStates blendStates;
	DepthStencilStates depthStencilStates;
	Samplers *samplers;
	TransformMatrix<COORD_DIRECTX> matrices;
	D3D11_RECT scissorRect{};
	u32 width = 0;
	u32 height = 0;
	bool frameRendered = false;
	bool frameRenderedOnce = false;
	Naomi2Helper n2Helper;
	float aspectRatio = 4.f / 3.f;
	bool dithering = false;

private:
	void readDCFramebuffer();
	void prepareRttRenderTarget(u32 texAddress);
	void setBaseScissor();
	void drawStrips();
	template <u32 Type, bool SortingEnabled>
	void drawList(const std::vector<PolyParam>& gply, int first, int count);
	template <u32 Type, bool SortingEnabled>
	void setRenderState(const PolyParam *gp);
	void drawSorted(int first, int count, bool multipass);
	void drawModVols(int first, int count);

	u32 vertexBufferSize = 0;
	u32 modvolBufferSize = 0;
	u32 indexBufferSize = 0;

	ComPtr<ID3D11Texture2D> fbTex;
	ComPtr<ID3D11Texture2D> dcfbTexture;
	ComPtr<ID3D11ShaderResourceView> dcfbTextureView;
	ComPtr<ID3D11Texture2D> paletteTexture;
	ComPtr<ID3D11ShaderResourceView> paletteTextureView;
	ComPtr<ID3D11Texture2D> fogTexture;
	ComPtr<ID3D11ShaderResourceView> fogTextureView;
	ComPtr<ID3D11Texture2D> rttTexture;
	ComPtr<ID3D11Texture2D> rttDepthTex;
	ComPtr<ID3D11DepthStencilView> rttDepthTexView;
	ComPtr<ID3D11Texture2D> whiteTexture;
	ComPtr<ID3D11ShaderResourceView> whiteTextureView;
	ComPtr<ID3D11Texture2D> fbScaledTexture;
	ComPtr<ID3D11ShaderResourceView> fbScaledTextureView;
	ComPtr<ID3D11RenderTargetView> fbScaledRenderTarget;
	ComPtr<ID3D11Texture2D> vrStagingTexture;
	ComPtr<ID3D11ShaderResourceView> vrStagingTextureSRV;
	ComPtr<ID3D11Texture2D> vrScaledTexture;
	ComPtr<ID3D11RenderTargetView> vrScaledRenderTarget;

	ComPtr<ID3D11RasterizerState> rasterCullNone, rasterCullFront, rasterCullBack;

	DX11TextureCache texCache;
	DX11Shaders *shaders;
	std::unique_ptr<Quad> quad;
	ComPtr<ID3D11Buffer> vtxConstants;
	ComPtr<ID3D11Buffer> pxlConstants;
	bool scissorEnable = false;
};
