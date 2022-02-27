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
#include <windows.h>
#include <d3d11.h>
#include <array>
#include "windows/comptr.h"
#include "dx11_quad.h"
#include "dx11_texture.h"
#include "dx11_renderstate.h"

class DX11Overlay
{
public:
	void init(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> deviceContext, DX11Shaders *shaders, Samplers *samplers)
	{
		this->device = device;
		this->deviceContext = deviceContext;
		this->samplers = samplers;
		quad.init(device, deviceContext, shaders);
	}

	void term()
	{
		blendStates.term();
		depthStencilStates.term();
		xhairTextureView.reset();
		xhairTexture.reset();
		for (auto& view : vmuTextureViews)
			view.reset();
		for (auto& tex : vmuTextures)
			tex.reset();
		deviceContext.reset();
		device.reset();
	}

	void draw(u32 width, u32 height, bool vmu, bool crosshair);

private:
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> deviceContext;
	ComPtr<ID3D11Texture2D> xhairTexture;
	ComPtr<ID3D11ShaderResourceView> xhairTextureView;
	std::array<ComPtr<ID3D11Texture2D>, 8> vmuTextures;
	std::array<ComPtr<ID3D11ShaderResourceView>, 8> vmuTextureViews;
	Quad quad;
	Samplers *samplers;
	BlendStates blendStates;
	DepthStencilStates depthStencilStates;
};
