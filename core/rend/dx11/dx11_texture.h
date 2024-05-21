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
#include <unordered_map>
#include "windows/comptr.h"

class DX11Texture final : public BaseTextureCacheData
{
public:
	DX11Texture(TSP tsp = {}, TCW tcw = {}) : BaseTextureCacheData(tsp, tcw) {}
	DX11Texture(DX11Texture&& other) : BaseTextureCacheData(std::move(other)) {
		std::swap(texture, other.texture);
		std::swap(textureView, other.textureView);
	}

	ComPtr<ID3D11Texture2D> texture;
	ComPtr<ID3D11ShaderResourceView> textureView;

	std::string GetId() override { return std::to_string((uintptr_t)texture.get()); }
	void UploadToGPU(int width, int height, const u8* temp_tex_buffer, bool mipmapped,
			bool mipmapsIncluded = false) override;
	bool Delete() override;
	void loadCustomTexture();
#ifndef TARGET_UWP
	bool Force32BitTexture(TextureType type) const override;
#endif
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
	ComPtr<ID3D11SamplerState> getSampler(bool linear, bool clampU = true, bool clampV = true, bool flipU = false, bool flipV = false, bool punchThrough = false)
	{
		int hash = (int)clampU | ((int)clampV << 1) | ((int)flipU << 2) | ((int)flipV << 3) | ((int)linear << 4) | ((int)punchThrough << 5);
		auto& sampler = samplers[hash];
		if (!sampler)
		{
			// Create texture sampler
			D3D11_SAMPLER_DESC desc{};
			if (linear)
			{
				if (punchThrough)
					desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
				else if (config::AnisotropicFiltering > 1)
					desc.Filter = D3D11_FILTER_ANISOTROPIC;
				else
					desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			}
			else {
				desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			}
			desc.AddressU = clampU ?  D3D11_TEXTURE_ADDRESS_CLAMP : flipU ? D3D11_TEXTURE_ADDRESS_MIRROR : D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = clampV ?  D3D11_TEXTURE_ADDRESS_CLAMP : flipV ? D3D11_TEXTURE_ADDRESS_MIRROR : D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
			desc.MaxAnisotropy = config::AnisotropicFiltering;
			desc.MaxLOD = D3D11_FLOAT32_MAX;
			desc.MipLODBias = -1.5f;
			createSampler(&desc, &sampler.get());
		}
		return sampler;
	}

	void term() {
		samplers.clear();
	}

private:
	HRESULT createSampler(const D3D11_SAMPLER_DESC *desc, ID3D11SamplerState **sampler);

	std::unordered_map<int, ComPtr<ID3D11SamplerState>> samplers;
};
