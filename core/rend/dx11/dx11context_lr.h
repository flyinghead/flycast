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
#include "build.h"
#if defined(HAVE_D3D11)
#include "types.h"
#include <windows.h>
#include <d3d11.h>
#include "windows/comptr.h"
#include "dx11_overlay.h"
#include "dx11_shaders.h"
#include "dx11_texture.h"
#include "wsi/context.h"
#include "dx11_renderstate.h"
#include "dx11_quad.h"

class DX11Context : public GraphicsContext
{
public:
	bool init(ID3D11Device *device, ID3D11DeviceContext *deviceContext, pD3DCompile D3DCompile, D3D_FEATURE_LEVEL featureLevel);
	void term() override;

	const ComPtr<ID3D11Device>& getDevice() const { return pDevice; }
	const ComPtr<ID3D11DeviceContext>& getDeviceContext() const { return pDeviceContext; }
	const pD3DCompile getCompiler() const { return this->D3DCompile; }
	void presentFrame(ComPtr<ID3D11ShaderResourceView>& textureView, int width, int height);

	std::string getDriverName() override { return ""; }
	std::string getDriverVersion() override { return ""; }
	bool hasPerPixel() override {
		return featureLevel >= D3D_FEATURE_LEVEL_11_0;
	}

	bool isIntel() const {
		return vendorId == VENDOR_INTEL;
	}

	DX11Shaders& getShaders() {
		return shaders;
	}
	Samplers& getSamplers() {
		return samplers;
	}
	bool hasShaderCache() const {
		return _hasShaderCache;
	}
	void drawOverlay(int width, int height);

	bool textureFormatSupported(TextureType texType) {
		return supportedTexFormats[(int)texType];
	}

private:
	bool checkTextureSupport();
	ComPtr<ID3D11RenderTargetView>& getRenderTarget(int width, int height);

	ComPtr<ID3D11Device> pDevice;
	ComPtr<ID3D11DeviceContext> pDeviceContext;
	pD3DCompile D3DCompile = nullptr;
	UINT vendorId = 0;
	bool _hasShaderCache = false;
	DX11Shaders shaders;
	Samplers samplers;
	DX11Overlay overlay;
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> textureView;
	ComPtr<ID3D11RenderTargetView> renderTargetView;
	int renderTargetWidth = 0;
	int renderTargetHeight = 0;
	BlendStates blendStates;
	std::unique_ptr<Quad> quad;

	D3D_FEATURE_LEVEL featureLevel{};
	bool supportedTexFormats[5] {}; // indexed by TextureType enum

	static constexpr UINT VENDOR_INTEL = 0x8086;
};
extern DX11Context theDX11Context;
#endif
