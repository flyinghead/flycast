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
#include "rend/TexCache.h"
#include <d3d11.h>
#include "dx11context.h"
#include <unordered_map>

class DX11Texture final : public BaseTextureCacheData
{
public:
	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> textureView;

	std::string GetId() override { return std::to_string((uintptr_t)texture.get()); }
	void UploadToGPU(int width, int height, u8* temp_tex_buffer, bool mipmapped,
			bool mipmapsIncluded = false) override;
	bool Delete() override;
	void loadCustomTexture();
};

class DX11TextureCache final : public BaseTextureCache<DX11Texture>
{
public:
	DX11TextureCache() {
		DX11Texture::SetDirectXColorOrder(true);
	}
	~DX11TextureCache() {
		Clear();
	}
	void Cleanup()
	{
		texturesToDelete.clear();
		CollectCleanup();
	}
	void DeleteLater(ComPtr<ID3D11Texture2D> tex) { texturesToDelete.push_back(tex); }

private:
	std::vector<ComPtr<ID3D11Texture2D>> texturesToDelete;
};

class Samplers
{
public:
	ComPtr<ID3D11SamplerState> getSampler(bool linear, bool clampU = true, bool clampV = true, bool flipU = false, bool flipV = false)
	{
		int hash = clampU | (clampV << 1) | (flipU << 2) | (flipV << 3) | (linear << 4);
		auto& sampler = samplers[hash];
		if (!sampler)
		{
			// Create texture sampler
			D3D11_SAMPLER_DESC desc{};
			desc.Filter = linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU = flipU ? D3D11_TEXTURE_ADDRESS_MIRROR : clampU ?  D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = flipV ? D3D11_TEXTURE_ADDRESS_MIRROR : clampV ?  D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			desc.MaxAnisotropy = 1;
			desc.MaxLOD = D3D11_FLOAT32_MAX;
			theDX11Context.getDevice()->CreateSamplerState(&desc, &sampler.get());
		}
		return sampler;
	}

	void term() {
		samplers.clear();
	}

private:
	std::unordered_map<int, ComPtr<ID3D11SamplerState>> samplers;
};
