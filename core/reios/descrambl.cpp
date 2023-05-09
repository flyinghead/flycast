/*
	Based on work of Marcus Comstedt
	http://mc.pp.se, http://mc.pp.se/dc/sw.html, http://mc.pp.se/dc/files/scramble.c
	License: Gotta verify

	Adapted by Stefanos Kornilios Mitsis Poiitidis (skmp) for reicast
*/

#include "descrambl.h"

#include <cstring>
#include <utility>

#define MAXCHUNK (2048*1024)

static u32 seed;

static void my_srand(u32 n)
{
	seed = n & 0xffff;
}

static u32 my_rand()
{
	seed = (seed * 2109 + 9273) & 0x7fff;
	return (seed + 0xc000) & 0xffff;
}

static void load_chunk(const u8* &src, u8 *ptr, u32 sz)
{
	verify(sz <= MAXCHUNK);

	static int idx[MAXCHUNK / 32];

	/* Convert chunk size to number of slices */
	sz /= 32;

	/* Initialize index table with unity,
	so that each slice gets loaded exactly once */
	for (u32 i = 0; i < sz; i++)
		idx[i] = i;

	for (int i = sz - 1; i >= 0; --i)
	{
		/* Select a replacement index */
		int x = (my_rand() * i) >> 16;

		/* Swap */
		std::swap(idx[i], idx[x]);

		/* Load resulting slice */
		memcpy(ptr + 32 * idx[i], src, 32);
		src += 32;
	}
}

void descrambl_buffer(const u8 *src, u8 *dst, u32 size)
{
	u32 chunksz;

	my_srand(size);

	/* Descramble 2 meg blocks for as long as possible, then
	gradually reduce the window down to 32 bytes (1 slice) */
	for (chunksz = MAXCHUNK; chunksz >= 32; chunksz >>= 1)
		while (size >= chunksz)
		{
			load_chunk(src, dst, chunksz);
			size -= chunksz;
			dst += chunksz;
		}

	/* Load final incomplete slice */
	if (size)
		memcpy(dst, src, size);
}
