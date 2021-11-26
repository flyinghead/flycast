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
#if defined(_WIN32)
#include "types.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include "imgui_impl_dx11.h"
#include "../dx9/comptr.h"
// TODO #include "d3d_overlay.h"
#include "wsi/context.h"

class DX11Context : public GraphicsContext
{
public:
	bool init(bool keepCurrentWindow = false);
	void term() override;
	void EndImGuiFrame();
	void Present();
	const ComPtr<ID3D11Device>& getDevice() const { return pDevice; }
	const ComPtr<ID3D11DeviceContext>& getDeviceContext() const { return pDeviceContext; }
	ComPtr<ID3D11RenderTargetView>& getRenderTarget() { return renderTargetView; }
	void resize() override;
	void setOverlay(bool overlayOnly) { this->overlayOnly = overlayOnly; }
	std::string getDriverName() override {
		return adapterDesc;
	}
	std::string getDriverVersion() override {
		return adapterVersion;
	}
	void setFrameRendered() {
		frameRendered = true;
	}

private:
	void handleDeviceLost();

	ComPtr<ID3D11Device> pDevice;
	ComPtr<ID3D11DeviceContext> pDeviceContext;
	ComPtr<IDXGISwapChain> swapchain;
	ComPtr<IDXGISwapChain1> swapchain1;
	ComPtr<ID3D11RenderTargetView> renderTargetView;
	bool overlayOnly = false;
	// TODO D3DOverlay overlay;
	bool swapOnVSync = false;
	bool frameRendered = false;
	std::string adapterDesc;
	std::string adapterVersion;
};
extern DX11Context theDX11Context;
#endif
