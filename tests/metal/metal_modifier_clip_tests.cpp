// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal/metal_modifier_clip.h"
#include <cstdlib>
#include <random>
#include <cstdio>
#include <limits>

using namespace metal_modifier;
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); std::abort(); } } while (false)
static double edge(const Vertex& a, const Vertex& b, double x, double y)
{
	return (double(b[0]) - a[0]) * (y - a[1]) - (double(b[1]) - a[1]) * (x - a[0]);
}
static bool contains(const Triangle& t, double x, double y)
{
	const double a = edge(t[0], t[1], x, y), b = edge(t[1], t[2], x, y), c = edge(t[2], t[0], x, y);
	return (a > 0 && b > 0 && c > 0) || (a < 0 && b < 0 && c < 0);
}
int main()
{
	const std::array<float, 16> identity{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
	const Triangle ordinary{{Vertex{-.7f, -.3f, 1}, Vertex{.8f, -.3f, 2}, Vertex{.1f, .9f, 3}}};
	std::vector<Triangle> clipped;
	CHECK(!appendClipped(ordinary, identity.data(), clipped));
	CHECK(clipped.empty());
	// Oversized off-screen caps must contribute no stencil coverage.
	Triangle outside{{Vertex{1e10f, -1e10f, 100000}, Vertex{2e10f, -1e10f, 100000}, Vertex{1e10f, 1e10f, 100000}}};
	CHECK(appendClipped(outside, identity.data(), clipped) && clipped.empty());
	const Triangle extreme{{Vertex{-9.8e9f, -5.9e10f, 100000}, Vertex{1.03e10f, 8.88e9f, 100000}, Vertex{-2.3e9f, 9.0e9f, 100000}}};
	CHECK(appendClipped(extreme, identity.data(), clipped) && !clipped.empty());
	for (const auto& t : clipped) for (const auto& v : t) {
		CHECK(std::isfinite(v[0]) && std::isfinite(v[1]));
		CHECK(std::abs(v[0]) <= 2.001 && std::abs(v[1]) <= 2.001);
		CHECK(v[2] == 100000);
	}
	for (int y = 0; y < 93; ++y) for (int x = 0; x < 127; ++x) {
		const double px = -1 + (x + .317) * 2 / 127, py = -1 + (y + .739) * 2 / 93;
		int count = 0;
		for (const auto& t : clipped) count += contains(t, px, py);
		CHECK(count == int(contains(extreme, px, py)));
	}
	// Opposite traversal of a shared cap edge must give identical clipped
	// vertices; otherwise an XOR volume acquires artificial seams.
	Triangle reverse = extreme; std::swap(reverse[0], reverse[2]);
	std::vector<Triangle> reversed; CHECK(appendClipped(reverse, identity.data(), reversed));
	std::vector<Vertex> a, b;
	for (const auto& t : clipped) a.insert(a.end(), t.begin(), t.end());
	for (const auto& t : reversed) b.insert(b.end(), t.begin(), t.end());
	std::sort(a.begin(), a.end()); std::sort(b.begin(), b.end());
	a.erase(std::unique(a.begin(), a.end()), a.end()); b.erase(std::unique(b.begin(), b.end()), b.end());
	CHECK(a == b);
	// Screen-space clipping must interpolate reciprocal depth, including
	// native-depth draws. A simple affine plane gives an independent check.
	Triangle slope{{Vertex{-1e6f, -1e6f, 1e6f}, Vertex{1e6f, -1e6f, 3e6f}, Vertex{0, 1e6f, 3e6f}}};
	for (bool native : {false, true}) {
		std::vector<Triangle> result; CHECK(appendClipped(slope, identity.data(), result, native));
		for (const auto& t : result) for (const auto& v : t)
			CHECK(std::abs(double(v[2]) - (2.5e6 + v[0] + .5 * v[1])) <= .125);
	}
	// Supported viewport transforms include scale/translation and rotation.
	auto rotated = identity; rotated[0] = rotated[5] = 0; rotated[1] = 1; rotated[4] = -1;
	std::vector<Triangle> rotation; CHECK(appendClipped(extreme, rotated.data(), rotation));
	CHECK(!rotation.empty());
	auto projected = identity; projected[11] = .1f;
	std::vector<Triangle> unchanged; CHECK(!appendClipped(extreme, projected.data(), unchanged));
	Triangle negative = extreme; negative[0][2] = -1;
	CHECK(!appendClipped(negative, identity.data(), unchanged, true));
	Triangle invalid = extreme; invalid[0][0] = std::numeric_limits<float>::infinity();
	CHECK(!appendClipped(invalid, identity.data(), unchanged));
	// Exercise viewport transforms and many unrelated cap shapes without
	// requiring a ROM. Check sample coverage against the original triangle.
	std::mt19937 random(0x4d455441);
	std::uniform_real_distribution<float> coordinate(-1.f, 1.f);
	for (unsigned fixture = 0; fixture < 240; ++fixture) {
		const float scale = fixture % 3 == 0 ? 1e10f : fixture % 3 == 1 ? 1e6f : 1e4f;
		Triangle cap;
		for (auto& v : cap) v = {coordinate(random) * scale, coordinate(random) * scale, 100000};
		std::vector<Triangle> result;
		CHECK(appendClipped(cap, identity.data(), result));
		const double winding = edge(cap[0], cap[1], cap[2][0], cap[2][1]);
		for (const auto& t : result) {
			CHECK(edge(t[0], t[1], t[2][0], t[2][1]) * winding >= 0);
			for (const auto& v : t) CHECK(std::abs(v[0]) <= 2.001 && std::abs(v[1]) <= 2.001);
		}
		for (int y = 0; y < 37; ++y) for (int x = 0; x < 43; ++x) {
			const double px = -1 + (x + .317) * 2 / 43, py = -1 + (y + .739) * 2 / 37;
			int count = 0;
			for (const auto& t : result) count += contains(t, px, py);
			CHECK(count == int(contains(cap, px, py)));
		}
	}
	std::puts("PASS extreme modifier coverage, winding, shared edges, depth and viewport transforms");
}
