// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_geometry_proof.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

static bool trackAllocations;
static size_t allocations;
void *operator new(size_t size)
{
	if (trackAllocations) ++allocations;
	if(void *p=std::malloc(std::max(size,size_t(1))))return p;
	throw std::bad_alloc();
}
void operator delete(void *p) noexcept {std::free(p);}
void operator delete(void *p,size_t) noexcept {std::free(p);}
using namespace metal_geometry;
using Matrix=std::array<std::array<float,4>,4>;
struct Vertex { float x,y,z,u,v; };
struct Vertices {
	std::vector<Vertex> values;
	mutable size_t reads=0;
	size_t size() const {return values.size();}
	const Vertex& operator[](size_t i) const {++reads;return values[i];}
};
static Matrix identity() {Matrix m{};for(size_t i=0;i<4;++i)m[i][i]=1;return m;}
static void require(bool ok,const char *message) {if(!ok)throw std::runtime_error(message);}
static uint32_t bits(float f) {uint32_t value;std::memcpy(&value,&f,4);return value;}
static bool same(Proof a,Proof b) {return bits(a.depth)==bits(b.depth) && a.colourCoordinates==b.colourCoordinates;}
// Independent monolithic pre-cache proof. Kept as a numerical oracle while
// production factors the same constraints into the testable shared helper.
static Proof reference(const Matrix& m,const std::vector<uint32_t>& indices,const std::vector<Vertex>& vertices,
	uint32_t first,uint32_t count,bool needUV)
{
	Proof out{};
	if(count<3 || first>indices.size() || count>indices.size()-first)return out;
	if(m[0][2]!=0 || m[1][2]!=0 || m[3][2]!=0 || m[0][3]!=0 || m[1][3]!=0 || m[2][3]!=0 || m[3][3]!=1
		|| !std::isnormal(m[2][2]) || m[2][2]<=0)return out;
	bool uv=needUV;double minimum=std::numeric_limits<double>::infinity(),maximum=0;
	for(size_t i=first;i<size_t(first)+count;++i) {
		const uint32_t index=indices[i];if(index==UINT32_MAX)continue;
		if(index>=vertices.size())return out;
		const Vertex& v=vertices[index];
		if(!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isnormal(v.z))return out;
		if(uv && (!std::isfinite(v.u) || !std::isfinite(v.v) || std::abs(v.u)>4.f || std::abs(v.v)>4.f))uv=false;
		const double z=double(m[2][2])*v.z;if(z<1.e-12 || z>1.e5)return out;
		for(int axis=0;axis<2;++axis) {
			const double extent=std::abs(double(m[0][axis])*v.x)+std::abs(double(m[1][axis])*v.y)
				+std::abs(double(m[2][axis])*v.z)+std::abs(double(m[3][axis]));
			if(!std::isfinite(extent) || extent>65536)return out;
			if(extent>4)uv=false;
		}
		minimum=std::min(minimum,z);maximum=std::max(maximum,z);
	}
	if(maximum==0 || maximum/minimum>65536)return out;
	out.depth=float(std::log1p(100000.0*maximum*1.125)/(std::log(2.0)*34.0)+0x1p-16);out.colourCoordinates=uv;
	return out;
}
template<class VertexSource>
static Proof cached(ProofCache& cache,const Matrix& m,const std::vector<uint32_t>& indices,const VertexSource& vertices,
	uint32_t first,uint32_t count,bool uv)
{
	return cache.get(first,count,uv,[&](bool needUV){return prove(m,indices,vertices,first,count,needUV);});
}
static volatile uint64_t sink;
static double median(std::vector<double> values) {std::sort(values.begin(),values.end());return values[values.size()/2];}
static void benchmark(uint32_t ranges,uint32_t count,uint32_t passes)
{
	const Matrix m=identity();std::vector<uint32_t> indices;Vertices vertices;
	for(uint32_t i=0;i<ranges*count;++i) {
		indices.push_back(i);vertices.values.push_back({.5f,.25f,1.f+float(i%17)*.0625f,.25f,.75f});
	}
	ProofCache cache;std::vector<double> before,after;size_t oldReads=0,newReads=0;
	for(unsigned pair=0;pair<35;++pair) {
		uint64_t a=0,b=0;
		for(unsigned half=0;half<2;++half) {
			const bool memoized=half==(pair&1);uint64_t checksum=0;
			const auto started=std::chrono::steady_clock::now();cache.reset();
			for(uint32_t pass=0;pass<passes;++pass) {
				// Prevent hoisting repeated proof work out of the timed passes.
				// This is a compiler barrier, with no emitted CPU/GPU fence.
				asm volatile("" ::: "memory");
				for(uint32_t range=0;range<ranges;++range) {
					const uint32_t first=range*count;
					const auto proof=memoized ? cached(cache,m,indices,vertices.values,first,count,true)
						: prove(m,indices,vertices.values,first,count,true);
					checksum+=bits(proof.depth)+uint32_t(proof.colourCoordinates);
				}
			}
			const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-started).count();
			sink=checksum;
			if(memoized)b=checksum;else a=checksum;
			if(pair>=4)(memoized?after:before).push_back(us);
		}
		require(a==b,"cache benchmark proof bits differ");
	}
	// Count separately: instrumentation must not inflate the timed raw scan.
	for(bool memoized:{false,true}) {
		cache.reset();vertices.reads=0;
		for(uint32_t pass=0;pass<passes;++pass)for(uint32_t range=0;range<ranges;++range) {
			const auto proof=memoized ? cached(cache,m,indices,vertices,range*count,count,true)
				: prove(m,indices,vertices,range*count,count,true);
			sink=bits(proof.depth);
		}
		(memoized?newReads:oldReads)=vertices.reads;
	}
	std::printf("proofs: ranges=%u indices=%u passes=%u old_reads=%zu cached_reads=%zu old_us=%.3f cached_us=%.3f\n",
		ranges,count,passes,oldReads,newReads,median(before),median(after));
}
int main(int argc,char **argv)
{
	try {
		std::mt19937 random(0x50524f4f);ProofCache cache;
		size_t comparisons=0,positive=0;
		for(unsigned fixture=0;fixture<10000;++fixture) {
			Matrix m=identity();Vertices vertices;std::vector<uint32_t> indices;
			for(uint32_t i=0;i<32;++i)vertices.values.push_back({float(int(random()%200)-100)/64,
				float(int(random()%200)-100)/64,.125f*(1+random()%24),float(int(random()%640)-320)/64,float(int(random()%640)-320)/64});
			for(unsigned i=0;i<64;++i)indices.push_back(i%17==0?UINT32_MAX:random()%32);
			if(fixture%5==0)m[0][3]=.125f;
			if(fixture%7==0)m[2][2]=std::numeric_limits<float>::denorm_min();
			if(fixture%11==0)vertices.values[fixture%32].z=0;
			if(fixture%13==0)indices[fixture%64]=32;
			if(fixture%17==0)vertices.values[fixture%32].u=std::numeric_limits<float>::quiet_NaN();
			if(fixture%19==0)m[0][0]=1.e30f;
			if(fixture%23==0)m[3][0]=4;
			cache.reset();
			for(unsigned range=0;range<16;++range) {
				const uint32_t first=random()%68,count=random()%24;
				for(bool uv:{false,true,false,true}) {
					const auto expected=reference(m,indices,vertices.values,first,count,uv);
					const auto raw=prove(m,indices,vertices,first,count,uv);
					const auto memo=cached(cache,m,indices,vertices,first,count,uv);
					require(same(expected,raw) && same(expected,memo),"legacy/raw/cached proof differs");
					++comparisons;positive+=expected.depth>0;
				}
			}
		}
		Matrix m=identity();Vertices v;v.values={{0,0,1,0,0},{1,0,1,0,0},{0,1,1,0,0},{1,1,2,0,0}};
		std::vector<uint32_t> idx{0,1,2,3,UINT32_MAX,0,2};
		for(float uv:{-4.f,4.f,std::nextafter(4.f,INFINITY),-std::nextafter(4.f,INFINITY),INFINITY,-INFINITY,NAN}) {
			v.values[0].u=uv;cache.reset();
			for(bool needUV:{false,true,false,true})require(same(reference(m,idx,v.values,0,4,needUV),cached(cache,m,idx,v,0,4,needUV)),"UV boundary differs");
		}
		v.values[0].u=0;
		for(float z:{0.f,-1.f,INFINITY,-INFINITY,NAN,std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::min(),1.e30f}) {
			v.values[0].z=z;cache.reset();
			for(bool uv:{false,true})require(same(reference(m,idx,v.values,0,4,uv),cached(cache,m,idx,v,0,4,uv)),"depth boundary differs");
		}
		v.values[0].z=1;
		for(float scale:{std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::min(),1.f,1.e30f}) {
			m[2][2]=scale;cache.reset();
			for(bool uv:{false,true})require(same(reference(m,idx,v.values,0,4,uv),cached(cache,m,idx,v,0,4,uv)),"projection boundary differs");
		}
		m=identity();cache.reset();
		for(auto range:{std::pair<uint32_t,uint32_t>{UINT32_MAX,4},{0,UINT32_MAX},{3,3},{4,3},{0,0}})
			require(same(reference(m,idx,v.values,range.first,range.second,true),cached(cache,m,idx,v,range.first,range.second,true)),"restart/range boundary differs");
		// Same range across a changed vertex/index/projection snapshot must be re-evaluated.
		cache.reset();auto old=cached(cache,m,idx,v,0,3,true);
		v.values[0].z=0;cache.reset();require(cached(cache,m,idx,v,0,3,true).depth==0,"vertex invalidation failed");
		v.values[0].z=1;idx[0]=3;cache.reset();require(!same(old,cached(cache,m,idx,v,0,3,true)),"index invalidation failed");
		m[2][2]=.5f;cache.reset();require(same(reference(m,idx,v.values,0,3,true),cached(cache,m,idx,v,0,3,true)),"projection invalidation failed");
		// More keys than slots force collisions. Every result must retain its full range identity.
		m=identity();v.values.clear();idx.clear();cache.reset();
		for(uint32_t i=0;i<4100;++i) {idx.push_back(i);v.values.push_back({.5f,.25f,1.f+i*.0001f,i%17==0?5.f:0.f,0});}
		for(unsigned pass=0;pass<3;++pass)for(uint32_t first=0;first<4096;++first)for(uint32_t count:{3u,4u}) {
			const bool uv=first&1;
			require(same(reference(m,idx,v.values,first,count,uv),cached(cache,m,idx,v,first,count,uv)),"cache collision returned another range");
		}
		require(cache.statistics.evaluations>ProofCache::Capacity,"collision coverage missing");
		cache.reset();v.reads=0;
		for(unsigned i=0;i<100000;++i)cached(cache,m,idx,v,0,4,true);
		require(v.reads==4 && cache.statistics.evaluations==1 && cache.statistics.hits==99999,"repeated range was rescanned");
		cache.reset();v.reads=0;
		cached(cache,m,idx,v,1,4,false);cached(cache,m,idx,v,1,4,true);cached(cache,m,idx,v,1,4,false);cached(cache,m,idx,v,1,4,true);
		require(v.reads==8 && cache.statistics.evaluations==2 && cache.statistics.hits==2,"UV demand/upgrade handling changed");
		cache.reset();allocations=0;trackAllocations=true;
		for(unsigned i=0;i<1000;++i)cached(cache,m,idx,v,i%32,4,true);
		trackAllocations=false;require(allocations==0,"proof cache allocates");
		cache.reset();bool threw=false;
		try {cache.get(42,4,true,[](bool)->Proof{throw std::runtime_error("test evaluation failure");});}
		catch(const std::runtime_error&){threw=true;}
		require(threw && same(cache.get(42,4,true,[](bool){return Proof{.5f,true};}),{.5f,true}) && cache.statistics.evaluations==2,"failed proof was published");
		require(sizeof(ProofCache)<=33*1024,"proof cache storage grew beyond its fixed budget");
		std::printf("PASS: %zu exact legacy/raw/cached comparisons (%zu positive), 24576 collision checks; demand/invalidation/rejection/boundaries; 100000 queries read 4 vertices; zero new allocations; cache_bytes=%zu\n",comparisons,positive,sizeof(ProofCache));
		if(argc>1 && std::string(argv[1])=="--benchmark") {
			for(uint32_t ranges:{64u,512u})for(uint32_t count:{3u,4u,32u})for(uint32_t passes:{1u,4u,16u})benchmark(ranges,count,passes);
			benchmark(4096,3,1);benchmark(4096,4,1);
		}
		return 0;
	} catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
