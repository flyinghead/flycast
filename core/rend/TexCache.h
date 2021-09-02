#pragma once
#include "oslib/oslib.h"
#include "hw/pvr/Renderer_if.h"
#include "cfg/option.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <unordered_map>

extern u8* vq_codebook;
extern u32 palette_index;
extern u32 palette16_ram[1024];
extern u32 palette32_ram[1024];
extern bool pal_needs_update,fog_needs_update;
extern u32 pal_hash_256[4];
extern u32 pal_hash_16[64];
extern bool KillTex;
extern bool palette_updated;

extern u32 detwiddle[2][11][1024];

void palette_update();

template<class pixel_type>
class PixelBuffer
{
	pixel_type* p_buffer_start = nullptr;
	pixel_type* p_current_mipmap = nullptr;
	pixel_type* p_current_line = nullptr;
	pixel_type* p_current_pixel = nullptr;

	u32 pixels_per_line = 0;

public:
	~PixelBuffer()
	{
		deinit();
	}

	void init(u32 width, u32 height, bool mipmapped)
	{
		deinit();
		size_t size = width * height * sizeof(pixel_type);
		if (mipmapped)
		{
			do
			{
				width /= 2;
				height /= 2;
				size += width * height * sizeof(pixel_type);
			}
			while (width != 0 && height != 0);
		}
		p_buffer_start = p_current_line = p_current_pixel = p_current_mipmap = (pixel_type *)malloc(size);
		this->pixels_per_line = 1;
	}

	void init(u32 width, u32 height)
	{
		deinit();
		p_buffer_start = p_current_line = p_current_pixel = p_current_mipmap = (pixel_type *)malloc(width * height * sizeof(pixel_type));
		this->pixels_per_line = width;
	}

	void deinit()
	{
		if (p_buffer_start != NULL)
		{
			free(p_buffer_start);
			p_buffer_start = p_current_mipmap = p_current_line = p_current_pixel = NULL;
		}
	}

	void steal_data(PixelBuffer &buffer)
	{
		deinit();
		p_buffer_start = p_current_mipmap = p_current_line = p_current_pixel = buffer.p_buffer_start;
		pixels_per_line = buffer.pixels_per_line;
		buffer.p_buffer_start = buffer.p_current_mipmap = buffer.p_current_line = buffer.p_current_pixel = NULL;
	}

	void set_mipmap(int level)
	{
		size_t offset = 0;
		for (int i = 0; i < level; i++)
			offset += (1 << (2 * i));
		p_current_mipmap = p_current_line = p_current_pixel = p_buffer_start + offset;
		pixels_per_line = 1 << level;
	}

	__forceinline pixel_type *data(u32 x = 0, u32 y = 0)
	{
		return p_current_mipmap + pixels_per_line * y + x;
	}

	__forceinline void prel(u32 x, pixel_type value)
	{
		p_current_pixel[x] = value;
	}

	__forceinline void prel(u32 x, u32 y, pixel_type value)
	{
		p_current_pixel[y * pixels_per_line + x] = value;
	}

	__forceinline void rmovex(u32 value)
	{
		p_current_pixel += value;
	}
	__forceinline void rmovey(u32 value)
	{
		p_current_line += pixels_per_line * value;
		p_current_pixel = p_current_line;
	}
	__forceinline void amove(u32 x_m, u32 y_m)
	{
		//p_current_pixel=p_buffer_start;
		p_current_line = p_current_mipmap + pixels_per_line * y_m;
		p_current_pixel = p_current_line + x_m;
	}
};

#define clamp(minv, maxv, x) ((x) < (minv) ? (minv) : (x) > (maxv) ? (maxv) : (x))

// Open GL
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

template<typename Packer>
inline static u32 YUV422(s32 Y, s32 Yu, s32 Yv)
{
	Yu -= 128;
	Yv -= 128;

	s32 R = Y + Yv * 11 / 8;				// Y + (Yv-128) * (11/8) ?
	s32 G = Y - (Yu * 11 + Yv * 22) / 32;	// Y - (Yu-128) * (11/8) * 0.25 - (Yv-128) * (11/8) * 0.5 ?
	s32 B = Y + Yu * 110 / 64;				// Y + (Yu-128) * (11/8) * 1.25 ?

	return Packer::pack(clamp(0, 255, R), clamp(0, 255, G), clamp(0, 255, B), 0xFF);
}

#define twop(x,y,bcx,bcy) (detwiddle[0][bcy][x]+detwiddle[1][bcx][y])

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

template<typename Unpacker>
struct ConvertPlanar
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 4;
	static constexpr u32 ypp = 1;
	static void Convert(PixelBuffer<unpacked_type> *pb, u8 *data)
	{
		u16 *p_in = (u16 *)data;
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
	static void Convert(PixelBuffer<u32> *pb, u8 *data)
	{
		//convert 4x1 4444 to 4x1 8888
		u32 *p_in = (u32 *)data;


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
	static void Convert(PixelBuffer<unpacked_type> *pb, u8 *data)
	{
		u16 *p_in = (u16 *)data;
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
	static void Convert(PixelBuffer<u32> *pb, u8 *data)
	{
		//convert 4x1 4444 to 4x1 8888
		u16* p_in = (u16 *)data;

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
	static void Convert(PixelBuffer<unpacked_type> *pb, u8 *data)
	{
		u8 *p_in = (u8 *)data;

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

template <typename Unpacker>
struct ConvertTwiddlePal8
{
	using unpacked_type = typename Unpacker::unpacked_type;
	static constexpr u32 xpp = 2;
	static constexpr u32 ypp = 4;
	static void Convert(PixelBuffer<unpacked_type> *pb, u8 *data)
	{
		u8* p_in = (u8 *)data;

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
template<class PixelConvertor>
void texture_PL(PixelBuffer<typename PixelConvertor::unpacked_type>* pb,u8* p_in,u32 Width,u32 Height)
{
	pb->amove(0,0);

	Height/=PixelConvertor::ypp;
	Width/=PixelConvertor::xpp;

	for (u32 y=0;y<Height;y++)
	{
		for (u32 x=0;x<Width;x++)
		{
			u8* p = p_in;
			PixelConvertor::Convert(pb,p);
			p_in+=8;

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<class PixelConvertor>
void texture_TW(PixelBuffer<typename PixelConvertor::unpacked_type>* pb,u8* p_in,u32 Width,u32 Height)
{
	pb->amove(0, 0);

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;

	const u32 bcx = bitscanrev(Width);
	const u32 bcy = bitscanrev(Height);

	for (u32 y = 0; y < Height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < Width; x += PixelConvertor::xpp)
		{
			u8* p = &p_in[(twop(x, y, bcx, bcy) / divider) << 3];
			PixelConvertor::Convert(pb, p);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

template<class PixelConvertor>
void texture_VQ(PixelBuffer<typename PixelConvertor::unpacked_type>* pb,u8* p_in,u32 Width,u32 Height)
{
	p_in += 256 * 4 * 2;	// Skip VQ codebook
	pb->amove(0, 0);

	const u32 divider = PixelConvertor::xpp * PixelConvertor::ypp;
	const u32 bcx = bitscanrev(Width);
	const u32 bcy = bitscanrev(Height);

	for (u32 y = 0; y < Height; y += PixelConvertor::ypp)
	{
		for (u32 x = 0; x < Width; x += PixelConvertor::xpp)
		{
			u8 p = p_in[twop(x, y, bcx, bcy) / divider];
			PixelConvertor::Convert(pb, &vq_codebook[p * 8]);

			pb->rmovex(PixelConvertor::xpp);
		}
		pb->rmovey(PixelConvertor::ypp);
	}
}

typedef void (*TexConvFP)(PixelBuffer<u16> *pb, u8 *p_in, u32 width, u32 height);
typedef void (*TexConvFP8)(PixelBuffer<u8> *pb, u8 *p_in, u32 width, u32 height);
typedef void (*TexConvFP32)(PixelBuffer<u32> *pb, u8 *p_in, u32 width, u32 height);

//Planar
constexpr TexConvFP tex565_PL = texture_PL<ConvertPlanar<UnpackerNop<u16>>>;
//Twiddle
constexpr TexConvFP tex565_TW = texture_TW<ConvertTwiddle<UnpackerNop<u16>>>;
// Palette
constexpr TexConvFP texPAL4_TW = texture_TW<ConvertTwiddlePal4<UnpackerPalToRgb<u16>>>;
constexpr TexConvFP texPAL8_TW = texture_TW<ConvertTwiddlePal8<UnpackerPalToRgb<u16>>>;
constexpr TexConvFP32 texPAL4_TW32 = texture_TW<ConvertTwiddlePal4<UnpackerPalToRgb<u32>>>;
constexpr TexConvFP32 texPAL8_TW32 = texture_TW<ConvertTwiddlePal8<UnpackerPalToRgb<u32>>>;
constexpr TexConvFP8 texPAL4PT_TW = texture_TW<ConvertTwiddlePal4<UnpackerNop<u8>>>;
constexpr TexConvFP8 texPAL8PT_TW = texture_TW<ConvertTwiddlePal8<UnpackerNop<u8>>>;
//VQ
constexpr TexConvFP tex565_VQ = texture_VQ<ConvertTwiddle<UnpackerNop<u16>>>;
// According to the documentation, a texture cannot be compressed and use
// a palette at the same time. However the hardware displays them
// just fine.
constexpr TexConvFP texPAL4_VQ = texture_VQ<ConvertTwiddlePal4<UnpackerPalToRgb<u16>>>;
constexpr TexConvFP texPAL8_VQ = texture_VQ<ConvertTwiddlePal8<UnpackerPalToRgb<u16>>>;
constexpr TexConvFP32 texPAL4_VQ32 = texture_VQ<ConvertTwiddlePal4<UnpackerPalToRgb<u32>>>;
constexpr TexConvFP32 texPAL8_VQ32 = texture_VQ<ConvertTwiddlePal8<UnpackerPalToRgb<u32>>>;

namespace opengl {
// Open GL

//Planar
constexpr TexConvFP tex1555_PL = texture_PL<ConvertPlanar<Unpacker1555>>;
constexpr TexConvFP tex4444_PL = texture_PL<ConvertPlanar<Unpacker4444>>;
constexpr TexConvFP texBMP_PL = tex4444_PL;
constexpr TexConvFP32 texYUV422_PL = texture_PL<ConvertPlanarYUV<RGBAPacker>>;

constexpr TexConvFP32 tex565_PL32 = texture_PL<ConvertPlanar<Unpacker565_32<RGBAPacker>>>;
constexpr TexConvFP32 tex1555_PL32 = texture_PL<ConvertPlanar<Unpacker1555_32<RGBAPacker>>>;
constexpr TexConvFP32 tex4444_PL32 = texture_PL<ConvertPlanar<Unpacker4444_32<RGBAPacker>>>;

//Twiddle
constexpr TexConvFP tex1555_TW = texture_TW<ConvertTwiddle<Unpacker1555>>;
constexpr TexConvFP tex4444_TW = texture_TW<ConvertTwiddle<Unpacker4444>>;
constexpr TexConvFP texBMP_TW = tex4444_TW;
constexpr TexConvFP32 texYUV422_TW = texture_TW<ConvertTwiddleYUV<RGBAPacker>>;

constexpr TexConvFP32 tex565_TW32 = texture_TW<ConvertTwiddle<Unpacker565_32<RGBAPacker>>>;
constexpr TexConvFP32 tex1555_TW32 = texture_TW<ConvertTwiddle<Unpacker1555_32<RGBAPacker>>>;
constexpr TexConvFP32 tex4444_TW32 = texture_TW<ConvertTwiddle<Unpacker4444_32<RGBAPacker>>>;

//VQ
constexpr TexConvFP tex1555_VQ = texture_VQ<ConvertTwiddle<Unpacker1555>>;
constexpr TexConvFP tex4444_VQ = texture_VQ<ConvertTwiddle<Unpacker4444>>;
constexpr TexConvFP texBMP_VQ = tex4444_VQ;
constexpr TexConvFP32 texYUV422_VQ = texture_VQ<ConvertTwiddleYUV<RGBAPacker>>;

constexpr TexConvFP32 tex565_VQ32 = texture_VQ<ConvertTwiddle<Unpacker565_32<RGBAPacker>>>;
constexpr TexConvFP32 tex1555_VQ32 = texture_VQ<ConvertTwiddle<Unpacker1555_32<RGBAPacker>>>;
constexpr TexConvFP32 tex4444_VQ32 = texture_VQ<ConvertTwiddle<Unpacker4444_32<RGBAPacker>>>;
}

namespace directx {
// DirectX

//Planar
constexpr TexConvFP tex1555_PL = texture_PL<ConvertPlanar<UnpackerNop<u16>>>;
constexpr TexConvFP tex4444_PL = texture_PL<ConvertPlanar<UnpackerNop<u16>>>;
constexpr TexConvFP texBMP_PL = tex4444_PL;
constexpr TexConvFP32 texYUV422_PL = texture_PL<ConvertPlanarYUV<BGRAPacker>>;

constexpr TexConvFP32 tex565_PL32 = texture_PL<ConvertPlanar<Unpacker565_32<BGRAPacker>>>;
constexpr TexConvFP32 tex1555_PL32 = texture_PL<ConvertPlanar<Unpacker1555_32<BGRAPacker>>>;
constexpr TexConvFP32 tex4444_PL32 = texture_PL<ConvertPlanar<Unpacker4444_32<BGRAPacker>>>;

//Twiddle
constexpr TexConvFP tex1555_TW = texture_TW<ConvertTwiddle<UnpackerNop<u16>>>;
constexpr TexConvFP tex4444_TW = texture_TW<ConvertTwiddle<UnpackerNop<u16>>>;
constexpr TexConvFP texBMP_TW = tex4444_TW;
constexpr TexConvFP32 texYUV422_TW = texture_TW<ConvertTwiddleYUV<BGRAPacker>>;

constexpr TexConvFP32 tex565_TW32 = texture_TW<ConvertTwiddle<Unpacker565_32<BGRAPacker>>>;
constexpr TexConvFP32 tex1555_TW32 = texture_TW<ConvertTwiddle<Unpacker1555_32<BGRAPacker>>>;
constexpr TexConvFP32 tex4444_TW32 = texture_TW<ConvertTwiddle<Unpacker4444_32<BGRAPacker>>>;

//VQ
constexpr TexConvFP tex1555_VQ = texture_VQ<ConvertTwiddle<UnpackerNop<u16>>>;
constexpr TexConvFP tex4444_VQ = texture_VQ<ConvertTwiddle<UnpackerNop<u16>>>;
constexpr TexConvFP texBMP_VQ = tex4444_VQ;
constexpr TexConvFP32 texYUV422_VQ = texture_VQ<ConvertTwiddleYUV<BGRAPacker>>;

constexpr TexConvFP32 tex565_VQ32 = texture_VQ<ConvertTwiddle<Unpacker565_32<BGRAPacker>>>;
constexpr TexConvFP32 tex1555_VQ32 = texture_VQ<ConvertTwiddle<Unpacker1555_32<BGRAPacker>>>;
constexpr TexConvFP32 tex4444_VQ32 = texture_VQ<ConvertTwiddle<Unpacker4444_32<BGRAPacker>>>;
}

class BaseTextureCacheData;

struct vram_block
{
	u32 start;
	u32 end;

	BaseTextureCacheData *texture;
};

bool VramLockedWriteOffset(size_t offset);
bool VramLockedWrite(u8* address);
void libCore_vramlock_Lock(u32 start_offset, u32 end_offset, BaseTextureCacheData *texture);

void UpscalexBRZ(int factor, u32* source, u32* dest, int width, int height, bool has_alpha);

struct PvrTexInfo;
enum class TextureType { _565, _5551, _4444, _8888, _8 };

class BaseTextureCacheData
{
public:
	TSP tsp;        	//dreamcast texture parameters
	TCW tcw;

	// Decoded/filtered texture format
	TextureType tex_type;
	u32 sa_tex;			// texture data start address in vram

	u32 dirty;			// frame number at which texture was overwritten
	vram_block* lock_block;

	u32 sa;         	// pixel data start address of max level mipmap
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

	void Create();
	void ComputeHash();
	void Update();
	virtual void UploadToGPU(int width, int height, u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded = false) = 0;
	virtual bool Force32BitTexture(TextureType type) const { return false; }
	void CheckCustomTexture();
	//true if : dirty or paletted texture and hashes don't match
	bool NeedsUpdate();
	virtual bool Delete();
	virtual ~BaseTextureCacheData() = default;
	static bool IsGpuHandledPaletted(TSP tsp, TCW tcw)
	{
		// Some palette textures are handled on the GPU
		// This is currently limited to textures using nearest filtering and not mipmapped.
		// Enabling texture upscaling or dumping also disables this mode.
		return (tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
				&& config::TextureUpscale == 1
				&& !config::DumpTextures
				&& tsp.FilterMode == 0
				&& !tcw.MipMapped
				&& !tcw.VQ_Comp;
	}
	static void SetDirectXColorOrder(bool enabled);
};

template<typename Texture>
class BaseTextureCache
{
	using TexCacheIter = typename std::unordered_map<u64, Texture>::iterator;
public:
	Texture *getTextureCacheData(TSP tsp, TCW tcw)
	{
		u64 key = tsp.full & TSPTextureCacheMask.full;
		if ((tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8)
				&& !BaseTextureCacheData::IsGpuHandledPaletted(tsp, tcw))
			// Paletted textures have a palette selection that must be part of the key
			// We also add the palette type to the key to avoid thrashing the cache
			// when the palette type is changed. If the palette type is changed back in the future,
			// this texture will stil be available.
			key |= ((u64)tcw.full << 32) | ((PAL_RAM_CTRL & 3) << 6) | ((tsp.FilterMode != 0) << 8);
		else
			key |= (u64)(tcw.full & TCWTextureCacheMask.full) << 32;

		TexCacheIter it = cache.find(key);

		Texture* texture;
		if (it != cache.end())
		{
			texture = &it->second;
			// Needed if the texture is updated
			texture->tcw.StrideSel = tcw.StrideSel;
		}
		else //create if not existing
		{
			texture = &cache[key];

			texture->tsp = tsp;
			texture->tcw = tcw;
		}

		return texture;
	}

	void CollectCleanup()
	{
		std::vector<u64> list;

		u32 TargetFrame = std::max((u32)120, FrameCount) - 120;

		for (const auto& pair : cache)
		{
			if (pair.second.dirty && pair.second.dirty < TargetFrame)
				list.push_back(pair.first);

			if (list.size() > 5)
				break;
		}

		for (u64 id : list)
		{
			if (cache[id].Delete())
				cache.erase(id);
		}
	}

	void Clear()
	{
		for (auto& pair : cache)
			pair.second.Delete();

		cache.clear();
		KillTex = false;
		INFO_LOG(RENDERER, "Texture cache cleared");
	}

protected:
	std::unordered_map<u64, Texture> cache;
	// Only use TexU and TexV from TSP in the cache key
	//     TexV : 7, TexU : 7
	const TSP TSPTextureCacheMask = { { 7, 7 } };
	//     TexAddr : 0x1FFFFF, Reserved : 0, StrideSel : 0, ScanOrder : 1, PixelFmt : 7, VQ_Comp : 1, MipMapped : 1
	const TCW TCWTextureCacheMask = { { 0x1FFFFF, 0, 0, 1, 7, 1, 1 } };
};

template<typename Packer = RGBAPacker>
void ReadFramebuffer(PixelBuffer<u32>& pb, int& width, int& height);
template<int Red = 0, int Green = 1, int Blue = 2, int Alpha = 3>
void WriteTextureToVRam(u32 width, u32 height, u8 *data, u16 *dst, u32 fb_w_ctrl = -1, u32 linestride = -1);

static inline void MakeFogTexture(u8 *tex_data)
{
	u8 *fog_table = (u8 *)FOG_TABLE;
	for (int i = 0; i < 128; i++)
	{
		tex_data[i] = fog_table[i * 4];
		tex_data[i + 128] = fog_table[i * 4 + 1];
	}
}

void dump_screenshot(u8 *buffer, u32 width, u32 height, bool alpha = false, u32 rowPitch = 0, bool invertY = true);

extern const std::array<f32, 16> D_Adjust_LoD_Bias;
#undef clamp

extern float fb_scale_x, fb_scale_y;
static inline void rend_set_fb_scale(float x, float y)
{
	fb_scale_x = x;
	fb_scale_y = y;
}
