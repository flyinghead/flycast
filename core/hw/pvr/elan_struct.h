/*
	Copyright 2022 flyinghead

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

namespace elan
{

union PCW
{
	enum Command {
		null         = 0,
		_matrix2     = 1,
		_matrix1     = 2,
		projMatrix   = 3,
		matrixOrLight = 4,
		gmp          = 5,
		ich          = 7,
		model        = 8,
		registerWait = 0xe,
		link         = 0xf
	};

	struct
	{
		// Obj Control
		u32 uv16bit    : 1;
		u32 gouraud     : 1;
		u32 offset      : 1;
		u32 texture     : 1;
		u32 colType     : 2;
		u32 volume      : 1;
		u32 shadow      : 1;

		// Naomi 2
		u32 n2Command   : 4;
		u32 reserved    : 4;

		// Group Control
		u32 userClip    : 2;
		u32 stripLen    : 2;
		u32 parallelLight:1;
		u32 res_2       : 2;
		u32 groupEnable : 1;

		// Para Control
		u32 listType    : 3;
		u32 naomi2      : 1;
		u32 endOfStrip  : 1;
		u32 paraType    : 3;
	};
	u8 objectControl;
	u32 full;
};

struct ElanBase
{
	PCW pcw;
};

struct Model : public ElanBase
{
	// 08000800
	struct {
		u32 _res:27;
		u32 cwCulling:1;
		u32 openVolume:1;
		u32 _res0:3;
	} param;
	TSP tsp;
	u32 zero; // 0
	u32 offset;
	u32 one; // 1
	u32 size;
	u32 _res0; // 0
};
static_assert(sizeof(Model) % 32 == 0, "Invalid size for Model");

struct InstanceMatrix : public ElanBase
{
	// 08000400
	u32 id1; // f
	u32 id2; // 7f
	u32 _res[5];

	u32 _res1; // 08000200
	float envMapU; // env map U offset
	float lm00;
	float lm10;
	float lm20;
	float lm01;
	float lm11;
	float lm21;
	float lm02;
	float lm12;
	float lm22;
	float envMapV; // env map V offset
	float _res2[4];

	u32 _res3; // 08000100
	float _near;
	float tm00;
	float tm01;
	float tm02;
	float tm10;
	float tm11;
	float tm12;
	float tm20;
	float tm21;
	float tm22;
	float tm30;
	float tm31;
	float tm32;
	float _far;
	float mproj6; // 1 / near

	bool isInstanceMatrix() const {
		return id1 == 0xf && id2 == 0x7f;
	}
};
static_assert(sizeof(InstanceMatrix) % 32 == 0, "Invalid size for InstanceMatrix");

struct ProjMatrix : public ElanBase
{
	// 08000300
	u32 _res;
	float fx, tx;
	float fy, ty;
	u32 _res0[2];
};
static_assert(sizeof(ProjMatrix) % 32 == 0, "Invalid size for ProjMatrix");

// Global Model Parameters
struct GMP : public ElanBase
{
	// 08000500
	struct {
		u32 frac0:5;
		u32 exp0:3;
		u32 frac1:5;
		u32 exp1:3;

		float getCoef0() {
			return pow(2.f, exp0 - 1.f) * (1 + (float)frac0 / 32);
		}
		float getCoef1() {
			return pow(2.f, exp1 - 1.f) * (1 + (float)frac1 / 32);
		}
	} gloss;
	union {
		struct {
			u32 d0:1; // diffuse
			u32 s0:1; // specular
			u32 a0:1; // alpha?
			u32 f0:1; // fog?

			u32 d1:1;
			u32 s1:1;
			u32 a1:1;
			u32 f1:1;

			u32 vol1UsesVol0UV:1;
			u32 b0:1; // constant color
			u32 b1:1;
			u32 e0:1; // environmental mapping
			u32 e1:1;

			u32 _res:19;
		};
		u32 full;
	} paramSelect;
	// (e=env)
	// ee000 1111 1111 specular, lambert
	// ee110 0000 0000 vertex  color
	// ee110 1111 1111 constant
	// 00000 1111 1111 bump shading?

	// seen:
	// 00110 1111 1111 (b0 and b1 set)
	// 11000 1111 1111 (e0 and e1 set, followed by vtx type2 (vtx only))
	// 11110 1111 1111 (everything! except v1uv0, rt66, vtx type2 (vtx only))
	// 00110 0000 0000 (b0 and b1, vf4)
	// 00000 1010 1010 specular and fog? soul surfer
	// 00010 0010 0010 b0, s0 s1 (initd, headlights)
	// 00010 1111 1111 2-volume (clubk2k3 road surface)

	u32 diffuse0;
	u32 specular0;
	u32 diffuse1;
	u32 specular1;
	u32 _reserved;
	u32 _reserved0;
	u32 tex0;
	u32 _reserved1;
	u32 pal0;
	u32 _reserved2;
	u32 tex1;
	u32 _reserved3;
	u32 pal1;
};
static_assert(sizeof(GMP) % 32 == 0, "Invalid size for GMP");

union HeaderAndNormal
{
	struct {
		u32 nx:8;
		u32 ny:8;
		u32 nz:8;

		u32 _res:5;
		u32 strip:1;
		u32 fan:1;
		u32 endOfStrip:1;
	};
	u32 full;

	bool isFirstOrSecond() const { return strip == 0 && fan == 0; }
	bool isThird() const { return strip == 1 && fan == 1; }
	bool isFan() const { return strip == 0 && fan == 1; }
	bool isStrip() const { return strip == 1 && fan == 0; }
};

struct N2_VERTEX
{
	HeaderAndNormal header;
	float x;
	float y;
	float z;
};

struct Normal
{
	float nx;
	float ny;
	float nz;
	u32 _reserved;
};

struct UnpackedUV
{
	float u;
	float v;
};

struct PackedUV
{
	u32 uv0;
	u32 uv1;
};

struct PackedRGB
{
	u32 argb0;
	u32 argb1;
};

struct BumpMap
{
	struct {
		u8 bumpDegree;
		u8 fixedOffset;
		u8 _res[2];
	} scaleFactor;
	// Normal vectors
	struct {
		int8_t x;
		int8_t y;
		int8_t z;
		u8 _res;
	} tangent;
	struct {
		int8_t x;
		int8_t y;
		int8_t z;
		u8 _res;
	} bitangent;
	u32 _res;
};

//
// textured, 1 or 2 para
//
struct N2_VERTEX_VU : public N2_VERTEX
{
	UnpackedUV uv;
};

//
// textured, 1 or 2 para with unpacked normal
//
struct N2_VERTEX_VNU : public N2_VERTEX
{
	Normal normal;
	UnpackedUV uv;
};

//
// for colored vertex, 1 para
//
struct N2_VERTEX_VUR : public N2_VERTEX
{
	UnpackedUV uv;
	PackedRGB rgb;
};

//
// for bumpmapped, 1 para
//
struct N2_VERTEX_VUB : public N2_VERTEX
{
	UnpackedUV uv;
	BumpMap bump;
};

struct N2_VERTEX_VR : public N2_VERTEX
{
	PackedRGB rgb;
};

struct ICHList : public ElanBase
{
	enum {
		VTX_TYPE_V		= 0x00000002,
		VTX_TYPE_VU		= 0x0000000A,
		VTX_TYPE_VNU	= 0x0000000E,
		VTX_TYPE_VR     = 0X00000042,
		VTX_TYPE_VUR	= 0x0000004A,
		VTX_TYPE_VUB	= 0x0000010A,
	};

	// 08000700
	ISP_TSP isp;
	TSP tsp0;
	TCW tcw0;
	TSP tsp1;
	TCW tcw1;
	u32 flags;
	u32 vtxCount;

	u32 vertexSize() const
	{
		switch (flags)
		{
			case VTX_TYPE_V: return sizeof(N2_VERTEX);
			case VTX_TYPE_VU: return sizeof(N2_VERTEX_VU);
			case VTX_TYPE_VNU: return sizeof(N2_VERTEX_VNU);
			case VTX_TYPE_VR: return sizeof(N2_VERTEX_VR);
			case VTX_TYPE_VUR: return sizeof(N2_VERTEX_VUR);
			case VTX_TYPE_VUB: return sizeof(N2_VERTEX_VUB);
			default: return 0;
		}
	}
};
static_assert(sizeof(ICHList) % 32 == 0, "Invalid size for ICHList");

struct RegisterWait : public ElanBase
{
	// 08000e00
	u32 offset; // -1
	u32 _res; // d, or 1080000d when waiting for ta reg
	u32 mask; // 0
	u32 _res0[4];
};
static_assert(sizeof(RegisterWait) % 32 == 0, "Invalid size for RegisterWait");

struct Link : public ElanBase
{
	// 08000f00
	u32 offset;
	u32 vramAddress; // for texture DMA xfers, otherwise 09000000
	u32 size;
	u32 _res0[4];
};
static_assert(sizeof(Link) % 32 == 0, "Invalid size for Link");

constexpr size_t MAX_LIGHTS = 16;

struct LightModel : public ElanBase
{
	// 08000400
	struct {
		u32 zero:4;
		u32 lightFlag:1;
		u32 useAmbientBase0:1;	// When on, ambient color is multiplied with vertex color.
		u32 useAmbientOffset0:1;// Otherwise it's added to the diffuse/offset color as is.
		u32 useAmbientBase1:1;	// Same for volume1
		u32 useAmbientOffset1:1;
		u32 useBaseOver:1;		// diffusion saturated light: overflow ambient+diffuse into specular

		u32 _res:2;
		u32 bumpId1:4;
		u32 bumpId2:4;
		u32 _res0:12;
	};
	u16 diffuseMask0;
	u16 specularMask0;
	u32 ambientBase0;
	u32 ambientOffset0;
	u16 diffuseMask1;
	u16 specularMask1;
	u32 ambientBase1;
	u32 ambientOffset1;

	bool isDiffuse(int lightId, int volume) const {
		if (volume == 0)
			return (diffuseMask0 & (1 << lightId)) != 0;
		else
			return (diffuseMask1 & (1 << lightId)) != 0;
	}

	bool isSpecular(int lightId, int volume) const {
		if (volume == 0)
			return (specularMask0 & (1 << lightId)) != 0;
		else
			return (specularMask1 & (1 << lightId)) != 0;
	}
};

// dmode, smode
enum {
	// diffuse and specular
	N2_LMETHOD_SINGLE_SIDED,
	N2_LMETHOD_DOUBLE_SIDED,
	N2_LMETHOD_DOUBLE_SIDED_WITH_TOLERANCE,
	N2_LMETHOD_SPECIAL_EFFECT,
	// diffuse only
	N2_LMETHOD_THIN_SURFACE,
	N2_LMETHOD_BUMP_MAP
};

// routing
enum {
	N2_LFUNC_BASEDIFF_BASESPEC_ADD,
	N2_LFUNC_BASEDIFF_OFFSSPEC_ADD,
	N2_LFUNC_OFFSDIFF_BASESPEC_ADD,
	N2_LFUNC_OFFSDIFF_OFFSSPEC_ADD,
	N2_LFUNC_ALPHADIFF_ADD,
	N2_LFUNC_ALPHAATTEN_ADD,
	N2_LFUNC_FOGDIFF_ADD,
	N2_LFUNC_FOGATTENUATION_ADD,
	N2_LFUNC_BASEDIFF_BASESPEC_SUB,
	N2_LFUNC_BASEDIFF_OFFSSPEC_SUB,
	N2_LFUNC_OFFSDIFF_BASESPEC_SUB,
	N2_LFUNC_OFFSDIFF_OFFSSPEC_SUB,
	N2_LFUNC_ALPHADIFF_SUB,
	N2_LFUNC_ALPHAATTEN_SUB,
	N2_LFUNC_FOGDIFF_SUB,
	N2_LFUNC_FOGATTEN_SUB,
};

struct ParallelLight : public ElanBase
{
	// 08100400
	struct {
		u32 lightId:4;
		u32 _res:4;
		u32 blue:8;
		u32 green:8;
		u32 red:8;
	};
	struct {
		u32 dirZ:8;
		u32 dirY:8;
		u32 dirX:8;

		u32 routing:4;
		u32 dmode:2;
		u32 _res1:2;
	};
	u32 _res2[5];

	float getDirX() const {
		return (((int8_t)dirX << 4) | (int)((pcw.full >> 16) & 0xf)) / 2047.f;
	}
	float getDirY() const {
		return (((int8_t)dirY << 4) | (int)((pcw.full >> 4) & 0xf)) / 2047.f;
	}
	float getDirZ() const {
		return (((int8_t)dirZ << 4) | (int)((pcw.full >> 0) & 0xf)) / 2047.f;
	}
};

struct PointLight : public ElanBase
{
	// 08000400
	struct {
		u32 lightId:4;
		u32 _res:1;
		u32 dmode:3;
		u32 blue:8;
		u32 green:8;
		u32 red:8;
	};
	struct {
		u32 dirZ:8;
		u32 dirY:8;
		u32 dirX:8;

		u32 routing:4;
		u32 smode:2;
		u32 one:1;
		u32 dattenmode:1;
	};
	float posX;
	float posY;
	float posZ;
	u16 _distA;
	u16 _distB;
	u16 _angleA;
	u16 _angleB;

	float getDirX() const {
		return (((int8_t)dirX << 4) | (int)((pcw.full >> 16) & 0xf)) / 2047.f;
	}
	float getDirY() const {
		return (((int8_t)dirY << 4) | (int)((pcw.full >> 4) & 0xf)) / 2047.f;
	}
	float getDirZ() const {
		return (((int8_t)dirZ << 4) | (int)((pcw.full >> 0) & 0xf)) / 2047.f;
	}

	static float f16tof32(u16 v)
	{
		u32 z = v << 16;
		return (float&)z;
	}

	float distA() const { return f16tof32(_distA); }
	float distB() const { return f16tof32(_distB); }
	float angleA() const { return f16tof32(_angleA); }
	float angleB() const { return f16tof32(_angleB); }

	float attnMinDistance() const {
		return -distB() / (distA() - 1);
	}

	float attnMaxDistance() const {
		return -distB() / distA();
	}

	float attnDist(float dist) const {
		float rv;
		if (dattenmode)
			rv = distB() * dist + distA();
		else
			rv = distB() / dist + distA();
		return std::max(0.f, std::min(1.f, rv));
	}

	bool isAttnDist() const {
		return distA() != 1 || distB() != 0;
	}

	bool isAttnAngle() const {
		return angleA() != 1 || angleB() != 0;
	}
};

}
