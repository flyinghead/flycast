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
#ifdef LIBRETRO
#include "dx11context_lr.h"
#include <dxgi1_2.h>
#include "rend/osd.h"
#include "rend/transform_matrix.h"

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
	quad = std::make_unique<Quad>();
	quad->init(pDevice, pDeviceContext, &shaders);

	bool success = checkTextureSupport();
	if (!success)
		term();
	return success;
}

void DX11Context::term()
{
	NOTICE_LOG(RENDERER, "DX11 Context terminating");
	GraphicsContext::instance = nullptr;
	blendStates.term();
	quad.reset();
	textureView.reset();
	renderTargetView.reset();
	texture.reset();
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

ComPtr<ID3D11RenderTargetView>& DX11Context::getRenderTarget(int width, int height)
{
	if (width != renderTargetWidth || height != renderTargetHeight || !renderTargetView)
	{
		renderTargetWidth = width;
		renderTargetHeight = height;
		texture.reset();
		renderTargetView.reset();
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = width;
		desc.Height = height;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.MipLevels = 1;

		HRESULT hr = pDevice->CreateTexture2D(&desc, nullptr, &texture.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "Framebuffer texture(%d x %d) creation failed: %x", width, height, hr);

		D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc{};
		viewDesc.Format = desc.Format;
		viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		viewDesc.Texture2D.MipLevels = 1;
		hr = pDevice->CreateShaderResourceView(texture, &viewDesc, &textureView.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "DC Framebuffer texture view creation failed");

		hr = pDevice->CreateRenderTargetView(texture, nullptr, &renderTargetView.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "Framebuffer render target creation failed");

		FLOAT black[4] = { 0.f, 0.f, 0.f, 0.f };
		pDeviceContext->ClearRenderTargetView(renderTargetView, black);
	}
	return renderTargetView;
}

void DX11Context::presentFrame(ComPtr<ID3D11ShaderResourceView>& textureView, int width, int height)
{
	ComPtr<ID3D11RenderTargetView> renderTarget = getRenderTarget(width, height);
	pDeviceContext->OMSetRenderTargets(1, &renderTarget.get(), nullptr);

	D3D11_VIEWPORT vp{};
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.f;
	vp.MaxDepth = 1.f;
	pDeviceContext->RSSetViewports(1, &vp);

	const D3D11_RECT r = { 0, 0, width, height };
	pDeviceContext->RSSetScissorRects(1, &r);
	float colors[4];
	VO_BORDER_COL.getRGBColor(colors);
	colors[3] = 1.f;
	pDeviceContext->ClearRenderTargetView(renderTarget, colors);

	float shiftX, shiftY;
	getVideoShift(shiftX, shiftY);
	shiftX *=  2.f / width;
	shiftY *=  -2.f / height;

	pDeviceContext->OMSetBlendState(blendStates.getState(false), nullptr, 0xffffffff);
	quad->draw(textureView, samplers.getSampler(false), nullptr, -1 + shiftX, -1 + shiftY, 2, 2, false);
	drawOverlay(width, height);

	ID3D11RenderTargetView *nullView = nullptr;
	pDeviceContext->OMSetRenderTargets(1, &nullView, nullptr);
	pDeviceContext->PSSetShaderResources(0, 1, &this->textureView.get());
}

#endif
