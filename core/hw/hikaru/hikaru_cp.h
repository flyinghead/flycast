/*
	Sega Hikaru GPU command processor.

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
#include <array>
#include <vector>

#include "serialize.h"

namespace hikaru::cp
{

using Vec4 = std::array<float, 4>;
using Matrix = std::array<float, 16>;	// row-vector convention, row 3 = translation

struct Viewport
{
	float cx, cy;			// centre of projection
	float minx, miny, maxx, maxy;
	Vec4 ambient;
	float fx, fy;			// focal lengths in pixels
	float nearZ, farZ;
	u32 priority;			// 0..7 (word 3 bits 29-31)
	u32 slot;				// viewport table index 0..7: drawing pass (Demul)
	Vec4 fogColor;
	bool fogEnable;
	u8 fogFlags;			// 0x021 sub 3 word 0 bits 16-20: bit 0 no fog, bit 1 depth blend, bits 2-4 its type
	float fogW3, offsetW3;	// raw 0x021 sub 3 / sub 0 word 3
	float fogDensity, fogOffset;
};

struct Material
{
	Vec4 diffuse1, diffuse2, specular, unknown;
	u8 modeB;				// 0x081 sub 5 bits 16-17: blend mode of the blended polygon types
	u8 alphaIndex;			// 0x081 sub 6 bits 16-21: entry of the 0x154 alpha range table
	u8 shading;				// 1 = lit
	bool fog, textured, altDiffuse, highlight;
};

struct Texture
{
	u8 sizeCode;			// width = 16 << (c & 7), height = 16 << ((c >> 3) & 7)
	u8 x, y;				// position in 16-texel units
	u8 format;
	u8 bank;
	bool wrapU, wrapV, mirrorU, mirrorV;
	u32 raw0;
	u8 combine;
	bool texelUV;			// UVs are whole texels instead of 1/16
};

struct Light
{
	Vec4 ambient;
	Vec4 color;
	Vec4 dir;				// w = 1: directional
	Vec4 pos;				// w = 1: positional
	float attK, attB;
	bool noAtten;
	u8 attMode;
	// Spot light: 0xB61 axis (eye space, from the light to its target) and 0x451 cone
	Vec4 spotAxis;			// w = 1 when set
	float cosOuter;			// cos of the outer angle
	float spotScale;		// 1 / (cos inner - cos outer), 0 without a cone
};

struct Vertex
{
	float pos[3];
	float normal[3];
	float u, v;
	float du, dv;			// 0x0d1 texture panning, 1/16 texel
	float intensity;
	bool matrix2;
	u8 mvMode;				// header bits: 1 second modelview (12), 2 no rotation (13 clear), 4 no translation (14 clear)
};

// A run of triangles sharing the same state
struct Batch
{
	bool is2D;
	bool translucent;
	u8 polyType;			// 0x103/0x113 bits 9-11: 1 opaque, 4 alpha-tested, 5 background, 6 translucent
	u8 blendMode;
	float alphaMin, alphaMax;	// type 4 alpha test range
	bool doubleSided;
	float alpha;
	int depthBias;
	u8 pattern;				// 0x501: 4x4 pixel pattern code (0-31)
	int texture;			// index in Frame::textures or -1
	int material;			// index in Frame::materials or -1
	int matrix, matrix2;	// indices in Frame::matrices
	int viewport;			// index in Frame::viewports
	int lights[4];			// indices in Frame::lights or -1 when disabled
	u32 firstIndex;
	u32 indexCount;
};

// Everything a command list produced for one frame
struct Frame
{
	std::vector<Vertex> vertices;
	std::vector<u32> indices;
	std::vector<Batch> opaque;
	std::vector<Batch> translucent;
	std::vector<Matrix> matrices;
	std::vector<Viewport> viewports;
	std::vector<Material> materials;
	std::vector<Texture> textures;
	std::vector<Light> lights;
	u8 layerAlpha[2];
	Vec4 clearColor;		// 0x191: framebuffer clear color, shown where no 3D is drawn
	// Frame blender words, persistent: 0x181 sub0 (bits 16-23 alpha, bit 24 blend enable) and
	// 0x781 (bits 16-17 input 1, bits 24-25 input 2: 0-1 3D buffers, 2 layer 1, 3 layer 2)
	u32 fbW181;
	u32 fbW781;

	void clear();
};

void init();
void term();
void reset();
void serialize(Serializer& ser);
void deserialize(Deserializer& deser);
// Execute the command list starting at pc (bus address), with the call stack at sp
void run(u32 pc, u32 sp);
void vblank();
// Last completed frame
const Frame& lastFrame();

}
