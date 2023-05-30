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
#include "../dx11context.h"

class Buffers
{
public:
	void init(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> deviceContext)
	{
		this->device = device;
		this->deviceContext = deviceContext;
		pixelsBufferView.reset();
		pixelsBuffer.reset();
		D3D11_BUFFER_DESC desc{};
		desc.ByteWidth = (UINT)std::min<u64>(config::PixelBufferSize, UINT_MAX);
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
		desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		desc.StructureByteStride = 16;	// sizeof(struct Pixel)

		HRESULT hr = device->CreateBuffer(&desc, nullptr, &pixelsBuffer.get());
		if (FAILED(hr))
		{
			if (desc.ByteWidth > D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_C_TERM * 1024u * 1024u)
			{
				desc.ByteWidth = D3D11_REQ_RESOURCE_SIZE_IN_MEGABYTES_EXPRESSION_C_TERM * 1024u * 1024u;
				hr = device->CreateBuffer(&desc, nullptr, &pixelsBuffer.get());
			}
			if (FAILED(hr))
			{
				WARN_LOG(RENDERER, "Pixels buffer creation failed: %x", hr);
				return;
			}
		}

		D3D11_UNORDERED_ACCESS_VIEW_DESC uaView{};
		uaView.Format = DXGI_FORMAT_UNKNOWN;
		uaView.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uaView.Buffer.FirstElement = 0;
		uaView.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
		uaView.Buffer.NumElements = desc.ByteWidth / desc.StructureByteStride;

		hr = device->CreateUnorderedAccessView(pixelsBuffer, &uaView, &pixelsBufferView.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "Pixels buffer UAV creation failed: %x", hr);
	}

	void resize(int width, int height)
	{
		if (this->width >= width && this->height >= height)
			return;

		this->width = std::max(this->width, width);
		this->height = std::max(this->height, height);
		abufferPointersView.reset();
		abufferPointersTex.reset();
		D3D11_TEXTURE2D_DESC desc{};
		desc.Width = this->width;
		desc.Height = this->height;
		desc.ArraySize = 1;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.Format = DXGI_FORMAT_R32_UINT;
		desc.MipLevels = 1;

		HRESULT hr = device->CreateTexture2D(&desc, nullptr, &abufferPointersTex.get());
		if (FAILED(hr))
		{
			WARN_LOG(RENDERER, "A-buffer texture creation failed: %x", hr);
			return;
		}
		D3D11_UNORDERED_ACCESS_VIEW_DESC uaView{};
		uaView.Format = DXGI_FORMAT_UNKNOWN;
		uaView.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;

		hr = device->CreateUnorderedAccessView(abufferPointersTex, &uaView, &abufferPointersView.get());
		if (FAILED(hr))
			WARN_LOG(RENDERER, "A-buffer texture UAV creation failed: %x", hr);
	}

	void bind()
	{
		ID3D11UnorderedAccessView *uavs[] { pixelsBufferView, abufferPointersView };
		UINT initialCounts[] { 0, (UINT)-1 };
		deviceContext->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL, nullptr, nullptr, 1, std::size(uavs), uavs, initialCounts);
	}

	void term()
	{
		width = 0;
		height = 0;
		abufferPointersView.reset();
		abufferPointersTex.reset();
		pixelsBufferView.reset();
		pixelsBuffer.reset();
		deviceContext.reset();
		device.reset();
	}

private:
	int width = 0;
	int height = 0;
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;
	ComPtr<ID3D11Texture2D> abufferPointersTex;
	ComPtr<ID3D11UnorderedAccessView> abufferPointersView;
	ComPtr<ID3D11Buffer> pixelsBuffer;
	ComPtr<ID3D11UnorderedAccessView> pixelsBufferView;
};
