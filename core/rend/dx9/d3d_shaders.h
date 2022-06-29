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
#include <unordered_map>
#include "dxcontext.h"
#include <d3dx9shader.h>

class D3DShaders
{
public:
	void init(const ComPtr<IDirect3DDevice9>& device)
	{
		this->device = device;
	}

	const ComPtr<IDirect3DPixelShader9>& getShader(bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr,
			bool pp_Offset, u32 pp_FogCtrl, bool pp_BumpMap, bool fog_clamping, bool trilinear, bool palette, bool gouraud,
			bool clipInside);
	const ComPtr<IDirect3DVertexShader9>& getVertexShader(bool gouraud);
	const ComPtr<IDirect3DPixelShader9>& getModVolShader();
	void term() {
		shaders.clear();
		for (auto& shader : vertexShaders)
			shader.reset();
		for (auto& shader : modVolShaders)
			shader.reset();
		device.reset();
	}

private:
	ComPtr<ID3DXBuffer> compileShader(const char* source, const char* function, const char* profile, const D3DXMACRO* pDefines);
	ComPtr<IDirect3DVertexShader9> compileVS(const char* source, const char* function, const D3DXMACRO* pDefines);
	ComPtr<IDirect3DPixelShader9> compilePS(const char* source, const char* function, const D3DXMACRO* pDefines);

	ComPtr<IDirect3DDevice9> device;
	std::unordered_map<u32, ComPtr<IDirect3DPixelShader9>> shaders;
	ComPtr<IDirect3DVertexShader9> vertexShaders[4];
	ComPtr<IDirect3DPixelShader9> modVolShaders[2];
};
