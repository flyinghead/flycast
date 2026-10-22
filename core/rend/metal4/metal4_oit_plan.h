// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_oit_tiling.h"
#include <array>
#include <limits>

namespace metal_oit {
// Exact, integer-only acceleration of the existing conservative area bound.
// This affects tile planning only: draw order and shader sequence are untouched.
class TilePlanner {
	static constexpr size_t none = std::numeric_limits<size_t>::max();
	struct Primitive { Tile bounds; uint32_t key; };
	struct Node {
		Tile bounds{}, common{};
		uint64_t area = 0;
		size_t right = none;
		uint32_t first = 0, count = 0;
		bool overflow = false;
	};
	std::vector<Primitive> primitives, sortScratch;
	std::vector<Node> nodes;
	std::vector<Tile> result;
	size_t used = 0;
	bool indexed = false, hierarchy = false;
	static bool contains(Tile a, Tile b)
	{
		return a.x <= b.x && a.y <= b.y
			&& uint64_t(a.x)+a.width >= uint64_t(b.x)+b.width
			&& uint64_t(a.y)+a.height >= uint64_t(b.y)+b.height;
	}
	static Tile unite(Tile a, Tile b)
	{
		if (!a.width || !a.height) return b;
		if (!b.width || !b.height) return a;
		const uint32_t x=std::min(a.x,b.x),y=std::min(a.y,b.y);
		return {x,y,uint32_t(std::max(uint64_t(a.x)+a.width,uint64_t(b.x)+b.width)-x),
			uint32_t(std::max(uint64_t(a.y)+a.height,uint64_t(b.y)+b.height)-y)};
	}
	static void include(Node& node, Tile bounds)
	{
		if (!bounds.width || !bounds.height) return;
		const uint64_t area=uint64_t(bounds.width)*bounds.height;
		if (area > UINT64_MAX-node.area) node.overflow = true;
		node.area = node.overflow ? UINT64_MAX : node.area+area;
		node.common = node.count ? intersection(node.common,bounds) : bounds;
		node.bounds = unite(node.bounds,bounds); ++node.count;
	}
	static uint32_t spread(uint32_t bits)
	{
		bits=(bits|(bits<<8))&0x00ff00ffu;
		bits=(bits|(bits<<4))&0x0f0f0f0fu;
		bits=(bits|(bits<<2))&0x33333333u;
		return (bits|(bits<<1))&0x55555555u;
	}
	void spatialSort(Tile whole)
	{
		for (auto& p:primitives) {
			const auto b=p.bounds;
			const uint32_t x=uint32_t((uint64_t(b.x)*2+b.width)*65535/(uint64_t(whole.width)*2));
			const uint32_t y=uint32_t((uint64_t(b.y)*2+b.height)*65535/(uint64_t(whole.height)*2));
			p.key=spread(x)|(spread(y)<<1);
		}
		sortScratch.resize(primitives.size());
		// Four stable radix passes, with reused storage and no comparison-sort
		// allocations. Morton order groups nearby primitive rectangles.
		for (unsigned shift=0;shift<32;shift+=8) {
			std::array<size_t,256> offsets{};
			for (const auto& p:primitives) ++offsets[(p.key>>shift)&255];
			size_t sum=0;
			for (auto& n:offsets) { const auto count=n; n=sum; sum+=count; }
			for (const auto& p:primitives) sortScratch[offsets[(p.key>>shift)&255]++]=p;
			primitives.swap(sortScratch);
		}
	}
	size_t build(size_t first, size_t last)
	{
		const size_t index=used++;
		auto& node=nodes[index]; node={};
		if (last-first<=8) {
			node.first=uint32_t(first);
			for (size_t i=first;i<last;++i) include(node,primitives[i].bounds);
		} else {
			const size_t middle=first+(last-first)/2;
			const size_t left=build(first,middle); node.right=build(middle,last);
			const auto& a=nodes[left]; const auto& b=nodes[node.right];
			node.bounds=unite(a.bounds,b.bounds);node.common=intersection(a.common,b.common);
			node.count=a.count+b.count;
			node.overflow=a.overflow || b.overflow || b.area>UINT64_MAX-a.area;
			node.area=node.overflow ? UINT64_MAX : a.area+b.area;
		}
		return index;
	}
	bool accumulate(size_t index, Tile tile, uint64_t capacity, uint64_t& usedArea) const
	{
		const auto& n=nodes[index];
		const auto overlap=intersection(tile,n.bounds);
		if (!overlap.width || !overlap.height) return true;
		const auto add=[&](uint64_t area) {
			if (area>capacity-usedArea) return false;
			usedArea+=area;return true;
		};
		if (contains(tile,n.bounds)) return !n.overflow && add(n.area);
		if (n.common.width && n.common.height && contains(n.common,overlap)) {
			const uint64_t area=uint64_t(overlap.width)*overlap.height;
			if (n.count>(capacity-usedArea)/area) return false;
			return add(area*n.count);
		}
		if (n.right==none) {
			// Morton leaves hold at most eight neighboring rectangles. The
			// final leaf can be shorter; node.count is its valid length.
			for (size_t i=n.first;i<size_t(n.first)+n.count;++i) {
				const auto r=intersection(tile,primitives[i].bounds);
				if (!add(uint64_t(r.width)*r.height)) return false;
			}
			return true;
		}
		return accumulate(index+1,tile,capacity,usedArea) && accumulate(n.right,tile,capacity,usedArea);
	}
	template<class Fits> void subdivide(Tile tile, uint64_t capacity, Fits&& fits)
	{
		if (fits(tile,capacity)) { result.push_back(tile);return; }
		if (tile.width<=1 && tile.height<=1)
			throw std::runtime_error("Metal OIT buffer cannot hold one pixel's primitive coverage");
		Tile a=tile,b=tile;
		if (tile.width>=tile.height && tile.width>1) {
			a.width=tile.width/2;b.x+=a.width;b.width-=a.width;
		} else { a.height=tile.height/2;b.y+=a.height;b.height-=a.height; }
		subdivide(a,capacity,fits);subdivide(b,capacity,fits);
	}
public:
	// Caller computes this while collecting coverage rectangles. Saturation is
	// intentional: a value beyond uint64 capacity still proves heavy overlap.
	static void addCoverage(uint64_t& total, Tile bounds)
	{
		const uint64_t area=uint64_t(bounds.width)*bounds.height;
		total=area>UINT64_MAX-total ? UINT64_MAX : total+area;
	}
	const std::vector<Tile>& plan(uint32_t width,uint32_t height,const std::vector<Tile>& triangles,
		uint64_t capacity,uint64_t totalCoverage)
	{
		if (!width || !height || !capacity)
			throw std::runtime_error("Metal OIT tile dimensions/capacity must be positive");
		const Tile whole{0,0,width,height};
		result.clear();indexed=hierarchy=false;
		// The sum is a conservative bound even for rectangles outside the
		// framebuffer. A saturated sum cannot prove that UINT64_MAX fits.
		if (totalCoverage!=UINT64_MAX && totalCoverage<=capacity) {
			result.push_back(whole);return result;
		}
		// A minimum tile-count estimate selects the heavy path without building
		// its index for small passes. Bound scratch by primitive count as well.
		constexpr size_t maxPrimitives=256*1024;
		if (triangles.size()<64 || totalCoverage/capacity<16) {
			partition(whole,triangles,capacity,result);return result;
		}
		const auto first=triangles.front();
		if (std::all_of(triangles.begin()+1,triangles.end(),[&](Tile r) {
			return r.x==first.x && r.y==first.y && r.width==first.width && r.height==first.height;
		})) {
			// Identical conservative fallbacks need no tree or radix sort.
			// Division is exact for testing area*count <= capacity, without
			// overflowing even when the full-frame sum exceeds uint64_t.
			indexed=true;
			const uint64_t perPrimitive=capacity/triangles.size();
			subdivide(whole,capacity,[&](Tile tile,uint64_t) {
				const auto r=intersection(tile,first);
				return uint64_t(r.width)*r.height<=perPrimitive;
			});
			return result;
		}
		// Sparse rectangles benefit from pruning even at modest tile counts.
		// Broad, varied rectangles only amortize sorting for larger passes.
		const uint64_t frameArea=uint64_t(width)*height;
		const bool sparse=totalCoverage/triangles.size()<=frameArea/64;
		const bool heavy=triangles.size()>=1024 && totalCoverage/capacity>=64;
		if (triangles.size()>maxPrimitives || (!sparse && !heavy)) {
			partition(whole,triangles,capacity,result);return result;
		}
		primitives.clear(); Node root;
		for (auto bounds:triangles) {
			bounds=intersection(whole,bounds);
			if (!bounds.width || !bounds.height) continue;
			primitives.push_back({bounds,0});include(root,bounds);
		}
		if (primitives.empty()) { result.push_back(whole);return result; }
		// Every rectangle equal: any query has intersection area * count.
		// Avoid sorting/building a tree for this common full-screen fallback.
		if (contains(root.common,root.bounds)) {
			nodes.resize(1);nodes[0]=root;used=1;
		} else {
			spatialSort(whole);
			// Binary subdivision to leaves of at most eight elements uses less
			// than one node per two primitives; reserve a conservative exact cap.
			if (nodes.size()<primitives.size()) nodes.resize(primitives.size());
			used=0;build(0,primitives.size());
			hierarchy=true;
		}
		indexed=true;
		subdivide(whole,capacity,[&](Tile tile,uint64_t limit) {
			uint64_t area=0;return accumulate(0,tile,limit,area);
		});
		return result;
	}
	const std::vector<Tile>& uniform(uint32_t width,uint32_t height,uint64_t pixels)
	{
		if (!width || !height || !pixels)
			throw std::runtime_error("Metal OIT tile dimensions/capacity must be positive");
		result.clear();indexed=hierarchy=false;
		subdivide({0,0,width,height},pixels,[](Tile tile,uint64_t limit) {
			return uint64_t(tile.width)*tile.height<=limit;
		});
		return result;
	}
	bool usedIndex() const { return indexed; }
	bool usedHierarchy() const { return hierarchy; }
};
}
