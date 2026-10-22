// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_oit_draw_plan.h"
#include "rend/metal4/metal4_oit_geometry.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <string>

static bool trackAllocations;
static size_t allocations;
void *operator new(size_t size)
{
	if (trackAllocations) ++allocations;
	if (void *p = std::malloc(std::max(size,size_t(1)))) return p;
	throw std::bad_alloc();
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p,size_t) noexcept { std::free(p); }
using namespace metal_oit;
static void require(bool ok,const char *message) { if (!ok) throw std::runtime_error(message); }
static std::vector<Tile> grid(uint32_t cols,uint32_t rows)
{
	std::vector<Tile> result;
	for (uint32_t y=0;y<rows;++y) for (uint32_t x=0;x<cols;++x)
		result.push_back({x*17,y*13,17,13});
	return result;
}
static size_t verify(DrawPlan& plan,const std::vector<Tile>& tiles,const std::vector<BoundedPolygon>& polygons)
{
	plan.prepare(tiles,polygons);
	size_t draws=0;
	for (size_t tile=0;tile<tiles.size();++tile) {
		std::vector<uint32_t> expected,actual;
		for (size_t p=0;p<polygons.size();++p) {
			const auto overlap=intersection(tiles[tile],polygons[p].bounds);
			if (overlap.width && overlap.height) expected.push_back(uint32_t(p));
		}
		plan.forEach(tile,[&](uint32_t p) { actual.push_back(p); });
		require(expected==actual,"tile candidates or original polygon order changed");
		draws+=actual.size();
	}
	return draws;
}
static volatile uint64_t sink;
static double median(std::vector<double> values)
{
	std::sort(values.begin(),values.end());return values[values.size()/2];
}
static void benchmark(const char *name,const std::vector<Tile>& tiles,const std::vector<BoundedPolygon>& polygons)
{
	DrawPlan plan; verify(plan,tiles,polygons);
	std::vector<double> oldTimes,newTimes;
	uint64_t oldSum=0,newSum=0;
	for (int iteration=0;iteration<35;++iteration) {
		for (int half=0;half<2;++half) {
			const bool indexed = half == (iteration&1);
			uint64_t sum=0;
			const auto start=std::chrono::steady_clock::now();
			if (indexed && DrawPlan::worthIndexing(tiles.size(),polygons.size())) {
				plan.prepare(tiles,polygons);
				for (size_t tile=0;tile<tiles.size();++tile)
					plan.forEach(tile,[&](uint32_t p) { sum+=polygons[p].index+1; });
			} else {
				for (auto tile:tiles) for (const auto& polygon:polygons) {
					const auto overlap=intersection(tile,polygon.bounds);
					if (overlap.width && overlap.height) sum+=polygon.index+1;
				}
			}
			const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
			sink=sum;
			if (indexed) newSum=sum; else oldSum=sum;
			if (iteration>=4) (indexed?newTimes:oldTimes).push_back(us);
		}
		require(oldSum==newSum,"benchmark candidate checksum differs");
	}
	std::printf("%s: tiles=%zu polygons=%zu stored=%zu old_us=%.3f indexed_us=%.3f\n",
		name,tiles.size(),polygons.size(),plan.storedReferences(),median(oldTimes),median(newTimes));
}
int main(int argc,char **argv)
{
	try {
		std::mt19937 random(0x4d4f4954);
		DrawPlan plan;
		size_t cases=0,draws=0;
		for (uint32_t iteration=0;iteration<5000;++iteration) {
			const uint32_t cols=1+random()%17,rows=1+random()%13;
			auto tiles=grid(cols,rows);
			std::vector<BoundedPolygon> polygons;
			for (uint32_t p=0,count=random()%300;p<count;++p) {
				Tile bounds{random()%(cols*17+40),random()%(rows*13+40),random()%90,random()%70};
				if (p%11==0) bounds={0,0,cols*17,rows*13};
				if (p%13==0) bounds={0,0,0,0};
				polygons.push_back({p*7+3,bounds}); // Original parser indices are sparse.
			}
			if (iteration%19==0) tiles.clear();
			draws+=verify(plan,tiles,polygons);++cases;
		}
		for (uint32_t iteration=0;iteration<250;++iteration) {
			std::vector<Tile> triangles;
			std::vector<BoundedPolygon> polygons;
			for (uint32_t p=0;p<64;++p) {
				const uint32_t x=random()%47,y=random()%31;
				Tile bounds{x,y,1+random()%(47-x),1+random()%(31-y)};
				triangles.push_back(bounds);polygons.push_back({p*3,bounds});
			}
			// Use the production recursive partition order, including odd edges.
			draws+=verify(plan,metal_oit::plan(47,31,triangles,128),polygons);++cases;
		}
		// Full-screen fallbacks must use one reference, even with many tiles.
		auto tiles=grid(32,32);
		std::vector<BoundedPolygon> full(100,{91,{0,0,32*17,32*13}});
		draws+=verify(plan,tiles,full);++cases;
		require(plan.storedReferences()==full.size(),"full-screen polygon was copied per tile");
		plan.prepare(tiles,full); allocations=0;trackAllocations=true;
		plan.prepare(tiles,full);
		for (size_t i=0;i<tiles.size();++i) plan.forEach(i,[](uint32_t p) { sink=p; });
		trackAllocations=false;
		require(allocations==0,"warm draw plan allocates");
		verify(plan,{{UINT32_MAX-3,UINT32_MAX-4,3,4}},{{17,{UINT32_MAX-2,UINT32_MAX-3,2,3}}});++cases;
		verify(plan,{},{});verify(plan,grid(2,2),{});cases+=2;
		require(!DrawPlan::worthIndexing(4,4096) && !DrawPlan::worthIndexing(256,8)
			&& DrawPlan::worthIndexing(16,64) && !DrawPlan::worthIndexing(SIZE_MAX,4096),"draw index cost/memory gate failed");
		bool rejected=false;
		try { plan.forEach(4,[](uint32_t){}); } catch (const std::out_of_range&) { rejected=true; }
		require(rejected,"invalid tile was accepted");
		CoverageWorkspace workspace;
		auto rectangles=grid(19,17);
		rectangles.push_back({0,0,19*17,17*13});
		require(peakCoverage(rectangles,256,&workspace)==2,"coverage workspace changes overlap");
		allocations=0;trackAllocations=true;
		const auto peak=peakCoverage(rectangles,256,&workspace);
		trackAllocations=false;
		require(peak==2 && allocations==0,"warm coverage calculation allocates or changes result");
		std::array<std::array<float,4>,4> projection{};
		for (size_t i=0;i<4;++i) projection[i][i]=1;
		GeometryBounds geometry(projection,17,13,1000);
		allocations=0;trackAllocations=true;
		geometry.reset(projection,17,13,1000);
		trackAllocations=false;
		require(allocations==0,"warm projection cache reset allocates");
		std::printf("PASS: %zu exact candidate/order cases, %zu draws, zero warm workspace allocations\n",cases,draws);
		if (argc>1 && std::string(argv[1])=="--benchmark") {
			for (uint32_t dim:{1u,2u,3u,4u,8u,16u}) {
				auto scene=grid(dim,dim);
				std::vector<BoundedPolygon> sparse,broad,mixed;
				for (uint32_t p=0;p<4096;++p) {
					const auto tile=scene[random()%scene.size()];
					sparse.push_back({p,tile});broad.push_back({p,{0,0,dim*17,dim*13}});
					const uint32_t x=random()%(dim*17),y=random()%(dim*13);
					mixed.push_back({p,p%11==0 ? Tile{0,0,dim*17,dim*13}
						: Tile{x,y,1+random()%(dim*17-x),1+random()%(dim*13-y)}});
				}
				benchmark("localized",scene,sparse);benchmark("full-screen",scene,broad);
				benchmark("mixed",scene,mixed);
			}
			for (uint32_t count:{8u,64u,256u}) for (uint32_t dim:{4u,8u,16u}) {
				auto scene=grid(dim,dim);
				std::vector<BoundedPolygon> sparse,broad;
				for (uint32_t p=0;p<count;++p) {
					sparse.push_back({p,scene[random()%scene.size()]});
					broad.push_back({p,{0,0,dim*17,dim*13}});
				}
				benchmark("few-localized",scene,sparse);benchmark("few-full-screen",scene,broad);
			}
		}
		return 0;
	} catch (const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what());return 1; }
}
