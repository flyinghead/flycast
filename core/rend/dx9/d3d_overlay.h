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
#include <d3d9.h>
#include <array>
#include "windows/comptr.h"

class D3DOverlay
{
public:
	void init(const ComPtr<IDirect3DDevice9>& device) {
		this->device = device;
	}

	void term() {
		device.reset();
		xhairTexture.reset();
		for (auto& vmu : vmuTextures)
			vmu.reset();
	}

	void draw(u32 width, u32 height, bool vmu, bool crosshair);

private:
	void setupRenderState(u32 displayWidth, u32 displayHeight);
	void drawQuad(const RECT& rect, D3DCOLOR color);

	struct Vertex
	{
	    float    pos[3];
	    float    uv[2];
	};

	ComPtr<IDirect3DDevice9> device;
	ComPtr<IDirect3DTexture9> xhairTexture;
	std::array<ComPtr<IDirect3DTexture9>, 8> vmuTextures;
};
