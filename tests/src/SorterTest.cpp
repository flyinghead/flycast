#include <gtest/gtest.h>

#include "rend/sorter.h"
#include "hw/pvr/ta_ctx.h"

// setFirstProvokingVertex adapts Dreamcast flat shading (provoking vertex =
// LAST vertex of each triangle) to backends whose rasterizer reads the FIRST
// vertex (Vulkan without VK_EXT_provoking_vertex, D3D).
//
// For triangle strips the vertex colors are copied in place (each strip
// triangle k has a unique first vertex k, and the forward pass reads every
// source before overwriting it). For the depth-sorted triangle list this is
// unsound: strip emission shares vertices between triangles — tri0=(P0,P1,P2)
// tri1=(P2,P1,P3) — so P2 is tri0's source AND tri1's destination, and the
// depth-sorted processing order decides which wins. Consecutive triangles can
// even share their first vertex outright, which can't hold two different flat
// colors at all. The sorted path must therefore rotate each triangle's
// indices (a,b,c) -> (c,a,b) instead of copying colors: winding is preserved
// and every triangle keeps its own provoking color.
// Regression coverage for the THPS2 / Mat Hoffman shadow-decal bug.

namespace {

class SorterTest : public ::testing::Test
{
protected:
	rend_context ctx;

	void addVertex(u8 colorSeed)
	{
		Vertex v{};
		for (int i = 0; i < 4; i++) {
			v.col[i] = colorSeed;
			v.spc[i] = colorSeed + 0x40;
			v.col1[i] = colorSeed + 0x80;
			v.spc1[i] = colorSeed + 0xC0;
		}
		ctx.verts.push_back(v);
	}

	PolyParam makeTrPoly(bool gouraud, u32 first, u32 count)
	{
		PolyParam pp{};
		pp.pcw.Gouraud = gouraud;
		pp.first = first;
		pp.count = count;
		return pp;
	}
};

// Flat poly in the depth-sorted list: indices must be rotated so the DC
// provoking (last) vertex becomes the first, and NO vertex data may change —
// the vertices are shared between triangles.
TEST_F(SorterTest, SortedFlatRotatesIndicesWithoutTouchingVertices)
{
	for (u8 seed = 0; seed < 4; seed++)
		addVertex(seed + 1);
	// 4-vertex strip (a shadow decal quad) emitted as a sorted triangle list:
	// tri0 = (P0,P1,P2), tri1 = (P2,P1,P3) — same emission order as
	// ta_util.cpp sortTriangles().
	ctx.idx = { 0, 1, 2, 2, 1, 3 };
	ctx.global_param_tr.push_back(makeTrPoly(false, 0, 4));
	ctx.sortedTriangles.push_back({ 0, 0, 6 });

	const std::vector<Vertex> vertsBefore = ctx.verts;

	setFirstProvokingVertex(ctx);

	// (a,b,c) -> (c,a,b): provoking vertex first, winding preserved.
	const std::vector<u32> expected = { 2, 0, 1, 3, 2, 1 };
	EXPECT_EQ(ctx.idx, expected);

	for (size_t i = 0; i < ctx.verts.size(); i++) {
		EXPECT_EQ(0, memcmp(ctx.verts[i].col, vertsBefore[i].col, 4)) << "vertex " << i;
		EXPECT_EQ(0, memcmp(ctx.verts[i].spc, vertsBefore[i].spc, 4)) << "vertex " << i;
		EXPECT_EQ(0, memcmp(ctx.verts[i].col1, vertsBefore[i].col1, 4)) << "vertex " << i;
		EXPECT_EQ(0, memcmp(ctx.verts[i].spc1, vertsBefore[i].spc1, 4)) << "vertex " << i;
	}
}

// Gouraud polys are interpolated; neither indices nor vertices may change.
TEST_F(SorterTest, SortedGouraudUntouched)
{
	for (u8 seed = 0; seed < 4; seed++)
		addVertex(seed + 1);
	ctx.idx = { 0, 1, 2, 2, 1, 3 };
	ctx.global_param_tr.push_back(makeTrPoly(true, 0, 4));
	ctx.sortedTriangles.push_back({ 0, 0, 6 });

	setFirstProvokingVertex(ctx);

	const std::vector<u32> expected = { 0, 1, 2, 2, 1, 3 };
	EXPECT_EQ(ctx.idx, expected);
}

// Strip path (no sorted triangles): flat strips still copy the last vertex's
// colors onto each triangle's first vertex.
TEST_F(SorterTest, StripFlatCopiesProvokingColorForward)
{
	for (u8 seed = 0; seed < 4; seed++)
		addVertex(seed + 1);
	ctx.idx = { 0, 1, 2, 3 };
	ctx.global_param_tr.push_back(makeTrPoly(false, 0, 4));

	setFirstProvokingVertex(ctx);

	// tri0 = (0,1,2) -> v0 takes v2's original color; tri1 = (1,2,3) -> v1
	// takes v3's. Sources v2/v3 keep their own colors.
	EXPECT_EQ(3, ctx.verts[0].col[0]);
	EXPECT_EQ(4, ctx.verts[1].col[0]);
	EXPECT_EQ(3, ctx.verts[2].col[0]);
	EXPECT_EQ(4, ctx.verts[3].col[0]);
	// Indices are untouched on the strip path.
	const std::vector<u32> expected = { 0, 1, 2, 3 };
	EXPECT_EQ(ctx.idx, expected);
}

} // namespace
