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
#pragma once
#include "oslib/oslib.h"
#include "hw/pvr/Renderer_if.h"
#include "cfg/option.h"
#include "texconv.h"
#include "CustomTexture.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <string>
#include <unordered_map>
#include <vector>
#include <utility>

class BaseTextureCacheData;

struct vram_block
{
	u32 start;
	u32 end;

	BaseTextureCacheData *texture;
};

bool VramLockedWriteOffset(size_t offset);
bool VramLockedWrite(u8* address);

void UpscalexBRZ(int factor, u32* source, u32* dest, int width, int height, bool has_alpha);

class BaseTextureCacheData
{
protected:
	BaseTextureCacheData(TSP tsp, TCW tcw);

public:
	BaseTextureCacheData(BaseTextureCacheData&& other)
	{
		tsp = other.tsp;
		tcw = other.tcw;
		tex_type = other.tex_type;
		startAddress = other.startAddress;
		dirty = other.dirty;
		std::swap(lock_block, other.lock_block);
		mmStartAddress = other.mmStartAddress;
		width = other.width;
		height = other.height;
		size = other.size;
		tex = other.tex;
		texconv = other.texconv;
		texconv32 = other.texconv32;
		texconv8 = other.texconv8;
		Updates = other.Updates;
		palette_hash = other.palette_hash;
		texture_hash = other.texture_hash;
		old_vqtexture_hash = other.old_vqtexture_hash;
		old_texture_hash = other.old_texture_hash;
		std::swap(custom_image_data, other.custom_image_data);
		custom_width = other.custom_width;
		custom_height = other.custom_height;
		custom_load_in_progress = 0;
		gpuPalette = other.gpuPalette;
	}

	TSP tsp;        	//dreamcast texture parameters
	TCW tcw;

	// Decoded/filtered texture format
	TextureType tex_type;
	u32 startAddress;	// texture data start address in vram

	u32 dirty;			// frame number at which texture was overwritten
	vram_block* lock_block;

	u32 mmStartAddress; // pixel data start address of max level mipmap
	u16 width, height;	// width & height of the texture
	u32 size;       	// size in bytes of max level mipmap in vram

	const PvrTexInfo* tex;
	TexConvFP texconv;
	TexConvFP32 texconv32;
	TexConvFP8 texconv8;

	u32 Updates;

	//used for palette updates
	u32 palette_hash;			// Palette hash at time of last update
	u32 texture_hash;			// xxhash of texture data, used for custom textures
	u32 old_vqtexture_hash;		// legacy hash for vq textures
	u32 old_texture_hash;		// legacy hash
	u8* custom_image_data;		// loaded custom image data
	u32 custom_width;
	u32 custom_height;
	std::atomic_int custom_load_in_progress;
	bool gpuPalette;

	void PrintTextureName();
	virtual std::string GetId() = 0;

	bool IsPaletted()
	{
		return tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8;
	}

	bool IsMipmapped()
	{
		return tcw.MipMapped != 0 && tcw.ScanOrder == 0 && config::UseMipmaps;
	}

	const char* GetPixelFormatName()
	{
		switch (tcw.PixelFmt)
		{
		case Pixel1555: return "1555";
		case Pixel565: return "565";
		case Pixel4444: return "4444";
		case PixelYUV: return "yuv";
		case PixelBumpMap: return "bumpmap";
		case PixelPal4: return "pal4";
		case PixelPal8: return "pal8";
		default: return "unknown";
		}
	}

	bool IsCustomTextureAvailable()
	{
		return custom_load_in_progress == 0 && custom_image_data != NULL;
	}

	void ComputeHash();
	bool Update();
	virtual void UploadToGPU(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded = false) = 0;
	virtual bool Force32BitTexture(TextureType type) const { return false; }
	void CheckCustomTexture();
	//true if : dirty or paletted texture and hashes don't match
	bool NeedsUpdate();
	virtual bool Delete();
	virtual ~BaseTextureCacheData() = default;
	void protectVRam();
	void unprotectVRam();
	void invalidate();

	static bool IsGpuHandledPaletted(TSP tsp, TCW tcw)
	{
		// Some palette textures are handled on the GPU
		// This is currently limited to textures using nearest or bilinear filtering and not mipmapped.
		// Enabling texture upscaling or dumping also disables this mode.
		return (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
				&& config::TextureUpscale == 1
				&& !config::DumpTextures
				&& tsp.FilterMode <= 1
				&& !tcw.MipMapped
				&& !tcw.VQ_Comp;
	}
	static void SetDirectXColorOrder(bool enabled);
};

template<typename Texture>
class BaseTextureCache
{
public:
	Texture *getTextureCacheData(TSP tsp, TCW tcw)
	{
		u64 key = tsp.full & TSPTextureCacheMask.full;
		if (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
		{
			if (BaseTextureCacheData::IsGpuHandledPaletted(tsp, tcw))
				// texaddr, pixelfmt, VQ, MipMap
				key |= (u64)(tcw.full & TCWPalTextureCacheMask.full) << 32;
			else
				// Paletted textures have a palette selection that must be part of the key
				// We also add the palette type to the key to avoid thrashing the cache
				// when the palette type is changed. If the palette type is changed back in the future,
				// this texture will stil be available.
				key |= ((u64)tcw.full << 32) | ((PAL_RAM_CTRL & 3) << 6) | ((tsp.FilterMode != 0) << 8);
		}
		else
			key |= (u64)(tcw.full & TCWTextureCacheMask.full) << 32;

		auto it = cache.find(key);

		Texture* texture;
		if (it != cache.end())
		{
			texture = &it->second;
			// Needed if the texture is updated
			texture->tcw.StrideSel = tcw.StrideSel;
		}
		else //create if not existing
		{
			texture = &cache.emplace(std::make_pair(key, Texture(tsp, tcw))).first->second;
		}

		return texture;
	}

	Texture *getRTTexture(u32 address, u32 fb_packmode, u32 width, u32 height)
	{
		// TexAddr : (address), StrideSel : 0, ScanOrder : 1
		TCW tcw{ { address >> 3, 0, 1 } };
		switch (fb_packmode)
		{
		case 0:
		case 3:
			tcw.PixelFmt = Pixel1555;
			break;
		case 1:
			tcw.PixelFmt = Pixel565;
			break;
		case 2:
			tcw.PixelFmt = Pixel4444;
			break;
		}
		TSP tsp{};
		for (tsp.TexU = 0; tsp.TexU <= 7 && (8u << tsp.TexU) < width; tsp.TexU++);
		for (tsp.TexV = 0; tsp.TexV <= 7 && (8u << tsp.TexV) < height; tsp.TexV++);

		return getTextureCacheData(tsp, tcw);
	}

	void CollectCleanup()
	{
		std::vector<u64> list;

		u32 TargetFrame = std::max((u32)120, FrameCount) - 120;

		for (const auto& [id, texture] : cache)
		{
			if (texture.dirty && texture.dirty < TargetFrame)
				list.push_back(id);

			if (list.size() > 5)
				break;
		}

		for (u64 id : list)
		{
			if (cache.find(id)->second.Delete())
				cache.erase(id);
		}
	}

	void Clear()
	{
		custom_texture.Terminate();
		for (auto& [id, texture] : cache)
			texture.Delete();

		cache.clear();
		INFO_LOG(RENDERER, "Texture cache cleared");
	}

protected:
	std::unordered_map<u64, Texture> cache;
	// Only use TexU and TexV from TSP in the cache key
	//     TexV : 7, TexU : 7
	const TSP TSPTextureCacheMask = { { 7, 7 } };
	//     TexAddr : 0x1FFFFF, StrideSel : 0, ScanOrder : 1, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1
	const TCW TCWTextureCacheMask = { { 0x1FFFFF, 0, 1, 7, 1, 1 } };
	//     TexAddr : 0x1FFFFF, PalSelect : 0, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1
	const TCW TCWPalTextureCacheMask = { { 0x1FFFFF, 0, 0, 7, 1, 1 } };
};

template<typename Packer = RGBAPacker>
void ReadFramebuffer(const FramebufferInfo& info, PixelBuffer<u32>& pb, int& width, int& height);

// width and height in pixels. linestride in bytes
template<int Red = 0, int Green = 1, int Blue = 2, int Alpha = 3>
void WriteFramebuffer(u32 width, u32 height, const u8 *data, u32 dstAddr, FB_W_CTRL_type fb_w_ctrl, u32 linestride, FB_X_CLIP_type xclip, FB_Y_CLIP_type yclip);

// width and height in pixels. linestride in bytes
template<int Red = 0, int Green = 1, int Blue = 2, int Alpha = 3>
void WriteTextureToVRam(u32 width, u32 height, const u8 *data, u16 *dst, FB_W_CTRL_type fb_w_ctrl, u32 linestride);
void getRenderToTextureDimensions(u32& width, u32& height, u32& pow2Width, u32& pow2Height);

static inline void MakeFogTexture(u8 *tex_data)
{
	u8 *fog_table = (u8 *)FOG_TABLE;
	for (int i = 0; i < 128; i++)
	{
		tex_data[i] = fog_table[i * 4];
		tex_data[i + 128] = fog_table[i * 4 + 1];
	}
}

void dump_screenshot(u8 *buffer, u32 width, u32 height, bool alpha = false, u32 rowPitch = 0, bool invertY = false);

extern const std::array<f32, 16> D_Adjust_LoD_Bias;
