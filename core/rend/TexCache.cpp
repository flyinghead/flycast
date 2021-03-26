#include "TexCache.h"
#include "CustomTexture.h"
#include "deps/xbrz/xbrz.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/mem/_vmem.h"
#include "hw/mem/vmem32.h"
#include "hw/sh4/modules/mmu.h"

#include <algorithm>
#include <mutex>
#include <xxhash.h>

#ifndef TARGET_NO_OPENMP
#include <omp.h>
#endif

u8* vq_codebook;
u32 palette_index;
bool KillTex=false;
u32 palette16_ram[1024];
u32 palette32_ram[1024];
u32 pal_hash_256[4];
u32 pal_hash_16[64];
bool palette_updated;
float fb_scale_x, fb_scale_y;

// Rough approximation of LoD bias from D adjust param, only used to increase LoD
const std::array<f32, 16> D_Adjust_LoD_Bias = {
		0.f, -4.f, -2.f, -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
};
static void rend_text_invl(vram_block* bl);

u32 detwiddle[2][11][1024];
//input : address in the yyyyyxxxxx format
//output : address in the xyxyxyxy format
//U : x resolution , V : y resolution
//twiddle works on 64b words


static u32 twiddle_slow(u32 x,u32 y,u32 x_sz,u32 y_sz)
{
	u32 rv=0;//low 2 bits are directly passed  -> needs some misc stuff to work.However
			 //Pvr internally maps the 64b banks "as if" they were twiddled :p

	u32 sh=0;
	x_sz>>=1;
	y_sz>>=1;
	while(x_sz!=0 || y_sz!=0)
	{
		if (y_sz)
		{
			u32 temp=y&1;
			rv|=temp<<sh;

			y_sz>>=1;
			y>>=1;
			sh++;
		}
		if (x_sz)
		{
			u32 temp=x&1;
			rv|=temp<<sh;

			x_sz>>=1;
			x>>=1;
			sh++;
		}
	}	
	return rv;
}

static void BuildTwiddleTables()
{
	for (u32 s = 0; s < 11; s++)
	{
		u32 x_sz = 1024;
		u32 y_sz = 1 << s;
		for (u32 i = 0; i < x_sz; i++)
		{
			detwiddle[0][s][i] = twiddle_slow(i, 0, x_sz, y_sz);
			detwiddle[1][s][i] = twiddle_slow(0, i, y_sz, x_sz);
		}
	}
}

static OnLoad btt(&BuildTwiddleTables);

void palette_update()
{
	if (!pal_needs_update)
		return;
	pal_needs_update = false;
	palette_updated = true;

	switch(PAL_RAM_CTRL&3)
	{
	case 0:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB1555(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB1555_32(PALETTE_RAM[i]);
		}
		break;

	case 1:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB565(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB565_32(PALETTE_RAM[i]);
		}
		break;

	case 2:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB4444(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB4444_32(PALETTE_RAM[i]);
		}
		break;

	case 3:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB8888(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB8888_32(PALETTE_RAM[i]);
		}
		break;
	}
	for (int i = 0; i < 64; i++)
		pal_hash_16[i] = XXH32(&PALETTE_RAM[i << 4], 16 * 4, 7);
	for (int i = 0; i < 4; i++)
		pal_hash_256[i] = XXH32(&PALETTE_RAM[i << 8], 256 * 4, 7);
}

static std::vector<vram_block*> VramLocks[VRAM_SIZE_MAX / PAGE_SIZE];

//List functions
//
void vramlock_list_remove(vram_block* block)
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
 
void vramlock_list_add(vram_block* block)
{
	u32 base = block->start / PAGE_SIZE;
	u32 end = block->end / PAGE_SIZE;

	for (u32 i = base; i <= end; i++)
	{
		std::vector<vram_block*>& list = VramLocks[i];
		// If the list is empty then we need to protect vram, otherwise it's already been done
		if (list.empty() || std::all_of(list.begin(), list.end(), [](vram_block *block) { return block == nullptr; }))
			_vmem_protect_vram(i * PAGE_SIZE, PAGE_SIZE);
		auto it = std::find(list.begin(), list.end(), nullptr);
		if (it != list.end())
			*it = block;
		else
			list.push_back(block);
	}
}
 
std::mutex vramlist_lock;

void libCore_vramlock_Lock(u32 start_offset64, u32 end_offset64, BaseTextureCacheData *texture)
{
	vram_block* block=(vram_block* )malloc(sizeof(vram_block));
 
	if (end_offset64>(VRAM_SIZE-1))
	{
		WARN_LOG(PVR, "vramlock_Lock_64: end_offset64>(VRAM_SIZE-1) \n Tried to lock area out of vram , possibly bug on the pvr plugin");
		end_offset64=(VRAM_SIZE-1);
	}

	if (start_offset64>end_offset64)
	{
		WARN_LOG(PVR, "vramlock_Lock_64: start_offset64>end_offset64 \n Tried to lock negative block , possibly bug on the pvr plugin");
		start_offset64=0;
	}

	

	block->end=end_offset64;
	block->start=start_offset64;
	block->len=end_offset64-start_offset64+1;
	block->userdata = texture;
	block->type=64;

	{
		std::lock_guard<std::mutex> lock(vramlist_lock);

		if (texture->lock_block == nullptr)
		{
			// This also protects vram if needed
			vramlock_list_add(block);
			texture->lock_block = block;
		}
		else
			free(block);
	}
}

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
				rend_text_invl(lock);

				if (lock != nullptr)
				{
					ERROR_LOG(PVR, "Error : pvr is supposed to remove lock");
					die("Invalid state");
				}
			}
		}
		list.clear();

		_vmem_unprotect_vram((u32)(offset & ~PAGE_MASK), PAGE_SIZE);
	}

	return true;
}

bool VramLockedWrite(u8* address)
{
	u32 offset = _vmem_get_vram_offset(address);
	if (offset == (u32)-1)
		return false;
	return VramLockedWriteOffset(offset);
}

//unlocks mem
//also frees the handle
static void libCore_vramlock_Unlock_block_wb(vram_block* block)
{
	if (mmu_enabled())
		vmem32_unprotect_vram(block->start, block->len);
	vramlock_list_remove(block);
	free(block);
}

void libCore_vramlock_Unlock_block(vram_block* block)
{
	std::lock_guard<std::mutex> lock(vramlist_lock);
	libCore_vramlock_Unlock_block_wb(block);
}

#ifndef TARGET_NO_OPENMP
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
#ifndef TARGET_NO_OPENMP
	parallelize([=](int start, int end) {
		xbrz::scale(factor, source, dest, width, height, has_alpha ? xbrz::ColorFormat::ARGB : xbrz::ColorFormat::RGB,
				xbrz_cfg, start, end);
	}, 0, height);
#else
	xbrz::scale(factor, source, dest, width, height, has_alpha ? xbrz::ColorFormat::ARGB : xbrz::ColorFormat::RGB, xbrz_cfg);
#endif
}

struct PvrTexInfo
{
	const char* name;
	int bpp;        //4/8 for pal. 16 for yuv, rgb, argb
	TextureType type;
	// Conversion to 16 bpp
	TexConvFP *PL;
	TexConvFP *TW;
	TexConvFP *VQ;
	// Conversion to 32 bpp
	TexConvFP32 *PL32;
	TexConvFP32 *TW32;
	TexConvFP32 *VQ32;
	// Conversion to 8 bpp (palette)
	TexConvFP8 *TW8;
};

static const PvrTexInfo format[8] =
{	// name     bpp Final format			   Planar		Twiddled	 VQ				Planar(32b)    Twiddled(32b)  VQ (32b)      Palette (8b)
	{"1555", 	16,	TextureType::_5551,        tex1555_PL,  tex1555_TW,  tex1555_VQ,    tex1555_PL32,  tex1555_TW32,  tex1555_VQ32, nullptr },	    //1555
	{"565", 	16, TextureType::_565,         tex565_PL,   tex565_TW,   tex565_VQ,     tex565_PL32,   tex565_TW32,   tex565_VQ32,  nullptr },	    //565
	{"4444", 	16, TextureType::_4444,        tex4444_PL,  tex4444_TW,  tex4444_VQ,    tex4444_PL32,  tex4444_TW32,  tex4444_VQ32, nullptr },	    //4444
	{"yuv", 	16, TextureType::_8888,        nullptr,     nullptr,     nullptr,       texYUV422_PL,  texYUV422_TW,  texYUV422_VQ, nullptr },	    //yuv
	{"bumpmap", 16, TextureType::_4444,        texBMP_PL,   texBMP_TW,	 texBMP_VQ,     tex4444_PL32,  tex4444_TW32,  tex4444_VQ32, nullptr },      //bump map
	{"pal4", 	4,	TextureType::_5551,		   nullptr,     texPAL4_TW,  texPAL4_VQ,    nullptr,       texPAL4_TW32,  texPAL4_VQ32, texPAL4PT_TW },	//pal4
	{"pal8", 	8,	TextureType::_5551,		   nullptr,     texPAL8_TW,  texPAL8_VQ,    nullptr,       texPAL8_TW32,  texPAL8_VQ32, texPAL8PT_TW },	//pal8
	{"ns/1555", 0},	                                                                                                                                // Not supported (1555)
};

static const u32 VQMipPoint[11] =
{
	0x00000,//1
	0x00001,//2
	0x00002,//4
	0x00006,//8
	0x00016,//16
	0x00056,//32
	0x00156,//64
	0x00556,//128
	0x01556,//256
	0x05556,//512
	0x15556//1024
};
static const u32 OtherMipPoint[11] =
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
	char str[512];
	sprintf(str, "Texture: %s", GetPixelFormatName());

	if (tcw.VQ_Comp)
		strcat(str, " VQ");
	else if (tcw.ScanOrder == 0)
		strcat(str, " TW");
	else if (tcw.StrideSel)
		strcat(str, " Stride");

	if (tcw.ScanOrder == 0 && tcw.MipMapped)
		strcat(str, " MM");
	if (tsp.FilterMode != 0)
		strcat(str, " Bilinear");

	sprintf(str + strlen(str), " %dx%d @ 0x%X", 8 << tsp.TexU, 8 << tsp.TexV, tcw.TexAddr << 3);
	std::string id = GetId();
	sprintf(str + strlen(str), " id=%s", id.c_str());
	DEBUG_LOG(RENDERER, "%s", str);
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

bool BaseTextureCacheData::Delete()
{
	if (custom_load_in_progress > 0)
		return false;

	{
		std::lock_guard<std::mutex> lock(vramlist_lock);
		if (lock_block)
			libCore_vramlock_Unlock_block_wb(lock_block);
		lock_block = nullptr;
	}

	free(custom_image_data);

	return true;
}

void BaseTextureCacheData::Create()
{
	//Reset state info ..
	Updates = 0;
	dirty = FrameCount;
	lock_block = nullptr;
	custom_image_data = nullptr;
	custom_load_in_progress = 0;

	//decode info from tsp/tcw into the texture struct
	tex = &format[tcw.PixelFmt == PixelReserved ? Pixel1555 : tcw.PixelFmt];	//texture format table entry

	sa_tex = (tcw.TexAddr << 3) & VRAM_MASK;	//texture start address
	sa = sa_tex;								//data texture start address (modified for MIPs, as needed)
	w = 8 << tsp.TexU;							//tex width
	h = 8 << tsp.TexV;							//tex height

	//PAL texture
	if (tex->bpp == 4)
		palette_index = tcw.PalSelect << 4;
	else if (tex->bpp == 8)
		palette_index = (tcw.PalSelect >> 4) << 8;

	texconv8 = nullptr;

	if (tcw.ScanOrder && (tex->PL || tex->PL32))
	{
		//Texture is stored 'planar' in memory, no deswizzle is needed
		//verify(tcw.VQ_Comp==0);
		if (tcw.VQ_Comp != 0)
		{
			WARN_LOG(RENDERER, "Warning: planar texture with VQ set (invalid)");
			tcw.VQ_Comp = 0;
		}
		if (tcw.MipMapped != 0)
		{
			WARN_LOG(RENDERER, "Warning: planar texture with mipmaps (invalid)");
			tcw.MipMapped = 0;
		}

		//Planar textures support stride selection, mostly used for non power of 2 textures (videos)
		int stride = w;
		if (tcw.StrideSel)
			stride = (TEXT_CONTROL & 31) * 32;

		//Call the format specific conversion code
		texconv = tex->PL;
		texconv32 = tex->PL32;
		//calculate the size, in bytes, for the locking
		size = stride * h * tex->bpp / 8;
	}
	else
	{
		tcw.ScanOrder = 0;
		tcw.StrideSel = 0;
		// Quake 3 Arena uses one
		if (tcw.MipMapped)
			// Mipmapped texture must be square and TexV is ignored
			h = w;

		if (tcw.VQ_Comp)
		{
			verify(tex->VQ != NULL || tex->VQ32 != NULL);
			vq_codebook = sa;
			if (tcw.MipMapped)
				sa += VQMipPoint[tsp.TexU + 3];
			texconv = tex->VQ;
			texconv32 = tex->VQ32;
			size = w * h / 8;
		}
		else
		{
			verify(tex->TW != NULL || tex->TW32 != NULL);
			if (tcw.MipMapped)
				sa += OtherMipPoint[tsp.TexU + 3] * tex->bpp / 8;
			texconv = tex->TW;
			texconv32 = tex->TW32;
			size = w * h * tex->bpp / 8;
			texconv8 = tex->TW8;
		}
	}
}

void BaseTextureCacheData::ComputeHash()
{
	texture_hash = XXH32(&vram[sa], size, 7);
	if (IsPaletted())
		texture_hash ^= palette_hash;
	old_texture_hash = texture_hash;
	texture_hash ^= tcw.full & 0xFC000000;	// everything but texaddr, reserved and stride
}

void BaseTextureCacheData::Update()
{
	//texture state tracking stuff
	Updates++;
	dirty=0;

	tex_type = tex->type;

	bool has_alpha = false;
	if (IsPaletted())
	{
		if (IsGpuHandledPaletted(tsp, tcw))
			tex_type = TextureType::_8;
		else
		{
			tex_type = PAL_TYPE[PAL_RAM_CTRL&3];
			if (tex_type != TextureType::_565)
				has_alpha = true;
		}

		// Get the palette hash to check for future updates
		if (tcw.PixelFmt == PixelPal4)
			palette_hash = pal_hash_16[tcw.PalSelect];
		else
			palette_hash = pal_hash_256[tcw.PalSelect >> 4];
	}

	::palette_index = this->palette_index; // might be used if pal. tex
	::vq_codebook = &vram[vq_codebook];    // might be used if VQ tex

	//texture conversion work
	u32 stride = w;

	if (tcw.StrideSel && tcw.ScanOrder && (tex->PL || tex->PL32))
		stride = (TEXT_CONTROL & 31) * 32;

	u32 original_h = h;
	if (sa_tex > VRAM_SIZE || size == 0 || sa + size > VRAM_SIZE)
	{
		if (sa < VRAM_SIZE && sa + size > VRAM_SIZE && tcw.ScanOrder && stride > 0)
		{
			// Shenmue Space Harrier mini-arcade loads a texture that goes beyond the end of VRAM
			// but only uses the top portion of it
			h = (VRAM_SIZE - sa) * 8 / stride / tex->bpp;
			size = stride * h * tex->bpp/8;
		}
		else
		{
			WARN_LOG(RENDERER, "Warning: invalid texture. Address %08X %08X size %d", sa_tex, sa, size);
			return;
		}
	}
	if (config::CustomTextures)
		custom_texture.LoadCustomTextureAsync(this);

	void *temp_tex_buffer = NULL;
	u32 upscaled_w = w;
	u32 upscaled_h = h;

	PixelBuffer<u16> pb16;
	PixelBuffer<u32> pb32;
	PixelBuffer<u8> pb8;

	// Figure out if we really need to use a 32-bit pixel buffer
	bool textureUpscaling = config::TextureUpscale > 1
			// Don't process textures that are too big
			&& (int)(w * h) <= config::MaxFilteredTextureSize * config::MaxFilteredTextureSize
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
			pb32.init(w, h, true);
			for (u32 i = 0; i <= tsp.TexU + 3u; i++)
			{
				pb32.set_mipmap(i);
				u32 vram_addr;
				if (tcw.VQ_Comp)
				{
					vram_addr = sa_tex + VQMipPoint[i];
					if (i == 0)
					{
						PixelBuffer<u32> pb0;
						pb0.init(2, 2 ,false);
						texconv32(&pb0, (u8*)&vram[vram_addr], 2, 2);
						*pb32.data() = *pb0.data(1, 1);
						continue;
					}
				}
				else
					vram_addr = sa_tex + OtherMipPoint[i] * tex->bpp / 8;
				if (tcw.PixelFmt == PixelYUV && i == 0)
					// Special case for YUV at 1x1 LoD
					format[Pixel565].TW32(&pb32, &vram[vram_addr], 1, 1);
				else
					texconv32(&pb32, &vram[vram_addr], 1 << i, 1 << i);
			}
			pb32.set_mipmap(0);
		}
		else
		{
			pb32.init(w, h);
			texconv32(&pb32, (u8*)&vram[sa], stride, h);

			// xBRZ scaling
			if (textureUpscaling)
			{
				PixelBuffer<u32> tmp_buf;
				tmp_buf.init(w * config::TextureUpscale, h * config::TextureUpscale);

				if (tcw.PixelFmt == Pixel1555 || tcw.PixelFmt == Pixel4444)
					// Alpha channel formats. Palettes with alpha are already handled
					has_alpha = true;
				UpscalexBRZ(config::TextureUpscale, pb32.data(), tmp_buf.data(), w, h, has_alpha);
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
			pb8.init(w, h, true);
			for (u32 i = 0; i <= tsp.TexU + 3u; i++)
			{
				pb8.set_mipmap(i);
				u32 vram_addr = sa_tex + OtherMipPoint[i] * tex->bpp / 8;
				texconv8(&pb8, &vram[vram_addr], 1 << i, 1 << i);
			}
			pb8.set_mipmap(0);
		}
		else
		{
			pb8.init(w, h);
			texconv8(&pb8, &vram[sa], stride, h);
		}
		temp_tex_buffer = pb8.data();
	}
	else if (texconv != NULL)
	{
		if (mipmapped)
		{
			pb16.init(w, h, true);
			for (u32 i = 0; i <= tsp.TexU + 3u; i++)
			{
				pb16.set_mipmap(i);
				u32 vram_addr;
				if (tcw.VQ_Comp)
				{
					vram_addr = sa_tex + VQMipPoint[i];
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
					vram_addr = sa_tex + OtherMipPoint[i] * tex->bpp / 8;
				texconv(&pb16, (u8*)&vram[vram_addr], 1 << i, 1 << i);
			}
			pb16.set_mipmap(0);
		}
		else
		{
			pb16.init(w, h);
			texconv(&pb16,(u8*)&vram[sa],stride,h);
		}
		temp_tex_buffer = pb16.data();
	}
	else
	{
		//fill it in with a temp color
		WARN_LOG(RENDERER, "UNHANDLED TEXTURE");
		pb16.init(w, h);
		memset(pb16.data(), 0x80, w * h * 2);
		temp_tex_buffer = pb16.data();
		mipmapped = false;
	}
	// Restore the original texture height if it was constrained to VRAM limits above
	h = original_h;

	//lock the texture to detect changes in it
	libCore_vramlock_Lock(sa_tex, sa + size - 1, this);

	UploadToGPU(upscaled_w, upscaled_h, (u8*)temp_tex_buffer, IsMipmapped(), mipmapped);
	if (config::DumpTextures)
	{
		ComputeHash();
		custom_texture.DumpTexture(texture_hash, upscaled_w, upscaled_h, tex_type, temp_tex_buffer);
		NOTICE_LOG(RENDERER, "Dumped texture %x.png. Old hash %x", texture_hash, old_texture_hash);
	}
	PrintTextureName();
}

void BaseTextureCacheData::CheckCustomTexture()
{
	if (IsCustomTextureAvailable())
	{
		tex_type = TextureType::_8888;
		UploadToGPU(custom_width, custom_height, custom_image_data, IsMipmapped(), false);
		free(custom_image_data);
		custom_image_data = NULL;
	}
}

void ReadFramebuffer(PixelBuffer<u32>& pb, int& width, int& height)
{
	width = (FB_R_SIZE.fb_x_size + 1) << 1;     // in 16-bit words
	height = FB_R_SIZE.fb_y_size + 1;
	int modulus = (FB_R_SIZE.fb_modulus - 1) << 1;

	int bpp;
	switch (FB_R_CTRL.fb_depth)
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

	u32 addr = FB_R_SOF1;
	if (SPG_CONTROL.interlace)
	{
		if (width == modulus && FB_R_SOF2 == FB_R_SOF1 + width * bpp)
		{
			// Typical case alternating even and odd lines -> take the whole buffer at once
			modulus = 0;
			height *= 2;
		}
		else
		{
			addr = SPG_STATUS.fieldnum ? FB_R_SOF2 : FB_R_SOF1;
		}
	}

	pb.init(width, height);
	u8 *dst = (u8*)pb.data();

	switch (FB_R_CTRL.fb_depth)
	{
		case fbde_0555:    // 555 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u16 src = pvr_read32p<u16>(addr);
					*dst++ = (((src >> 10) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = (((src >> 5) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = 0xFF;
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
					*dst++ = (((src >> 11) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = (((src >> 5) & 0x3F) << 2) + (FB_R_CTRL.fb_concat & 3);
					*dst++ = (((src >> 0) & 0x1F) << 3) + FB_R_CTRL.fb_concat;
					*dst++ = 0xFF;
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
					*dst++ = src >> 16;
					*dst++ = src >> 8;
					*dst++ = src;
					*dst++ = 0xFF;
					addr += 4;
					if (i + 1 >= width)
						break;
					u32 src2 = pvr_read32p<u32>(addr);
					*dst++ = src2 >> 8;
					*dst++ = src2;
					*dst++ = src >> 24;
					*dst++ = 0xFF;
					addr += 4;
					if (i + 2 >= width)
						break;
					u32 src3 = pvr_read32p<u32>(addr);
					*dst++ = src3;
					*dst++ = src2 >> 24;
					*dst++ = src2 >> 16;
					*dst++ = 0xFF;
					addr += 4;
					if (i + 3 >= width)
						break;
					*dst++ = src3 >> 24;
					*dst++ = src3 >> 16;
					*dst++ = src3 >> 8;
					*dst++ = 0xFF;
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
					*dst++ = src >> 16;
					*dst++ = src >> 8;
					*dst++ = src;
					*dst++ = 0xFF;
					addr += bpp;
				}
				addr += modulus * bpp;
			}
			break;
	}
}

void WriteTextureToVRam(u32 width, u32 height, u8 *data, u16 *dst)
{
	u32 stride = FB_W_LINESTRIDE.stride * 8;
	if (stride == 0)
		stride = width * 2;
	else if (width * 2 > stride) {
    	// Happens for Virtua Tennis
		width = stride / 2;
    }

	const u16 kval_bit = (FB_W_CTRL.fb_kval & 0x80) << 8;
	const u8 fb_alpha_threshold = FB_W_CTRL.fb_alpha_threshold;

	u8 *p = data;

	for (u32 l = 0; l < height; l++) {
		switch(FB_W_CTRL.fb_packmode)
		{
		case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
			for (u32 c = 0; c < width; c++) {
				*dst++ = (((p[0] >> 3) & 0x1F) << 10) | (((p[1] >> 3) & 0x1F) << 5) | ((p[2] >> 3) & 0x1F) | kval_bit;
				p += 4;
			}
			break;
		case 1: //0x1   565 RGB 16 bit
			for (u32 c = 0; c < width; c++) {
				*dst++ = (((p[0] >> 3) & 0x1F) << 11) | (((p[1] >> 2) & 0x3F) << 5) | ((p[2] >> 3) & 0x1F);
				p += 4;
			}
			break;
		case 2: //0x2   4444 ARGB 16 bit
			for (u32 c = 0; c < width; c++) {
				*dst++ = (((p[0] >> 4) & 0xF) << 8) | (((p[1] >> 4) & 0xF) << 4) | ((p[2] >> 4) & 0xF) | (((p[3] >> 4) & 0xF) << 12);
				p += 4;
			}
			break;
		case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
			for (u32 c = 0; c < width; c++) {
				*dst++ = (((p[0] >> 3) & 0x1F) << 10) | (((p[1] >> 3) & 0x1F) << 5) | ((p[2] >> 3) & 0x1F) | (p[3] > fb_alpha_threshold ? 0x8000 : 0);
				p += 4;
			}
			break;
		}
		dst += (stride - width * 2) / 2;
	}
}

static void rend_text_invl(vram_block* bl)
{
	BaseTextureCacheData* tcd = (BaseTextureCacheData*)bl->userdata;
	tcd->dirty = FrameCount;
	tcd->lock_block = nullptr;

	libCore_vramlock_Unlock_block_wb(bl);
}

#ifdef TEST_AUTOMATION
#include <stb_image_write.h>

void dump_screenshot(u8 *buffer, u32 width, u32 height, bool alpha, u32 rowPitch, bool invertY)
{
	stbi_flip_vertically_on_write((int)invertY);
	stbi_write_png("screenshot.png", width, height, alpha ? 4 : 3, buffer, rowPitch);
}
#endif

