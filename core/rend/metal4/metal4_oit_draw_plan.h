// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_oit_tiling.h"
#include <array>
#include <limits>

namespace metal_oit {
struct BoundedPolygon {
	uint32_t index;
	Tile bounds;
};

// A polygon covering a subtree is stored at its root, rather than copied
// into every tile. Ancestor lists are individually ordered by original draw
// order; merging them preserves that order without changing any sort keys.
class DrawPlan {
	static constexpr size_t none = std::numeric_limits<size_t>::max();
	struct Word { uint32_t index; uint64_t bits; };
	struct Node {
		Tile bounds{};
		size_t left = none, right = none, parent = none;
		std::vector<Word> polygons;
	};
	std::vector<Node> nodes;
	std::vector<size_t> leaves;
	size_t used = 0, references = 0;
	static bool contains(Tile a, Tile b)
	{
		return a.x <= b.x && a.y <= b.y
			&& uint64_t(a.x) + a.width >= uint64_t(b.x) + b.width
			&& uint64_t(a.y) + a.height >= uint64_t(b.y) + b.height;
	}
	size_t build(const std::vector<Tile>& tiles, size_t first, size_t last, size_t parent)
	{
		const size_t index = used++;
		auto& n = nodes[index];
		n.parent = parent; n.left = n.right = none; n.polygons.clear();
		if (last - first == 1) {
			n.bounds = tiles[first]; leaves[first] = index;
		} else {
			const size_t middle = first + (last - first) / 2;
			n.left = build(tiles, first, middle, index);
			n.right = build(tiles, middle, last, index);
			const auto a = nodes[n.left].bounds, b = nodes[n.right].bounds;
			const auto x = std::min(a.x,b.x), y = std::min(a.y,b.y);
			const uint64_t right = std::max(uint64_t(a.x)+a.width,uint64_t(b.x)+b.width);
			const uint64_t bottom = std::max(uint64_t(a.y)+a.height,uint64_t(b.y)+b.height);
			// Renderer tiles partition a uint32_t-sized framebuffer. Reject an
			// unrepresentable union rather than dropping coverage on overflow.
			if (right-x > UINT32_MAX || bottom-y > UINT32_MAX)
				throw std::overflow_error("Metal OIT tile union is not representable");
			n.bounds = {x,y,uint32_t(right-x),uint32_t(bottom-y)};
		}
		return index;
	}
	void insert(size_t index, Tile bounds, uint32_t polygon)
	{
		auto& n = nodes[index];
		const auto overlap = intersection(n.bounds,bounds);
		if (!overlap.width || !overlap.height) return;
		if (n.left == none || contains(bounds,n.bounds)) {
			const uint32_t word = polygon / 64;
			if (n.polygons.empty() || n.polygons.back().index != word) n.polygons.push_back({word,0});
			n.polygons.back().bits |= uint64_t(1) << (polygon % 64);
			++references;
		} else {
			insert(n.left,bounds,polygon); insert(n.right,bounds,polygon);
		}
	}
public:
	static bool worthIndexing(size_t tiles, size_t polygons)
	{
		// Measured crossover: short scans win for tiny passes. Also bound
		// the tree's fixed workspace; exceeding it retains the linear path.
		constexpr size_t nodeBudget = 16 * 1024 * 1024;
		return tiles >= 16 && polygons >= 64 && tiles <= nodeBudget / sizeof(Node) / 2;
	}
	void prepare(const std::vector<Tile>& tiles, const std::vector<BoundedPolygon>& polygons)
	{
		used = references = 0; leaves.resize(tiles.size());
		if (tiles.empty()) return;
		if (tiles.size() > nodes.max_size()/2)
			throw std::length_error("Metal OIT draw plan is too large");
		const size_t needed = tiles.size()*2-1;
		// Retain the workspace at its high-water mark. No GPU resources live here.
		if (nodes.size() < needed) nodes.resize(needed);
		build(tiles,0,tiles.size(),none);
		if (polygons.size() > UINT32_MAX)
			throw std::length_error("Metal OIT polygon plan is too large");
		for (size_t i=0;i<polygons.size();++i) insert(0,polygons[i].bounds,uint32_t(i));
	}
	template<class Draw> void forEach(size_t tile, Draw&& draw) const
	{
		if (tile >= leaves.size()) throw std::out_of_range("Metal OIT draw tile is invalid");
		struct Cursor { size_t node, offset; };
		// Balanced tree depth is bounded by the number of bits in size_t.
		std::array<Cursor,std::numeric_limits<size_t>::digits+1> cursors;
		size_t count = 0;
		for (size_t n=leaves[tile];n!=none;n=nodes[n].parent)
			if (!nodes[n].polygons.empty()) cursors[count++] = {n,0};
		const auto emit = [&](uint32_t word,uint64_t bits) {
			while (bits) {
				const unsigned bit = __builtin_ctzll(bits);
				draw(word*64+bit); bits &= bits-1;
			}
		};
		if (count == 1) {
			for (auto word : nodes[cursors[0].node].polygons) emit(word.index,word.bits);
			return;
		}
		const auto greater = [&](Cursor a,Cursor b) {
			return nodes[a.node].polygons[a.offset].index > nodes[b.node].polygons[b.offset].index;
		};
		std::make_heap(cursors.begin(),cursors.begin()+count,greater);
		while (count) {
			const uint32_t word = nodes[cursors[0].node].polygons[cursors[0].offset].index;
			uint64_t bits = 0;
			do {
				std::pop_heap(cursors.begin(),cursors.begin()+count,greater);
				auto& cursor = cursors[count-1];
				bits |= nodes[cursor.node].polygons[cursor.offset++].bits;
				if (cursor.offset == nodes[cursor.node].polygons.size()) --count;
				else std::push_heap(cursors.begin(),cursors.begin()+count,greater);
			} while (count && nodes[cursors[0].node].polygons[cursors[0].offset].index == word);
			emit(word,bits);
		}
	}
	size_t storedReferences() const { return references; }
};
}
