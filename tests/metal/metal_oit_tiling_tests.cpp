// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_oit_tiling.h"
#include "rend/metal4/metal4_oit_sequence.h"
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
	for (uint32_t polygon : {0u, 1u, 32767u, 32768u, ~0u})
	for (uint32_t base : {0u, 17u, 0xffff0000u, ~0u})
	for (uint32_t vertex : {0u, 16u, 17u, 131071u, 131072u, 131088u, 131089u, 0xffff0000u, ~0u}) {
		const bool valid = sequenceCarriesPolygon(polygon,base,vertex);
		const uint64_t mathematicalOffset = vertex >= base ? uint64_t(vertex)-base : ~uint64_t(0);
		assert(valid == (polygon < 32768 && mathematicalOffset < 131072));
		if (valid) {
			const uint32_t sequence = (polygon << 17) - base + vertex;
			assert((sequence >> 17) == polygon);
			assert(uint64_t(sequence) == (uint64_t(polygon) << 17) + mathematicalOffset);
		}
	}
	puts("PASS: polygon sequence metadata round-trips without truncation; borrow/carry and uint32 boundaries rejected");

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
	assert(peakCoverage({},256)==0);
	assert(peakCoverage({{0,0,4,4},{4,0,4,4},{0,4,4,4}},256)==1);
	assert(peakCoverage({{0,0,0,4},{0,0,4,0}},256)==0);
	assert(peakCoverage({{0xfffffff0u,0xfffffff0u,16,16},{0xfffffff8u,0xfffffff8u,8,8}},256)==2);
	for(unsigned fixture=0;fixture<1000;++fixture) {
		std::vector<Tile> bounds;unsigned pixels[19*17]{};
		for(unsigned i=0;i<30;++i) {
			const unsigned x=random()%19,y=random()%17,w=random()%(20-x),h=random()%(18-y);
			bounds.push_back({x,y,w,h});
			for(unsigned yy=y;yy<y+h;++yy)for(unsigned xx=x;xx<x+w;++xx)++pixels[yy*19+xx];
		}
		const auto actual=*std::max_element(std::begin(pixels),std::end(pixels));
		for(unsigned limit : {0u,1u,2u,4u,8u,16u,32u,256u})assert(peakCoverage(bounds,limit)==std::min(actual,limit));
		const uint32_t layers=std::max(1u,peakCoverage(bounds,33));
		for(unsigned stride : {12u,16u}) {
			const unsigned bytes=20000,capacity=bytes/stride/layers;
			if(capacity)for(const auto& tile:plan(19,17,{{0,0,19,17}},capacity))assert(uint64_t(tile.width)*tile.height*layers*stride<=bytes);
		}
	}
	puts("PASS: peak overlap matches 1000 brute-force grids; dense 12/16-byte tile addresses fit physical capacity");
	bool rejected = false;
	try { plan(1,1,{{0,0,1,1},{0,0,1,1}},1); } catch (const std::runtime_error&) { rejected = true; }
	assert(rejected);
	// Uint32 endpoints must not wrap during intersection or area accounting.
	assert(!fits({0,0,0xffffffffu,0xffffffffu},{{0,0,0xffffffffu,0xffffffffu}},0xffffffffu));
	assert(intersection({0xfffffff0u,0xfffffff0u,16,16},{0xfffffff8u,0xfffffff8u,8,8}).width == 8);
	puts("PASS: OIT capacity bound, disjoint complete coverage, 1000 random overlap fixtures, tiny budgets and integer limits");
}
