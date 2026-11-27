#include <gtest/gtest.h>

#include "hw/pvr/ta.h"
#include "hw/pvr/ta_ctx.h"

#include <cmath>

// ta_clip_modvols: games clamp the 1/w of a modifier volume vertex that reaches the camera (Jet Set
// Radio: 100000) and send its projected position as is, so a volume around the camera has vertices
// at x, y ~ 1e10. A GPU interpolates the depth of such a triangle across the screen with float error
// far above the gap to the geometry behind it, the volume's parity flips on part of the screen and
// its shadow becomes a flickering black area. The triangles are clipped to a band around the screen;
// inside the band nothing may change: the same pixels covered, the same z (1/w is affine in screen
// space), the same winding, and every volume still summed.

namespace {

constexpr float Band = 4096.f;

// z = 1/w of a plane, affine in screen space
float planeZ(float x, float y) { return 0.002f + 1e-7f * x - 3e-8f * y; }

ModTriangle tri(float x0, float y0, float x1, float y1, float x2, float y2)
{
	return { x0, y0, planeZ(x0, y0), x1, y1, planeZ(x1, y1), x2, y2, planeZ(x2, y2) };
}

double signedArea(const ModTriangle& t)
{
	return ((double)t.x1 - t.x0) * ((double)t.y2 - t.y0) - ((double)t.x2 - t.x0) * ((double)t.y1 - t.y0);
}

bool covers(const ModTriangle& t, double px, double py)
{
	auto edge = [](double ax, double ay, double bx, double by, double px, double py) {
		return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
	};
	const double e0 = edge(t.x0, t.y0, t.x1, t.y1, px, py);
	const double e1 = edge(t.x1, t.y1, t.x2, t.y2, px, py);
	const double e2 = edge(t.x2, t.y2, t.x0, t.y0, px, py);
	return (e0 > 0 && e1 > 0 && e2 > 0) || (e0 < 0 && e1 < 0 && e2 < 0);
}

class ModVolClipTest : public ::testing::Test
{
protected:
	rend_context ctx;

	void addVolume(std::vector<ModTriangle> tris, bool translucent = false)
	{
		ModifierVolumeParam p{};
		p.init();
		p.first = (u32)ctx.modtrig.size();
		p.count = (u32)tris.size();
		p.isp.VolumeLast = 1;
		p.isp.DepthMode = 1;
		for (const ModTriangle& t : tris)
			ctx.modtrig.push_back(t);
		(translucent ? ctx.global_param_mvo_tr : ctx.global_param_mvo).push_back(p);
	}
};

TEST_F(ModVolClipTest, TrianglesInsideTheBandAreUntouched)
{
	addVolume({ tri(0, 0, 640, 0, 0, 480), tri(-4000, -4000, 4000, 100, 300, 4000) });
	const std::vector<ModTriangle> before = ctx.modtrig;
	ta_clip_modvols(ctx);
	ASSERT_EQ(before.size(), ctx.modtrig.size());
	EXPECT_EQ(0, memcmp(before.data(), ctx.modtrig.data(), before.size() * sizeof(ModTriangle)));
	EXPECT_EQ(0u, ctx.global_param_mvo[0].first);
	EXPECT_EQ(2u, ctx.global_param_mvo[0].count);
}

TEST_F(ModVolClipTest, HugeTriangleIsClippedExactly)
{
	const ModTriangle huge = tri(-2.3e9f, -1.1e9f, 1.0e10f, -4.7e9f, 4.0e8f, 1.6e10f);
	addVolume({ huge });
	ta_clip_modvols(ctx);
	auto dump = [this]() {
		std::string d;
		for (const ModTriangle& t : ctx.modtrig)
			d += "(" + std::to_string(t.x0) + "," + std::to_string(t.y0) + " " + std::to_string(t.x1) + "," + std::to_string(t.y1)
				+ " " + std::to_string(t.x2) + "," + std::to_string(t.y2) + ") ";
		return d;
	};

	const ModifierVolumeParam& p = ctx.global_param_mvo[0];
	ASSERT_GT(p.count, 0u);
	ASSERT_EQ(p.first + p.count, ctx.modtrig.size());
	const bool ccw = signedArea(huge) > 0;
	// the plane through the triangle's own (float) vertices: what the clipped vertices must lie on
	const double ux = (double)huge.x1 - huge.x0, uy = (double)huge.y1 - huge.y0, uz = (double)huge.z1 - huge.z0;
	const double vx = (double)huge.x2 - huge.x0, vy = (double)huge.y2 - huge.y0, vz = (double)huge.z2 - huge.z0;
	const double nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
	auto zAt = [&](double x, double y) { return huge.z0 - (nx * (x - huge.x0) + ny * (y - huge.y0)) / nz; };
	for (const ModTriangle& t : ctx.modtrig)
	{
		const float xs[] { t.x0, t.x1, t.x2 }, ys[] { t.y0, t.y1, t.y2 }, zs[] { t.z0, t.z1, t.z2 };
		for (int k = 0; k < 3; k++)
		{
			EXPECT_GE(xs[k], -Band); EXPECT_LE(xs[k], Band);
			EXPECT_GE(ys[k], -Band); EXPECT_LE(ys[k], Band);
			EXPECT_NEAR(zAt(xs[k], ys[k]), zs[k], 1e-6);
		}
		if (signedArea(t) != 0)
			EXPECT_EQ(ccw, signedArea(t) > 0);
	}
	// sample points of the screen and around it are covered as before, exactly once (off pixel centers, so
	// that none lies exactly on an edge two clipped triangles share, which a GPU's fill rule decides)
	for (int y = -600; y <= 1100; y += 37)
		for (int x = -700; x <= 1300; x += 41)
		{
			int count = 0;
			for (const ModTriangle& t : ctx.modtrig)
				count += covers(t, x + 0.37, y + 0.61);
			EXPECT_EQ(covers(huge, x + 0.37, y + 0.61) ? 1 : 0, count) << x << "," << y << " " << dump();
		}
}

TEST_F(ModVolClipTest, FollowingVolumesAreReindexed)
{
	addVolume({ tri(0, 0, 640, 0, 0, 480) });
	addVolume({ tri(-1e10f, -1e10f, 1e10f, -1e10f, 0, 1e10f), tri(10, 10, 20, 10, 10, 20) });
	addVolume({ tri(100, 100, 200, 100, 100, 200) }, true);
	ta_clip_modvols(ctx);

	const ModifierVolumeParam& a = ctx.global_param_mvo[0];
	const ModifierVolumeParam& b = ctx.global_param_mvo[1];
	const ModifierVolumeParam& c = ctx.global_param_mvo_tr[0];
	EXPECT_EQ(0u, a.first);
	EXPECT_EQ(1u, a.count);
	EXPECT_EQ(a.first + a.count, b.first);
	EXPECT_GT(b.count, 2u);
	EXPECT_EQ(b.first + b.count, c.first);
	EXPECT_EQ(1u, c.count);
	EXPECT_EQ(c.first + c.count, ctx.modtrig.size());
	EXPECT_EQ(100.f, ctx.modtrig[c.first].x0);
}

TEST_F(ModVolClipTest, VolumeClippedAwayKeepsATriangleOfNoArea)
{
	addVolume({ tri(1e6f, 1e6f, 2e6f, 1e6f, 1e6f, 2e6f) });
	ta_clip_modvols(ctx);
	const ModifierVolumeParam& p = ctx.global_param_mvo[0];
	ASSERT_EQ(1u, p.count);
	EXPECT_EQ(0.0, signedArea(ctx.modtrig[p.first]));
}

TEST_F(ModVolClipTest, NonFiniteTrianglesAreDropped)
{
	addVolume({ tri(0, 0, 640, 0, 0, 480), { NAN, 0, 0.002f, 1e9f, 0, 0.002f, 0, 1e9f, 0.002f } });
	ta_clip_modvols(ctx);
	EXPECT_EQ(1u, ctx.global_param_mvo[0].count);
}

} // namespace
