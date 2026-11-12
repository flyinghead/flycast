/*
	Sega Hikaru GPU command processor.

	Walks the command list and turns it into a Frame: transformed-later vertices, triangle
	indices and batches carrying snapshots of the material, texture, light, matrix and
	viewport state. See HIKARU_SPEC.md sections 11 and 12.

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
#include "hikaru_cp.h"
#include "hikaru.h"
#include "hikaru_render.h"

#include "stdclass.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace hikaru::cp
{

// Safety net against runaway lists
constexpr u32 MAX_COMMANDS = 4'000'000;

static u32 frameCount;
static Frame frames[2];
static int curFrame;

static const Matrix Identity { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1 };

void Frame::clear()
{
	vertices.clear();
	indices.clear();
	opaque.clear();
	translucent.clear();
	matrices.clear();
	viewports.clear();
	materials.clear();
	textures.clear();
	lights.clear();
}

//
// Processor state. Tables persist across lists (lists commit records once and select them later).
//
struct LightSet
{
	u16 light[4];
	bool valid;
};

template<typename T>
struct Table
{
	T staging {};
	u32 base = 0;
	u32 current = 0;	// last 0x083 / 0x0C3 bit 12 index; the slot used is base + current
	std::unordered_map<u32, T> slots;

	// 0x084 / 0x0C4: bits 16-31 are added to the base (bit 12 set) or to the current index
	// (Demul 2025)
	void commit(u32 word) {
		slots[((word & 0x1000 ? base : current) + (word >> 16)) & 0xffff] = staging;
	}
	// 0x083 / 0x0C3: bit 12 sets the current index, else the base. The polygons that follow take
	// slot base + current, the sum made when each polygon starts (Demul 2025).
	void select(u32 word) {
		if (word & 0x1000)
			current = word >> 16;
		else
			base = word >> 16;
	}
	// offset 1: the slot after the one in use, latched by 0x158 / 0x0E8 bit 16 (Demul 2025)
	const T *get(u32 offset = 0) const {
		auto it = slots.find((base + current + offset) & 0xffff);
		return it == slots.end() ? nullptr : &it->second;
	}
};

static Viewport viewports[8];
static Viewport vpStaging;
static int curViewport;
static int vpStack[8];
static int vpStackSize;
static Vec4 clearColor;
static u32 fbW181, fbW781;

static Table<Material> materials;
static Table<Texture> textures;
static Light lightStaging;
static std::unordered_map<u32, Light> lights;
static std::unordered_map<u32, LightSet> lightSets;
static u32 lightSetBase;
static int curLightSet = -1;
static bool lightEnable[4];

static Matrix matrix[2];
static bool matrixChanged[2];	// rows written since the last snapshot
static int matrixIndex[2];		// current snapshot in the frame, -1 = identity not yet pushed

// Level of detail and visibility (Demul 2025)
static float lodVec[4];		// 0x161 sub 2 vector, after the bit 12-15 transforms
static float lodSize;		// 0x055: size of the bounding sphere on the screen
static bool lodVisible;		// 0x005: the sphere touches the view volume
static int lodReg[2];		// 0x095 forms 4 and 8: -1 below, 0 between, 1 above the two sizes

static u8 polyType = 1;
static float polyAlpha = 1.f;
static float alphaTable[64][2];	// 0x154: min, max
static int depthBias;
static u8 pattern;		// 0x501 pixel pattern
// 0x101 bias and 0x501 pattern of the callers, restored on return (Demul 2025, Odin)
static std::vector<std::pair<int, u8>> callStack;
static float vertexScale;
static bool mode2D;
static u8 byteReg;

// Geometry assembly
static Frame *frame;
static bool stateDirty;
static Batch *batch;
static u32 window[3];			// strip window, oldest first
static u32 lastFamily;
static u32 count2D;				// vertices in the current 2D primitive
static u32 window2D[3];			// last 2D vertices, newest first

static u32 sp;
static bool running;
static u32 opcodeCounts[0x200];

//
// Memory
//
static u8 *gpuPtr(u32 addr)
{
	switch (addr >> 24)
	{
	case 0x40:
	case 0x41:
		return getSlaveRam() + (addr & (SLAVE_RAM_SIZE - 1));
	case 0x48:
	case 0x4c:
		return getCmdRam() + (addr & (CMDRAM_SIZE - 1));
	default:
		return nullptr;
	}
}

static u32 readWord(u32 addr)
{
	u8 *p = gpuPtr(addr);
	if (p == nullptr)
		return 0;
	u32 v;
	memcpy(&v, p, 4);
	return v;
}

static float readFloat(u32 addr)
{
	u32 w = readWord(addr);
	float f;
	memcpy(&f, &w, 4);
	return f;
}

static float asFloat(u32 w)
{
	float f;
	memcpy(&f, &w, 4);
	return f;
}

static Vec4 rgba8(u32 w) {
	return { (w & 0xff) / 255.f, ((w >> 8) & 0xff) / 255.f, ((w >> 16) & 0xff) / 255.f, (w >> 24) / 255.f };
}

//
// Matrices
//
static void snapshotMatrix(int bank)
{
	frame->matrices.push_back(matrix[bank]);
	matrixIndex[bank] = (int)frame->matrices.size() - 1;
	matrixChanged[bank] = false;
	stateDirty = true;
}

static void resetMatrices()
{
	matrix[0] = matrix[1] = Identity;
	snapshotMatrix(0);
	snapshotMatrix(1);
}

//
// Batches and vertices
//
static int pushViewport()
{
	frame->viewports.push_back(viewports[curViewport]);
	frame->viewports.back().slot = curViewport;
	return (int)frame->viewports.size() - 1;
}

static void startBatch()
{
	Batch b {};
	b.is2D = mode2D;
	b.polyType = polyType;
	b.translucent = polyType == 5 || polyType == 6;
	b.alpha = polyAlpha;
	b.depthBias = depthBias;
	b.pattern = pattern;
	if (const Texture *t = textures.get()) {
		frame->textures.push_back(*t);
		b.texture = (int)frame->textures.size() - 1;
	}
	else {
		b.texture = -1;
	}
	if (const Material *m = materials.get()) {
		frame->materials.push_back(*m);
		b.material = (int)frame->materials.size() - 1;
	}
	else {
		b.material = -1;
	}
	if (b.material >= 0) {
		const Material& m = frame->materials[b.material];
		b.blendMode = m.modeB;
		b.alphaMin = alphaTable[m.alphaIndex][0];
		b.alphaMax = alphaTable[m.alphaIndex][1];
	}
	for (int i = 0; i < 2; i++)
		if (matrixChanged[i])
			snapshotMatrix(i);
	b.matrix = matrixIndex[0];
	b.matrix2 = matrixIndex[1];
	b.viewport = pushViewport();
	auto ls = lightSets.find(curLightSet);
	for (int i = 0; i < 4; i++)
	{
		b.lights[i] = -1;
		if (ls == lightSets.end() || curLightSet < 0 || !lightEnable[i])
			continue;
		auto l = lights.find(ls->second.light[i]);
		if (l == lights.end())
			continue;
		frame->lights.push_back(l->second);
		b.lights[i] = (int)frame->lights.size() - 1;
	}
	b.firstIndex = (u32)frame->indices.size();
	b.indexCount = 0;
	std::vector<Batch>& list = b.translucent ? frame->translucent : frame->opaque;
	list.push_back(b);
	batch = &list.back();
	stateDirty = false;
}

static void ensureBatch(u32 family, bool doubleSided)
{
	if (batch == nullptr || stateDirty || family != lastFamily || batch->doubleSided != doubleSided)
	{
		startBatch();
		batch->doubleSided = doubleSided;
	}
	lastFamily = family;
}

static void emit(u32 a, u32 b, u32 c)
{
	frame->indices.push_back(a);
	frame->indices.push_back(b);
	frame->indices.push_back(c);
	batch->indexCount += 3;
}

static u32 pushVertex(const Vertex& v)
{
	frame->vertices.push_back(v);
	return (u32)frame->vertices.size() - 1;
}

// Strip/fan assembly common to the 3D vertex commands (spec 11 Â§7.2)
// Strip assembly (Demul 2025): every vertex moves the window; bit 1 keeps its first vertex (fan).
static void rotateWindow(u32 w, u32 v)
{
	if ((w & 2) == 0)
		window[0] = window[1];
	window[1] = window[2];
	window[2] = v;
}

// A vertex or 0x158 word with any of bits 9-11 set closes a triangle on the window, bit 0 picks
// the winding (Demul 2025)
static void emitStrip(u32 w)
{
	if ((w & 0xe00) == 0 || batch == nullptr)
		return;
	if (w & 1)
		emit(window[0], window[1], window[2]);
	else
		emit(window[0], window[2], window[1]);
}

static s16 texPan[4][2];

static void applyPan(Vertex& v, u32 hdr)
{
	u32 sel = (hdr >> 18) & 3;
	if (sel != 0) {
		v.du = texPan[sel][0];
		v.dv = texPan[sel][1];
	}
}

static void vertexCommon(Vertex& v, u32 w0)
{
	v.intensity = (w0 >> 24) / 255.f;
	v.matrix2 = (w0 & 0x1000) != 0;
	// Bit 12: second modelview, bit 13: apply its 3x3, bit 14: add its translation (Odin, Demul 2025)
	v.mvMode = (v.matrix2 ? 1 : 0) | ((w0 & 0x2000) ? 0 : 2) | ((w0 & 0x4000) ? 0 : 4);
}

static u32 vertexFull(u32 pc, u32 w0)
{
	ensureBatch(0x1b8, (w0 >> 23) & 1);
	Vertex v {};
	vertexCommon(v, w0);
	for (int i = 0; i < 3; i++)
	{
		v.pos[i] = readFloat(pc + 4 + i * 4);
		v.normal[i] = readFloat(pc + 20 + i * 4);
	}
	u32 uv = readWord(pc + 16);
	v.u = (s16)uv;
	v.v = (s16)(uv >> 16);
	applyPan(v, w0);
	rotateWindow(w0, pushVertex(v));
	emitStrip(w0);
	return pc + 32;
}

// 0x158 / 0x0E8 bit 16: the polygon being built takes the texture slot after the current one
// (Demul 2025)
static void latchTexture(u32 w)
{
	if ((w & 0x10000) == 0 || batch == nullptr)
		return;
	if (const Texture *t = textures.get(1)) {
		frame->textures.push_back(*t);
		batch->texture = (int)frame->textures.size() - 1;
	}
	else {
		batch->texture = -1;
	}
}

static u32 vertexPacked(u32 pc, u32 w0)
{
	ensureBatch(0x128, (w0 >> 23) & 1);
	Vertex v {};
	vertexCommon(v, w0);
	for (int i = 0; i < 3; i++)
	{
		u32 w = readWord(pc + 4 + i * 4);
		// bits 10-31: signed position in 1/64 of the 0x901 unit (Demul 2025, Odin)
		v.pos[i] = (float)((s32)w >> 10) * (vertexScale / 64.f);
		v.normal[i] = (s16)(w << 6);
	}
	applyPan(v, w0);
	rotateWindow(w0, pushVertex(v));
	emitStrip(w0);
	return pc + 16;
}

// 0x158: texture coordinates of the last vertex, whatever command made it. Bits 18-19 replace
// its panning slot; the word closes a triangle and latches a texture like a vertex word
// (Demul 2025)
static u32 vertexTexCoords(u32 pc, u32 w0)
{
	if (!frame->vertices.empty())
	{
		Vertex& v = frame->vertices.back();
		u32 uv = readWord(pc + 4);
		v.u = (s16)uv;
		v.v = (s16)(uv >> 16);
		v.du = v.dv = 0;
		applyPan(v, w0);
	}
	emitStrip(w0);
	latchTexture(w0);
	return pc + 8;
}

static u32 vertex2D(u32 pc, u32 w0)
{
	if (lastFamily != 0x1a8 || (w0 & 4) == 0)
		count2D = 0;
	ensureBatch(0x1a8, true);
	Vertex v {};
	vertexCommon(v, w0);
	v.matrix2 = false;
	v.mvMode = 0;
	for (int i = 0; i < 3; i++)
		v.pos[i] = readFloat(pc + 4 + i * 4);
	v.pos[1] = viewports[curViewport].maxy - std::fabs(v.pos[1]);
	window2D[2] = window2D[1];
	window2D[1] = window2D[0];
	window2D[0] = pushVertex(v);
	count2D++;
	return pc + 16;
}

static u32 triTexCoords(u32 pc, u32 w0)
{
	if (lastFamily != 0x1a8 || count2D < 3)
		return pc + 16;
	u32 idx[3];
	// UV pairs: w1 = newest, w2 = previous, w3 = oldest
	for (int i = 0; i < 3; i++)
	{
		Vertex v = frame->vertices[window2D[i]];
		u32 uv = readWord(pc + 4 + i * 4);
		v.u = (s16)uv;
		v.v = (s16)(uv >> 16);
		idx[i] = pushVertex(v);
	}
	latchTexture(w0);
	emit(idx[2], idx[1], idx[0]);
	return pc + 16;
}

//
// State commands
//
static void viewportSet(u32 pc, u32 w0)
{
	u32 w1 = readWord(pc + 4), w2 = readWord(pc + 8), w3 = readWord(pc + 12);
	Viewport& v = vpStaging;
	switch ((w0 >> 9) & 7)
	{
	case 0:
		v.fx = asFloat(w1);
		v.fy = asFloat(w2);
		v.offsetW3 = asFloat(w3);
		break;
	case 1:
		v.cx = (s16)w1;
		v.cy = (s16)(w1 >> 16);
		v.minx = (s16)w2;
		v.miny = (s16)(w2 >> 16);
		v.maxx = (s16)w3;
		v.maxy = (s16)(w3 >> 16);
		break;
	case 2:
		v.nearZ = asFloat(w1);
		v.farZ = asFloat(w2);
		v.priority = w3 >> 29;
		break;
	case 3:
		v.fogColor = { (w1 & 0xff) / 255.f, ((w1 >> 8) & 0xff) / 255.f, ((w1 >> 16) & 0xff) / 255.f, 1.f };
		// flags: bit 16 disables fog, bit 17 selects depth-blend (alpha from depth), bits 18-20 its type
		v.fogEnable = (w0 & 0x30000) == 0;
		v.fogFlags = (w0 >> 16) & 0x1f;	// bit 2-4: depth queue type (Odin)
		v.fogW3 = asFloat(w3);
		v.fogDensity = asFloat(w2);
		v.fogOffset = asFloat(w3);
		break;
	}
}

static void viewportSelect(u32 w0)
{
	int idx = (w0 >> 16) & 7;
	switch ((w0 >> 13) & 3)
	{
	case 1:
		if (vpStackSize < (int)std::size(vpStack))
			vpStack[vpStackSize++] = curViewport;
		curViewport = idx;
		break;
	case 2:
		if (vpStackSize > 0)
			curViewport = vpStack[--vpStackSize];
		break;
	default:
		curViewport = idx;
		break;
	}
	if (curViewport == 7)
	{
		// reset the 3x3 part of matrix 0, keeping the translation
		for (int r = 0; r < 3; r++)
			for (int c = 0; c < 3; c++)
				matrix[0][r * 4 + c] = r == c ? 1.f : 0.f;
		snapshotMatrix(0);
	}
	stateDirty = true;
}

static void matrixCommand(u32 pc, u32 w0)
{
	float f[3] = { readFloat(pc + 4), readFloat(pc + 8), readFloat(pc + 12) };
	float fw = 0.f;
	// Bits 12-15 (Demul 2025): the vector goes through matrix 0 or 1 (bit 12) before it is used:
	// bit 13 rotates it by the 3x3 part, bit 14 adds the translation, bit 15 normalizes it.
	// The libraries build a local matrix times the modelview this way (Odin).
	{
		const Matrix& src = matrix[(w0 >> 12) & 1];
		float v[4] = { f[0], f[1], f[2], 0.f };
		if (w0 & 0x2000)
			for (int c = 0; c < 4; c++)
				v[c] = f[0] * src[c] + f[1] * src[4 + c] + f[2] * src[8 + c];
		if (w0 & 0x4000)
			for (int c = 0; c < 4; c++)
				v[c] += src[12 + c];
		if (w0 & 0x8000)
		{
			float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
			if (l != 0.f)
				for (int c = 0; c < 4; c++)
					v[c] /= l;
		}
		f[0] = v[0];
		f[1] = v[1];
		f[2] = v[2];
		fw = v[3];
	}
	switch ((w0 >> 9) & 7)
	{
	case 0:
		{
			int bank = (w0 >> 18) & 1;
			int row = (w0 >> 16) & 3;
			Matrix& m = matrix[bank];
			m[row * 4 + 0] = f[0];
			m[row * 4 + 1] = f[1];
			m[row * 4 + 2] = f[2];
			m[row * 4 + 3] = row == 3 ? 1.f : 0.f;
			matrixChanged[bank] = true;
			// bit 19: then matrices 0 and 1 swap (Demul 2025)
			if (w0 & 0x80000)
			{
				std::swap(matrix[0], matrix[1]);
				matrixChanged[0] = matrixChanged[1] = true;
			}
			stateDirty = true;
			break;
		}
	case 2:
		// level of detail vector, as transformed above (Demul 2025)
		lodVec[0] = f[0];
		lodVec[1] = f[1];
		lodVec[2] = f[2];
		lodVec[3] = fw;
		break;
	case 4:
		switch ((w0 >> 12) & 0xff)
		{
		case 0x08:
			lightStaging.dir = { f[0], f[1], f[2], 1.f };
			break;
		case 0x10:
			lightStaging.pos = { f[0], f[1], f[2], 1.f };
			break;
		}
		break;
	case 5:
		// 0xB61: spot axis, from the light to its target, in eye space (Odin)
		if (((w0 >> 12) & 0xff) == 0x08)
			lightStaging.spotAxis = { f[0], f[1], f[2], 1.f };
		break;
	}
}

static void materialColor(u32 pc, u32 w0)
{
	u32 w1 = readWord(pc + 4);
	Material& m = materials.staging;
	switch ((w0 >> 9) & 7)
	{
	case 0: m.diffuse1 = rgba8(w1); break;
	case 1: m.diffuse2 = rgba8(w1); break;
	case 2: m.specular = rgba8(w1); break;
	case 3:
		m.unknown = { ((w1 >> 16) & 0x3ff) / 255.f, (w1 & 0x3ff) / 255.f, ((w0 >> 16) & 0x3ff) / 255.f, 1.f };
		break;
	}
}

static void materialFlags(u32 w0)
{
	Material& m = materials.staging;
	switch ((w0 >> 9) & 7)
	{
	case 4:
		m.shading = (w0 >> 16) & 3;
		m.fog = (w0 >> 18) & 1;
		m.textured = (w0 >> 19) & 1;
		m.altDiffuse = (w0 >> 20) & 1;
		m.highlight = (w0 >> 21) & 1;
		break;
	case 5:
		m.modeB = (w0 >> 16) & 3;
		break;
	case 6:
		m.alphaIndex = (w0 >> 16) & 0x3f;
		break;
	}
}

static void textureParam(u32 w0)
{
	Texture& t = textures.staging;
	switch ((w0 >> 9) & 7)
	{
	case 0:
		t.raw0 = w0;
		break;
	case 1:
		t.texelUV = (w0 >> 14) & 1;
		t.sizeCode = (w0 >> 16) & 0x3f;
		t.wrapU = (w0 >> 22) & 1;
		t.wrapV = (w0 >> 23) & 1;
		t.mirrorU = (w0 >> 24) & 1;
		t.mirrorV = (w0 >> 25) & 1;
		t.format = (w0 >> 26) & 7;
		t.combine = (w0 >> 29) & 7;
		break;
	case 2:
		t.bank = (w0 >> 12) & 1;
		t.x = (w0 >> 16) & 0x7f;
		t.y = (w0 >> 24) & 0x3f;
		break;
	}
}

static void lightColor(u32 pc, u32 w0)
{
	if (((w0 >> 8) & 7) == 4)
	{
		// 0x451: spot cone (Odin). Bit 24: no cone. Word 1 bits 0-15: cos of the outer angle, 1.15 fixed point.
		// Bits 16-23: scale of the fade, 1 / (cos inner - cos outer), as 4 bits of exponent and 4 of mantissa.
		Light& l = lightStaging;
		if (w0 & 0x01000000)
		{
			l.spotScale = 0.f;
			return;
		}
		u32 w1 = readWord(pc + 4);
		l.cosOuter = (w1 & 0xffff) / 32768.f;
		u32 w = (w1 >> 16) & 0xff;
		l.spotScale = std::ldexp(1.f + (w & 0xf) / 16.f, (int)(w >> 4));
		return;
	}
	if ((w0 & 0x01000e00) != 0)
		return;
	u32 w1 = readWord(pc + 4);
	Light& l = lightStaging;
	// 10-bit components scaled by 1/255: lights may be up to 4x brighter than white (Demul 2025)
	l.color = { (w1 & 0x3ff) / 255.f, ((w1 >> 10) & 0x3ff) / 255.f, ((w1 >> 20) & 0x3ff) / 255.f, 0.f };
	float i = ((w0 >> 16) & 0xf) / 15.f;
	l.ambient = { l.color[0] * i, l.color[1] * i, l.color[2] * i, 0.f };
	l.dir[3] = 0.f;
	l.pos[3] = 0.f;
}

static void lightParam(u32 pc, u32 w0)
{
	if (((w0 >> 9) & 7) != 1)
		return;
	Light& l = lightStaging;
	l.attMode = (w0 >> 16) & 3;
	l.attK = readFloat(pc + 4);
	l.attB = readFloat(pc + 8);
	l.noAtten = l.attMode == 0 && l.attK == l.attB;
}

static void lightSetDef(u32 pc, u32 w0)
{
	u32 w1 = readWord(pc + 4), w2 = readWord(pc + 8);
	LightSet& s = lightSets[lightSetBase + ((w0 >> 16) & 0xff)];
	// light indices are 10 bits
	s.light[0] = w1 & 0x3ff;
	s.light[1] = (w1 >> 16) & 0x3ff;
	s.light[2] = w2 & 0x3ff;
	s.light[3] = (w2 >> 16) & 0x3ff;
	s.valid = true;
}

// 0x043: recall a light set. Bits 16-23: index. Bit 12 clear: the index also becomes the new base.
// Bits 24-27: disabled lights. A set never committed has no light.
static void lightSetSelect(u32 w0)
{
	u32 idx = (w0 >> 16) & 0xff;
	if ((w0 & 0x1000) == 0)
		lightSetBase = idx;
	idx += lightSetBase;
	curLightSet = lightSets.count(idx) ? (int)idx : -1;
	for (int i = 0; i < 4; i++)
		lightEnable[i] = ((w0 >> (24 + i)) & 1) == 0;
	stateDirty = true;
}

// 0x055: the radius in word 1 over the distance of the 0x161 vector, times the larger focal length
// of the viewport, in 1/16 pixel (Demul 2025)
static void lodSetSize(float radius)
{
	const Viewport& vp = viewports[curViewport];
	float d = std::sqrt(lodVec[0] * lodVec[0] + lodVec[1] * lodVec[1] + lodVec[2] * lodVec[2]);
	lodSize = radius / d * std::max(std::fabs(vp.fx), std::fabs(vp.fy)) * 16.f;
}

// 0x005: does the sphere around the 0x161 vector touch the view volume of the viewport? Bits 12-31:
// the radius. The side planes go through the eye and the edges of a 640x480 screen seen with the
// 0x021 focal lengths; the near plane is the 0x021 sub 0 word 3 (Demul 2025).
static void lodTestSphere(u32 w0)
{
	const Viewport& vp = viewports[curViewport];
	float r = asFloat(w0 & 0xfffff000);
	lodVisible = false;
	if (lodVec[2] + vp.offsetW3 > r)
		return;
	float a = 2.f / 640.f * vp.fx;
	float b = 2.f / 480.f * vp.fy;
	const float planes[4][3] { { -a, 0.f, 1.f }, { a, 0.f, 1.f }, { 0.f, b, 1.f }, { 0.f, -b, 1.f } };
	for (const auto& p : planes)
	{
		float l = std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
		float d = (lodVec[0] * p[0] + lodVec[1] * p[1] + lodVec[2] * p[2]) / l;
		if (!(d <= r))
			return;
	}
	lodVisible = true;
}

// Condition of a jump, call or return: bits 12-15, with bits 16-31 (Demul 2025)
static bool jumpCondition(u32 w0)
{
	float imm = (float)(w0 >> 16);
	switch ((w0 >> 12) & 0xf)
	{
	case 0x1: return lodSize < imm;
	case 0x4: return !lodVisible;
	case 0x5: return lodSize > imm;
	case 0x6: return lodReg[0] == 0;
	case 0x7: return lodReg[1] == 0;
	case 0x8: return lodVisible;
	case 0x9: return lodSize <= imm;
	case 0xd: return lodSize >= imm;
	default: return true;
	}
}

static u32 jumpTarget(u32 pc, u32 w0)
{
	u32 w1 = readWord(pc + 4);
	u32 t = (w0 & 0x800) ? pc + w1 * 4 : w1 * 4;
	return t;
}

// Execute one command, returning the next PC (0 halts)
static u32 execute(u32 pc)
{
	u32 w0 = readWord(pc);
	u32 op = w0 & 0x1ff;
	opcodeCounts[op]++;
	switch (op)
	{
	// No-ops
	case 0x000: case 0x013: case 0x046: case 0x006: case 0x088:
		return pc + 4;
	case 0x0d1:
		{
			u32 w1 = readWord(pc + 4);
			texPan[(w0 >> 16) & 3][0] = (s16)w1;
			texPan[(w0 >> 16) & 3][1] = (s16)(w1 >> 16);
			return pc + 8;
		}
	case 0x154:
		// alpha test range table: entry bits 16-21, min byte 4, max byte 5
		{
			u32 w1 = readWord(pc + 4);
			alphaTable[(w0 >> 16) & 0x3f][0] = (w1 & 0xff) / 255.f;
			alphaTable[(w0 >> 16) & 0x3f][1] = ((w1 >> 8) & 0xff) / 255.f;
			return pc + 8;
		}
	case 0x194:
		return pc + 8;
	case 0x158: case 0x159: case 0x15a: case 0x15b: case 0x15c: case 0x15d: case 0x15e: case 0x15f:
		return vertexTexCoords(pc, w0);
	case 0x1a1:
		return pc + 16;

	// Control flow
	case 0x012:
		{
			if (!jumpCondition(w0))
				return pc + 8;
			u32 target = jumpTarget(pc, w0);
			if (target == pc)
			{
				running = false;
				return 0;
			}
			return target;
		}
	case 0x052:
		{
			if (!jumpCondition(w0))
				return pc + 8;
			if (u8 *p = gpuPtr(sp))
			{
				u32 ret = pc + 8;
				memcpy(p, &ret, 4);
			}
			sp += 4;
			callStack.emplace_back(depthBias, pattern);
			return jumpTarget(pc, w0);
		}
	case 0x082:
		if (!jumpCondition(w0))
			return pc + 4;
		sp -= 4;
		if (!callStack.empty())
		{
			depthBias = callStack.back().first;
			pattern = callStack.back().second;
			callStack.pop_back();
			stateDirty = true;
		}
		return readWord(sp);
	case 0x1c2:
		running = false;
		return 0;

	// LOD
	case 0x005:
		lodTestSphere(w0);
		return pc + 4;
	case 0x055:
		lodSetSize(readFloat(pc + 4));
		return pc + 8;
	case 0x095:
		{
			u32 w1 = readWord(pc + 4);
			// 0x095: the 0x055 size against two sizes, words 1 bits 0-15 and 16-31 (Demul 2025)
			float lo = (float)(u16)w1, hi = (float)(w1 >> 16);
			int res = lo <= lodSize ? (hi < lodSize ? 1 : 0) : -1;
			switch (w0 & 0xf000)
			{
			case 0x4000: lodReg[0] = res; break;
			case 0x8000: lodReg[1] = res; break;
			}
			return pc + 8;
		}

	// Viewports
	case 0x003:
		viewportSelect(w0);
		return pc + 4;
	case 0x004:
		viewports[(w0 >> 16) & 7] = vpStaging;
		stateDirty = true;
		return pc + 4;
	case 0x011:
		{
			u32 w1 = readWord(pc + 4);
			vpStaging.ambient = { (w0 >> 16) / 255.f, (w1 & 0xffff) / 255.f, (w1 >> 16) / 255.f, 1.f };
			return pc + 8;
		}
	case 0x021:
		viewportSet(pc, w0);
		return pc + 16;
	case 0x191:
		// R byte 0, G byte 1, B byte 2, bit 24 = alpha. Persists across frames.
		clearColor = rgba8(readWord(pc + 4));
		frame->clearColor = clearColor;
		return pc + 8;

	// Misc state
	case 0x101:
		switch ((w0 >> 9) & 7)
		{
		case 0:
			// 11 bits, signed: bits 16-25 bias, bit 26 flag (Odin)
			depthBias = (int)((w0 >> 16) & 0x7ff) - (int)((w0 >> 15) & 0x800);
			stateDirty = true;
			break;
		case 2:
			// 0x501: the polygons that follow are drawn on the pixels of a 4x4 screen pattern only,
			// bits 16-20 (Demul 2025, Odin)
			pattern = (w0 >> 16) & 0x1f;
			stateDirty = true;
			break;
		case 4:
			// Exponent field: scale = 2^(e - 141), not wrapped (Demul builds the float exponent directly)
			vertexScale = std::ldexp(1.f, (int)((w0 >> 16) & 0xff) - 141);
			break;
		}
		return pc + 4;
	case 0x103:
	case 0x113:
		// Polygon type (bits 9-11; other values are drawn as opaque) and polygon alpha (bits 24-31).
		// The last command wins. Length: 4 << bits 4-5.
		{
			u32 type = (w0 >> 9) & 7;
			polyType = type == 4 || type == 5 || type == 6 ? type : 1;
			polyAlpha = (w0 >> 24) / 255.f;
			stateDirty = true;
			return pc + (4 << ((w0 >> 4) & 3));
		}
	case 0x181:
		switch ((w0 >> 9) & 7)
		{
		case 0:
			if (w0 & 0x1000000)
				byteReg = (w0 >> 16) & 0xff;
			fbW181 = frame->fbW181 = w0;
			break;
		case 3:
			fbW781 = frame->fbW781 = w0;
			if (w0 & 0x2000000)
				frame->layerAlpha[(w0 >> 16) & 1] = byteReg;
			mode2D = (w0 & 0x40000) == 0;
			if (mode2D)
				resetMatrices();
			stateDirty = true;
			break;
		}
		return pc + 4;
	case 0x161:
		matrixCommand(pc, w0);
		return pc + 16;

	// Materials
	case 0x081:
		materialFlags(w0);
		return pc + 4;
	case 0x091:
		materialColor(pc, w0);
		return pc + 8;
	case 0x083:
		materials.select(w0);
		stateDirty = true;
		return pc + 4;
	case 0x084:
		materials.commit(w0);
		return pc + 4;

	// Textures
	case 0x0c1:
		textureParam(w0);
		return pc + 4;
	case 0x0c3:
		textures.select(w0);
		stateDirty = true;
		return pc + 4;
	case 0x0c4:
		textures.commit(w0);
		return pc + 4;

	// Lights
	case 0x051:
		lightColor(pc, w0);
		return pc + 8;
	case 0x061:
		lightParam(pc, w0);
		return pc + 16;
	case 0x104:
		lights[(w0 >> 16) & 0x3ff] = lightStaging;
		return pc + 4;
	case 0x064:
		lightSetDef(pc, w0);
		return pc + 16;
	case 0x043:
		lightSetSelect(w0);
		return pc + 4;

	// Geometry
	case 0x1b8: case 0x1b9: case 0x1ba: case 0x1bb: case 0x1bc: case 0x1bd: case 0x1be: case 0x1bf:
		return vertexFull(pc, w0);
	case 0x128: case 0x129: case 0x12a: case 0x12b: case 0x12c: case 0x12d: case 0x12e: case 0x12f:
		return vertexPacked(pc, w0);
	case 0x1a8: case 0x1a9: case 0x1aa: case 0x1ab: case 0x1ac: case 0x1ad: case 0x1ae: case 0x1af:
		return vertex2D(pc, w0);
	case 0x0e8: case 0x0e9: case 0x0ea: case 0x0eb: case 0x0ec: case 0x0ed: case 0x0ee: case 0x0ef:
		return triTexCoords(pc, w0);

	default:
		WARN_LOG(PVR, "hikaru: unknown GPU command %08x at %08x", w0, pc);
		running = false;
		return 0;
	}
}

static void frameReset()
{
	curFrame ^= 1;
	frame = &frames[curFrame];
	frame->clear();
	frame->layerAlpha[0] = frame->layerAlpha[1] = 0xff;
	frame->clearColor = clearColor;
	frame->fbW181 = fbW181;
	frame->fbW781 = fbW781;
	batch = nullptr;
	stateDirty = true;
	lastFamily = 0;
	count2D = 0;
	memset(window, 0, sizeof(window));
	// cleared at the start of each list (Demul 2025)
	depthBias = 0;
	pattern = 0;
	callStack.clear();
	vertexScale = 1.f;
	polyType = 1;
	polyAlpha = 1.f;
	memset(alphaTable, 0, sizeof(alphaTable));
	mode2D = false;
	curViewport = 0;
	vpStackSize = 0;
	lodVisible = false;
	lodSize = 0.f;
	memset(lodVec, 0, sizeof(lodVec));
	lodReg[0] = lodReg[1] = 0;
	resetMatrices();
}

void run(u32 pc, u32 stackPtr)
{
	frameReset();
	sp = stackPtr;
	running = true;
	u32 n = 0;
	while (running && pc != 0 && gpuPtr(pc) != nullptr)
	{
		pc = execute(pc);
		// batch pointers into the lists are invalidated on reallocation
		if (batch != nullptr)
		{
			std::vector<Batch>& list = batch->translucent ? frame->translucent : frame->opaque;
			batch = &list.back();
		}
		if (++n >= MAX_COMMANDS)
		{
			WARN_LOG(PVR, "hikaru: command list too long, stopped at %08x", pc);
			break;
		}
	}
	render::submit(*frame);
	DEBUG_LOG(PVR, "hikaru: list done after %d commands at %08x: %d vertices, %d triangles, %d opaque + %d translucent batches",
			n, pc, (int)frame->vertices.size(), (int)frame->indices.size() / 3,
			(int)frame->opaque.size(), (int)frame->translucent.size());
}

const Frame& lastFrame() {
	return frames[curFrame];
}

void vblank()
{
	frameCount++;
	if (frameCount % 300 == 0)
	{
		std::string s;
		for (u32 i = 0; i < std::size(opcodeCounts); i++)
			if (opcodeCounts[i] != 0)
				s += strprintf("%03x:%d ", i, opcodeCounts[i]);
		DEBUG_LOG(PVR, "hikaru: GPU command counts: %s", s.c_str());
	}
}

void init() {
}

void term()
{
	frames[0] = Frame();
	frames[1] = Frame();
}

//
// Save states: the persistent processor state (tables, staging records, matrices, lights...).
// Games send static state once, so it must survive a reload.
//
template<typename K, typename V>
static void serializeMap(Serializer& ser, const std::unordered_map<K, V>& m)
{
	ser << (u32)m.size();
	for (const auto& [k, v] : m) {
		ser << k;
		ser << v;
	}
}
template<typename K, typename V>
static void deserializeMap(Deserializer& deser, std::unordered_map<K, V>& m)
{
	m.clear();
	u32 n;
	deser >> n;
	for (u32 i = 0; i < n; i++) {
		K k;
		V v;
		deser >> k;
		deser >> v;
		m[k] = v;
	}
}
template<typename T>
static void serializeTable(Serializer& ser, const Table<T>& t)
{
	ser << t.staging;
	ser << t.base;
	ser << t.current;
	serializeMap(ser, t.slots);
}
template<typename T>
static void deserializeTable(Deserializer& deser, Table<T>& t)
{
	deser >> t.staging;
	deser >> t.base;
	deser >> t.current;
	deserializeMap(deser, t.slots);
}

void serialize(Serializer& ser)
{
	ser << viewports;
	ser << vpStaging;
	ser << curViewport;
	ser << vpStack;
	ser << vpStackSize;
	ser << clearColor;
	ser << fbW181;
	ser << fbW781;
	serializeTable(ser, materials);
	serializeTable(ser, textures);
	ser << lightStaging;
	serializeMap(ser, lights);
	serializeMap(ser, lightSets);
	ser << lightSetBase;
	ser << curLightSet;
	ser << lightEnable;
	ser << matrix;
	ser << lodSize;
	ser << lodVisible;
	ser << (u16)0;
	ser << polyType;
	ser << polyAlpha;
	ser << alphaTable;
	ser << depthBias;
	ser << vertexScale;
	ser << mode2D;
	ser << byteReg;
	ser << texPan;
}

// Lights are saved as they are in memory. Before V60 they had no spot light fields.
struct LightV59
{
	Vec4 ambient;
	Vec4 color;
	Vec4 dir;
	Vec4 pos;
	float attK, attB;
	bool noAtten;
	u8 attMode;
};

static void deserializeLight(Deserializer& deser, Light& l)
{
	if (deser.version() >= Deserializer::V60)
	{
		deser >> l;
		return;
	}
	LightV59 old;
	deser >> old;
	l = Light{};
	l.ambient = old.ambient;
	l.color = old.color;
	l.dir = old.dir;
	l.pos = old.pos;
	l.attK = old.attK;
	l.attB = old.attB;
	l.noAtten = old.noAtten;
	l.attMode = old.attMode;
}

void deserialize(Deserializer& deser)
{
	deser >> viewports;
	deser >> vpStaging;
	deser >> curViewport;
	deser >> vpStack;
	deser >> vpStackSize;
	deser >> clearColor;
	deser >> fbW181;
	deser >> fbW781;
	deserializeTable(deser, materials);
	deserializeTable(deser, textures);
	deserializeLight(deser, lightStaging);
	{
		lights.clear();
		u32 n;
		deser >> n;
		for (u32 i = 0; i < n; i++)
		{
			u32 k;
			deser >> k;
			deserializeLight(deser, lights[k]);
		}
	}
	deserializeMap(deser, lightSets);
	deser >> lightSetBase;
	deser >> curLightSet;
	deser >> lightEnable;
	deser >> matrix;
	deser >> lodSize;
	deser >> lodVisible;
	{
		u16 unused;
		deser >> unused;
	}
	deser >> polyType;
	deser >> polyAlpha;
	deser >> alphaTable;
	deser >> depthBias;
	deser >> vertexScale;
	deser >> mode2D;
	deser >> byteReg;
	deser >> texPan;
	stateDirty = true;
}

void reset()
{
	frameCount = 0;
	for (Viewport& v : viewports)
		v = Viewport {};
	vpStaging = Viewport {};
	materials = Table<Material>();
	textures = Table<Texture>();
	lights.clear();
	lightSets.clear();
	lightSetBase = 0;
	curLightSet = -1;
	lightStaging = Light {};
	memset(lightEnable, 0, sizeof(lightEnable));
	memset(opcodeCounts, 0, sizeof(opcodeCounts));
	frames[0].clear();
	frames[1].clear();
	frame = &frames[curFrame];
}

}
