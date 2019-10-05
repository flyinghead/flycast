#include <memory>
#include <unordered_map>
#ifndef TARGET_NO_OPENMP
#include <omp.h>
#endif

#include "TexCache.h"
#include "hw/pvr/pvr_regs.h"
#include "hw/pvr/pvr_mem.h"
#include "hw/pvr/Renderer_if.h"
#include "hw/mem/_vmem.h"
#include "hw/mem/vmem32.h"
#include "hw/sh4/modules/mmu.h"
#include "deps/xbrz/xbrz.h"
#include <xxhash.h>
#include "CustomTexture.h"

u8* vq_codebook;
u32 palette_index;
bool KillTex=false;
u32 palette16_ram[1024];
u32 palette32_ram[1024];
u32 pal_hash_256[4];
u32 pal_hash_16[64];

u32 detwiddle[2][8][1024];
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
	for (u32 s=0;s<8;s++)
	{
		u32 x_sz=1024;
		u32 y_sz=8<<s;
		for (u32 i=0;i<x_sz;i++)
		{
			detwiddle[0][s][i]=twiddle_slow(i,0,x_sz,y_sz);
			detwiddle[1][s][i]=twiddle_slow(0,i,y_sz,x_sz);
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


using namespace std;

vector<vram_block*> VramLocks[VRAM_SIZE_MAX / PAGE_SIZE];
VArray2 vram;  // vram 32-64b

//List functions
//
void vramlock_list_remove(vram_block* block)
{
	u32 base = block->start / PAGE_SIZE;
	u32 end = block->end / PAGE_SIZE;

	for (u32 i = base; i <= end; i++)
	{
		vector<vram_block*>& list = VramLocks[i];
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
		vector<vram_block*>& list = VramLocks[i];
		// If the list is empty then we need to protect vram, otherwise it's already been done
		if (list.empty())
		{
			_vmem_protect_vram(i * PAGE_SIZE, PAGE_SIZE);
		}
		else
		{
			for (u32 j = 0; j < list.size(); j++)
			{
				if (list[j] == nullptr)
				{
					list[j] = block;
					goto added_it;
				}
			}
		}

		list.push_back(block);
added_it:
		i=i;
	}
}
 
cMutex vramlist_lock;

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
		vramlist_lock.Lock();

		// This also protects vram if needed
		vramlock_list_add(block);

		vramlist_lock.Unlock();
	}

	return block;
}

bool VramLockedWriteOffset(size_t offset)
{
	if (offset >= VRAM_SIZE)
		return false;

	size_t addr_hash = offset / PAGE_SIZE;
	vector<vram_block *>& list = VramLocks[addr_hash];

	{
		vramlist_lock.Lock();

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

		vramlist_lock.Unlock();
	}

	return true;
}

bool VramLockedWrite(u8* address)
{
	u32 offset = _vmem_get_vram_offset(address);
	if (offset == -1)
		return false;
	return VramLockedWriteOffset(offset);
}

//unlocks mem
//also frees the handle
void libCore_vramlock_Unlock_block(vram_block* block)
{
	vramlist_lock.Lock();
	libCore_vramlock_Unlock_block_wb(block);
	vramlist_lock.Unlock();
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
	return min(tcount, (int)settings.pvr.MaxThreads);
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
	{"bumpmap", 16, TextureType::_4444,        texBMP_PL,	texBMP_TW,	 texBMP_VQ, 	NULL},											//bump map
	{"pal4", 	4,	TextureType::_5551,		   0,			texPAL4_TW,  texPAL4_VQ, 	NULL, 		   texPAL4_TW32,  texPAL4_VQ32 },	//pal4
	{"pal8", 	8,	TextureType::_5551,		   0,			texPAL8_TW,  texPAL8_VQ, 	NULL, 		   texPAL8_TW32,  texPAL8_VQ32 },	//pal8
	{"ns/1555", 0},																														// Not supported (1555)
};

static const u32 MipPoint[8] =
{
	0x00006,//8
	0x00016,//16
	0x00056,//32
	0x00156,//64
	0x00556,//128
	0x01556,//256
	0x05556,//512
	0x15556//1024
};

static const TextureType PAL_TYPE[4] = {
	TextureType::_5551, TextureType::_565, TextureType::_4444, TextureType::_8888
};

static CustomTexture custom_texture;

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
	lock_block=0;

	delete[] custom_image_data;

	return true;
}

void BaseTextureCacheData::Create()
{
	//Reset state info ..
	Lookups=0;
	Updates=0;
	dirty=FrameCount;
	lock_block = nullptr;
	custom_image_data = nullptr;

	//decode info from tsp/tcw into the texture struct
	tex=&format[tcw.PixelFmt == PixelReserved ? Pixel1555 : tcw.PixelFmt];	//texture format table entry

	sa_tex = (tcw.TexAddr<<3) & VRAM_MASK;	//texture start address
	sa = sa_tex;							//data texture start address (modified for MIPs, as needed)
	w=8<<tsp.TexU;                   //tex width
	h=8<<tsp.TexV;                   //tex height

	//PAL texture
	if (tex->bpp == 4)
		palette_index = tcw.PalSelect << 4;
	else if (tex->bpp == 8)
		palette_index = (tcw.PalSelect >> 4) << 8;

	//VQ table (if VQ tex)
	if (tcw.VQ_Comp)
		vq_codebook = sa;

	//Convert a pvr texture into OpenGL
	switch (tcw.PixelFmt)
	{

	case Pixel1555: 	//0     1555 value: 1 bit; RGB values: 5 bits each
	case PixelReserved: //7     Reserved        Regarded as 1555
	case Pixel565: 		//1     565      R value: 5 bits; G value: 6 bits; B value: 5 bits
	case Pixel4444: 	//2     4444 value: 4 bits; RGB values: 4 bits each
	case PixelYUV:		//3     YUV422 32 bits per 2 pixels; YUYV values: 8 bits each
	case PixelBumpMap:	//4		Bump Map 	16 bits/pixel; S value: 8 bits; R value: 8 bits
	case PixelPal4:		//5     4 BPP Palette   Palette texture with 4 bits/pixel
	case PixelPal8:		//6     8 BPP Palette   Palette texture with 8 bits/pixel
		if (tcw.ScanOrder && (tex->PL || tex->PL32))
		{
			//Texture is stored 'planar' in memory, no deswizzle is needed
			//verify(tcw.VQ_Comp==0);
			if (tcw.VQ_Comp != 0)
				WARN_LOG(RENDERER, "Warning: planar texture with VQ set (invalid)");

			//Planar textures support stride selection, mostly used for non power of 2 textures (videos)
			int stride = w;
			if (tcw.StrideSel)
			{
				stride = std::max((TEXT_CONTROL & 31) * 32, w);
			}
			//Call the format specific conversion code
			texconv = tex->PL;
			texconv32 = tex->PL32;
			//calculate the size, in bytes, for the locking
			size=stride*h*tex->bpp/8;
		}
		else
		{
			// Quake 3 Arena uses one. Not sure if valid but no need to crash
			//verify(w==h || !tcw.MipMapped); // are non square mipmaps supported ? i can't recall right now *WARN*

			if (tcw.VQ_Comp)
			{
				verify(tex->VQ != NULL || tex->VQ32 != NULL);
				vq_codebook = sa;
				if (tcw.MipMapped)
					sa+=MipPoint[tsp.TexU];
				texconv = tex->VQ;
				texconv32 = tex->VQ32;
				size=w*h/8;
			}
			else
			{
				verify(tex->TW != NULL || tex->TW32 != NULL);
				if (tcw.MipMapped)
					sa+=MipPoint[tsp.TexU]*tex->bpp/2;
				texconv = tex->TW;
				texconv32 = tex->TW32;
				size=w*h*tex->bpp/8;
			}
		}
		break;
	default:
		WARN_LOG(RENDERER, "Unhandled texture format %d", tcw.PixelFmt);
		size=w*h*2;
		texconv = NULL;
		texconv32 = NULL;
	}
}

void BaseTextureCacheData::ComputeHash()
{
	texture_hash = XXH32(&vram[sa], size, 7);
	if (IsPaletted())
		texture_hash ^= palette_hash;
	old_texture_hash = texture_hash;
	texture_hash ^= tcw.full;
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
		if (tex_type == TextureType::_8888)
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
	u32 stride=w;

	if (tcw.StrideSel && tcw.ScanOrder && (tex->PL || tex->PL32))
		stride = std::max(w, (TEXT_CONTROL & 31) * 32);

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
	bool need_32bit_buffer = true;
	if ((settings.rend.TextureUpscale <= 1
			|| w * h > settings.rend.MaxFilteredTextureSize
				* settings.rend.MaxFilteredTextureSize		// Don't process textures that are too big
			|| tcw.PixelFmt == PixelYUV)					// Don't process YUV textures
		&& (!IsPaletted() || tex_type != TextureType::_8888)
		&& texconv != NULL)
		need_32bit_buffer = false;
	// TODO avoid upscaling/depost. textures that change too often

	if (texconv32 != NULL && need_32bit_buffer)
	{
		// Force the texture type since that's the only 32-bit one we know
		tex_type = TextureType::_8888;

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
		if (settings.rend.TextureUpscale > 1)
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
		temp_tex_buffer = pb32.data();
	}
	else if (texconv != NULL)
	{
		pb16.init(w, h);

		texconv(&pb16,(u8*)&vram[sa],stride,h);
		temp_tex_buffer = pb16.data();
	}
	else
	{
		//fill it in with a temp color
		WARN_LOG(RENDERER, "UNHANDLED TEXTURE");
		pb16.init(w, h);
		memset(pb16.data(), 0x80, w * h * 2);
		temp_tex_buffer = pb16.data();
	}
	// Restore the original texture height if it was constrained to VRAM limits above
	h = original_h;

	//lock the texture to detect changes in it
	lock_block = libCore_vramlock_Lock(sa_tex,sa+size-1,this);

	UploadToGPU(upscaled_w, upscaled_h, (u8*)temp_tex_buffer);
	if (settings.rend.DumpTextures)
	{
		ComputeHash();
		custom_texture.DumpTexture(texture_hash, upscaled_w, upscaled_h, tex_type, temp_tex_buffer);
	}
	PrintTextureName();
}

void BaseTextureCacheData::CheckCustomTexture()
{
	if (custom_load_in_progress == 0 && custom_image_data != NULL)
	{
		tex_type = TextureType::_8888;
		UploadToGPU(custom_width, custom_height, custom_image_data);
		delete [] custom_image_data;
		custom_image_data = NULL;
	}
}

static std::unordered_map<u64, std::unique_ptr<BaseTextureCacheData>> TexCache;
typedef std::unordered_map<u64, std::unique_ptr<BaseTextureCacheData>>::iterator TexCacheIter;

// Only use TexU and TexV from TSP in the cache key
//     TexV : 7, TexU : 7
static const TSP TSPTextureCacheMask = { { 7, 7 } };
//     TexAddr : 0x1FFFFF, Reserved : 0, StrideSel : 0, ScanOrder : 1, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1
static const TCW TCWTextureCacheMask = { { 0x1FFFFF, 0, 0, 1, 7, 1, 1 } };

BaseTextureCacheData *getTextureCacheData(TSP tsp, TCW tcw, BaseTextureCacheData *(*factory)())
{
	u64 key = tsp.full & TSPTextureCacheMask.full;
	if (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
		// Paletted textures have a palette selection that must be part of the key
		// We also add the palette type to the key to avoid thrashing the cache
		// when the palette type is changed. If the palette type is changed back in the future,
		// this texture will stil be available.
		key |= ((u64)tcw.full << 32) | ((PAL_RAM_CTRL & 3) << 6);
	else
		key |= (u64)(tcw.full & TCWTextureCacheMask.full) << 32;

	TexCacheIter tx = TexCache.find(key);

	BaseTextureCacheData* tf;
	if (tx != TexCache.end())
	{
		tf = tx->second.get();
		// Needed if the texture is updated
		tf->tcw.StrideSel = tcw.StrideSel;
	}
	else //create if not existing
	{
		tf = factory();
		TexCache[key] = std::unique_ptr<BaseTextureCacheData>(tf);

		tf->tsp = tsp;
		tf->tcw = tcw;
	}

	return tf;
}

void CollectCleanup()
{
	vector<u64> list;

	u32 TargetFrame = max((u32)120,FrameCount) - 120;

	for (const auto& pair : TexCache)
	{
		if (pair.second->dirty && pair.second->dirty < TargetFrame)
			list.push_back(pair.first);

		if (list.size() > 5)
			break;
	}

	for (u64 id : list) {
		if (TexCache[id]->Delete())
		{
			//printf("Deleting %d\n", TexCache[list[i]].texID);
			TexCache.erase(id);
		}
	}
}

void killtex()
{
	for (auto& pair : TexCache)
		pair.second->Delete();

	TexCache.clear();
	KillTex = false;
	INFO_LOG(RENDERER, "Texture cache cleared");
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
					*dst++ = (((src >> 5) & 0x3F) << 2) + (FB_R_CTRL.fb_concat >> 1);
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
