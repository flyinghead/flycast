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
#ifdef LIBRETRO
#include "dx11context_lr.h"
#elif defined(_WIN32)
#include "types.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include "windows/comptr.h"
#include "dx11_overlay.h"
#include "wsi/context.h"

class DX11Context : public GraphicsContext
{
public:
	bool init(bool keepCurrentWindow = false);
	void initVideoRouting() override;
	void term() override;
	void EndImGuiFrame();
	void Present();
	const ComPtr<ID3D11Device>& getDevice() const { return pDevice; }
	const ComPtr<ID3D11DeviceContext>& getDeviceContext() const { return pDeviceContext; }
	ComPtr<ID3D11RenderTargetView>& getRenderTarget() { return renderTargetView; }
	const pD3DCompile getCompiler();

	void resize() override;
	void setOverlay(bool overlayOnly) { this->overlayOnly = overlayOnly; }
	std::string getDriverName() override {
		return adapterDesc;
	}
	std::string getDriverVersion() override {
		return adapterVersion;
	}
	bool hasPerPixel() override {
		return featureLevel >= D3D_FEATURE_LEVEL_11_0;
	}
	bool isIntel() const {
		return vendorId == VENDOR_INTEL;
	}

	void setFrameRendered() {
		frameRendered = true;
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
	bool textureFormatSupported(TextureType texType) {
		return supportedTexFormats[(int)texType];
	}

private:
	void handleDeviceLost();
	bool checkTextureSupport();

	ComPtr<ID3D11Device> pDevice;
	ComPtr<ID3D11DeviceContext> pDeviceContext;
	ComPtr<IDXGISwapChain> swapchain;
	ComPtr<IDXGISwapChain1> swapchain1;
	ComPtr<ID3D11RenderTargetView> renderTargetView;
	bool overlayOnly = false;
	DX11Overlay overlay;
	bool swapOnVSync = false;
	bool frameRendered = false;
	std::string adapterDesc;
	std::string adapterVersion;
	UINT vendorId = 0;
	bool _hasShaderCache = false;
	DX11Shaders shaders;
	Samplers samplers;
	D3D_FEATURE_LEVEL featureLevel{};
	bool supportedTexFormats[5] {}; // indexed by TextureType enum
	HMODULE d3dcompilerHandle = NULL;
	pD3DCompile d3dcompiler = nullptr;

	static constexpr UINT VENDOR_INTEL = 0x8086;
};
extern DX11Context theDX11Context;
#endif
