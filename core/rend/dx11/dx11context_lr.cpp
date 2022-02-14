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
#ifdef HAVE_D3D11
#include "dx11context_lr.h"
#include <dxgi1_2.h>
#include "rend/osd.h"

DX11Context theDX11Context;

bool DX11Context::init(ID3D11Device *device, ID3D11DeviceContext *deviceContext, pD3DCompile D3DCompile, D3D_FEATURE_LEVEL featureLevel)
{
	NOTICE_LOG(RENDERER, "DX11 Context initializing");
	device->AddRef();
	pDevice.reset(device);
	deviceContext->AddRef();
	pDeviceContext.reset(deviceContext);
	this->D3DCompile = D3DCompile;
	this->featureLevel = featureLevel;
	GraphicsContext::instance = this;

	ComPtr<IDXGIDevice2> dxgiDevice;
	pDevice.as(dxgiDevice);

	ComPtr<IDXGIAdapter> dxgiAdapter;
	dxgiDevice->GetAdapter(&dxgiAdapter.get());
	DXGI_ADAPTER_DESC desc;
	dxgiAdapter->GetDesc(&desc);
	vendorId = desc.VendorId;

	D3D11_FEATURE_DATA_SHADER_CACHE cacheSupport{};
	if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D11_FEATURE_SHADER_CACHE, &cacheSupport, (UINT)sizeof(cacheSupport))))
	{
		_hasShaderCache = cacheSupport.SupportFlags & D3D11_SHADER_CACHE_SUPPORT_AUTOMATIC_DISK_CACHE;
		if (!_hasShaderCache)
			NOTICE_LOG(RENDERER, "No system-provided shader cache");
	}

	shaders.init(pDevice, D3DCompile);
	overlay.init(pDevice, pDeviceContext, &shaders, &samplers);
	return true;
}

void DX11Context::term()
{
	NOTICE_LOG(RENDERER, "DX11 Context terminating");
	GraphicsContext::instance = nullptr;
	overlay.term();
	samplers.term();
	shaders.term();
	if (pDeviceContext)
	{
		pDeviceContext->ClearState();
		pDeviceContext->Flush();
	}
	pDeviceContext.reset();
	pDevice.reset();
	NOTICE_LOG(RENDERER, "DX11 Context terminated");
}

void DX11Context::drawOverlay(int width, int height)
{
	overlay.draw(width, height, true, true);
}

#endif
