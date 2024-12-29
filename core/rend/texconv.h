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
#include "types.h"

constexpr int VQ_CODEBOOK_SIZE = 256 * 8;
extern const u8 *vq_codebook;
extern u32 palette_index;
extern u32 palette16_ram[1024];
extern u32 palette32_ram[1024];
extern u32 pal_hash_256[4];
extern u32 pal_hash_16[64];

void palette_update();

template<typename Pixel>
class PixelBuffer
{
	Pixel* p_buffer_start = nullptr;
	Pixel* p_current_mipmap = nullptr;
	Pixel* p_current_line = nullptr;
	Pixel* p_current_pixel = nullptr;

	u32 pixels_per_line = 0;

public:
	~PixelBuffer() {
		deinit();
	}

	void init(u32 width, u32 height, bool mipmapped)
	{
		deinit();
		size_t size = width * height * sizeof(Pixel);
		if (mipmapped)
		{
			do
			{
				width /= 2;
				height /= 2;
				size += width * height * sizeof(Pixel);
			}
			while (width != 0 && height != 0);
		}
		p_buffer_start = p_current_line = p_current_pixel = p_current_mipmap = (Pixel *)malloc(size);
		this->pixels_per_line = 1;
	}

	void init(u32 width, u32 height)
	{
		deinit();
		p_buffer_start = p_current_line = p_current_pixel = p_current_mipmap = (Pixel *)malloc(width * height * sizeof(Pixel));
		this->pixels_per_line = width;
	}

	void deinit()
	{
		if (p_buffer_start != nullptr)
		{
			free(p_buffer_start);
			p_buffer_start = p_current_mipmap = p_current_line = p_current_pixel = nullptr;
		}
	}

	void steal_data(PixelBuffer &buffer)
	{
		deinit();
		p_buffer_start = p_current_mipmap = p_current_line = p_current_pixel = buffer.p_buffer_start;
		pixels_per_line = buffer.pixels_per_line;
		buffer.p_buffer_start = buffer.p_current_mipmap = buffer.p_current_line = buffer.p_current_pixel = nullptr;
	}

	void set_mipmap(int level)
	{
		u32 offset = 0;
		for (int i = 0; i < level; i++)
			offset += (1 << (2 * i));
		p_current_mipmap = p_current_line = p_current_pixel = p_buffer_start + offset;
		pixels_per_line = 1 << level;
	}

	Pixel *data(u32 x = 0, u32 y = 0)
	{
		return p_current_mipmap + pixels_per_line * y + x;
	}

	void prel(u32 x, Pixel value)
	{
		p_current_pixel[x] = value;
	}

	void prel(u32 x, u32 y, Pixel value)
	{
		p_current_pixel[y * pixels_per_line + x] = value;
	}

	void rmovex(u32 value)
	{
		p_current_pixel += value;
	}

	void rmovey(u32 value)
	{
		p_current_line += pixels_per_line * value;
		p_current_pixel = p_current_line;
	}

	void amove(u32 x_m, u32 y_m)
	{
		//p_current_pixel=p_buffer_start;
		p_current_line = p_current_mipmap + pixels_per_line * y_m;
		p_current_pixel = p_current_line + x_m;
	}
};

// OpenGL
struct RGBAPacker {
	static u32 pack(u8 r, u8 g, u8 b, u8 a) {
		return r | (g << 8) | (b << 16) | (a << 24);
	}
};
// DirectX
struct BGRAPacker {
	static u32 pack(u8 r, u8 g, u8 b, u8 a) {
		return b | (g << 8) | (r << 16) | (a << 24);
	}
};

template<typename Pixel>
struct UnpackerNop {
	using unpacked_type = Pixel;
	static Pixel unpack(Pixel word) {
		return word;
	}
};

// ARGB1555 to RGBA5551
struct Unpacker1555 {
	using unpacked_type = u16;
	static u16 unpack(u16 word) {
		return ((word >> 15) & 1) | (((word >> 10) & 0x1F) << 11)  | (((word >> 5) & 0x1F) << 6)  | (((word >> 0) & 0x1F) << 1);
	}
};

// ARGB4444 to RGBA4444
struct Unpacker4444 {
	using unpacked_type = u16;
	static u16 unpack(u16 word) {
		return (((word >> 0) & 0xF) << 4) | (((word >> 4) & 0xF) << 8) | (((word >> 8) & 0xF) << 12) | (((word >> 12) & 0xF) << 0);
	}
};

template <typename Packer>
struct Unpacker1555_32 {
	using unpacked_type = u32;
	static u32 unpack(u16 word) {
		return Packer::pack(
				(((word >> 10) & 0x1F) << 3) | ((word >> 12) & 7),
				(((word >> 5) & 0x1F) << 3) | ((word >> 7) & 7),
				(((word >> 0) & 0x1F) << 3) | ((word >> 2) & 7),
				(word & 0x8000) ? 0xFF : 0);
	}
};

template <typename Packer>
struct Unpacker565_32 {
	using unpacked_type = u32;
	static u32 unpack(u16 word) {
		return Packer::pack(
				(((word >> 11) & 0x1F) << 3) | ((word >> 13) & 7),
				(((word >> 5) & 0x3F) << 2) | ((word >> 9) & 3),
				(((word >> 0) & 0x1F) << 3) | ((word >> 2) & 7),
				0xFF);
	}
};

template <typename Packer>
struct Unpacker4444_32 {
	using unpacked_type = u32;
	static u32 unpack(u16 word) {
		return Packer::pack(
				(((word >> 8) & 0xF) << 4) | ((word >> 8) & 0xF),
				(((word >> 4) & 0xF) << 4) | ((word >> 4) & 0xF),
				(((word >> 0) & 0xF) << 4) | ((word >> 0) & 0xF),
				(((word >> 12) & 0xF) << 4) | ((word >> 12) & 0xF));
	}
};

// ARGB8888 to whatever
template <typename Packer>
struct Unpacker8888 {
	using unpacked_type = u32;
	static u32 unpack(u32 word) {
		return Packer::pack(
				(word >> 16) & 0xFF,
				(word >> 8) & 0xFF,
				(word >> 0) & 0xFF,
				(word >> 24) & 0xFF);
	}
};

enum class TextureType { _565, _5551, _4444, _8888, _8 };

typedef void (*TexConvFP)(PixelBuffer<u16> *pb, const u8 *p_in, u32 width, u32 height);
typedef void (*TexConvFP8)(PixelBuffer<u8> *pb, const u8 *p_in, u32 width, u32 height);
typedef void (*TexConvFP32)(PixelBuffer<u32> *pb, const u8 *p_in, u32 width, u32 height);

struct PvrTexInfo
{
	const char* name;
	int bpp;        //4/8 for pal. 16 for yuv, rgb, argb
	TextureType type;
	// Conversion to 16 bpp
	TexConvFP TW;
	TexConvFP VQ;
	// Conversion to 32 bpp
	TexConvFP32 PL32;
	TexConvFP32 TW32;
	TexConvFP32 VQ32;
	TexConvFP32 PLVQ32;
	// Conversion to 8 bpp (palette)
	TexConvFP8 TW8;
};

namespace opengl
{
	extern const TexConvFP32 tex1555_TW32;
	extern const TexConvFP32 tex1555_VQ32;
	extern const TexConvFP32 tex1555_PL32;
	extern const TexConvFP32 tex565_TW32;
	extern const TexConvFP32 tex565_VQ32;
	extern const TexConvFP32 tex565_PL32;
	extern const TexConvFP32 tex4444_TW32;
	extern const TexConvFP32 tex4444_VQ32;
	extern const TexConvFP32 tex4444_PL32;

	extern const PvrTexInfo pvrTexInfo[8];
}
namespace directx
{
	extern const PvrTexInfo pvrTexInfo[8];
}
extern const PvrTexInfo *pvrTexInfo;
