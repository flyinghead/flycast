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

void D3DTexture::UploadToGPU(int width, int height, const u8* temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
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
	int mipmapLevels = 1;
	if (mipmapsIncluded)
	{
		mipmapLevels = 0;
		int dim = width;
		while (dim != 0)
		{
			mipmapLevels++;
			dim >>= 1;
		}
	}

	D3DLOCKED_RECT rect;
	while (true)
	{
		if (texture == nullptr)
		{
			u32 levels = mipmapLevels;
			u32 usage = 0;
			if (mipmapped && !mipmapsIncluded)
			{
				levels = 0;
				usage = D3DUSAGE_AUTOGENMIPMAP;
			}
			theDXContext.getDevice()->CreateTexture(width, height, levels, usage, d3dFormat, D3DPOOL_MANAGED, &texture.get(), 0); // TODO the managed pool persists between device resets
			verify(texture != nullptr);
		}
		if (SUCCEEDED(texture->LockRect(mipmapLevels - 1, &rect, nullptr, 0)))
			break;
		D3DSURFACE_DESC desc;
		texture->GetLevelDesc(0, &desc);
		if (desc.Pool != D3DPOOL_DEFAULT)
			// it should be lockable so error out
			return;
		// RTT targets are created in the default pool and aren't lockable, so delete it and recreate it in the managed pool
		texture.reset();
	}
	for (int i = 0; i < mipmapLevels; i++)
	{
		u32 w = mipmapLevels == 1 ? width : 1 << i;
		u32 h = mipmapLevels == 1 ? height : 1 << i;
		if (w * bpp == (u32)rect.Pitch)
			memcpy(rect.pBits, temp_tex_buffer, w * bpp * h);
		else
		{
			u8 *dst = (u8 *)rect.pBits;
			const u8 *src = temp_tex_buffer;
			for (u32 l = 0; l < h; l++)
			{
				memcpy(dst, src, w * bpp);
				dst += rect.Pitch;
				src += w * bpp;
			}
		}
		texture->UnlockRect(mipmapLevels - i - 1);
		temp_tex_buffer += (1 << (2 * i)) * bpp;
		if (i < mipmapLevels - 1)
			if (FAILED(texture->LockRect(mipmapLevels - i - 2, &rect, nullptr, 0)))
				break;
	}
}

bool D3DTexture::Delete()
{
	if (!BaseTextureCacheData::Delete())
		return false;

	texture.reset();
	return true;
}

void D3DTexture::loadCustomTexture()
{
	u32 size = custom_width * custom_height;
	u8 *p = custom_image_data;
	while (size--)
	{
		// RGBA -> BGRA
		std::swap(p[0], p[2]);
		p += 4;
	}
	CheckCustomTexture();
}
