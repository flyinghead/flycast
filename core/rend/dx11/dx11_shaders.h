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
#include <memory>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "types.h"
#include "windows/comptr.h"

class CachedDX11Shaders
{
protected:
	void enableCache(bool enable) { this->enabled = enable; }
	void saveCache(const std::string& filename);
	void loadCache(const std::string& filename);
	u64 hashShader(const char* source, const char* function, const char* profile, const D3D_SHADER_MACRO *pDefines = nullptr, const char *includeFile = nullptr);
	bool lookupShader(u64 hash, ComPtr<ID3DBlob>& blob);
	void cacheShader(u64 hash, const ComPtr<ID3DBlob>& blob);

private:
	struct ShaderBlob
	{
		u32 size;
		std::unique_ptr<u8[]> blob;
	};
	std::unordered_map<u64, ShaderBlob> shaderCache;
	bool enabled = true;
};

class DX11Shaders : CachedDX11Shaders
{
public:
	void init(const ComPtr<ID3D11Device>& device, pD3DCompile D3DCompile);
	void term();

	const ComPtr<ID3D11PixelShader>& getShader(bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr,
			bool pp_Offset, u32 pp_FogCtrl, bool pp_BumpMap, bool fog_clamping, bool trilinear, bool palette, bool gouraud,
			bool alphaTest, bool clipInside, bool dithering);
	const ComPtr<ID3D11VertexShader>& getVertexShader(bool gouraud, bool naomi2);
	const ComPtr<ID3D11PixelShader>& getModVolShader();
	const ComPtr<ID3D11VertexShader>& getMVVertexShader(bool naomi2);
	const ComPtr<ID3D11PixelShader>& getQuadPixelShader();
	const ComPtr<ID3D11VertexShader>& getQuadVertexShader(bool rotate);

	ComPtr<ID3DBlob> getVertexShaderBlob();
	ComPtr<ID3DBlob> getMVVertexShaderBlob();
	ComPtr<ID3DBlob> getQuadVertexShaderBlob();

private:
	ComPtr<ID3DBlob> compileShader(const char *source, const char* function, const char* profile, const D3D_SHADER_MACRO *pDefines);
	ComPtr<ID3D11VertexShader> compileVS(const char *source, const char* function, const D3D_SHADER_MACRO *pDefines);
	ComPtr<ID3D11PixelShader> compilePS(const char *source, const char* function, const D3D_SHADER_MACRO *pDefines);

	ComPtr<ID3D11Device> device;
	std::unordered_map<u32, ComPtr<ID3D11PixelShader>> shaders;
	ComPtr<ID3D11VertexShader> vertexShaders[8];
	ComPtr<ID3D11PixelShader> modVolShader;
	ComPtr<ID3D11VertexShader> modVolVertexShaders[4];
	ComPtr<ID3D11PixelShader> quadPixelShader;
	ComPtr<ID3D11VertexShader> quadVertexShader;
	ComPtr<ID3D11VertexShader> quadRotateVertexShader;
	pD3DCompile D3DCompile = nullptr;

	constexpr static const char *CacheFile = "dx11_shader_cache.bin";
};
