/*
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
// Sega Hikaru renderer for DX11, independent of the PVR / Naomi 2 rendering path.
// It draws a hikaru::DrawList with its own shaders, render states and textures (taken from the
// Hikaru texture store); only the render target, depth buffer and output are the DX11 renderer's.
#pragma once
#include <windows.h>
#include <d3d11.h>
#include "windows/comptr.h"
#include "hw/hikaru/hikaru_drawlist.h"
#include <vector>

class HikaruRenderer
{
public:
	bool init(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext> deviceContext);
	void term();

	// Draw a frame into the render target and depth buffer, both width x height pixels
	void draw(const hikaru::DrawList& dl, ID3D11RenderTargetView *renderTarget, ID3D11DepthStencilView *depth,
			int width, int height);

private:
	struct Texture
	{
		ComPtr<ID3D11Texture2D> texture;
		ComPtr<ID3D11ShaderResourceView> view;
		u32 width = 0;
		u32 height = 0;
		u32 levels = 0;
		u32 version = 0;
	};

	bool updateTexture(int id);
	ID3D11ShaderResourceView *textureView(int id) const;
	void uploadTextures(const hikaru::DrawList& dl);
	void uploadGeometry(const hikaru::DrawList& dl);
	ID3D11SamplerState *getSampler(bool linear, bool mipmapped, hikaru::Address u, hikaru::Address v);
	void ensureBuffers(int width, int height);
	void drawScreen(ID3D11PixelShader *shader, ID3D11ShaderResourceView *const views[3], const hikaru::DrawList& dl);

	template<typename T>
	void setConstBuffer(const ComPtr<ID3D11Buffer>& buffer, const T& data)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubres;
		deviceContext->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubres);
		memcpy(mappedSubres.pData, &data, sizeof(T));
		deviceContext->Unmap(buffer, 0);
	}

	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;

	ComPtr<ID3D11VertexShader> vertexShader;
	ComPtr<ID3D11PixelShader> pixelShaders[2];	// [textured]
	ComPtr<ID3D11InputLayout> inputLayout;
	ComPtr<ID3D11VertexShader> screenVertexShader;
	ComPtr<ID3D11PixelShader> blendPixelShader;
	ComPtr<ID3D11PixelShader> compositePixelShader;

	ComPtr<ID3D11Buffer> drawConstants;
	ComPtr<ID3D11Buffer> shadingConstants;
	ComPtr<ID3D11Buffer> screenConstants;
	ComPtr<ID3D11Buffer> vertexBuffer;
	ComPtr<ID3D11Buffer> indexBuffer;
	u32 vertexBufferSize = 0;
	u32 indexBufferSize = 0;

	ComPtr<ID3D11RasterizerState> rasterizers[3];		// [hikaru::Cull]
	ComPtr<ID3D11DepthStencilState> depthStates[2][2];	// [test][write]
	ComPtr<ID3D11BlendState> blendStates[5];			// [hikaru::Blend]
	ComPtr<ID3D11SamplerState> samplers[2 * 2 * 3 * 3];	// [linear][mipmapped][u][v]
	ComPtr<ID3D11SamplerState> screenSamplers[2];	// point: [clamp, wrap]

	// The 3D buffers (0x781 selects the one drawn into); they persist from frame to frame
	struct Buffer
	{
		ComPtr<ID3D11Texture2D> texture;
		ComPtr<ID3D11RenderTargetView> renderTarget;
		ComPtr<ID3D11ShaderResourceView> view;
	};
	Buffer buffers[hikaru::HIKARU_3D_BUFFERS];
	int bufferWidth = 0;
	int bufferHeight = 0;

	std::vector<Texture> textures;	// by texture store id
	float scaleX = 1.f;
	float scaleY = 1.f;
};
