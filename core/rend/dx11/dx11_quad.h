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
#include <d3d11.h>
#include "dx11_shaders.h"

class Quad
{
public:
	void init(const ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext> deviceContext, DX11Shaders *shaders)
	{
		this->device = device;
		this->deviceContext = deviceContext;
		this->shaders = shaders;

		vertexShader = shaders->getQuadVertexShader(false);
		rotateVertexShader = shaders->getQuadVertexShader(true);
		pixelShader = shaders->getQuadPixelShader();

		// Input layout
		D3D11_INPUT_ELEMENT_DESC layout[]
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0, 2 * sizeof(float),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		ComPtr<ID3DBlob> blob = shaders->getQuadVertexShaderBlob();
		if (FAILED(device->CreateInputLayout(layout, ARRAY_SIZE(layout), blob->GetBufferPointer(), blob->GetBufferSize(), &inputLayout.get())))
			WARN_LOG(RENDERER, "Input layout creation failed");

		// Rasterizer state
		{
			D3D11_RASTERIZER_DESC desc{};
			desc.FillMode = D3D11_FILL_SOLID;
			desc.CullMode = D3D11_CULL_NONE;
			desc.ScissorEnable = true;
			desc.DepthClipEnable = true;
			device->CreateRasterizerState(&desc, &rasterizerState.get());
		}
		// Depth-stencil state
		{
			D3D11_DEPTH_STENCIL_DESC desc{};
			desc.DepthEnable = false;
			desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
			desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
			desc.StencilEnable = false;
			desc.FrontFace.StencilFailOp = desc.FrontFace.StencilDepthFailOp = desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
			desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
			desc.BackFace = desc.FrontFace;
			device->CreateDepthStencilState(&desc, &depthStencilState.get());
	    }
		// Vertex buffer
		{
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = sizeof(float) * 4 * 4;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		    device->CreateBuffer(&desc, nullptr, &vertexBuffer.get());
		}
		// Constant buffer
		{
			D3D11_BUFFER_DESC desc{};
			desc.ByteWidth = sizeof(float) * 4;
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		    device->CreateBuffer(&desc, nullptr, &constantBuffer.get());
		}
	}

	void draw(ComPtr<ID3D11ShaderResourceView>& texView, ComPtr<ID3D11SamplerState> sampler, const float *color = nullptr,
			float x = -1.f, float y = -1.f, float w = 2.f, float h = 2.f, bool rotate = false)
	{
		// Vertex buffer
		Vertex vertices[4] {
			{ x,     y,     0.f, 1.f },
			{ x,     y + h, 0.f, 0.f },
			{ x + w, y,     1.f, 1.f },
			{ x + w, y + h, 1.f, 0.f },
		};
		D3D11_MAPPED_SUBRESOURCE mappedSubRes{};
		deviceContext->Map(vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubRes);
		memcpy(mappedSubRes.pData, vertices, sizeof(vertices));
		deviceContext->Unmap(vertexBuffer, 0);
	    unsigned int stride = sizeof(Vertex);
	    unsigned int offset = 0;
		deviceContext->IASetInputLayout(inputLayout);
		deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer.get(), &stride, &offset);

		// Render states
		deviceContext->OMSetDepthStencilState(depthStencilState, 0);
		deviceContext->RSSetState(rasterizerState);
		deviceContext->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		deviceContext->VSSetShader(rotate ? rotateVertexShader : vertexShader, nullptr, 0);
		deviceContext->PSSetShader(pixelShader, nullptr, 0);

		// TODO Scissor?
		//const D3D11_RECT r = { (LONG)(pcmd->ClipRect.x - clip_off.x), (LONG)(pcmd->ClipRect.y - clip_off.y), (LONG)(pcmd->ClipRect.z - clip_off.x), (LONG)(pcmd->ClipRect.w - clip_off.y) };
		//deviceContext->RSSetScissorRects(1, &r);

		static const float white[] = { 1.f, 1.f, 1.f, 1.f };
		if (color == nullptr)
			color = white;
		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		deviceContext->Map(constantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, color, sizeof(float) * 4);
		deviceContext->Unmap(constantBuffer, 0);
		deviceContext->PSSetConstantBuffers(0, 1, &constantBuffer.get());

		// Bind texture and draw
        deviceContext->PSSetShaderResources(0, 1, &texView.get());
        deviceContext->PSSetSamplers(0, 1, &sampler.get());
        deviceContext->Draw(4, 0);
	}

private:
	struct Vertex {
		float x, y, u, v;
	};

	DX11Shaders *shaders = nullptr;
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;
	ComPtr<ID3D11InputLayout> inputLayout;
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> constantBuffer;
	ComPtr<ID3D11RasterizerState> rasterizerState;
	ComPtr<ID3D11DepthStencilState> depthStencilState;
	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11VertexShader> rotateVertexShader;
	ComPtr<ID3D11PixelShader> pixelShader;
};
