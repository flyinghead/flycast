/*
	Sega Hikaru: turn the command processor's output into a DrawList for the Hikaru renderer.

	Nothing here goes through the PVR / Naomi 2 rendering path: batches become draws with their
	own state, the vertices stay in object space (the vertex shader picks each vertex's modelview)
	and textures are converted from texture RAM into the shared TextureStore, only when the texture
	RAM they come from changed.

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
#include "hikaru_render.h"
#include "hikaru_drawlist.h"
#include "hikaru_cp.h"
#include "hikaru.h"
#include "hikaru_gpu.h"
#include "hw/pvr/ta_ctx.h"
#include "hw/pvr/Renderer_if.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

namespace hikaru
{

static TextureStore store;

TextureStore& textureStore() {
	return store;
}

}

namespace hikaru::render
{

static u32 frameNumber;
static u32 nextVersion = 1;

static u32 newVersion()
{
	if (++nextVersion == 0)
		nextVersion = 1;
	return nextVersion;
}

static int allocEntry()
{
	// store.mutex must be held
	for (size_t i = 0; i < store.entries.size(); i++)
		if (store.entries[i].version == 0 && store.entries[i].texels.empty())
			return (int)i;
	store.entries.emplace_back();
	return (int)store.entries.size() - 1;
}

static void freeEntry(int id)
{
	// store.mutex must be held
	TextureEntry& e = store.entries[id];
	e = TextureEntry{};
}

//
// Textures
//
struct TexSlot
{
	int id;
	u32 width, height;
	u32 levels;			// mip levels, largest first
	u32 convertedGen;	// texture RAM generation of the last conversion, 0 if never converted
	u32 lastUsed;		// frame number
};

static std::unordered_map<u32, TexSlot> texSlots;
static std::vector<u32> texBuffer;

// 0x0C1 bits 16-17: filter. 0 nearest, 1 trilinear, 2 bilinear without mipmaps, 3 sampler default
// (trilinear, as Odin). The mip levels are the ones the game uploaded to texture RAM.
static u32 texFilter(const cp::Texture& t) {
	return (t.raw0 >> 16) & 3;
}
static bool texMipmapped(const cp::Texture& t) {
	u32 f = texFilter(t);
	return f == 1 || f == 3;
}

static u32 makeKey(const cp::Texture& t) {
	return (u32)t.sizeCode | (t.x << 6) | (t.y << 13) | (t.bank << 19) | (t.format << 20) | ((u32)texMipmapped(t) << 23);
}

static u32 log2u(u32 v)
{
	u32 l = 0;
	while ((1u << l) < v)
		l++;
	return l;
}

// Fetch a Hikaru texel: linear 2048-texel rows, texel pairs swapped within 32-bit words
static u16 texel(const u16 *page, u32 x, u32 y) {
	return page[((y & 1023) * 2048 + (x & 2047)) ^ 1];
}

static u32 rgba(u32 r, u32 g, u32 b, u32 a) {
	return r | (g << 8) | (b << 16) | (a << 24);
}
static u32 expand5(u32 v) {
	return (v << 3) | (v >> 2);
}
static u32 expand4(u32 v) {
	return v * 0x11;
}

// ABGR1555: R in the low bits
static u32 abgr1555(u16 v) {
	return rgba(expand5(v & 0x1f), expand5((v >> 5) & 0x1f), expand5((v >> 10) & 0x1f), (v & 0x8000) ? 0xff : 0);
}
// ABGR4444: R in the low bits
static u32 abgr4444(u16 v) {
	return rgba(expand4(v & 0xf), expand4((v >> 4) & 0xf), expand4((v >> 8) & 0xf), expand4(v >> 12));
}

// Convert a Hikaru texture into the texture store. Returns nullptr if unsupported.
static const TexSlot *convertTexture(const cp::Texture& t)
{
	u32 width = 16 << (t.sizeCode & 7);
	u32 height = 16 << ((t.sizeCode >> 3) & 7);
	switch (t.format)
	{
	case 0:		// ABGR1555
	case 1:		// ABGR4444
	case 4:		// LA8: luminance in the low byte, alpha in the high byte
		break;
	case 2:		// I4: 2x2 texels per word, alpha = intensity
	case 3:		// I4, opaque (Demul 2025)
		width *= 2;
		height *= 2;
		break;
	default:
		return nullptr;
	}
	if (width > 2048 || height > 2048)
		return nullptr;

	// Formats 2 and 3 (I4) have no mip chain (Demul 2025). Other chains go down to a smaller side of 1 texel.
	u32 levels = 1;
	if (texMipmapped(t) && t.format != 2 && t.format != 3)
		levels = log2u(std::min(width, height)) + 1;

	u32 key = makeKey(t);
	auto it = texSlots.find(key);
	if (it == texSlots.end())
	{
		std::lock_guard<std::mutex> lock(store.mutex);
		TexSlot slot { allocEntry(), width, height, levels, 0, 0 };
		it = texSlots.emplace(key, slot).first;
	}
	TexSlot& slot = it->second;
	if (slot.lastUsed == frameNumber && slot.convertedGen != 0)
		return &slot;
	slot.lastUsed = frameNumber;

	// Only convert again when the texture RAM it comes from was written since
	{
		u32 gen = 0;
		u32 bank = t.bank;
		u32 y = t.y * 16;
		for (u32 l = 0; l < levels; l++)
		{
			u32 rows = std::min(height >> l, 1024u - std::min(y, 1024u));
			gen = std::max(gen, gpu::texRamGeneration((bank ? 0xc00000 : 0x800000) + y * 4096, rows * 4096));
			y += (1024 - y) / 2;
			bank ^= 1;
		}
		if (slot.convertedGen != 0 && gen <= slot.convertedGen)
			return &slot;
		slot.convertedGen = gpu::texRamCurrentGeneration();
	}

	// Level n+1 lies halfway from level n to the right and bottom edges of the 2048x1024 bank,
	// in the other bank (the upload engine stores the mip chain this way).
	u32 total = 0;
	for (u32 l = 0; l < levels; l++)
		total += (width >> l) * (height >> l);
	texBuffer.resize(total);
	u32 *dst = texBuffer.data();
	u32 bank = t.bank;
	u32 x0 = t.x * 16;
	u32 y0 = t.y * 16;
	for (u32 l = 0; l < levels; l++)
	{
		const u16 *page = (const u16 *)(getTexRam() + (bank ? 0xc00000 : 0x800000));
		u32 w = width >> l;
		u32 h = height >> l;
		if (t.format == 2 || t.format == 3)
		{
			// I4: each 16-bit word is a 2x2 block of 4-bit intensities, top-left in bits 12-15.
			// Grey. Format 2: alpha = intensity (Odin), format 3: opaque (Demul 2025).
			for (u32 by = 0; by < h / 2; by++)
				for (u32 bx = 0; bx < w / 2; bx++)
				{
					u16 v = texel(page, x0 + bx, y0 + by);
					for (int i = 0; i < 4; i++)
					{
						u32 n = expand4((v >> (12 - i * 4)) & 0xf);
						dst[(by * 2 + i / 2) * w + bx * 2 + (i & 1)] = rgba(n, n, n, t.format == 2 ? n : 0xff);
					}
				}
		}
		else
		{
			for (u32 y = 0; y < h; y++)
				for (u32 x = 0; x < w; x++)
				{
					u16 v = texel(page, x0 + x, y0 + y);
					u32 c;
					switch (t.format)
					{
					case 0:
						c = abgr1555(v);
						break;
					case 1:
						c = abgr4444(v);
						break;
					default:	// 4: LA8, luminance in the low byte, alpha in the high byte (Demul 2025, Odin)
						{
							u32 lum = v & 0xff;
							c = rgba(lum, lum, lum, v >> 8);
							break;
						}
					}
					dst[y * w + x] = c;
				}
		}
		dst += w * h;
		x0 += (2048 - x0) / 2;
		y0 += (1024 - y0) / 2;
		bank ^= 1;
	}

	std::lock_guard<std::mutex> lock(store.mutex);
	TextureEntry& e = store.entries[slot.id];
	e.width = width;
	e.height = height;
	e.levels = levels;
	e.texels.swap(texBuffer);
	e.version = newVersion();
	e.dirtyY0 = 0;
	e.dirtyY1 = height;
	return &slot;
}

// Textures not used for a while give their memory back
static void evictTextures()
{
	constexpr u32 MAX_UNUSED_FRAMES = 600;
	std::lock_guard<std::mutex> lock(store.mutex);
	for (auto it = texSlots.begin(); it != texSlots.end(); )
	{
		if (frameNumber - it->second.lastUsed > MAX_UNUSED_FRAMES)
		{
			freeEntry(it->second.id);
			it = texSlots.erase(it);
		}
		else
			++it;
	}
}

//
// Geometry
//

// Projection: eye space (looking down -Z, +Y up) to screen pixels (+Y down, centre of projection cy
// measured from the top) times w, with w = -z. Row-vector convention.
static Mat4 projection(const cp::Viewport& vp)
{
	return Mat4 {
		vp.fx, 0, 0, 0,
		0, -vp.fy, 0, 0,
		-vp.cx, -vp.cy, -1, -1,
		0, 0, 0, 0
	};
}

// 2D batches: positions are already in viewport pixels, with y up from the viewport bottom
static Mat4 projection2D(const cp::Viewport& vp)
{
	return Mat4 {
		1, 0, 0, 0,
		0, -1, 0, 0,
		0, 0, 0, 0,
		0, vp.maxy + vp.miny, 1, 1
	};
}

static const Mat4 IDENTITY { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

struct BuildState
{
	DrawList *dl;
	const cp::Frame *frame;
	std::unordered_map<int, int> matrixMap;		// frame matrix -> draw list matrix
	std::unordered_map<int, int> projMap;		// viewport * 2 + is2D -> draw list matrix
	std::unordered_map<u64, int> shadingMap;
	std::vector<u32> remap;						// frame vertex -> draw list vertex, valid when stamp matches
	std::vector<u32> stamp;
	u32 drawStamp = 0;
	int identity = 0;

	int addMatrix(const Mat4& m) {
		dl->matrices.push_back(m);
		return (int)dl->matrices.size() - 1;
	}
	int frameMatrix(int idx)
	{
		if (idx < 0)
			return identity;
		auto it = matrixMap.find(idx);
		if (it != matrixMap.end())
			return it->second;
		int n = addMatrix(frame->matrices[idx]);
		matrixMap[idx] = n;
		return n;
	}
	int projectionOf(int viewport, bool is2D)
	{
		int key = viewport * 2 + is2D;
		auto it = projMap.find(key);
		if (it != projMap.end())
			return it->second;
		const cp::Viewport& vp = frame->viewports[viewport];
		int n = addMatrix(is2D ? projection2D(vp) : projection(vp));
		projMap[key] = n;
		return n;
	}
};

// Depth queue (0x621 bit 17, type 0): opaque polygons fade with their eye depth into what is behind
// them. Not the background and translucent types (Odin heuristic, from the PHARRIER cabinet).
static bool depthCued(const cp::Batch& b, const cp::Viewport& vp)
{
	return !b.is2D && (vp.fogFlags & 2) != 0 && ((vp.fogFlags >> 2) & 7) == 0
			&& vp.fogDensity > 0.f && vp.fogW3 > 0.f
			&& b.polyType != 5 && !b.translucent;
}

// Hikaru shading record of a batch (Demul 2025 / Odin model):
//	color = texture mode combination of the texel and polygon colors 0 and 1 (0x091 sub 0, 1)
//	lit (material mode 1 or 2): color *= ambient + sum(lights); color += specular * sum(light specular)
//	ambient = mode 1: viewport ambient * material color (0x091 sub 3), mode 2: material color
static int addShading(BuildState& bs, const cp::Batch& b, const cp::Viewport& vp,
		const cp::Material *mat, const cp::Texture *tex)
{
	const cp::Frame& frame = *bs.frame;
	u32 lodBias = tex != nullptr ? (tex->raw0 >> 20) & 0xff : 0;
	u64 key = (u64)(u32)b.viewport << 40 | (u64)(u32)(b.material + 1) << 16 | (u64)(tex ? tex->combine + 1 : 0) << 4 | b.polyType | (u64)b.doubleSided << 3;
	key = key * 0x100000001b3ull ^ (lodBias | (u64)b.is2D << 8 | (u64)(b.depthBias & 0x3ff) << 9);
	for (int i = 0; i < 4; i++)
		key = key * 0x100000001b3ull ^ (u64)(u32)(b.lights[i] + 1);
	auto it = bs.shadingMap.find(key);
	if (it != bs.shadingMap.end())
		return it->second;

	Shading sh {};
	auto copy4 = [](float *dst, const cp::Vec4& v) { memcpy(dst, v.data(), sizeof(float) * 4); };
	if (mat != nullptr)
	{
		copy4(sh.base0, mat->diffuse1);
		copy4(sh.base1, mat->diffuse2);
		sh.modes[3] = mat->altDiffuse;
	}
	else
	{
		copy4(sh.base0, { 1.f, 1.f, 1.f, 1.f });
		copy4(sh.base1, { 1.f, 1.f, 1.f, 1.f });
	}
	sh.modes[1] = tex ? tex->combine : 0;
	bool lit = !b.is2D && mat != nullptr && (mat->shading == 1 || mat->shading == 2);
	sh.modes[0] = lit;
	if (lit)
	{
		for (int c = 0; c < 3; c++)
			sh.ambient[c] = mat->shading == 1 ? vp.ambient[c] * mat->unknown[c] : mat->unknown[c];
		sh.specular[0] = mat->specular[0];
		sh.specular[1] = mat->specular[1];
		sh.specular[2] = mat->specular[2];
		sh.specular[3] = mat->specular[3] * 255.f;	// power
		int count = 0;
		for (int i = 0; i < 4; i++)
		{
			if (b.lights[i] < 0)
				continue;
			const cp::Light& l = frame.lights[b.lights[i]];
			ShadingLight& hl = sh.lights[count++];
			copy4(hl.color, l.color);
			copy4(hl.ambient, l.ambient);
			bool positional = l.dir[3] != 1.f && l.pos[3] == 1.f;
			if (positional)
			{
				memcpy(hl.vec, l.pos.data(), sizeof(float) * 3);
				hl.vec[3] = 1.f;
			}
			else if (l.dir[3] == 1.f)
				memcpy(hl.vec, l.dir.data(), sizeof(float) * 3);
			else
				hl.color[0] = hl.color[1] = hl.color[2] = 0.f;	// ambient only
			hl.att[0] = l.attK;
			hl.att[1] = l.attB;
			hl.att[2] = l.noAtten ? 1.f : 0.f;
			hl.att[3] = (float)(l.attMode & 3);
			if (l.spotScale != 0.f && l.spotAxis[3] == 1.f)
			{
				// 0x451 cone around the 0xB61 axis: the light fades with
				// saturate((L . axis towards the light - cos outer) * scale) (Odin, from the X360 port)
				float len = std::sqrt(l.spotAxis[0] * l.spotAxis[0] + l.spotAxis[1] * l.spotAxis[1] + l.spotAxis[2] * l.spotAxis[2]);
				if (len > 0.f)
				{
					for (int c = 0; c < 3; c++)
						hl.spot[c] = -l.spotAxis[c] / len * l.spotScale;
					hl.spot[3] = l.cosOuter * l.spotScale;
				}
			}
		}
		sh.modes[2] = count;
	}
	if (depthCued(b, vp))
	{
		// 0x621 depth queue: the background shows through by clamp(density * (eye depth - offset)),
		// the fog offset 0x021 word 3 / 0x621 word 3 (Demul 2025 ZBLEND)
		sh.fog[0] = vp.fogDensity;
		sh.fog[1] = vp.offsetW3 / vp.fogW3;
		sh.fog[2] = 2.f;
	}
	else if (!b.is2D && mat != nullptr && mat->fog && (vp.fogFlags & 3) == 0 && vp.fogW3 != 0.f)
	{
		// visibility fa = clamp(density * (eye z + offset)), 0 = fully fogged (Demul 2025)
		copy4(sh.fogColor, vp.fogColor);
		sh.fog[0] = vp.fogDensity;
		sh.fog[1] = vp.offsetW3 / vp.fogW3;
		sh.fog[2] = 1.f;
	}
	sh.alphaTest[3] = b.doubleSided ? 1.f : 0.f;	// double-sided: back faces lit with the reversed normal
	if (b.polyType == 4) {
		// Alpha-tested: keep the 0x154 range of the material, then opaque (Demul). Games that never
		// send 0x154 (swracer) leave an empty range: Odin's punch-through rule (alpha > 0.5) then.
		if (b.alphaMin == 0.f && b.alphaMax == 0.f) {
			sh.alphaTest[0] = 0.5f + 0.5f / 255.f;
			sh.alphaTest[1] = 1.f;
		}
		else {
			sh.alphaTest[0] = b.alphaMin;
			sh.alphaTest[1] = b.alphaMax;
		}
		sh.alphaTest[2] = 1.f;
	}
	else if (b.translucent)
		sh.alphaTest[2] = 2.f;
	// 0x0C1 word 0 bits 20-27: texture LOD bias, signed, in 1/16 of a level (Demul 2025)
	sh.clipZ[2] = tex != nullptr ? (float)(s8)lodBias / 16.f : 0.f;
	// 0x101 depth bias: the depth test sees the eye depth scaled by 1 - bias * 5.6 / 65536, bias
	// bits 0-9, so that decals and shadows win over the surface they lie on (Odin). Bit 10 (two families
	// of meshes compared without bias against each other in Odin) is not emulated.
	sh.clipZ[3] = 1.f - (float)(b.depthBias & 0x3ff) * (5.6f / 65536.f);
	// The GPU clips at the near plane of the viewport (2D batches are not clipped)
	if (!b.is2D) {
		sh.clipZ[0] = std::clamp(vp.nearZ, 1e-4f, 1.f);
		sh.clipZ[1] = 1.f;
	}
	bs.dl->shadings.push_back(sh);
	int idx = (int)bs.dl->shadings.size() - 1;
	bs.shadingMap[key] = idx;
	return idx;
}

static void addBatch(BuildState& bs, const cp::Batch& b)
{
	if (b.indexCount < 3)
		return;
	const cp::Frame& frame = *bs.frame;
	DrawList& dl = *bs.dl;
	const cp::Viewport& vp = frame.viewports[b.viewport];
	const cp::Material *mat = b.material >= 0 ? &frame.materials[b.material] : nullptr;
	const cp::Texture *tex = b.texture >= 0 && mat != nullptr && mat->textured ? &frame.textures[b.texture] : nullptr;

	Draw d {};
	d.texture = -1;
	// Header bit 23 set: no culling (double-sided). Otherwise one face is culled (Demul 2025).
	// A viewport with a negative focal length (0x021 sub 0) mirrors the picture: the other face is culled.
	if (b.doubleSided || b.is2D)
		d.cull = Cull::None;
	else
		d.cull = (vp.fx < 0.f) != (vp.fy < 0.f) ? Cull::Back : Cull::Front;
	if (b.is2D || b.polyType == 5) {
		// 2D and background polygons: no depth test
		d.depthTest = false;
		d.depthWrite = false;
	}
	else {
		d.depthTest = true;
		d.depthWrite = !b.translucent;
	}
	// material blend mode (0x081 sub 5): alpha over, product, alpha added, source over
	d.blend = b.translucent ? (Blend)(1 + (b.blendMode & 3)) : Blend::Opaque;
	if (depthCued(b, vp))
		d.blend = Blend::AlphaOver;	// faded by the depth cue
	// Clip to the viewport rectangle (pixels)
	int x0 = std::clamp((int)vp.minx, 0, (int)SCREEN_WIDTH);
	int y0 = std::clamp((int)vp.miny, 0, (int)SCREEN_HEIGHT);
	d.clip[0] = (u16)x0;
	d.clip[1] = (u16)y0;
	d.clip[2] = (u16)std::clamp((int)vp.maxx + 1, x0, (int)SCREEN_WIDTH);
	d.clip[3] = (u16)std::clamp((int)vp.maxy + 1, y0, (int)SCREEN_HEIGHT);

	float uScale = 0.f, vScale = 0.f;
	const cp::Texture *shadingTex = nullptr;
	if (tex != nullptr)
	{
		if (const TexSlot *slot = convertTexture(*tex))
		{
			d.texture = slot->id;
			d.linear = texFilter(*tex) != 0;	// trilinear is a linear sampler over the mip levels
			d.mipmapped = d.linear && slot->levels > 1;
			// mirror wins over repeat
			d.addressU = tex->mirrorU ? Address::Mirror : tex->wrapU ? Address::Repeat : Address::Clamp;
			d.addressV = tex->mirrorV ? Address::Mirror : tex->wrapV ? Address::Repeat : Address::Clamp;
			float unit = tex->texelUV ? 1.f : 16.f;
			uScale = 1.f / (unit * slot->width);
			vScale = 1.f / (unit * slot->height);
			shadingTex = tex;
		}
	}
	d.shading = addShading(bs, b, vp, mat, shadingTex);
	if (b.is2D)
		d.modelview[0] = d.modelview[1] = bs.identity;
	else
	{
		d.modelview[0] = bs.frameMatrix(b.matrix);
		d.modelview[1] = bs.frameMatrix(b.matrix2);
	}
	d.projection = bs.projectionOf(b.viewport, b.is2D);
	// 0x501 pixel patterns (Demul 2025, Odin): every pixel (0), fewer down to none (8), then the
	// pixels the first codes leave out; codes 16-31 are codes 0-15 mirrored left to right
	static const u16 patterns[32] {
		0xffff, 0xfbfe, 0xfafa, 0x7ada, 0x5a5a, 0x5258, 0x5050, 0x1040,
		0x0000, 0x0401, 0x0505, 0x8525, 0xa5a5, 0xada7, 0xafaf, 0xefbf,
		0xffff, 0xfdf7, 0xf5f5, 0xe5b5, 0xa5a5, 0xa4a1, 0xa0a0, 0x8020,
		0x0000, 0x0208, 0x0a0a, 0x1a4a, 0x5a5a, 0x5b5e, 0x5f5f, 0x7fdf,
	};
	d.pattern = patterns[b.pattern & 31];
	if (d.pattern == 0)
		return;		// no pixel

	// Indexed triangles, each frame vertex sent once per draw
	bs.drawStamp++;
	d.firstIndex = (u32)dl.indices.size();
	for (u32 i = 0; i < b.indexCount; i++)
	{
		u32 vi = frame.indices[b.firstIndex + i];
		if (bs.stamp[vi] != bs.drawStamp)
		{
			const cp::Vertex& sv = frame.vertices[vi];
			DrawVertex v;
			memcpy(v.pos, sv.pos, sizeof(v.pos));
			memcpy(v.normal, sv.normal, sizeof(v.normal));
			v.uv[0] = sv.u * uScale;
			v.uv[1] = sv.v * vScale;
			v.alpha = sv.intensity * b.alpha;	// header bits 24-31: vertex alpha
			v.mode = b.is2D ? 0 : sv.mvMode;
			bs.stamp[vi] = bs.drawStamp;
			bs.remap[vi] = (u32)dl.vertices.size();
			dl.vertices.push_back(v);
		}
		dl.indices.push_back(bs.remap[vi]);
	}
	d.indexCount = (u32)dl.indices.size() - d.firstIndex;
	dl.draws.push_back(d);
}

//
// 2D layers and the final picture (Demul 2025, register block 0x1A000180)
//
// Per layer i (0: layer A, 1: layer B):
//	0x190 + i*8: base address in texture RAM / 8. 640x480 pixels, rows 0x1000 bytes apart
//	0x1A0 + i*8, 0x1A4 + i*8: scroll x (in 16-bit units: halved for 32-bit pixels), y
//	0x1B0 bit i+1: 32-bit A2BGR10 pixels (else 16-bit ABGR1555)
//	0x1B4 bit i: layer enabled
//	0x1BC bit i+1: keyed: transparent where the alpha is 0. Otherwise transparent where black.
// 0x1B8 bits 0-1: order of the three slots of the final picture. 0x1BC bit 0 keys the 3D picture.
struct LayerInfo
{
	bool enabled;
	bool keyed;
	bool bpp32;
	u32 base;
};

static LayerInfo layerInfo(int i)
{
	LayerInfo info;
	info.enabled = (gpu::layerReg(0x34 / 4) >> i) & 1;
	info.keyed = (gpu::layerReg(0x3c / 4) >> (i + 1)) & 1;
	info.bpp32 = (gpu::layerReg(0x30 / 4) >> (i + 1)) & 1;
	info.base = (gpu::layerReg((0x10 + i * 8) / 4) << 3) & 0xffffff;
	return info;
}

// One converted texture per layer and pixel size (the frame blender may read a layer with
// another pixel size than the final picture)
struct LayerTexture
{
	int id = -1;
	u32 gen = 0;			// texture RAM generation of the last conversion, 0 if none
	LayerInfo converted {};
};
static LayerTexture layerTextures[2][2];	// [layer][bpp32]

// Convert a layer into the texture store and return its id. Only the rows whose texture RAM was
// written since the last conversion are converted again, unless the layer settings changed.
// 32-bit pixels: 10-bit channels times 4, saturated (Demul 2025).
static int convertLayer(int i, const LayerInfo& info)
{
	LayerTexture& lt = layerTextures[i][info.bpp32];
	bool full = lt.gen == 0 || lt.converted.base != info.base || lt.converted.keyed != info.keyed;
	u32 since = full ? 0 : lt.gen;
	lt.gen = gpu::texRamCurrentGeneration();
	lt.converted = info;

	std::lock_guard<std::mutex> lock(store.mutex);
	if (lt.id < 0)
	{
		lt.id = allocEntry();
		full = true;
	}
	TextureEntry& e = store.entries[lt.id];
	if (e.width != SCREEN_WIDTH)
	{
		e.width = SCREEN_WIDTH;
		e.height = SCREEN_HEIGHT;
		e.levels = 1;
		e.texels.assign(SCREEN_WIDTH * SCREEN_HEIGHT, 0);
		full = true;
	}
	const u8 *texRam = getTexRam();
	u32 dirtyY0 = SCREEN_HEIGHT, dirtyY1 = 0;
	for (u32 y = 0; y < SCREEN_HEIGHT; y++)
	{
		u32 row = (info.base + y * 0x1000) & 0xffffff;
		if (!full && gpu::texRamGeneration(row, info.bpp32 ? SCREEN_WIDTH * 4 : SCREEN_WIDTH * 2) <= since)
			continue;
		dirtyY0 = std::min(dirtyY0, y);
		dirtyY1 = y + 1;
		u32 *dst = &e.texels[y * SCREEN_WIDTH];
		for (u32 x = 0; x < SCREEN_WIDTH; x++)
		{
			u32 c;
			bool alpha;
			if (info.bpp32)
			{
				u32 p;
				memcpy(&p, texRam + ((row + x * 4) & 0xffffff), 4);
				// A2BGR10: R bits 0-9, G bits 10-19, B bits 20-29, alpha bits 30-31
				c = rgba(std::min(p & 0x3ff, 0xffu), std::min((p >> 10) & 0x3ff, 0xffu), std::min((p >> 20) & 0x3ff, 0xffu), 0);
				alpha = (p >> 30) != 0;
			}
			else
			{
				u16 t;
				memcpy(&t, texRam + ((row + (x ^ 1) * 2) & 0xffffff), 2);
				c = abgr1555(t) & 0xffffff;
				alpha = (t & 0x8000) != 0;
			}
			bool visible = info.keyed ? alpha : c != 0;
			dst[x] = visible ? c | 0xff000000 : c;
		}
	}
	if (dirtyY1 != 0)
	{
		e.version = newVersion();
		if (e.dirtyY1 == 0) {
			e.dirtyY0 = dirtyY0;
			e.dirtyY1 = dirtyY1;
		}
		else {
			e.dirtyY0 = std::min(e.dirtyY0, dirtyY0);
			e.dirtyY1 = std::max(e.dirtyY1, dirtyY1);
		}
	}
	return lt.id;
}

// The frame blender and the final picture of a frame
static void setupComposite(DrawList& dl, const cp::Frame& frame)
{
	// 0x781 bits 16-17: the 3D buffer drawn into. 1A00001C bit 23 is the one shown (Demul 2025).
	dl.target = (frame.fbW781 >> 16) & 3;
	dl.displayed = (gpu::displayStatus() >> 23) & 1;

	// 0x181 bit 24: blend the 0x781 bits 24-25 source over the target, factor bits 16-23
	FrameBlend& fb = dl.blend;
	fb.enabled = (frame.fbW181 & 0x1000000) != 0;
	fb.factor = ((frame.fbW181 >> 16) & 0xff) / 255.f;
	fb.source = (frame.fbW781 >> 24) & 3;
	fb.texture = -1;
	if (fb.enabled && fb.source >= 2)
	{
		// the layer's pixel size is 0x781 bit 26
		LayerInfo info = layerInfo(fb.source - 2);
		info.bpp32 = (frame.fbW781 >> 26) & 1;
		fb.texture = convertLayer(fb.source - 2, info);
	}

	// 0x1B8 bits 0-1: slots bottom to top. 3: 3D picture, 0: layer A, 1: layer B.
	// Demul 2025 orders them 3D A B, A 3D B, A B 3D, B A 3D from the top: swracer sends 3 with its HUD
	// in the layers over the 3D picture. Odin reads bit i as layer i over the 3D picture.
	static const u8 orders[4][3] { { 1, 0, 3 }, { 1, 3, 0 }, { 3, 1, 0 }, { 3, 0, 1 } };
	const u8 *order = orders[gpu::layerReg(0x38 / 4) & 3];
	Composite& c = dl.composite;
	c.keyed3D = gpu::layerReg(0x3c / 4) & 1;
	for (int slot = 0; slot < 3; slot++)
	{
		CompositeSlot& cs = c.slots[slot];
		cs = CompositeSlot{};
		cs.texture = -1;
		if (order[slot] == 3)
		{
			cs.is3D = true;
			continue;
		}
		int i = order[slot];
		LayerInfo info = layerInfo(i);
		if (!info.enabled)
			continue;
		cs.texture = convertLayer(i, info);
		// scroll: x in 16-bit units, y in rows, added to the coordinate and wrapped
		cs.scrollX = (gpu::layerReg((0x20 + i * 8) / 4) >> (info.bpp32 ? 1 : 0)) % SCREEN_WIDTH;
		cs.scrollY = gpu::layerReg((0x24 + i * 8) / 4) % SCREEN_HEIGHT;
	}
}

// Draw lists in flight: one being built, one queued, one being drawn
static std::vector<std::shared_ptr<DrawList>> listPool;

static std::shared_ptr<DrawList> getList()
{
	for (auto& l : listPool)
		if (l.use_count() == 1)
		{
			l->clear();
			return l;
		}
	listPool.push_back(std::make_shared<DrawList>());
	return listPool.back();
}

void submit(const cp::Frame& frame)
{
	frameNumber++;
	if (frameNumber % 64 == 0)
		evictTextures();

	std::shared_ptr<DrawList> list = getList();
	DrawList& dl = *list;
	memcpy(dl.clearColor, frame.clearColor.data(), sizeof(dl.clearColor));
	dl.clearColor[3] = frame.clearColor[3] != 0.f ? 1.f : 0.f;

	BuildState bs;
	bs.dl = &dl;
	bs.frame = &frame;
	bs.identity = bs.addMatrix(IDENTITY);
	bs.remap.resize(frame.vertices.size());
	bs.stamp.assign(frame.vertices.size(), 0);

	// The target 3D buffer is cleared, then drawn in one pass per viewport priority (0x021 sub 2 word 3
	// bits 29-31), each starting with a depth clear: higher priorities always draw over lower ones.
	// Opaque first, then translucent in reverse submission order. Then the frame blender and the
	// final picture with the 2D layers.
	setupComposite(dl, frame);
	for (u32 prio = 0; prio < 8; prio++)
	{
		Pass pass { (u32)dl.draws.size(), 0 };
		// Background polygons first, without depth, for the 0x621 depth queue to fade the opaque ones
		// into them (Odin). Demul draws them after the opaque types, weighted by the depth of the scene
		// (ZBLEND): the same picture. Then opaque and alpha-tested, then translucent in reverse order.
		for (const cp::Batch& b : frame.translucent)
			if (b.polyType == 5 && frame.viewports[b.viewport].priority == prio)
				addBatch(bs, b);
		for (const cp::Batch& b : frame.opaque)
			if (frame.viewports[b.viewport].priority == prio)
				addBatch(bs, b);
		for (auto it = frame.translucent.rbegin(); it != frame.translucent.rend(); ++it)
			if (it->polyType != 5 && frame.viewports[it->viewport].priority == prio)
				addBatch(bs, *it);
		pass.drawCount = (u32)dl.draws.size() - pass.firstDraw;
		if (pass.drawCount != 0)
			dl.passes.push_back(pass);
	}

	TA_context *ctx = tactx_Alloc();
	rend_context& rc = ctx->rend;
	rc.Clear();
	rc.isRTT = false;
	rc.clearFramebuffer = true;
	rc.swapInterval = 1;
	rc.hikaru = list;
	rend_start_render(ctx);
}

void reset()
{
	texSlots.clear();
	frameNumber = 0;
	for (auto& l : layerTextures)
		for (auto& t : l)
			t = LayerTexture{};
	std::lock_guard<std::mutex> lock(store.mutex);
	store.entries.clear();
}

}
