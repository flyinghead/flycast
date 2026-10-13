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
inline std::vector<Tile> plan(uint32_t width, uint32_t height, const std::vector<Tile>& triangles, uint64_t capacity)
{
	if (!width || !height || !capacity) throw std::runtime_error("Metal OIT tile dimensions/capacity must be positive");
	std::vector<Tile> result;
	partition({0, 0, width, height}, triangles, capacity, result);
	return result;
}
}
