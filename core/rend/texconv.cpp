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
#include "texconv.h"
#include "cfg/option.h"
#include "hw/pvr/Renderer_if.h"
#include <algorithm>
#include <xxhash.h>

const u8 *vq_codebook;
u32 palette_index;
u32 palette16_ram[1024];
u32 palette32_ram[1024];
u32 pal_hash_256[4];
u32 pal_hash_16[64];
extern bool pal_needs_update;

u32 detwiddle[2][11][1024];
//input : address in the yyyyyxxxxx format
//output : address in the xyxyxyxy format
//U : x resolution , V : y resolution
//twiddle works on 64b words

static u32 twiddle_slow(u32 x, u32 y, u32 x_sz, u32 y_sz)
{
	u32 rv = 0; // low 2 bits are directly passed  -> needs some misc stuff to work.However
				// Pvr internally maps the 64b banks "as if" they were twiddled

	u32 sh = 0;
	x_sz >>= 1;
	y_sz >>= 1;
	while (x_sz != 0 || y_sz != 0)
	{
		if (y_sz != 0)
		{
			u32 temp = y & 1;
			rv |= temp << sh;

			y_sz >>= 1;
			y >>= 1;
			sh++;
		}
		if (x_sz != 0)
		{
			u32 temp = x & 1;
			rv |= temp << sh;

			x_sz >>= 1;
			x >>= 1;
			sh++;
		}
	}
	return rv;
}

static OnLoad _([]() {
	constexpr u32 x_sz = 1024;
	for (u32 s = 0; s < 11; s++)
	{
		u32 y_sz = 1 << s;
		for (u32 i = 0; i < x_sz; i++) {
			detwiddle[0][s][i] = twiddle_slow(i, 0, x_sz, y_sz);
			detwiddle[1][s][i] = twiddle_slow(0, i, y_sz, x_sz);
		}
	}
});

void palette_update()
{
	if (!pal_needs_update)
		return;
	pal_needs_update = false;
	rend_updatePalette();

	if (!isDirectX(config::RendererType))
	{
		switch (PAL_RAM_CTRL & 3)
		{
		case 0:
			for (int i = 0; i < 1024; i++) {
				palette16_ram[i] = Unpacker1555::unpack(PALETTE_RAM[i]);
				palette32_ram[i] = Unpacker1555_32<RGBAPacker>::unpack(PALETTE_RAM[i]);
			}
			break;

		case 1:
			for (int i = 0; i < 1024; i++) {
				palette16_ram[i] = UnpackerNop<u16>::unpack(PALETTE_RAM[i]);
				palette32_ram[i] = Unpacker565_32<RGBAPacker>::unpack(PALETTE_RAM[i]);
			}
			break;

		case 2:
			for (int i = 0; i < 1024; i++) {
				palette16_ram[i] = Unpacker4444::unpack(PALETTE_RAM[i]);
				palette32_ram[i] = Unpacker4444_32<RGBAPacker>::unpack(PALETTE_RAM[i]);
			}
			break;

		case 3:
			for (int i = 0; i < 1024; i++)
				palette32_ram[i] = Unpacker8888<RGBAPacker>::unpack(PALETTE_RAM[i]);
			break;
		}
	}
	else
	{
		switch (PAL_RAM_CTRL & 3)
		{
		case 0:
			for (int i = 0; i < 1024; i++) {
				palette16_ram[i] = UnpackerNop<u16>::unpack(PALETTE_RAM[i]);
				palette32_ram[i] = Unpacker1555_32<BGRAPacker>::unpack(PALETTE_RAM[i]);
			}
			break;

		case 1:
			for (int i = 0; i < 1024; i++) {
				palette16_ram[i] = UnpackerNop<u16>::unpack(PALETTE_RAM[i]);
				palette32_ram[i] = Unpacker565_32<BGRAPacker>::unpack(PALETTE_RAM[i]);
			}
			break;

		case 2:
			for (int i = 0; i < 1024; i++) {
				palette16_ram[i] = UnpackerNop<u16>::unpack(PALETTE_RAM[i]);
				palette32_ram[i] = Unpacker4444_32<BGRAPacker>::unpack(PALETTE_RAM[i]);
			}
			break;

		case 3:
			for (int i = 0; i < 1024; i++)
				palette32_ram[i] = UnpackerNop<u32>::unpack(PALETTE_RAM[i]);
			break;
		}
	}
	for (std::size_t i = 0; i < std::size(pal_hash_16); i++)
		pal_hash_16[i] = XXH32(&PALETTE_RAM[i << 4], 16 * 4, 7);
	for (std::size_t i = 0; i < std::size(pal_hash_256); i++)
		pal_hash_256[i] = XXH32(&PALETTE_RAM[i << 8], 256 * 4, 7);
}

template<typename Packer>
inline static u32 YUV422(s32 Y, s32 Yu, s32 Yv)
{
	Yu -= 128;
	Yv -= 128;

	s32 R = Y + Yv * 11 / 8;				// Y + (Yv-128) * (11/8) ?
	s32 G = Y - (Yu * 11 + Yv * 22) / 32;	// Y - (Yu-128) * (11/8) * 0.25 - (Yv-128) * (11/8) * 0.5 ?
	s32 B = Y + Yu * 110 / 64;				// Y + (Yu-128) * (11/8) * 1.25 ?

	return Packer::pack(std::clamp(R, 0, 255), std::clamp(G, 0, 255), std::clamp(B, 0, 255), 0xFF);
}

static u32 twop(u32 x, u32 y, u32 bcx, u32 bcy) {
	return detwiddle[0][bcy][x] + detwiddle[1][bcx][y];
}

template<typename Unpacker>
struct ConvertPlanar
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 4;
	static constexpr u32 ypp = 1;
	static void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u16 *p_in = (const u16 *)data;
		pb->prel(0, Unpacker::unpack(p_in[0]));
		pb->prel(1, Unpacker::unpack(p_in[1]));
		pb->prel(2, Unpacker::unpack(p_in[2]));
		pb->prel(3, Unpacker::unpack(p_in[3]));
	}
};

template<typename Packer>
struct ConvertPlanarYUV
{
	using unpacked_type = u32;
	static constexpr u32 xpp = 4;
	static constexpr u32 ypp = 1;
	static void Convert(PixelBuffer<u32> *pb, const u8 *data)
	{
		//convert 4x1 4444 to 4x1 8888
		const u32 *p_in = (const u32 *)data;


		s32 Y0 = (p_in[0] >> 8) & 255; //
		s32 Yu = (p_in[0] >> 0) & 255; //p_in[0]
		s32 Y1 = (p_in[0] >> 24) & 255; //p_in[3]
		s32 Yv = (p_in[0] >> 16) & 255; //p_in[2]

		//0,0
		pb->prel(0, YUV422<Packer>(Y0, Yu, Yv));
		//1,0
		pb->prel(1, YUV422<Packer>(Y1, Yu, Yv));

		//next 4 bytes
		p_in += 1;

		Y0 = (p_in[0] >> 8) & 255; //
		Yu = (p_in[0] >> 0) & 255; //p_in[0]
		Y1 = (p_in[0] >> 24) & 255; //p_in[3]
		Yv = (p_in[0] >> 16) & 255; //p_in[2]

		//0,0
		pb->prel(2, YUV422<Packer>(Y0, Yu, Yv));
		//1,0
		pb->prel(3, YUV422<Packer>(Y1, Yu, Yv));
	}
};

template<typename Unpacker>
struct ConvertTwiddle
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 2;
	static constexpr u32 ypp = 2;
	static void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u16 *p_in = (const u16 *)data;
		pb->prel(0, 0, Unpacker::unpack(p_in[0]));
		pb->prel(0, 1, Unpacker::unpack(p_in[1]));
		pb->prel(1, 0, Unpacker::unpack(p_in[2]));
		pb->prel(1, 1, Unpacker::unpack(p_in[3]));
	}
};

template<typename Packer>
struct ConvertTwiddleYUV
{
	using unpacked_type = u32;
	static constexpr u32 xpp = 2;
	static constexpr u32 ypp = 2;
	static void Convert(PixelBuffer<u32> *pb, const u8 *data)
	{
		//convert 4x1 4444 to 4x1 8888
		const u16* p_in = (const u16 *)data;

		s32 Y0 = (p_in[0] >> 8) & 255; //
		s32 Yu = (p_in[0] >> 0) & 255; //p_in[0]
		s32 Y1 = (p_in[2] >> 8) & 255; //p_in[3]
		s32 Yv = (p_in[2] >> 0) & 255; //p_in[2]

		//0,0
		pb->prel(0, 0, YUV422<Packer>(Y0, Yu, Yv));
		//1,0
		pb->prel(1, 0, YUV422<Packer>(Y1, Yu, Yv));

		//next 4 bytes
		//p_in+=2;

		Y0 = (p_in[1] >> 8) & 255; //
		Yu = (p_in[1] >> 0) & 255; //p_in[0]
		Y1 = (p_in[3] >> 8) & 255; //p_in[3]
		Yv = (p_in[3] >> 0) & 255; //p_in[2]

		//0,1
		pb->prel(0, 1, YUV422<Packer>(Y0, Yu, Yv));
		//1,1
		pb->prel(1, 1, YUV422<Packer>(Y1, Yu, Yv));
	}
};

template<typename Pixel>
struct UnpackerPalToRgb {
	using unpacked_type = Pixel;
	static Pixel unpack(u8 col)
	{
		u32 *pal = sizeof(Pixel) == 2 ? &palette16_ram[palette_index] : &palette32_ram[palette_index];
		return pal[col];
	}
};

template<typename Unpacker>
struct ConvertTwiddlePal4
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 4;
	static constexpr u32 ypp = 4;
	static void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u8 *p_in = data;

		pb->prel(0, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(0, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(1, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(1, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(0, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(0, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(1, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(1, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(2, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(2, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(3, 0, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(3, 1, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;

		pb->prel(2, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(2, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
		pb->prel(3, 2, Unpacker::unpack(p_in[0] & 0xF));
		pb->prel(3, 3, Unpacker::unpack((p_in[0] >> 4) & 0xF)); p_in++;
	}
};

template<typename Unpacker>
struct ConvertTwiddlePal8
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 2;
	static constexpr u32 ypp = 4;
	static void Convert(PixelBuffer<unpacked_type> *pb, const u8 *data)
	{
		const u8* p_in = (const u8 *)data;

		pb->prel(0, 0, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(0, 1, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 0, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 1, Unpacker::unpack(p_in[0])); p_in++;

		pb->prel(0, 2, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(0, 3, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 2, Unpacker::unpack(p_in[0])); p_in++;
		pb->prel(1, 3, Unpacker::unpack(p_in[0])); p_in++;
	}
};

//handler functions
template<typename PixelConvertor>
void texture_PL(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0,0);

	height /= PixelConvertor::ypp;
	width /= PixelConvertor::xpp;

	for (u32 y = 0; y < height; y++)
	{
		for (u32 x = 0; x < width; x++)
		{
			const u8* p = p_in;
			PixelConvertor::Convert(pb, p);
			p_in += 8;

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<typename PixelConvertor>
void texture_PLVQ(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0, 0);

	height /= PixelConvertor::ypp;
	width /= PixelConvertor::xpp;

	for (u32 y = 0; y < height; y++)
	{
		for (u32 x = 0; x < width; x++)
		{
			u8 p = *p_in++;
			PixelConvertor::Convert(pb, &vq_codebook[p * 8]);
			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<typename PixelConvertor>
void texture_TW(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0, 0);

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;

	const u32 bcx = bitscanrev(width);
	const u32 bcy = bitscanrev(height);

	for (u32 y = 0; y < height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < width; x += PixelConvertor::xpp)
		{
			const u8* p = &p_in[(twop(x, y, bcx, bcy) / divider) << 3];
			PixelConvertor::Convert(pb, p);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<typename PixelConvertor>
void texture_VQ(PixelBuffer<typename PixelConvertor::unpacked_type>* pb, const u8* p_in, u32 width, u32 height)
{
	pb->amove(0, 0);

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;
	const u32 bcx = bitscanrev(width);
	const u32 bcy = bitscanrev(height);

	for (u32 y = 0; y < height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < width; x += PixelConvertor::xpp)
		{
			u8 p = p_in[twop(x, y, bcx, bcy) / divider];
			PixelConvertor::Convert(pb, &vq_codebook[p * 8]);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

//Twiddle
const TexConvFP tex565_TW = texture_TW<ConvertTwiddle<UnpackerNop<u16>>>;
// Palette
const TexConvFP texPAL4_TW = texture_TW<ConvertTwiddlePal4<UnpackerPalToRgb<u16>>>;
const TexConvFP texPAL8_TW = texture_TW<ConvertTwiddlePal8<UnpackerPalToRgb<u16>>>;
const TexConvFP32 texPAL4_TW32 = texture_TW<ConvertTwiddlePal4<UnpackerPalToRgb<u32>>>;
const TexConvFP32 texPAL8_TW32 = texture_TW<ConvertTwiddlePal8<UnpackerPalToRgb<u32>>>;
const TexConvFP8 texPAL4PT_TW = texture_TW<ConvertTwiddlePal4<UnpackerNop<u8>>>;
const TexConvFP8 texPAL8PT_TW = texture_TW<ConvertTwiddlePal8<UnpackerNop<u8>>>;
//VQ
const TexConvFP tex565_VQ = texture_VQ<ConvertTwiddle<UnpackerNop<u16>>>;
// According to the documentation, a texture cannot be compressed and use
// a palette at the same time. However the hardware displays them
// just fine.
const TexConvFP texPAL4_VQ = texture_VQ<ConvertTwiddlePal4<UnpackerPalToRgb<u16>>>;
const TexConvFP texPAL8_VQ = texture_VQ<ConvertTwiddlePal8<UnpackerPalToRgb<u16>>>;
const TexConvFP32 texPAL4_VQ32 = texture_VQ<ConvertTwiddlePal4<UnpackerPalToRgb<u32>>>;
const TexConvFP32 texPAL8_VQ32 = texture_VQ<ConvertTwiddlePal8<UnpackerPalToRgb<u32>>>;

namespace opengl {
// OpenGL

//Planar
const TexConvFP32 texYUV422_PL = texture_PL<ConvertPlanarYUV<RGBAPacker>>;
const TexConvFP32 tex565_PL32 = texture_PL<ConvertPlanar<Unpacker565_32<RGBAPacker>>>;
const TexConvFP32 tex1555_PL32 = texture_PL<ConvertPlanar<Unpacker1555_32<RGBAPacker>>>;
const TexConvFP32 tex4444_PL32 = texture_PL<ConvertPlanar<Unpacker4444_32<RGBAPacker>>>;

const TexConvFP32 texYUV422_PLVQ = texture_PLVQ<ConvertPlanarYUV<RGBAPacker>>;
const TexConvFP32 tex565_PLVQ32 = texture_PLVQ<ConvertPlanar<Unpacker565_32<RGBAPacker>>>;
const TexConvFP32 tex1555_PLVQ32 = texture_PLVQ<ConvertPlanar<Unpacker1555_32<RGBAPacker>>>;
const TexConvFP32 tex4444_PLVQ32 = texture_PLVQ<ConvertPlanar<Unpacker4444_32<RGBAPacker>>>;

//Twiddle
const TexConvFP tex1555_TW = texture_TW<ConvertTwiddle<Unpacker1555>>;
const TexConvFP tex4444_TW = texture_TW<ConvertTwiddle<Unpacker4444>>;
const TexConvFP texBMP_TW = tex4444_TW;
const TexConvFP32 texYUV422_TW = texture_TW<ConvertTwiddleYUV<RGBAPacker>>;

const TexConvFP32 tex565_TW32 = texture_TW<ConvertTwiddle<Unpacker565_32<RGBAPacker>>>;
const TexConvFP32 tex1555_TW32 = texture_TW<ConvertTwiddle<Unpacker1555_32<RGBAPacker>>>;
const TexConvFP32 tex4444_TW32 = texture_TW<ConvertTwiddle<Unpacker4444_32<RGBAPacker>>>;

//VQ
const TexConvFP tex1555_VQ = texture_VQ<ConvertTwiddle<Unpacker1555>>;
const TexConvFP tex4444_VQ = texture_VQ<ConvertTwiddle<Unpacker4444>>;
const TexConvFP texBMP_VQ = tex4444_VQ;
const TexConvFP32 texYUV422_VQ = texture_VQ<ConvertTwiddleYUV<RGBAPacker>>;

const TexConvFP32 tex565_VQ32 = texture_VQ<ConvertTwiddle<Unpacker565_32<RGBAPacker>>>;
const TexConvFP32 tex1555_VQ32 = texture_VQ<ConvertTwiddle<Unpacker1555_32<RGBAPacker>>>;
const TexConvFP32 tex4444_VQ32 = texture_VQ<ConvertTwiddle<Unpacker4444_32<RGBAPacker>>>;
}

namespace directx {
// DirectX

//Planar
const TexConvFP32 texYUV422_PL = texture_PL<ConvertPlanarYUV<BGRAPacker>>;
const TexConvFP32 tex565_PL32 = texture_PL<ConvertPlanar<Unpacker565_32<BGRAPacker>>>;
const TexConvFP32 tex1555_PL32 = texture_PL<ConvertPlanar<Unpacker1555_32<BGRAPacker>>>;
const TexConvFP32 tex4444_PL32 = texture_PL<ConvertPlanar<Unpacker4444_32<BGRAPacker>>>;

const TexConvFP32 texYUV422_PLVQ = texture_PLVQ<ConvertPlanarYUV<BGRAPacker>>;
const TexConvFP32 tex565_PLVQ32 = texture_PLVQ<ConvertPlanar<Unpacker565_32<BGRAPacker>>>;
const TexConvFP32 tex1555_PLVQ32 = texture_PLVQ<ConvertPlanar<Unpacker1555_32<BGRAPacker>>>;
const TexConvFP32 tex4444_PLVQ32 = texture_PLVQ<ConvertPlanar<Unpacker4444_32<BGRAPacker>>>;

//Twiddle
const TexConvFP tex1555_TW = texture_TW<ConvertTwiddle<UnpackerNop<u16>>>;
const TexConvFP tex4444_TW = texture_TW<ConvertTwiddle<UnpackerNop<u16>>>;
const TexConvFP texBMP_TW = tex4444_TW;
const TexConvFP32 texYUV422_TW = texture_TW<ConvertTwiddleYUV<BGRAPacker>>;

const TexConvFP32 tex565_TW32 = texture_TW<ConvertTwiddle<Unpacker565_32<BGRAPacker>>>;
const TexConvFP32 tex1555_TW32 = texture_TW<ConvertTwiddle<Unpacker1555_32<BGRAPacker>>>;
const TexConvFP32 tex4444_TW32 = texture_TW<ConvertTwiddle<Unpacker4444_32<BGRAPacker>>>;

//VQ
const TexConvFP tex1555_VQ = texture_VQ<ConvertTwiddle<UnpackerNop<u16>>>;
const TexConvFP tex4444_VQ = texture_VQ<ConvertTwiddle<UnpackerNop<u16>>>;
const TexConvFP texBMP_VQ = tex4444_VQ;
const TexConvFP32 texYUV422_VQ = texture_VQ<ConvertTwiddleYUV<BGRAPacker>>;

const TexConvFP32 tex565_VQ32 = texture_VQ<ConvertTwiddle<Unpacker565_32<BGRAPacker>>>;
const TexConvFP32 tex1555_VQ32 = texture_VQ<ConvertTwiddle<Unpacker1555_32<BGRAPacker>>>;
const TexConvFP32 tex4444_VQ32 = texture_VQ<ConvertTwiddle<Unpacker4444_32<BGRAPacker>>>;
}

#define TEX_CONV_TABLE \
const PvrTexInfo pvrTexInfo[8] = \
{	/* name     bpp Final format               Twiddled     VQ             Planar(32b)    Twiddled(32b)  VQ (32b)      PL VQ (32b)     Palette (8b)	*/	\
	{"1555", 	16,	TextureType::_5551,        tex1555_TW,  tex1555_VQ,    tex1555_PL32,  tex1555_TW32,  tex1555_VQ32, tex1555_PLVQ32, nullptr },			\
	{"565", 	16, TextureType::_565,         tex565_TW,   tex565_VQ,     tex565_PL32,   tex565_TW32,   tex565_VQ32,  tex565_PLVQ32,  nullptr },	    	\
	{"4444", 	16, TextureType::_4444,        tex4444_TW,  tex4444_VQ,    tex4444_PL32,  tex4444_TW32,  tex4444_VQ32, tex4444_PLVQ32, nullptr },	    	\
	{"yuv", 	16, TextureType::_8888,        nullptr,     nullptr,       texYUV422_PL,  texYUV422_TW,  texYUV422_VQ, texYUV422_PLVQ, nullptr },			\
	{"bumpmap", 16, TextureType::_4444,        texBMP_TW,	texBMP_VQ,     tex4444_PL32,  tex4444_TW32,  tex4444_VQ32, tex4444_PLVQ32, nullptr },			\
	{"pal4", 	4,	TextureType::_5551,        texPAL4_TW,  texPAL4_VQ,    nullptr,       texPAL4_TW32,  texPAL4_VQ32, nullptr,        texPAL4PT_TW },		\
	{"pal8", 	8,	TextureType::_5551,        texPAL8_TW,  texPAL8_VQ,    nullptr,       texPAL8_TW32,  texPAL8_VQ32, nullptr,        texPAL8PT_TW },		\
	{"ns/1555", 0},	                                                                                                                        \
}

namespace opengl {
	TEX_CONV_TABLE;
}
namespace directx {
	TEX_CONV_TABLE;
}
#undef TEX_CONV_TABLE
const PvrTexInfo *pvrTexInfo = opengl::pvrTexInfo;
