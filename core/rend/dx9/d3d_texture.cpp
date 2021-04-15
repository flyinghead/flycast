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
#include "d3d_texture.h"

void D3DTexture::UploadToGPU(int width, int height, u8* temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
	D3DFORMAT d3dFormat;
	u32 bpp = 2;
	switch (tex_type)
	{
	case TextureType::_5551:
		d3dFormat = D3DFMT_A1R5G5B5;
		break;
	case TextureType::_4444:
		d3dFormat = D3DFMT_A4R4G4B4;
		break;
	case TextureType::_565:
		d3dFormat = D3DFMT_R5G6B5;
		break;
	case TextureType::_8888:
		bpp = 4;
		d3dFormat = D3DFMT_A8R8G8B8;
		break;
	case TextureType::_8:
		bpp = 1;
		d3dFormat = D3DFMT_A8;
		break;
	default:
		return;
	}
	if (mipmapsIncluded)
	{
		// TODO Upload all mipmap levels
		int mipmapLevels = 0;
		int dim = width;
		while (dim != 0)
		{
			mipmapLevels++;
			dim >>= 1;
		}
		for (int i = 0; i < mipmapLevels - 1; i++)
			temp_tex_buffer += (1 << (2 * i)) * bpp;
	}
	D3DLOCKED_RECT rect;
	while (true)
	{
		if (texture == nullptr)
		{
			if (mipmapped)
				theDXContext.getDevice()->CreateTexture(width, height, 0, D3DUSAGE_AUTOGENMIPMAP, d3dFormat, D3DPOOL_MANAGED, &texture.get(), 0);
			else
				theDXContext.getDevice()->CreateTexture(width, height, 1, 0, d3dFormat, D3DPOOL_MANAGED, &texture.get(), 0);
			verify(texture != nullptr);
		}
		if (SUCCEEDED(texture->LockRect(0, &rect, nullptr, 0)))
			break;
		D3DSURFACE_DESC desc;
		texture->GetLevelDesc(0, &desc);
		if (desc.Pool != D3DPOOL_DEFAULT)
			// it should be lockable so error out
			return;
		texture.reset();
	}
	if (width * bpp == (u32)rect.Pitch)
		memcpy(rect.pBits, temp_tex_buffer, width * bpp * height);
	else
	{
		u8 *dst = (u8 *)rect.pBits;
		for (int l = 0; l < height; l++)
		{
			memcpy(dst, temp_tex_buffer, width * bpp);
			dst += rect.Pitch;
			temp_tex_buffer += width * bpp;
		}
	}
	texture->UnlockRect(0);
}

bool D3DTexture::Delete()
{
	if (!BaseTextureCacheData::Delete())
		return false;

	texture.reset();
	return true;
}
