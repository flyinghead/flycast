// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal/metal_oit_tiling.h"
#include <cassert>
#include <cstdio>
#include <random>

using namespace metal_oit;
static void check(uint32_t width, uint32_t height, const std::vector<Tile>& triangles, uint64_t capacity)
{
	const auto tiles = plan(width, height, triangles, capacity);
	std::vector<unsigned> coverage(size_t(width) * height);
	for (const auto& tile : tiles) {
		assert(tile.width && tile.height && tile.x + tile.width <= width && tile.y + tile.height <= height);
		assert(fits(tile, triangles, capacity));
		for (uint32_t y = tile.y; y < tile.y + tile.height; ++y)
			for (uint32_t x = tile.x; x < tile.x + tile.width; ++x) ++coverage[y * width + x];
	}
	for (unsigned count : coverage) assert(count == 1);
}
int main()
{
	// More full-screen fragments than one framebuffer's node capacity.
	check(97, 61, {{0,0,97,61},{0,0,97,61},{0,0,97,61}}, 1024);
	check(13, 7, {{0,0,13,7}}, 1);
	check(13, 7, {}, 1);
	assert(plan(97,61,{{0,0,97,61}},97*61).size() == 1);
	std::mt19937 random(431);
	for (unsigned fixture = 0; fixture < 1000; ++fixture) {
		const unsigned width = 1 + random()%97, height = 1 + random()%61;
		std::vector<Tile> bounds;
		for (unsigned i = 0; i < 20; ++i) {
			const unsigned x = random()%width, y = random()%height;
			bounds.push_back({x,y,1+random()%(width-x),1+random()%(height-y)});
		}
		check(width,height,bounds,20+random()%2000);
	}
	bool rejected = false;
	try { plan(1,1,{{0,0,1,1},{0,0,1,1}},1); } catch (const std::runtime_error&) { rejected = true; }
	assert(rejected);
	// Uint32 endpoints must not wrap during intersection or area accounting.
	assert(!fits({0,0,0xffffffffu,0xffffffffu},{{0,0,0xffffffffu,0xffffffffu}},0xffffffffu));
	assert(intersection({0xfffffff0u,0xfffffff0u,16,16},{0xfffffff8u,0xfffffff8u,8,8}).width == 8);
	puts("PASS: OIT capacity bound, disjoint complete coverage, 1000 random overlap fixtures, tiny budgets and integer limits");
}
