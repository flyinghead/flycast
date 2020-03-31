#include "TexCache.h"
#include "CustomTexture.h"
#include "deps/xbrz/xbrz.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/mem/_vmem.h"
#include "hw/mem/vmem32.h"
#include "hw/sh4/modules/mmu.h"

#include <algorithm>
#include <mutex>
#include <png.h>
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

// Rough approximation of LoD bias from D adjust param, only used to increase LoD
const std::array<f32, 16> D_Adjust_LoD_Bias = {
		0.f, -4.f, -2.f, -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f
};

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

	pal_needs_update=false;

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
		pal_hash_16[i] = XXH32(&palette32_ram[i << 4], 16 * 4, 7);
	for (int i = 0; i < 4; i++)
		pal_hash_256[i] = XXH32(&palette32_ram[i << 8], 256 * 4, 7);
}

std::vector<vram_block*> VramLocks[VRAM_SIZE_MAX / PAGE_SIZE];
VArray2 vram;  // vram 32-64b

//List functions
//
void vramlock_list_remove(vram_block* block)
{
	u32 base = block->start / PAGE_SIZE;
	u32 end = block->end / PAGE_SIZE;

	for (u32 i = base; i <= end; i++)
	{
		std::vector<vram_block*>& list = VramLocks[i];
		for (size_t j = 0; j < list.size(); j++)
		{
			if (list[j] == block)
			{
				list[j] = nullptr;
			}
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

vram_block* libCore_vramlock_Lock(u32 start_offset64,u32 end_offset64,void* userdata)
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
	block->userdata=userdata;
	block->type=64;

	{
		std::lock_guard<std::mutex> lock(vramlist_lock);

		// This also protects vram if needed
		vramlock_list_add(block);
	}

	return block;
}

bool VramLockedWriteOffset(size_t offset)
{
	if (offset >= VRAM_SIZE)
		return false;

	size_t addr_hash = offset / PAGE_SIZE;
	std::vector<vram_block *>& list = VramLocks[addr_hash];

	{
		std::lock_guard<std::mutex> lock(vramlist_lock);

		for (size_t i = 0; i < list.size(); i++)
		{
			if (list[i] != nullptr)
			{
				libPvr_LockedBlockWrite(list[i], (u32)offset);

				if (list[i] != nullptr)
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
void libCore_vramlock_Unlock_block(vram_block* block)
{
	std::lock_guard<std::mutex> lock(vramlist_lock);
	libCore_vramlock_Unlock_block_wb(block);
}

void libCore_vramlock_Unlock_block_wb(vram_block* block)
{
	if (mmu_enabled())
		vmem32_unprotect_vram(block->start, block->len);
	vramlock_list_remove(block);
	free(block);
}

//
// deposterization: smoothes posterized gradients from low-color-depth (e.g. 444, 565, compressed) sources
// Shamelessly stolen from ppsspp
// Copyright (c) 2012- PPSSPP Project.
//
#define BLOCK_SIZE 32

static void deposterizeH(u32* data, u32* out, int w, int l, int u) {
	static const int T = 8;
	for (int y = l; y < u; ++y) {
		for (int x = 0; x < w; ++x) {
			int inpos = y*w + x;
			u32 center = data[inpos];
			if (x == 0 || x == w - 1) {
				out[y*w + x] = center;
				continue;
			}
			u32 left = data[inpos - 1];
			u32 right = data[inpos + 1];
			out[y*w + x] = 0;
			for (int c = 0; c < 4; ++c) {
				u8 lc = ((left >> c * 8) & 0xFF);
				u8 cc = ((center >> c * 8) & 0xFF);
				u8 rc = ((right >> c * 8) & 0xFF);
				if ((lc != rc) && ((lc == cc && abs((int)((int)rc) - cc) <= T) || (rc == cc && abs((int)((int)lc) - cc) <= T))) {
					// blend this component
					out[y*w + x] |= ((rc + lc) / 2) << (c * 8);
				} else {
					// no change for this component
					out[y*w + x] |= cc << (c * 8);
				}
			}
		}
	}
}
static void deposterizeV(u32* data, u32* out, int w, int h, int l, int u) {
	static const int T = 8;
	for (int xb = 0; xb < w / BLOCK_SIZE + 1; ++xb) {
		for (int y = l; y < u; ++y) {
			for (int x = xb*BLOCK_SIZE; x < (xb + 1)*BLOCK_SIZE && x < w; ++x) {
				u32 center = data[y    * w + x];
				if (y == 0 || y == h - 1) {
					out[y*w + x] = center;
					continue;
				}
				u32 upper = data[(y - 1) * w + x];
				u32 lower = data[(y + 1) * w + x];
				out[y*w + x] = 0;
				for (int c = 0; c < 4; ++c) {
					u8 uc = ((upper >> c * 8) & 0xFF);
					u8 cc = ((center >> c * 8) & 0xFF);
					u8 lc = ((lower >> c * 8) & 0xFF);
					if ((uc != lc) && ((uc == cc && abs((int)((int)lc) - cc) <= T) || (lc == cc && abs((int)((int)uc) - cc) <= T))) {
						// blend this component
						out[y*w + x] |= ((lc + uc) / 2) << (c * 8);
					} else {
						// no change for this component
						out[y*w + x] |= cc << (c * 8);
					}
				}
			}
		}
	}
}

#ifndef TARGET_NO_OPENMP
static inline int getThreadCount()
{
	int tcount = omp_get_num_procs() - 1;
	if (tcount < 1)
		tcount = 1;
	return std::min(tcount, (int)settings.pvr.MaxThreads);
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

void DePosterize(u32* source, u32* dest, int width, int height) {
	u32 *tmpbuf = (u32 *)malloc(width * height * sizeof(u32));

	parallelize([source, tmpbuf, width](int start, int end) { deposterizeH(source, tmpbuf, width, start, end); }, 0, height);
	parallelize([tmpbuf, dest, width, height](int start, int end) { deposterizeV(tmpbuf, dest, width, height, start, end); }, 0, height);
	parallelize([dest, tmpbuf, width](int start, int end) { deposterizeH(dest, tmpbuf, width, start, end); }, 0, height);
	parallelize([tmpbuf, dest, width, height](int start, int end) { deposterizeV(tmpbuf, dest, width, height, start, end); }, 0, height);

	free(tmpbuf);
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
};

static const PvrTexInfo format[8] =
{	// name     bpp Final format			   Planar		Twiddled	 VQ				Planar(32b)    Twiddled(32b)  VQ (32b)
	{"1555", 	16,	TextureType::_5551,        tex1555_PL,	tex1555_TW,  tex1555_VQ,	tex1555_PL32,  tex1555_TW32,  tex1555_VQ32 },	//1555
	{"565", 	16, TextureType::_565,         tex565_PL,	tex565_TW,   tex565_VQ, 	tex565_PL32,   tex565_TW32,   tex565_VQ32 },	//565
	{"4444", 	16, TextureType::_4444,        tex4444_PL,	tex4444_TW,  tex4444_VQ, 	tex4444_PL32,  tex4444_TW32,  tex4444_VQ32 },	//4444
	{"yuv", 	16, TextureType::_8888,        NULL, 		NULL, 		 NULL,			texYUV422_PL,  texYUV422_TW,  texYUV422_VQ },	//yuv
	{"bumpmap", 16, TextureType::_4444,        texBMP_PL,	texBMP_TW,	 texBMP_VQ, 	tex4444_PL32,  tex4444_TW32,  tex4444_VQ32 },   //bump map
	{"pal4", 	4,	TextureType::_5551,		   0,			texPAL4_TW,  texPAL4_VQ, 	NULL, 		   texPAL4_TW32,  texPAL4_VQ32 },	//pal4
	{"pal8", 	8,	TextureType::_5551,		   0,			texPAL8_TW,  texPAL8_VQ, 	NULL, 		   texPAL8_TW32,  texPAL8_VQ32 },	//pal8
	{"ns/1555", 0},																														// Not supported (1555)
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
	sprintf(str, "Texture: %s ", GetPixelFormatName());

	if (tcw.VQ_Comp)
		strcat(str, " VQ");

	if (tcw.ScanOrder==0)
		strcat(str, " TW");

	if (tcw.MipMapped)
		strcat(str, " MM");

	if (tcw.StrideSel)
		strcat(str, " Stride");

	sprintf(str + strlen(str), " %dx%d @ 0x%X", 8 << tsp.TexU, 8 << tsp.TexV, tcw.TexAddr << 3);
	std::string id = GetId();
	sprintf(str + strlen(str), " id=%s", id.c_str());
	DEBUG_LOG(RENDERER, "%s", str);
}

//true if : dirty or paletted texture and hashes don't match
bool BaseTextureCacheData::NeedsUpdate() {
	bool rc = dirty
			|| (tcw.PixelFmt == PixelPal4 && palette_hash != pal_hash_16[tcw.PalSelect])
			|| (tcw.PixelFmt == PixelPal8 && palette_hash != pal_hash_256[tcw.PalSelect >> 4]);
	return rc;
}

bool BaseTextureCacheData::Delete()
{
	if (custom_load_in_progress > 0)
		return false;

	if (lock_block)
		libCore_vramlock_Unlock_block(lock_block);
	lock_block = nullptr;

	delete[] custom_image_data;

	return true;
}

void BaseTextureCacheData::Create()
{
	//Reset state info ..
	Lookups = 0;
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

	if (tcw.ScanOrder && (tex->PL || tex->PL32))
	{
		//Texture is stored 'planar' in memory, no deswizzle is needed
		//verify(tcw.VQ_Comp==0);
		if (tcw.VQ_Comp != 0)
			WARN_LOG(RENDERER, "Warning: planar texture with VQ set (invalid)");

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
		tex_type = PAL_TYPE[PAL_RAM_CTRL&3];
		if (tex_type != TextureType::_565)
			has_alpha = true;

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
		if (sa + size > VRAM_SIZE)
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
	if (settings.rend.CustomTextures)
		custom_texture.LoadCustomTextureAsync(this);

	void *temp_tex_buffer = NULL;
	u32 upscaled_w = w;
	u32 upscaled_h = h;

	PixelBuffer<u16> pb16;
	PixelBuffer<u32> pb32;

	// Figure out if we really need to use a 32-bit pixel buffer
	bool textureUpscaling = settings.rend.TextureUpscale > 1
			// Don't process textures that are too big
			&& w * h <= settings.rend.MaxFilteredTextureSize * settings.rend.MaxFilteredTextureSize
			// Don't process YUV textures
			&& tcw.PixelFmt != PixelYUV;
	bool need_32bit_buffer = true;
	if (!textureUpscaling
		&& (!IsPaletted() || tex_type != TextureType::_8888)
		&& texconv != NULL
		&& !Force32BitTexture(tex_type))
		need_32bit_buffer = false;
	// TODO avoid upscaling/depost. textures that change too often

	bool mipmapped = IsMipmapped() && !settings.rend.DumpTextures;

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
			for (u32 i = 0; i <= tsp.TexU + 3; i++)
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
				texconv32(&pb32, (u8*)&vram[vram_addr], 1 << i, 1 << i);
			}
			pb32.set_mipmap(0);
		}
		else
		{
			pb32.init(w, h);
			texconv32(&pb32, (u8*)&vram[sa], stride, h);

#ifdef DEPOSTERIZE
			{
				// Deposterization
				PixelBuffer<u32> tmp_buf;
				tmp_buf.init(w, h);

				DePosterize(pb32.data(), tmp_buf.data(), w, h);
				pb32.steal_data(tmp_buf);
			}
#endif

			// xBRZ scaling
			if (textureUpscaling)
			{
				PixelBuffer<u32> tmp_buf;
				tmp_buf.init(w * settings.rend.TextureUpscale, h * settings.rend.TextureUpscale);

				if (tcw.PixelFmt == Pixel1555 || tcw.PixelFmt == Pixel4444)
					// Alpha channel formats. Palettes with alpha are already handled
					has_alpha = true;
				UpscalexBRZ(settings.rend.TextureUpscale, pb32.data(), tmp_buf.data(), w, h, has_alpha);
				pb32.steal_data(tmp_buf);
				upscaled_w *= settings.rend.TextureUpscale;
				upscaled_h *= settings.rend.TextureUpscale;
			}
		}
		temp_tex_buffer = pb32.data();
	}
	else if (texconv != NULL)
	{
		if (mipmapped)
		{
			pb16.init(w, h, true);
			for (u32 i = 0; i <= tsp.TexU + 3; i++)
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
	lock_block = libCore_vramlock_Lock(sa_tex,sa+size-1,this);

	UploadToGPU(upscaled_w, upscaled_h, (u8*)temp_tex_buffer, mipmapped, mipmapped);
	if (settings.rend.DumpTextures)
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
		delete [] custom_image_data;
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

	u32 addr = SPG_CONTROL.interlace && !SPG_STATUS.fieldnum ? FB_R_SOF2 : FB_R_SOF1;

	pb.init(width, height);
	u8 *dst = (u8*)pb.data();

	switch (FB_R_CTRL.fb_depth)
	{
		case fbde_0555:    // 555 RGB
			for (int y = 0; y < height; y++)
			{
				for (int i = 0; i < width; i++)
				{
					u16 src = pvr_read_area1_16(addr);
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
					u16 src = pvr_read_area1_16(addr);
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
					u32 src = pvr_read_area1_32(addr);
					*dst++ = src >> 16;
					*dst++ = src >> 8;
					*dst++ = src;
					*dst++ = 0xFF;
					addr += 4;
					if (i + 1 >= width)
						break;
					u32 src2 = pvr_read_area1_32(addr);
					*dst++ = src2 >> 8;
					*dst++ = src2;
					*dst++ = src >> 24;
					*dst++ = 0xFF;
					addr += 4;
					if (i + 2 >= width)
						break;
					u32 src3 = pvr_read_area1_32(addr);
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
					u32 src = pvr_read_area1_32(addr);
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

void rend_text_invl(vram_block* bl)
{
	BaseTextureCacheData* tcd = (BaseTextureCacheData*)bl->userdata;
	tcd->dirty = FrameCount;
	tcd->lock_block = nullptr;

	libCore_vramlock_Unlock_block_wb(bl);
}

static u8* loadPNGData(const u8 *header, png_voidp io_ptr, png_rw_ptr read_func, int &width, int &height)
{
	//test if png
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png)
	{
		WARN_LOG(RENDERER, "Passed data isn't a PNG file");
		return NULL;
	}

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
	{
		WARN_LOG(RENDERER, "Unable to create PNG struct");
		return NULL;
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		WARN_LOG(RENDERER, "Unable to create PNG info");
		return NULL;
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		WARN_LOG(RENDERER, "Unable to create PNG end info");
		return NULL;
	}

	//png error stuff, not sure libpng man suggests this.
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		WARN_LOG(RENDERER, "Error during setjmp");
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	//init png reading
	//png_init_io(png_ptr, fp);
	png_set_read_fn(png_ptr, io_ptr, read_func);

	//let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);

	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);

	//variables to pass to get info
	int bit_depth, color_type;
	png_uint_32 twidth, theight;

	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type,
		NULL, NULL, NULL);

	//update width and height based on png info
	width = twidth;
	height = theight;

	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);

	// Row size in bytes.
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	// Allocate the image_data as a big block, to be given to opengl
	png_byte *image_data = new png_byte[rowbytes * height];
	if (!image_data)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		WARN_LOG(RENDERER, "Unable to allocate image_data");
		return NULL;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep *row_pointers = new png_bytep[height];
	if (!row_pointers)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		delete[] image_data;
		WARN_LOG(RENDERER, "Unable to allocate row_pointer");
		return NULL;
	}

	// set the individual row_pointers to point at the correct offsets of image_data
	for (int i = 0; i < height; ++i)
		row_pointers[height - 1 - i] = image_data + i * rowbytes;

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	delete[] row_pointers;

	return image_data;
}

static size_t png_offset;

static void png_read_vector(png_structp png_ptr, png_bytep data, png_size_t length)
{
	const std::vector<u8> *v = (const std::vector<u8> *)png_get_io_ptr(png_ptr);
	memcpy(data, v->data() + png_offset, length);
	png_offset += length;
}

u8* loadPNGData(const std::vector<u8>& data, int &width, int &height)
{
	png_offset = 8;
	return loadPNGData(data.data(), (void *)&data, png_read_vector, width, height);
}

static void png_cstd_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
	if (fread(data, 1, length, (FILE *)png_get_io_ptr(png_ptr)) != length)
		png_error(png_ptr, "Truncated read error");
}

u8* loadPNGData(const std::string& fname, int &width, int &height)
{
	const char* filename=fname.c_str();
	FILE* file = fopen(filename, "rb");

	if (!file)
	{
		INFO_LOG(COMMON, "Error opening %s", filename);
		return NULL;
	}

	//header for testing if it is a png
	png_byte header[8];

	//read the header
	if (fread(header, 1, 8, file) != 8)
	{
		fclose(file);
		WARN_LOG(RENDERER, "Not a PNG file : %s", filename);
		return NULL;
	}
	u8 *data = loadPNGData(header, file, png_cstd_read, width, height);
	fclose(file);

	return data;
}

#ifdef TEST_AUTOMATION
void dump_screenshot(u8 *buffer, u32 width, u32 height, bool alpha, u32 rowPitch, bool invertY)
{
	FILE *fp = fopen("screenshot.png", "wb");
	if (fp == NULL)
	{
		ERROR_LOG(RENDERER, "Failed to open screenshot.png for writing");
		return;
	}

	png_bytepp rows = (png_bytepp)malloc(height * sizeof(png_bytep));
	for (int y = 0; y < height; y++)
	{
		rows[invertY ? (height - y - 1) : y] = (png_bytep)buffer + y * (rowPitch ? rowPitch : width * (alpha ? 4 : 3));
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);

	png_init_io(png_ptr, fp);


	// write header
	png_set_IHDR(png_ptr, info_ptr, width, height,
			 8, alpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);


	// write bytes
	png_write_image(png_ptr, rows);

	// end write
	png_write_end(png_ptr, NULL);
	fclose(fp);

	free(rows);

}
#endif

