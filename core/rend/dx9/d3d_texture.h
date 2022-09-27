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
#include <d3d9.h>
#include "dxcontext.h"

class D3DTexture final : public BaseTextureCacheData
{
public:
	D3DTexture(TSP tsp = {}, TCW tcw = {}) : BaseTextureCacheData(tsp, tcw) {}
	D3DTexture(D3DTexture&& other) : BaseTextureCacheData(std::move(other)) {
		std::swap(texture, other.texture);
	}

	ComPtr<IDirect3DTexture9> texture;
	std::string GetId() override { return std::to_string((uintptr_t)texture.get()); }
	void UploadToGPU(int width, int height, const u8* temp_tex_buffer, bool mipmapped,
			bool mipmapsIncluded = false) override;
	bool Delete() override;
	void loadCustomTexture();
};

class D3DTextureCache final : public BaseTextureCache<D3DTexture>
{
public:
	D3DTextureCache() {
		D3DTexture::SetDirectXColorOrder(true);
	}
	~D3DTextureCache() {
		Clear();
	}
	void Cleanup()
	{
		texturesToDelete.clear();
		CollectCleanup();
	}
	void DeleteLater(ComPtr<IDirect3DTexture9> tex) { texturesToDelete.push_back(tex); }

private:
	std::vector<ComPtr<IDirect3DTexture9>> texturesToDelete;
};
