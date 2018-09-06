#include <list>
#include <functional>
#ifndef TARGET_NO_OPENMP
#include <omp.h>
#endif

#include "TexCache.h"
#include "hw/pvr/pvr_regs.h"
#include "hw/mem/_vmem.h"
#include "deps/xbrz/xbrz.h"

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


u32 twiddle_slow(u32 x,u32 y,u32 x_sz,u32 y_sz)
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

void BuildTwiddleTables()
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

// FNV-1a hashing algorithm
#define HASH_OFFSET 2166136261
#define HASH_PRIME 16777619

#define HASH_PALETTE(palette_hash, bpp)	do { u32 &hash = palette_hash[i >> bpp]; \
		if ((i & ((1 << bpp) - 1)) == 0) \
			hash = HASH_OFFSET; \
		u8 *p = (u8 *)&palette32_ram[i]; \
		hash = (hash ^ p[0]) * HASH_PRIME; \
		hash = (hash ^ p[1]) * HASH_PRIME; \
		hash = (hash ^ p[2]) * HASH_PRIME; \
		hash = (hash ^ p[3]) * HASH_PRIME; } while (false)
#define HASH_PALETTE_16() HASH_PALETTE(pal_hash_16, 4)
#define HASH_PALETTE_256() HASH_PALETTE(pal_hash_256, 8)

void palette_update()
{
	if (pal_needs_update==false)
		return;

	pal_needs_update=false;
	switch(PAL_RAM_CTRL&3)
	{
	case 0:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB1555(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB1555_32(PALETTE_RAM[i]);
			HASH_PALETTE_16();
			HASH_PALETTE_256();
		}
		break;

	case 1:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB565(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB565_32(PALETTE_RAM[i]);
			HASH_PALETTE_16();
			HASH_PALETTE_256();
		}
		break;

	case 2:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB4444(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB4444_32(PALETTE_RAM[i]);
			HASH_PALETTE_16();
			HASH_PALETTE_256();
		}
		break;

	case 3:
		for (int i=0;i<1024;i++)
		{
			palette16_ram[i] = ARGB8888(PALETTE_RAM[i]);
			palette32_ram[i] = ARGB8888_32(PALETTE_RAM[i]);
			HASH_PALETTE_16();
			HASH_PALETTE_256();
		}
		break;
	}

}


using namespace std;

vector<vram_block*> VramLocks[VRAM_SIZE/PAGE_SIZE];
//vram 32-64b
VArray2 vram;

//List functions
//
void vramlock_list_remove(vram_block* block)
{
	u32 base = block->start/PAGE_SIZE;
	u32 end = block->end/PAGE_SIZE;

	for (u32 i=base;i<=end;i++)
	{
		vector<vram_block*>* list=&VramLocks[i];
		for (size_t j=0;j<list->size();j++)
		{
			if ((*list)[j]==block)
			{
				(*list)[j]=0;
			}
		}
	}
}
 
void vramlock_list_add(vram_block* block)
{
	u32 base = block->start/PAGE_SIZE;
	u32 end = block->end/PAGE_SIZE;


	for (u32 i=base;i<=end;i++)
	{
		vector<vram_block*>* list=&VramLocks[i];
		for (u32 j=0;j<list->size();j++)
		{
			if ((*list)[j]==0)
			{
				(*list)[j]=block;
				goto added_it;
			}
		}

		list->push_back(block);
added_it:
		i=i;
	}
}
 
cMutex vramlist_lock;

//simple IsInRange test
inline bool IsInRange(vram_block* block,u32 offset)
{
	return (block->start<=offset) && (block->end>=offset);
}


vram_block* libCore_vramlock_Lock(u32 start_offset64,u32 end_offset64,void* userdata)
{
	vram_block* block=(vram_block* )malloc(sizeof(vram_block));
 
	if (end_offset64>(VRAM_SIZE-1))
	{
		msgboxf("vramlock_Lock_64: end_offset64>(VRAM_SIZE-1) \n Tried to lock area out of vram , possibly bug on the pvr plugin",MBX_OK);
		end_offset64=(VRAM_SIZE-1);
	}

	if (start_offset64>end_offset64)
	{
		msgboxf("vramlock_Lock_64: start_offset64>end_offset64 \n Tried to lock negative block , possibly bug on the pvr plugin",MBX_OK);
		start_offset64=0;
	}

	

	block->end=end_offset64;
	block->start=start_offset64;
	block->len=end_offset64-start_offset64+1;
	block->userdata=userdata;
	block->type=64;

	{
		vramlist_lock.Lock();
	
		vram.LockRegion(block->start,block->len);

		//TODO: Fix this for 32M wrap as well
		if (_nvmem_enabled() && VRAM_SIZE == 0x800000) {
			vram.LockRegion(block->start + VRAM_SIZE, block->len);
		}
		
		vramlock_list_add(block);
		
		vramlist_lock.Unlock();
	}

	return block;
}


bool VramLockedWrite(u8* address)
{
	size_t offset=address-vram.data;

	if (offset<VRAM_SIZE)
	{

		size_t addr_hash = offset/PAGE_SIZE;
		vector<vram_block*>* list=&VramLocks[addr_hash];
		
		{
			vramlist_lock.Lock();

			for (size_t i=0;i<list->size();i++)
			{
				if ((*list)[i])
				{
					libPvr_LockedBlockWrite((*list)[i],(u32)offset);
				
					if ((*list)[i])
					{
						msgboxf("Error : pvr is supposed to remove lock",MBX_OK);
						dbgbreak;
					}

				}
			}
			list->clear();

			vram.UnLockRegion((u32)offset&(~(PAGE_SIZE-1)),PAGE_SIZE);

			//TODO: Fix this for 32M wrap as well
			if (_nvmem_enabled() && VRAM_SIZE == 0x800000) {
				vram.UnLockRegion((u32)offset&(~(PAGE_SIZE-1)) + VRAM_SIZE,PAGE_SIZE);
			}
			
			vramlist_lock.Unlock();
		}

		return true;
	}
	else
		return false;
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
		//VRAM_SIZE/PAGE_SIZE;
	if (block->end>VRAM_SIZE)
		msgboxf("Error : block end is after vram , skipping unlock",MBX_OK);
	else
	{
		vramlock_list_remove(block);
		//more work needed
		free(block);
	}
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
void parallelize(const std::function<void(int,int)> &func, int start, int end, int width /* = 0 */)
{
	int tcount = omp_get_num_procs() - 1;
	if (tcount < 1)
		tcount = 1;
	tcount = min(tcount, (int)settings.pvr.MaxThreads);
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

	parallelize(std::bind(&deposterizeH, source, tmpbuf, width, std::placeholders::_1, std::placeholders::_2), 0, height, width);
	parallelize(std::bind(&deposterizeV, tmpbuf, dest, width, height, std::placeholders::_1, std::placeholders::_2), 0, height, width);
	parallelize(std::bind(&deposterizeH, dest, tmpbuf, width, std::placeholders::_1, std::placeholders::_2), 0, height, width);
	parallelize(std::bind(&deposterizeV, tmpbuf, dest, width, height, std::placeholders::_1, std::placeholders::_2), 0, height, width);

	free(tmpbuf);
}
#endif

struct xbrz::ScalerCfg xbrz_cfg;

void UpscalexBRZ(int factor, u32* source, u32* dest, int width, int height, bool has_alpha) {
#ifndef TARGET_NO_OPENMP
	parallelize(
			std::bind(&xbrz::scale, factor, source, dest, width, height, has_alpha ? xbrz::ColorFormat::ARGB : xbrz::ColorFormat::RGB, xbrz_cfg,
					std::placeholders::_1, std::placeholders::_2), 0, height, width);
#else
	xbrz::scale(factor, source, dest, width, height, has_alpha ? xbrz::ColorFormat::ARGB : xbrz::ColorFormat::RGB, xbrz_cfg);
#endif
}
