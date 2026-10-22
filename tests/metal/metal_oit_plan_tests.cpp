// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_oit_plan.h"
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
	if (void *p=std::malloc(std::max(size,size_t(1)))) return p;
	throw std::bad_alloc();
}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,size_t) noexcept {std::free(p);}
using namespace metal_oit;
static void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
// Independent pre-index planner: no production intersection, fits or partition.
static bool referenceFits(Tile t,const std::vector<Tile>& triangles,uint64_t capacity)
{
	uint64_t used=0;
	for (auto r:triangles) {
		const uint64_t x=std::max(t.x,r.x),y=std::max(t.y,r.y);
		const uint64_t right=std::min(uint64_t(t.x)+t.width,uint64_t(r.x)+r.width);
		const uint64_t bottom=std::min(uint64_t(t.y)+t.height,uint64_t(r.y)+r.height);
		const uint64_t area=(right>x?right-x:0)*(bottom>y?bottom-y:0);
		if (area>capacity-used)return false;
		used+=area;
	}
	return true;
}
static void referencePartition(Tile tile,const std::vector<Tile>& triangles,uint64_t capacity,std::vector<Tile>& out)
{
	if(referenceFits(tile,triangles,capacity)) {out.push_back(tile);return;}
	if(tile.width<=1 && tile.height<=1)throw std::runtime_error("one pixel does not fit");
	Tile a=tile,b=tile;
	if(tile.width>=tile.height && tile.width>1) {a.width=tile.width/2;b.x+=a.width;b.width-=a.width;}
	else {a.height=tile.height/2;b.y+=a.height;b.height-=a.height;}
	referencePartition(a,triangles,capacity,out);referencePartition(b,triangles,capacity,out);
}
static bool same(const std::vector<Tile>& a,const std::vector<Tile>& b)
{
	return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](Tile x,Tile y) {
		return x.x==y.x && x.y==y.y && x.width==y.width && x.height==y.height;
	});
}
static uint64_t total(const std::vector<Tile>& triangles)
{
	uint64_t sum=0;for(auto r:triangles)TilePlanner::addCoverage(sum,r);return sum;
}
static size_t verify(TilePlanner& planner,uint32_t w,uint32_t h,const std::vector<Tile>& triangles,uint64_t capacity)
{
	std::vector<Tile> expected;
	bool oldRejected=false,newRejected=false;
	try{referencePartition({0,0,w,h},triangles,capacity,expected);}catch(const std::runtime_error&){oldRejected=true;}
	const std::vector<Tile> *actual=nullptr;
	try{actual=&planner.plan(w,h,triangles,capacity,total(triangles));}catch(const std::runtime_error&){newRejected=true;}
	require(oldRejected==newRejected,"planner changed one-pixel capacity rejection");
	if(oldRejected)return 0;
	if(!same(expected,*actual)) {
		std::fprintf(stderr,"Mismatch: %ux%u triangles=%zu capacity=%llu expected=%zu actual=%zu indexed=%d\n",w,h,triangles.size(),(unsigned long long)capacity,expected.size(),actual->size(),planner.usedIndex());
		throw std::runtime_error("tile geometry, coverage or DFS order changed");
	}
	return actual->size();
}
static volatile uint64_t sink;
static double median(std::vector<double> v) {std::sort(v.begin(),v.end());return v[v.size()/2];}
static void benchmark(const char *name,uint32_t width,uint32_t height,const std::vector<Tile>& triangles,uint64_t capacity)
{
	TilePlanner planner;verify(planner,width,height,triangles,capacity);
	std::vector<Tile> old;std::vector<double> before,after;
	size_t tiles=0;
	for(unsigned pair=0;pair<35;++pair)for(unsigned half=0;half<2;++half) {
		const bool candidate=half==(pair&1);
		const auto start=std::chrono::steady_clock::now();
		// Include the additional coverage-sum work, which the renderer folds
		// into its existing triangle collection loop, in the measured candidate.
		if(candidate)tiles=planner.plan(width,height,triangles,capacity,total(triangles)).size();
		else {old.clear();metal_oit::partition({0,0,width,height},triangles,capacity,old);tiles=old.size();}
		const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();sink=tiles;
		if(pair>=4)(candidate?after:before).push_back(us);
	}
	require(same(old,planner.plan(width,height,triangles,capacity,total(triangles))),"benchmark plan differs");
	std::printf("%s: triangles=%zu tiles=%zu indexed=%d old_us=%.3f planned_us=%.3f\n",name,triangles.size(),tiles,planner.usedIndex(),median(before),median(after));
}
int main(int argc,char **argv)
{
	try {
		std::mt19937 random(0x4f495450);TilePlanner planner;
		size_t tiles=0,indexedCases=0,hierarchyCases=0;
		for(unsigned fixture=0;fixture<10000;++fixture) {
			const uint32_t width=17+random()%47,height=9+random()%31;
			std::vector<Tile> triangles;
			for(unsigned n=0,count=random()%256;n<count;++n) {
				const uint32_t x=random()%(width+10),y=random()%(height+10);
				Tile bounds{x,y,random()%(width+1),random()%(height+1)};
				if(fixture%5==0)bounds={0,0,width,height};
				if(n%17==0 && fixture%5!=0)bounds={UINT32_MAX-2,UINT32_MAX-1,3,2};
				triangles.push_back(bounds);
			}
			const uint64_t sum=total(triangles),capacity=std::max<uint64_t>(1,sum/(1+random()%129));
			tiles+=verify(planner,width,height,triangles,capacity);indexedCases+=planner.usedIndex();
		}
		for(unsigned fixture=0;fixture<1000;++fixture) {
			std::vector<Tile> triangles;
			for(unsigned n=0,count=64+random()%448;n<count;++n)
				triangles.push_back({random()%135,random()%105,1+random()%8,1+random()%8});
			const auto capacity=std::max<uint64_t>(1,total(triangles)/(16+random()%113));
			tiles+=verify(planner,127,97,triangles,capacity);
			indexedCases+=planner.usedIndex();hierarchyCases+=planner.usedHierarchy();
		}
		for(unsigned fixture=0;fixture<250;++fixture) {
			std::vector<Tile> triangles;
			for(unsigned n=0,count=1024+random()%1024;n<count;++n) {
				const uint32_t x=random()%63,y=random()%47;
				triangles.push_back({x,y,1+random()%(63-x),1+random()%(47-y)});
			}
			const auto capacity=std::max<uint64_t>(1,total(triangles)/(64+random()%129));
			tiles+=verify(planner,63,47,triangles,capacity);
			indexedCases+=planner.usedIndex();hierarchyCases+=planner.usedHierarchy();
		}
		require(hierarchyCases==1250,"spatial hierarchy fixtures were bypassed");
		std::vector<Tile> enormous(64,{0,0,UINT32_MAX,UINT32_MAX});
		tiles+=verify(planner,UINT32_MAX,UINT32_MAX,enormous,UINT64_MAX/16);
		require(planner.usedIndex(),"overflow fixture did not exercise indexed area arithmetic");
		tiles+=verify(planner,UINT32_MAX,UINT32_MAX,enormous,UINT64_MAX);
		verify(planner,1,1,std::vector<Tile>(64,{0,0,1,1}),1);
		verify(planner,11,7,{},1);
		verify(planner,11,7,{{UINT32_MAX,UINT32_MAX,UINT32_MAX,UINT32_MAX}},7);
		for(uint32_t w:{1u,13u,97u})for(uint32_t h:{1u,7u,61u})for(uint64_t cap:{1u,17u,1024u}) {
			const auto expected=metal_oit::plan(w,h,{{0,0,w,h}},cap);
			require(same(expected,planner.uniform(w,h,cap)),"uniform dense plan changed");
		}
		std::vector<Tile> warm;
		for(uint32_t y=0;y<16;++y)for(uint32_t x=0;x<16;++x)warm.push_back({x*16,y*16,16,16});
		verify(planner,256,256,warm,1024);require(planner.usedIndex(),"warm fixture did not use tree");
		allocations=0;trackAllocations=true;
		planner.plan(256,256,warm,1024,total(warm));
		trackAllocations=false;require(allocations==0,"warm identical tile planning allocates");
		uint64_t overflow=UINT64_MAX-2;TilePlanner::addCoverage(overflow,{0,0,3,2});require(overflow==UINT64_MAX,"coverage sum wraps");
		for(unsigned invalid=0;invalid<3;++invalid) {
			bool rejected=false;
			try {planner.plan(invalid==0?0:1,invalid==1?0:1,{},invalid==2?0:1,0);}
			catch(const std::runtime_error&){rejected=true;}
			require(rejected,"invalid plan dimensions/capacity accepted");rejected=false;
			try {planner.uniform(invalid==0?0:1,invalid==1?0:1,invalid==2?0:1);}
			catch(const std::runtime_error&){rejected=true;}
			require(rejected,"invalid uniform dimensions/capacity accepted");
		}
		std::printf("PASS: 11250 exact legacy/indexed plans (%zu indexed, %zu spatial hierarchies), %zu tiles; overflow/rejection/uniform cases, zero warm allocations\n",indexedCases,hierarchyCases,tiles);
		if(argc>1 && std::string(argv[1])=="--benchmark") {
			for(uint32_t count:{8u,64u,256u,4096u}) {
				std::vector<Tile> full,local,mixed;
				for(uint32_t n=0;n<count;++n) {
					full.push_back({0,0,512,384});
					local.push_back({(n%32)*16,((n/32)%24)*16,16,16});
					const uint32_t x=random()%512,y=random()%384;
					mixed.push_back({x,y,1+random()%(512-x),1+random()%(384-y)});
				}
				for(uint32_t target:{1u,4u,16u,64u,256u}) {
					benchmark("full",512,384,full,std::max<uint64_t>(count,total(full)/target));
					benchmark("local",512,384,local,std::max<uint64_t>(count,total(local)/target));
					benchmark("mixed",512,384,mixed,std::max<uint64_t>(count,total(mixed)/target));
				}
			}
		}
		return 0;
	} catch(const std::exception& e) {std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
