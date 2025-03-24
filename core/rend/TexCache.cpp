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
#include "TexCache.h"
#include "deps/xbrz/xbrz.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/mem/addrspace.h"

#include <mutex>
#include <xxhash.h>

#ifdef _OPENMP
#include <omp.h>
#endif

extern bool pal_needs_update;

// Rough approximation of LoD bias from D adjust param, only used to increase LoD
const std::array<f32, 16> D_Adjust_LoD_Bias = {
		0.f, -4.f, -2.f, -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
};

static std::vector<vram_block*> VramLocks[VRAM_SIZE_MAX / PAGE_SIZE];

//List functions
//
static void vramlock_list_remove(vram_block* block)
{
	u32 base = block->start / PAGE_SIZE;
	u32 end = block->end / PAGE_SIZE;

	for (u32 i = base; i <= end; i++)
	{
		std::vector<vram_block*>& list = VramLocks[i];
		for (auto& lock : list)
		{
			if (lock == block)
				lock = nullptr;
		}
	}
}
 
static void vramlock_list_add(vram_block* block)
{
	u32 base = block->start / PAGE_SIZE;
	u32 end = block->end / PAGE_SIZE;

	for (u32 i = base; i <= end; i++)
	{
		std::vector<vram_block*>& list = VramLocks[i];
		// If the list is empty then we need to protect vram, otherwise it's already been done
		if (list.empty() || std::all_of(list.begin(), list.end(), [](vram_block *block) { return block == nullptr; }))
			addrspace::protectVram(i * PAGE_SIZE, PAGE_SIZE);
		auto it = std::find(list.begin(), list.end(), nullptr);
		if (it != list.end())
			*it = block;
		else
			list.push_back(block);
	}
}
 
static std::mutex vramlist_lock;

bool VramLockedWriteOffset(size_t offset)
{
	if (offset >= VRAM_SIZE)
		return false;

	size_t addr_hash = offset / PAGE_SIZE;
	std::vector<vram_block *>& list = VramLocks[addr_hash];

	{
		std::lock_guard<std::mutex> lockguard(vramlist_lock);

		for (auto& lock : list)
		{
			if (lock != nullptr)
			{
				lock->texture->invalidate();

				if (lock != nullptr)
				{
					ERROR_LOG(PVR, "Error : pvr is supposed to remove lock");
					die("Invalid state");
				}
			}
		}
		list.clear();

		addrspace::unprotectVram((u32)(offset & ~PAGE_MASK), PAGE_SIZE);
	}

	return true;
}

bool VramLockedWrite(u8* address)
{
	u32 offset = addrspace::getVramOffset(address);
	if (offset == (u32)-1)
		return false;
	return VramLockedWriteOffset(offset);
}

//unlocks mem
//also frees the handle
static void libCore_vramlock_Unlock_block_wb(vram_block* block)
{
	vramlock_list_remove(block);
	delete block;
}

#ifdef _OPENMP
static inline int getThreadCount()
{
	int tcount = omp_get_num_procs() - 1;
	if (tcount < 1)
		tcount = 1;
	return std::min(tcount, (int)config::MaxThreads);
}

template<typename Func>
void parallelize(Func func, int start, int end)
{
	int tcount = getThreadCount();
#pragma omp parallel num_threads(tcount)
	{
		int num_threads = omp_get_num_threads();
		int thread = omp_get_thread_num();
		int chunk = (end - start) / num_threads;
		func(start + chunk * thread,
				num_threads == thread + 1 ? end
						: (start + chunk * (thread + 1)));
	}
}
#endif

static struct xbrz::ScalerCfg xbrz_cfg;

void UpscalexBRZ(int factor, u32* source, u32* dest, int width, int height, bool has_alpha)
{
#ifdef _OPENMP
	parallelize([=](int start, int end) {
		xbrz::scale(factor, source, dest, width, height, has_alpha ? xbrz::ColorFormat::ARGB : xbrz::ColorFormat::RGB,
				xbrz_cfg, start, end);
	}, 0, height);
#else
	xbrz::scale(factor, source, dest, width, height, has_alpha ? xbrz::ColorFormat::ARGB : xbrz::ColorFormat::RGB, xbrz_cfg);
#endif
}

extern const u32 VQMipPoint[11] =
{
	VQ_CODEBOOK_SIZE + 0x00000, // 1
	VQ_CODEBOOK_SIZE + 0x00001, // 2
	VQ_CODEBOOK_SIZE + 0x00002, // 4
	VQ_CODEBOOK_SIZE + 0x00006, // 8
	VQ_CODEBOOK_SIZE + 0x00016, // 16
	VQ_CODEBOOK_SIZE + 0x00056, // 32
	VQ_CODEBOOK_SIZE + 0x00156, // 64
	VQ_CODEBOOK_SIZE + 0x00556, // 128
	VQ_CODEBOOK_SIZE + 0x01556, // 256
	VQ_CODEBOOK_SIZE + 0x05556, // 512
	VQ_CODEBOOK_SIZE + 0x15556  // 1024
};
extern const u32 OtherMipPoint[11] =
{
	0x00003,//1
	0x00004,//2
	0x00008,//4
	0x00018,//8
	0x00058,//16
	0x00158,//32
	0x00558,//64
	0x01558,//128
	0x05558,//256
	0x15558,//512
	0x55558//1024
};

static const TextureType PAL_TYPE[4] = {
	TextureType::_5551, TextureType::_565, TextureType::_4444, TextureType::_8888
};

void BaseTextureCacheData::PrintTextureName()
{
#if !defined(NDEBUG) || defined(DEBUGFAST)
	char str[512];
	snprintf(str, sizeof(str), "Texture: %s", GetPixelFormatName());

	if (tcw.VQ_Comp)
		strcat(str, " VQ");
	else if (tcw.ScanOrder == 0 || IsPaletted())
		strcat(str, " TW");
	else if (tcw.StrideSel == 1 && !IsPaletted())
		strcat(str, " Stride");

	if (tcw.ScanOrder == 0 && tcw.MipMapped)
		strcat(str, " MM");
	if (tsp.FilterMode != 0)
		strcat(str, " Bilinear");

	size_t len = strlen(str);
	snprintf(str + len, sizeof(str) - len, " %dx%d @ 0x%X", 8 << tsp.TexU, 8 << tsp.TexV, tcw.TexAddr << 3);
	std::string id = GetId();
	len = strlen(str);
	snprintf(str + len, sizeof(str) - len, " id=%s", id.c_str());
	DEBUG_LOG(RENDERER, "%s", str);
#endif
}

//true if : dirty or paletted texture and hashes don't match
bool BaseTextureCacheData::NeedsUpdate() {
	bool rc = dirty != 0;
	if (tex_type != TextureType::_8)
	{
		if (tcw.PixelFmt == PixelPal4 && palette_hash != pal_hash_16[tcw.PalSelect])
			rc = true;
		else if (tcw.PixelFmt == PixelPal8 && palette_hash != pal_hash_256[tcw.PalSelect >> 4])
			rc = true;
	}

	return rc;
}

void BaseTextureCacheData::protectVRam()
{
	u32 end = mmStartAddress + size - 1;
	if (end >= VRAM_SIZE)
	{
		WARN_LOG(PVR, "protectVRam: end >= VRAM_SIZE. Tried to lock area out of vram");
		end = VRAM_SIZE - 1;
	}

	if (startAddress > end)
	{
		WARN_LOG(PVR, "vramlock_Lock: startAddress > end. Tried to lock negative block");
		return;
	}

	vram_block *block = new vram_block();
	block->end = end;
	block->start = startAddress;
	block->texture = this;

	{
		std::lock_guard<std::mutex> lock(vramlist_lock);

		if (lock_block == nullptr)
		{
			// This also protects vram if needed
			vramlock_list_add(block);
			lock_block = block;
		}
		else
			delete block;
	}
}

void BaseTextureCacheData::unprotectVRam()
{
	std::lock_guard<std::mutex> lock(vramlist_lock);
	if (lock_block)
		libCore_vramlock_Unlock_block_wb(lock_block);
	lock_block = nullptr;
}

bool BaseTextureCacheData::Delete()
{
	unprotectVRam();

	if (custom_load_in_progress > 0)
		return false;

	free(custom_image_data);
	custom_image_data = nullptr;

	return true;
}

BaseTextureCacheData::BaseTextureCacheData(TSP tsp, TCW tcw)
{
	if (tcw.VQ_Comp == 1 && tcw.MipMapped == 1)
		// Star Wars Demolition
		tcw.ScanOrder = 0;
	this->tsp = tsp;
	this->tcw = tcw;

	//Reset state info ..
	Updates = 0;
	dirty = FrameCount;
	lock_block = nullptr;
	custom_image_data = nullptr;
	custom_load_in_progress = 0;
	gpuPalette = false;

	//decode info from tsp/tcw into the texture struct
	tex = &pvrTexInfo[tcw.PixelFmt == PixelReserved ? Pixel1555 : tcw.PixelFmt];	//texture format table entry

	startAddress = (tcw.TexAddr << 3) & VRAM_MASK;	// texture start address
	mmStartAddress = startAddress;					// data texture start address (modified for MIPs, as needed)
	width = 8 << tsp.TexU;						//tex width
	height = 8 << tsp.TexV;						//tex height

	texconv8 = nullptr;

	if (tcw.ScanOrder && tex->PL32 != nullptr)
	{
		//Texture is stored 'planar' in memory, no deswizzle is needed
		if (tcw.MipMapped != 0)
		{
			WARN_LOG(RENDERER, "Warning: planar texture with mipmaps (invalid)");
			this->tcw.MipMapped = 0;
		}

		//Planar textures support stride selection, mostly used for non power of 2 textures (videos)
		int stride = width;
		if (tcw.StrideSel)
		{
			stride = (TEXT_CONTROL & 31) * 32;
			if (stride == 0)
				stride = width;
		}

		//Call the format specific conversion code
		texconv = nullptr;
		if (tcw.VQ_Comp != 0)
		{
			// VQ
			texconv32 = tex->PLVQ32;
			mmStartAddress += VQ_CODEBOOK_SIZE;
			size = stride * height / 4;
		}
		else
		{
			// Normal
			texconv32 = tex->PL32;
			//calculate the size, in bytes, for the locking
			size = stride * height * tex->bpp / 8;
		}
	}
	else
	{
		if (!IsPaletted())
		{
			this->tcw.ScanOrder = 0;
			this->tcw.StrideSel = 0;
		}
		// Quake 3 Arena uses one
		if (tcw.MipMapped)
			// Mipmapped texture must be square and TexV is ignored
			height = width;

		if (tcw.VQ_Comp)
		{
			verify(tex->VQ != NULL || tex->VQ32 != NULL);
			if (tcw.MipMapped)
				mmStartAddress += VQMipPoint[tsp.TexU + 3];
			else
				mmStartAddress += VQ_CODEBOOK_SIZE;
			texconv = tex->VQ;
			texconv32 = tex->VQ32;
			size = width * height / 4;
		}
		else
		{
			verify(tex->TW != NULL || tex->TW32 != NULL);
			if (tcw.MipMapped)
				mmStartAddress += OtherMipPoint[tsp.TexU + 3] * tex->bpp / 8;
			texconv = tex->TW;
			texconv32 = tex->TW32;
			size = width * height * tex->bpp / 8;
			texconv8 = tex->TW8;
		}
	}
}

void BaseTextureCacheData::ComputeHash()
{
	// Include everything but texaddr, reserved and stride. Palette textures don't have ScanOrder
	const u32 tcwMask = IsPaletted() ? 0xF8000000 : 0xFC000000;
	if (tcw.VQ_Comp)
	{
		// The size for VQ textures wasn't correctly calculated.
		// We use the old size to compute the hash for backward-compatibility
		// with existing custom texture packs.
		int oldSize = width * height / 8;
		old_vqtexture_hash = XXH32(&vram[mmStartAddress - VQ_CODEBOOK_SIZE], oldSize, 7);
		if (IsPaletted())
			old_vqtexture_hash ^= palette_hash;
		old_texture_hash = old_vqtexture_hash;
		old_vqtexture_hash ^= tcw.full & tcwMask;
		// New hash
	    XXH32_state_t *state = XXH32_createState();
	    XXH32_reset(state, 7);
	    // hash vq codebook
	    XXH32_update(state, &vram[startAddress], VQ_CODEBOOK_SIZE);
	    // hash texture
	    XXH32_update(state, &vram[mmStartAddress], size);
	    texture_hash = XXH32_digest(state);
	    XXH32_freeState(state);
		if (IsPaletted())
			texture_hash ^= palette_hash;
		texture_hash ^= tcw.full & tcwMask;
	}
	else
	{
		old_vqtexture_hash = 0;
		texture_hash = XXH32(&vram[mmStartAddress], size, 7);
		if (IsPaletted())
			texture_hash ^= palette_hash;
		old_texture_hash = texture_hash;
		texture_hash ^= tcw.full & tcwMask;
	}
}

bool BaseTextureCacheData::Update()
{
	//texture state tracking stuff
	Updates++;
	dirty = 0;
	gpuPalette = false;
	tex_type = tex->type;

	bool has_alpha = false;
	if (IsPaletted())
	{
		if (IsGpuHandledPaletted(tsp, tcw))
		{
			tex_type = TextureType::_8;
			gpuPalette = true;
		}
		else
		{
			tex_type = PAL_TYPE[PAL_RAM_CTRL&3];
			if (tex_type != TextureType::_565)
				has_alpha = true;
		}

		// Get the palette hash to check for future updates
		// TODO get rid of ::palette_index and ::vq_codebook
		if (tcw.PixelFmt == PixelPal4)
		{
			palette_hash = pal_hash_16[tcw.PalSelect];
			::palette_index = tcw.PalSelect << 4;
		}
		else
		{
			palette_hash = pal_hash_256[tcw.PalSelect >> 4];
			::palette_index = (tcw.PalSelect >> 4) << 8;
		}
	}

	if (tcw.VQ_Comp)
		::vq_codebook = &vram[startAddress];

	//texture conversion work
	u32 stride = width;

	if (tcw.StrideSel && tcw.ScanOrder && tex->PL32 != nullptr)
	{
		stride = (TEXT_CONTROL & 31) * 32;
		if (stride == 0)
			stride = width;
	}

	u32 heightLimit = height;
	const u32 originalSize = size;
	if (startAddress > VRAM_SIZE || mmStartAddress + size > VRAM_SIZE)
	{
		heightLimit = 0;
		if (mmStartAddress < VRAM_SIZE && mmStartAddress + size > VRAM_SIZE && tcw.ScanOrder)
		{
			// Shenmue Space Harrier mini-arcade loads a texture that goes beyond the end of VRAM
			// but only uses the top portion of it
			heightLimit = (VRAM_SIZE - mmStartAddress) * 8 / stride / tex->bpp;
			size = stride * heightLimit * tex->bpp/8;
		}
		if (heightLimit == 0)
		{
			size = originalSize;
			WARN_LOG(RENDERER, "Warning: invalid texture. Address %08X %08X size %d", startAddress, mmStartAddress, size);
			dirty = 1;
			unprotectVRam();
			return false;
		}
	}
	if (config::CustomTextures)
	{
		u32 oldHash = texture_hash;
		ComputeHash();
		if (Updates > 1 && oldHash == texture_hash)
		{
			// Texture hasn't changed so skip the update.
			protectVRam();
			size = originalSize;
			return true;
		}
		custom_texture.LoadCustomTextureAsync(this);
	}

	void *temp_tex_buffer = NULL;
	u32 upscaled_w = width;
	u32 upscaled_h = height;

	PixelBuffer<u16> pb16;
	PixelBuffer<u32> pb32;
	PixelBuffer<u8> pb8;

	// Figure out if we really need to use a 32-bit pixel buffer
	bool textureUpscaling = config::TextureUpscale > 1
			// Don't process textures that are too big
			&& (int)(width * height) <= config::MaxFilteredTextureSize * config::MaxFilteredTextureSize
			// Don't process YUV textures
			&& tcw.PixelFmt != PixelYUV;
	bool need_32bit_buffer = true;
	if (!textureUpscaling
		&& (!IsPaletted() || tex_type != TextureType::_8888)
		&& texconv != NULL
		&& !Force32BitTexture(tex_type))
		need_32bit_buffer = false;
	// TODO avoid upscaling/depost. textures that change too often

	bool mipmapped = IsMipmapped() && !config::DumpTextures;

	if (texconv32 != NULL && need_32bit_buffer)
	{
		if (textureUpscaling)
			// don't use mipmaps if upscaling
			mipmapped = false;
		// Force the texture type since that's the only 32-bit one we know
		tex_type = TextureType::_8888;

		if (mipmapped)
		{
			pb32.init(width, height, true);
			for (u32 i = 0; i <= tsp.TexU + 3u; i++)
			{
				pb32.set_mipmap(i);
				u32 vram_addr;
				if (tcw.VQ_Comp)
				{
					vram_addr = startAddress + VQMipPoint[i];
					if (i == 0)
					{
						PixelBuffer<u32> pb0;
						pb0.init(2, 2 ,false);
						if (tcw.PixelFmt == PixelYUV)
							// Use higher LoD mipmap
							vram_addr = startAddress + VQMipPoint[1];
						texconv32(&pb0, &vram[vram_addr], 2, 2);
						*pb32.data() = *pb0.data(1, 1);
						continue;
					}
				}
				else
					vram_addr = startAddress + OtherMipPoint[i] * tex->bpp / 8;
				if (tcw.PixelFmt == PixelYUV && i == 0)
					// Special case for YUV at 1x1 LoD
					pvrTexInfo[Pixel565].TW32(&pb32, &vram[vram_addr], 1, 1);
				else
					texconv32(&pb32, &vram[vram_addr], 1 << i, 1 << i);
			}
			pb32.set_mipmap(0);
		}
		else
		{
			pb32.init(width, height);
			texconv32(&pb32, (u8*)&vram[mmStartAddress], stride, heightLimit);

			// xBRZ scaling
			if (textureUpscaling)
			{
				PixelBuffer<u32> tmp_buf;
				tmp_buf.init(width * config::TextureUpscale, height * config::TextureUpscale);

				if (tcw.PixelFmt == Pixel1555 || tcw.PixelFmt == Pixel4444)
					// Alpha channel formats. Palettes with alpha are already handled
					has_alpha = true;
				UpscalexBRZ(config::TextureUpscale, pb32.data(), tmp_buf.data(), width, height, has_alpha);
				pb32.steal_data(tmp_buf);
				upscaled_w *= config::TextureUpscale;
				upscaled_h *= config::TextureUpscale;
			}
		}
		temp_tex_buffer = pb32.data();
	}
	else if (texconv8 != NULL && tex_type == TextureType::_8)
	{
		if (mipmapped)
		{
			// This shouldn't happen since mipmapped palette textures are converted to rgba
			pb8.init(width, height, true);
			for (u32 i = 0; i <= tsp.TexU + 3u; i++)
			{
				pb8.set_mipmap(i);
				u32 vram_addr = startAddress + OtherMipPoint[i] * tex->bpp / 8;
				texconv8(&pb8, &vram[vram_addr], 1 << i, 1 << i);
			}
			pb8.set_mipmap(0);
		}
		else
		{
			pb8.init(width, height);
			texconv8(&pb8, &vram[mmStartAddress], stride, height);
		}
		temp_tex_buffer = pb8.data();
	}
	else if (texconv != NULL)
	{
		if (mipmapped)
		{
			pb16.init(width, height, true);
			for (u32 i = 0; i <= tsp.TexU + 3u; i++)
			{
				pb16.set_mipmap(i);
				u32 vram_addr;
				if (tcw.VQ_Comp)
				{
					vram_addr = startAddress + VQMipPoint[i];
					if (i == 0)
					{
						PixelBuffer<u16> pb0;
						pb0.init(2, 2 ,false);
						texconv(&pb0, (u8*)&vram[vram_addr], 2, 2);
						*pb16.data() = *pb0.data(1, 1);
						continue;
					}
				}
				else
					vram_addr = startAddress + OtherMipPoint[i] * tex->bpp / 8;
				texconv(&pb16, (u8*)&vram[vram_addr], 1 << i, 1 << i);
			}
			pb16.set_mipmap(0);
		}
		else
		{
			pb16.init(width, height);
			texconv(&pb16, (u8*)&vram[mmStartAddress], stride, heightLimit);
		}
		temp_tex_buffer = pb16.data();
	}
	else
	{
		//fill it in with a temp color
		WARN_LOG(RENDERER, "UNHANDLED TEXTURE");
		pb16.init(width, height);
		memset(pb16.data(), 0x80, width * height * 2);
		temp_tex_buffer = pb16.data();
		mipmapped = false;
	}
	//lock the texture to detect changes in it
	protectVRam();

	UploadToGPU(upscaled_w, upscaled_h, (const u8 *)temp_tex_buffer, IsMipmapped(), mipmapped);
	if (config::DumpTextures)
	{
		ComputeHash();
		custom_texture.DumpTexture(texture_hash, upscaled_w, upscaled_h, tex_type, temp_tex_buffer);
		NOTICE_LOG(RENDERER, "Dumped texture %x.png. Old hash %x", texture_hash, old_texture_hash);
	}
	PrintTextureName();
	// Restore the original texture size if it was constrained to VRAM limits above
	size = originalSize;

	return true;
}

void BaseTextureCacheData::CheckCustomTexture()
{
	if (IsCustomTextureAvailable())
	{
		tex_type = TextureType::_8888;
		gpuPalette = false;
		UploadToGPU(custom_width, custom_height, custom_image_data, IsMipmapped(), false);
		free(custom_image_data);
		custom_image_data = nullptr;
	}
}

void BaseTextureCacheData::SetDirectXColorOrder(bool enabled) {
	pvrTexInfo = enabled ? directx::pvrTexInfo : opengl::pvrTexInfo;
	pal_needs_update = true;
}

template<typename Packer>
void ReadFramebuffer(const FramebufferInfo& info, PixelBuffer<u32>& pb, int& width, int& height)
{
	width = (info.fb_r_size.fb_x_size + 1) * 2;     // in 16-bit words
	height = info.fb_r_size.fb_y_size + 1;
	int modulus = (info.fb_r_size.fb_modulus - 1) * 2;

	int bpp;
	switch (info.fb_r_ctrl.fb_depth)
	{
		case fbde_0555:
		case fbde_565:
			bpp = 2;
			break;
		case fbde_888:
			bpp = 3;
			width = (width * 2) / 3;		// in pixels
			modulus = (modulus * 2) / 3;	// in pixels
			break;
		case fbde_C888:
			bpp = 4;
			width /= 2;             // in pixels
			modulus /= 2;           // in pixels
			break;
		default:
			die("Invalid framebuffer format\n");
			bpp = 4;
			break;
	}

	u32 addr = info.fb_r_sof1;
	if (info.spg_control.interlace)
	{
		if (width == modulus && info.fb_r_sof2 == info.fb_r_sof1 + width * bpp)
		{
			// Typical case alternating even and odd lines -> take the whole buffer at once
			modulus = 0;
			height *= 2;
		}
		else
		{
			addr = info.spg_status.fieldnum ? info.fb_r_sof2 : info.fb_r_sof1;
		}
	}

	pb.init(width, height);
	u32 *dst = (u32 *)pb.data();
	const u32 fb_concat = info.fb_r_ctrl.fb_concat;

	switch (info.fb_r_ctrl.fb_depth)
	{
		case fbde_0555:    // 555 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u16 src = pvr_read32p<u16>(addr);
					*dst++ = Packer::pack(
							(((src >> 10) & 0x1F) << 3) | fb_concat,
							(((src >> 5) & 0x1F) << 3) | fb_concat,
							(((src >> 0) & 0x1F) << 3) | fb_concat,
							0xff);
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;

		case fbde_565:    // 565 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u16 src = pvr_read32p<u16>(addr);
					*dst++ = Packer::pack(
							(((src >> 11) & 0x1F) << 3) | fb_concat,
							(((src >> 5) & 0x3F) << 2) | (fb_concat & 3),
							(((src >> 0) & 0x1F) << 3) | fb_concat,
							0xFF);
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;
		case fbde_888:		// 888 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i += 4)
				{
					u32 src = pvr_read32p<u32>(addr);
					*dst++ = Packer::pack(src >> 16, src >> 8, src, 0xff);
					addr += 4;
					if (i + 1 >= width)
						break;
					u32 src2 = pvr_read32p<u32>(addr);
					*dst++ = Packer::pack(src2 >> 8, src2, src >> 24, 0xff);
					addr += 4;
					if (i + 2 >= width)
						break;
					u32 src3 = pvr_read32p<u32>(addr);
					*dst++ = Packer::pack(src3, src2 >> 24, src2 >> 16, 0xff);
					addr += 4;
					if (i + 3 >= width)
						break;
					*dst++ = Packer::pack(src3 >> 24, src3 >> 16, src3 >> 8, 0xff);
				}
				addr += modulus * bpp;
			}
			break;
		case fbde_C888:     // 0888 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u32 src = pvr_read32p<u32>(addr);
					*dst++ = Packer::pack(src >> 16, src >> 8, src, 0xff);
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;
	}
}
template void ReadFramebuffer<RGBAPacker>(const FramebufferInfo& info, PixelBuffer<u32>& pb, int& width, int& height);
template void ReadFramebuffer<BGRAPacker>(const FramebufferInfo& info, PixelBuffer<u32>& pb, int& width, int& height);

template<int bits>
static inline u8 roundColor(u8 in)
{
	u8 out = in >> (8 - bits);
	if (out != 0xffu >> (8 - bits))
		out += (in >> (8 - bits - 1)) & 1;
	return out;
}

// write to 32-bit vram area (framebuffer)
class FBPixelWriter
{
public:
	FBPixelWriter(u32 dstAddr) : dstAddr(dstAddr) {}

	template<typename T>
	void write(T pixel) {
		pvr_write32p<T, true>(dstAddr, pixel);
		dstAddr += sizeof(T);
	}

	void advance(int bytes) {
		dstAddr += bytes;
	}

private:
	u32 dstAddr;
};

// write to 64-bit vram area (render to texture)
class TexPixelWriter
{
public:
	TexPixelWriter(u16 *dest) : dest(dest) {}

	void write(u16 pixel) {
		*dest++ = pixel;
	}

	void advance(int bytes) {
		(u8 *&)dest += bytes;
	}

private:
	u16 *dest;
};

// 0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
template<int Red, int Green, int Blue, int Alpha, typename PixelWriter, bool Round = false>
class FBLineWriter0555
{
public:
	FBLineWriter0555(FB_W_CTRL_type fb_w_ctrl, PixelWriter& pixWriter) : pixWriter(pixWriter) {
		kval_bit = (fb_w_ctrl.fb_kval & 0x80) << 8;
	}

	void write(int xmin, int xmax, const u8 *& pixel, int y)
	{
		for (int c = xmin; c < xmax; c++)
		{
			u8 red, green, blue;
			if constexpr (Round)
			{
				red = roundColor<5>(pixel[Red]);
				green = roundColor<5>(pixel[Green]);
				blue = roundColor<5>(pixel[Blue]);
			}
			else
			{
				red = pixel[Red] >> 3;
				green = pixel[Green] >> 3;
				blue = pixel[Blue] >> 3;
			}
			pixWriter.write((u16)((red << 10) | (green << 5) | blue | kval_bit));
			pixel += 4;
		}
	}

	static constexpr int BytesPerPixel = 2;

private:
	u16 kval_bit;
	PixelWriter& pixWriter;
};

// 565 RGB 16 bit
template<int Red, int Green, int Blue, int Alpha, typename PixelWriter, bool Round = false>
class FBLineWriter565
{
public:
	FBLineWriter565(FB_W_CTRL_type fb_w_ctrl, PixelWriter& pixWriter) : pixWriter(pixWriter) {}

	void write(int xmin, int xmax, const u8 *& pixel, int y)
	{
		for (int c = xmin; c < xmax; c++)
		{
			u8 red, green, blue;
			if constexpr (Round)
			{
				red = roundColor<5>(pixel[Red]);
				green = roundColor<6>(pixel[Green]);
				blue = roundColor<5>(pixel[Blue]);
			}
			else
			{
				red = pixel[Red] >> 3;
				green = pixel[Green] >> 2;
				blue = pixel[Blue] >> 3;
			}
			pixWriter.write((u16)((red << 11) | (green << 5) | blue));
			pixel += 4;
		}
	}

	static constexpr int BytesPerPixel = 2;

private:
	PixelWriter& pixWriter;
};

// 4444 ARGB 16 bit
template<int Red, int Green, int Blue, int Alpha, typename PixelWriter, bool Round = false>
class FBLineWriter4444
{
public:
	FBLineWriter4444(FB_W_CTRL_type fb_w_ctrl, PixelWriter& pixWriter) : pixWriter(pixWriter) {}

	void write(int xmin, int xmax, const u8 *& pixel, int y)
	{
		for (int c = xmin; c < xmax; c++)
		{
			u8 red, green, blue, alpha;
			if constexpr (Round)
			{
				red = roundColor<4>(pixel[Red]);
				green = roundColor<4>(pixel[Green]);
				blue = roundColor<4>(pixel[Blue]);
				alpha = roundColor<4>(pixel[Alpha]);
			}
			else
			{
				red = pixel[Red] >> 4;
				green = pixel[Green] >> 4;
				blue = pixel[Blue] >> 4;
				alpha = pixel[Alpha] >> 4;
			}
			pixWriter.write((u16)((red << 8) | (green << 4) | blue | (alpha << 12)));
			pixel += 4;
		}
	}

	static constexpr int BytesPerPixel = 2;

private:
	PixelWriter& pixWriter;
};

// 1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
template<int Red, int Green, int Blue, int Alpha, typename PixelWriter, bool Round = false>
class FBLineWriter1555
{
public:
	FBLineWriter1555(FB_W_CTRL_type fb_w_ctrl, PixelWriter& pixWriter) : pixWriter(pixWriter) {
		fb_alpha_threshold = fb_w_ctrl.fb_alpha_threshold;
	}

	void write(int xmin, int xmax, const u8 *& pixel, int y)
	{
		for (int c = xmin; c < xmax; c++)
		{
			u8 red, green, blue;
			if constexpr (Round)
			{
				red = roundColor<5>(pixel[Red]);
				green = roundColor<5>(pixel[Green]);
				blue = roundColor<5>(pixel[Blue]);
			}
			else
			{
				red = pixel[Red] >> 3;
				green = pixel[Green] >> 3;
				blue = pixel[Blue] >> 3;
			}
			pixWriter.write((u16)((red << 10) | (green << 5) | blue
					| (pixel[Alpha] >= fb_alpha_threshold ? 0x8000 : 0)));
			pixel += 4;
		}
	}

	static constexpr int BytesPerPixel = 2;

private:
	PixelWriter& pixWriter;
	u8 fb_alpha_threshold;
};

// 888 RGB 24 bit packed
template<int Red, int Green, int Blue, int Alpha, typename PixelWriter>
class FBLineWriter888
{
public:
	FBLineWriter888(FB_W_CTRL_type fb_w_ctrl, PixelWriter& pixWriter) : pixWriter(pixWriter) {}

	void write(int xmin, int xmax, const u8 *& pixel, int y)
	{
		for (int c = xmin; c < xmax; c++)
		{
			pixWriter.write(pixel[Blue]);
			pixWriter.write(pixel[Green]);
			pixWriter.write(pixel[Red]);
			pixel += 4;
		}
	}

	static constexpr int BytesPerPixel = 3;

private:
	PixelWriter& pixWriter;
};

// 0888 KRGB 32 bit (K is the value of fb_kval.)
template<int Red, int Green, int Blue, int Alpha, typename PixelWriter>
class FBLineWriter0888
{
public:
	FBLineWriter0888(FB_W_CTRL_type fb_w_ctrl, PixelWriter& pixWriter) : pixWriter(pixWriter) {
		fb_kval = fb_w_ctrl.fb_kval << 24;
	}

	void write(int xmin, int xmax, const u8 *& pixel, int y)
	{
		for (int c = xmin; c < xmax; c++)
		{
			pixWriter.write((u32)((pixel[Red] << 16) | (pixel[Green] << 8) | pixel[Blue] | fb_kval));
			pixel += 4;
		}
	}

	static constexpr int BytesPerPixel = 4;

private:
	u32 fb_kval;
	PixelWriter& pixWriter;
};

// 8888 ARGB 32 bit
template<int Red, int Green, int Blue, int Alpha, typename PixelWriter>
class FBLineWriter8888
{
public:
	FBLineWriter8888(FB_W_CTRL_type fb_w_ctrl, PixelWriter& pixWriter) : pixWriter(pixWriter) {}

	void write(int xmin, int xmax, const u8 *& pixel, int y)
	{
		for (int c = xmin; c < xmax; c++)
		{
			pixWriter.write((u32)((pixel[Red] << 16) | (pixel[Green] << 8) | pixel[Blue] | (pixel[Alpha] << 24)));
			pixel += 4;
		}
	}

	static constexpr int BytesPerPixel = 4;

private:
	PixelWriter& pixWriter;
};

template<typename FBLineWriter>
static void writeTexture(u32 width, u32 height, const u8 *data, u16 *dst, FB_W_CTRL_type fb_w_ctrl, u32 linestride)
{
	u32 padding = linestride;
	if (padding > width * 2)
		padding = padding - width * 2;
	else
		padding = 0;

	TexPixelWriter pixWriter(dst);
	FBLineWriter lineWriter(fb_w_ctrl, pixWriter);

	for (u32 l = 0; l < height; l++) {
		lineWriter.write(0, width, data, l);
		pixWriter.advance(padding);
	}
}

template<int Red, int Green, int Blue, int Alpha>
void WriteTextureToVRam(u32 width, u32 height, const u8 *data, u16 *dst, FB_W_CTRL_type fb_w_ctrl, u32 linestride)
{
	bool dither = fb_w_ctrl.fb_dither && config::EmulateFramebuffer;
	switch (fb_w_ctrl.fb_packmode)
	{
	case 0: // 0555 KRGB 16 bit  (default)
		if (dither)
			writeTexture<FBLineWriter0555<Red, Green, Blue, Alpha, TexPixelWriter, false>>(width, height, data, dst, fb_w_ctrl, linestride);
		else
			writeTexture<FBLineWriter0555<Red, Green, Blue, Alpha, TexPixelWriter, true>>(width, height, data, dst, fb_w_ctrl, linestride);
		break;
	case 1: // 565 RGB 16 bit
		if (dither)
			writeTexture<FBLineWriter565<Red, Green, Blue, Alpha, TexPixelWriter, false>>(width, height, data, dst, fb_w_ctrl, linestride);
		else
			writeTexture<FBLineWriter565<Red, Green, Blue, Alpha, TexPixelWriter, true>>(width, height, data, dst, fb_w_ctrl, linestride);
		break;
	case 2: // 4444 ARGB 16 bit
		if (dither)
			writeTexture<FBLineWriter4444<Red, Green, Blue, Alpha, TexPixelWriter, false>>(width, height, data, dst, fb_w_ctrl, linestride);
		else
			writeTexture<FBLineWriter4444<Red, Green, Blue, Alpha, TexPixelWriter, true>>(width, height, data, dst, fb_w_ctrl, linestride);
		break;
	case 3: // 1555 ARGB 16 bit
		if (dither)
			writeTexture<FBLineWriter1555<Red, Green, Blue, Alpha, TexPixelWriter, false>>(width, height, data, dst, fb_w_ctrl, linestride);
		else
			writeTexture<FBLineWriter1555<Red, Green, Blue, Alpha, TexPixelWriter, true>>(width, height, data, dst, fb_w_ctrl, linestride);
		break;
	}
}
template void WriteTextureToVRam<0, 1, 2, 3>(u32 width, u32 height, const u8 *data, u16 *dst, FB_W_CTRL_type fb_w_ctrl, u32 linestride);
template void WriteTextureToVRam<2, 1, 0, 3>(u32 width, u32 height, const u8 *data, u16 *dst, FB_W_CTRL_type fb_w_ctrl, u32 linestride);

template<typename FBLineWriter>
static void writeFramebufferLW(u32 width, u32 height, const u8 *data, u32 dstAddr, FB_W_CTRL_type fb_w_ctrl, u32 linestride, FB_X_CLIP_type xclip, FB_Y_CLIP_type yclip)
{
	int bpp = FBLineWriter::BytesPerPixel;

	u32 padding = linestride;
	if (padding > width * bpp)
		padding = padding - width * bpp;
	else
		padding = 0;

	const u8 *p = data + 4 * yclip.min * width;
	dstAddr += bpp * yclip.min * (width + padding / bpp);

	const u32 clipWidth = std::min(width, xclip.max + 1u);
	height = std::min(height, yclip.max + 1u);

	FBPixelWriter pixWriter(dstAddr);
	FBLineWriter lineWriter(fb_w_ctrl, pixWriter);

	for (u32 l = yclip.min; l < height; l++)
	{
		p += 4 * xclip.min;
		pixWriter.advance(bpp * xclip.min);

		lineWriter.write(xclip.min, clipWidth, p, l);

		pixWriter.advance(padding + (width - xclip.max - 1) * bpp);
		p += (width - xclip.max - 1) * 4;
	}
}

template<int Red, int Green, int Blue, int Alpha>
void WriteFramebuffer(u32 width, u32 height, const u8 *data, u32 dstAddr, FB_W_CTRL_type fb_w_ctrl, u32 linestride, FB_X_CLIP_type xclip, FB_Y_CLIP_type yclip)
{
	switch (fb_w_ctrl.fb_packmode)
	{
	case 0: // 0555 KRGB 16 bit
		writeFramebufferLW<FBLineWriter0555<Red, Green, Blue, Alpha, FBPixelWriter>>(width, height, data, dstAddr, fb_w_ctrl, linestride, xclip, yclip);
		break;
	case 1: // 565 RGB 16 bit
		writeFramebufferLW<FBLineWriter565<Red, Green, Blue, Alpha, FBPixelWriter>>(width, height, data, dstAddr, fb_w_ctrl, linestride, xclip, yclip);
		break;
	case 2: // 4444 ARGB 16 bit
		writeFramebufferLW<FBLineWriter4444<Red, Green, Blue, Alpha, FBPixelWriter>>(width, height, data, dstAddr, fb_w_ctrl, linestride, xclip, yclip);
		break;
	case 3: // 1555 ARGB 16 bit
		writeFramebufferLW<FBLineWriter1555<Red, Green, Blue, Alpha, FBPixelWriter>>(width, height, data, dstAddr, fb_w_ctrl, linestride, xclip, yclip);
		break;
	case 4: // 888 RGB 24 bit packed
		writeFramebufferLW<FBLineWriter888<Red, Green, Blue, Alpha, FBPixelWriter>>(width, height, data, dstAddr, fb_w_ctrl, linestride, xclip, yclip);
		break;
	case 5: // 0888 KRGB 32 bit
		writeFramebufferLW<FBLineWriter0888<Red, Green, Blue, Alpha, FBPixelWriter>>(width, height, data, dstAddr, fb_w_ctrl, linestride, xclip, yclip);
		break;
	case 6: // 8888 ARGB 32 bit
		writeFramebufferLW<FBLineWriter8888<Red, Green, Blue, Alpha, FBPixelWriter>>(width, height, data, dstAddr, fb_w_ctrl, linestride, xclip, yclip);
		break;
	default:
		die("Invalid framebuffer format");
		break;
	}
}
template void WriteFramebuffer<0, 1, 2, 3>(u32 width, u32 height, const u8 *data, u32 dstAddr, FB_W_CTRL_type fb_w_ctrl,
		u32 linestride, FB_X_CLIP_type xclip, FB_Y_CLIP_type yclip);
template void WriteFramebuffer<2, 1, 0, 3>(u32 width, u32 height, const u8 *data, u32 dstAddr, FB_W_CTRL_type fb_w_ctrl,
		u32 linestride, FB_X_CLIP_type xclip, FB_Y_CLIP_type yclip);

void BaseTextureCacheData::invalidate()
{
	dirty = FrameCount;

	libCore_vramlock_Unlock_block_wb(lock_block);
	lock_block = nullptr;
}

void getRenderToTextureDimensions(u32& width, u32& height, u32& pow2Width, u32& pow2Height)
{
	pow2Width = 8;
	while (pow2Width < width)
		pow2Width *= 2;
	pow2Height = 8;
	while (pow2Height < height)
		pow2Height *= 2;
	if (!config::RenderToTextureBuffer)
	{
		float upscale = config::RenderResolution / 480.f;
		width *= upscale;
		height *= upscale;
		pow2Width *= upscale;
		pow2Height *= upscale;
	}
}

#ifdef TEST_AUTOMATION
#include <stb_image_write.h>

void dump_screenshot(u8 *buffer, u32 width, u32 height, bool alpha, u32 rowPitch, bool invertY)
{
	stbi_flip_vertically_on_write((int)invertY);
	stbi_write_png("screenshot.png", width, height, alpha ? 4 : 3, buffer, rowPitch);
}
#endif
