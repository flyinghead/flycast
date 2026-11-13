/*
	Sega Hikaru: a frame ready for the GPU, independent of the PVR / Naomi 2 rendering path.

	hikaru_render.cpp builds a DrawList from the command processor's frame on the emulation thread.
	A renderer with a Hikaru backend (DX11) draws it on the render thread. Textures and the 2D layers
	live in a TextureStore shared by both threads: converted texels are only produced again when
	texture RAM changes, and the renderer uploads an entry when its version changed.

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
#include <mutex>
#include <vector>

namespace hikaru
{

constexpr u32 SCREEN_WIDTH = 640;
constexpr u32 SCREEN_HEIGHT = 480;

// Matrices are row-vector (v' = v M), translation in row 3
using Mat4 = std::array<float, 16>;

// A vertex in object space. The vertex shader selects its modelview (Demul / Odin model):
// mode bit 0 second modelview, bit 1 no rotation (3x3 left out), bit 2 no translation.
struct DrawVertex
{
	float pos[3];
	float normal[3];
	float uv[2];		// normalized texture coordinates
	float alpha;
	u32 mode;
};

// Shading of a draw. Colors are linear 0-1+, vectors in eye space.
struct ShadingLight
{
	float color[4];		// rgb
	float ambient[4];	// rgb, added to N.L (times color) before attenuation
	float vec[4];		// xyz: direction to the light (w = 0) or position (w = 1)
	float att[4];		// x: k, y: b, z: 1 = no distance attenuation, w: distance function (0 d, 1 d^2, 2 1/d, 3 1/d^2)
	float spot[4];		// spot cone: the light fades by saturate(L . xyz - w), xyz = axis towards the light
						// times the fade scale, w = cos(outer angle) times the scale. All 0: no cone.
};

struct Shading
{
	float base0[4];		// polygon color 0 (0x091 sub 0)
	float base1[4];		// polygon color 1 (0x091 sub 1)
	float ambient[4];	// lit modes: ambient term multiplying the base color
	float specular[4];	// rgb, w: power
	float fogColor[4];
	float fog[4];		// z = 1, fog: x density, y offset. z = 2, depth cue (alpha fades with the eye depth): x scale, y start
	int modes[4];		// x: lit, y: texture mode, z: light count, w: alternate (alpha) mode
	float alphaTest[4];	// x: min, y: max, z: 0 none, 1 keep [min,max] then opaque, 2 drop alpha < 1/255; w: double-sided
	float clipZ[4];		// x: near plane (eye depth), y: 1 = clip at the near plane, z: texture LOD bias,
						// w: depth bias, scale of the eye depth used for the depth test
	ShadingLight lights[4];
};

enum class Blend : u8 {
	Opaque,
	AlphaOver,		// src alpha, 1 - src alpha
	Product,		// dest color, zero
	AlphaAdd,		// src alpha, one
	SourceOver,		// one, 1 - src alpha
};

enum class Cull : u8 {
	None,
	Front,		// the renderer's front faces are culled
	Back,
};

enum class Address : u8 { Clamp, Repeat, Mirror };

// A run of indexed triangles with the same state
struct Draw
{
	u32 firstIndex;
	u32 indexCount;
	int shading;			// index in DrawList::shadings
	int modelview[2];		// indices in DrawList::matrices
	int projection;			// index in DrawList::matrices: eye space to screen pixels (y down) times w
	int texture;			// TextureStore id, -1 if untextured
	bool linear;			// texture filter: linear, otherwise nearest
	bool mipmapped;			// trilinear over the texture's mip levels
	Address addressU, addressV;
	Blend blend;
	Cull cull;
	bool depthTest;			// greater or equal on the depth
	bool depthWrite;
	u16 clip[4];			// scissor rectangle in screen pixels: x0, y0, x1, y1 (exclusive)
	u16 pattern;			// 0x501: pixels drawn in each 4x4 block of the render target, bit x + 4 * y
							// (y down). 0xffff: all
};

constexpr int HIKARU_3D_BUFFERS = 4;

// Draws between depth clears (one per viewport priority)
struct Pass
{
	u32 firstDraw;
	u32 drawCount;
};

// The frame blender (0x181 / 0x781): after the 3D passes, a source is blended over the target 3D
// buffer: target = factor * source + (1 - factor) * target, target alpha = factor (Demul 2025)
struct FrameBlend
{
	bool enabled;			// 0x181 bit 24
	float factor;			// 0x181 bits 16-23 / 255
	u8 source;				// 0x781 bits 24-25: 0-1 3D buffer, 2-3 layer A or B
	int texture;			// TextureStore id of the layer when the source is a layer
};

// The final picture (Demul 2025): three slots, bottom to top, each the shown 3D buffer or a 2D layer.
// For each pixel the topmost slot whose pixel is not transparent wins (no blending); the bottom slot
// is always opaque. A keyed slot is transparent where its alpha is 0, an unkeyed one where it is black.
struct CompositeSlot
{
	bool is3D;
	int texture;			// layer: TextureStore id (alpha 0 where transparent), -1 if disabled
	u32 scrollX, scrollY;	// layer scroll in texels, wrapped modulo 640 x 480
};

struct Composite
{
	CompositeSlot slots[3];	// bottom to top
	bool keyed3D;			// 0x1BC bit 0
};

struct DrawList
{
	std::vector<DrawVertex> vertices;
	std::vector<u32> indices;
	std::vector<Draw> draws;
	std::vector<Pass> passes;
	std::vector<Mat4> matrices;
	std::vector<Shading> shadings;
	float clearColor[4];	// the target 3D buffer is cleared with it (0x191), alpha 0 or 1
	u8 target;				// 3D buffer drawn into (0x781 bits 16-17), 0-3
	u8 displayed;			// 3D buffer shown by the composite, 0-1
	FrameBlend blend;
	Composite composite;

	void clear()
	{
		vertices.clear();
		indices.clear();
		draws.clear();
		passes.clear();
		matrices.clear();
		shadings.clear();
	}
};

// Converted textures and layers, RGBA8 (R in the low byte), mip levels largest first
struct TextureEntry
{
	u32 width = 0;
	u32 height = 0;
	u32 levels = 0;
	std::vector<u32> texels;
	u32 version = 0;		// bumped on every change, 0 when free
	u32 dirtyY0 = 0;		// rows of level 0 changed since the renderer's last upload
	u32 dirtyY1 = 0;		// (exclusive; 0 when nothing changed)
};

class TextureStore
{
public:
	std::mutex mutex;
	std::vector<TextureEntry> entries;
};

TextureStore& textureStore();

}
