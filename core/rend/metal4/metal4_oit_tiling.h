// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace metal_oit {
struct Tile {
	uint32_t x, y, width, height;
};
inline Tile intersection(Tile a, Tile b)
{
	const uint32_t x = std::max(a.x, b.x), y = std::max(a.y, b.y);
	const uint64_t right = std::min(uint64_t(a.x) + a.width, uint64_t(b.x) + b.width);
	const uint64_t bottom = std::min(uint64_t(a.y) + a.height, uint64_t(b.y) + b.height);
	return {x, y, uint32_t(right > x ? right - x : 0), uint32_t(bottom > y ? bottom - y : 0)};
}
// One single-sample triangle can allocate at most one node per covered pixel.
// Bounding rectangles deliberately count discarded, culled and occluded pixels.
inline bool fits(Tile tile, const std::vector<Tile>& triangles, uint64_t capacity)
{
	uint64_t used = 0;
	for (Tile bounds : triangles) {
		const Tile covered = intersection(tile, bounds);
		const uint64_t area = uint64_t(covered.width) * covered.height;
		if (area > capacity - used) return false;
		used += area;
	}
	return true;
}
inline void partition(Tile tile, const std::vector<Tile>& triangles, uint64_t capacity, std::vector<Tile>& result)
{
	if (fits(tile, triangles, capacity)) { result.push_back(tile); return; }
	if (tile.width <= 1 && tile.height <= 1)
		throw std::runtime_error("Metal OIT buffer cannot hold one pixel's primitive coverage");
	Tile a = tile, b = tile;
	if (tile.width >= tile.height && tile.width > 1) {
		a.width = tile.width / 2; b.x += a.width; b.width -= a.width;
	} else {
		a.height = tile.height / 2; b.y += a.height; b.height -= a.height;
	}
	partition(a, triangles, capacity, result);
	partition(b, triangles, capacity, result);
}
// Bound per-pixel sorting storage by spatial overlap, not the total number
// of primitives in a pass. Half-open rectangles include every raster sample
// of their source triangle, so this can only overestimate the required layers.
struct CoverageWorkspace {
	struct Event { uint64_t x, top, bottom; int delta; };
	struct Node { int64_t maximum = 0, added = 0; };
	std::vector<Event> events;
	std::vector<uint64_t> ys;
	std::vector<Node> tree;
};
inline uint32_t peakCoverage(const std::vector<Tile>& triangles, uint32_t limit, CoverageWorkspace *workspace = nullptr)
{
	if (limit == 0) return 0;
	CoverageWorkspace local;
	auto& work = workspace ? *workspace : local;
	auto& events = work.events; auto& ys = work.ys; auto& tree = work.tree;
	events.clear(); ys.clear();
	events.reserve(triangles.size() * 2); ys.reserve(triangles.size() * 2);
	for (Tile r : triangles) {
		if (!r.width || !r.height) continue;
		const uint64_t bottom = uint64_t(r.y) + r.height;
		events.push_back({r.x,r.y,bottom,1});
		events.push_back({uint64_t(r.x)+r.width,r.y,bottom,-1});
		ys.push_back(r.y); ys.push_back(bottom);
	}
	if (events.empty()) return 0;
	std::sort(ys.begin(),ys.end()); ys.erase(std::unique(ys.begin(),ys.end()),ys.end());
	std::sort(events.begin(),events.end(),[](const auto& a,const auto& b) { return a.x < b.x; });
	tree.assign((ys.size()-1)*4,{});
	const auto add = [&](auto&& self,size_t node,size_t lo,size_t hi,size_t first,size_t last,int delta) -> void {
		if (first <= lo && hi <= last) { tree[node].added += delta; tree[node].maximum += delta; return; }
		const size_t mid = lo + (hi-lo)/2;
		if (first < mid) self(self,node*2+1,lo,mid,first,last,delta);
		if (last > mid) self(self,node*2+2,mid,hi,first,last,delta);
		tree[node].maximum = tree[node].added + std::max(tree[node*2+1].maximum,tree[node*2+2].maximum);
	};
	uint32_t maximum = 0;
	for (size_t i = 0; i < events.size();) {
		const uint64_t x = events[i].x;
		// Remove ending rectangles and add starting ones before measuring:
		// touching edges never add an extra layer at this X coordinate.
		do {
			const auto& e = events[i++];
			const size_t top = std::lower_bound(ys.begin(),ys.end(),e.top)-ys.begin();
			const size_t bottom = std::lower_bound(ys.begin(),ys.end(),e.bottom)-ys.begin();
			add(add,0,0,ys.size()-1,top,bottom,e.delta);
		} while (i < events.size() && events[i].x == x);
		if (tree[0].maximum >= limit) return limit;
		maximum = std::max(maximum,uint32_t(tree[0].maximum));
	}
	return maximum;
}
inline std::vector<Tile> plan(uint32_t width, uint32_t height, const std::vector<Tile>& triangles, uint64_t capacity)
{
	if (!width || !height || !capacity) throw std::runtime_error("Metal OIT tile dimensions/capacity must be positive");
	std::vector<Tile> result;
	partition({0, 0, width, height}, triangles, capacity, result);
	return result;
}
}
