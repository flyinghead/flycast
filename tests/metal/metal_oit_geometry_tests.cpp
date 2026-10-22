// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_oit_geometry.h"
#include <cassert>
#include <cstdio>
#include <random>
using u32 = uint32_t;
using Matrix = std::array<std::array<float,4>,4>;
struct Vertex { float x,y,z; };
struct Vertices {
	std::vector<Vertex> data;
	mutable size_t reads = 0;
	size_t size() const { return data.size(); }
	const Vertex& operator[](size_t i) const { ++reads; return data[i]; }
};
struct Frame { Matrix ndc; u32 width, height; };
// Independent pre-optimization renderer implementation, retained as an oracle.
metal_oit::Tile reference(const Frame& frame, const Vertices& vertices, const u32 *index, bool naomi2 = false)
{
	const metal_oit::Tile whole{0,0,frame.width,frame.height};
				// Naomi2's shader transforms, or a noncanonical projection, need
				// the full screen bound. Positive Dreamcast W is constant one;
				// native-depth division cancels when forming screen X/Y as well.
				const auto& m = frame.ndc;
				bool bounded = !naomi2 && m[0][3] == 0 && m[1][3] == 0 && m[2][3] == 0 && m[3][3] == 1
					&& m[0][2] == 0 && m[1][2] == 0 && m[3][2] == 0 && std::isnormal(m[2][2]) && m[2][2] > 0;
				double left = frame.width, top = frame.height, right = 0, bottom = 0;
				for (int j = 0; j < 3 && bounded; ++j) {
					if (index[j] >= vertices.size()) { bounded = false; break; }
					const auto& v = vertices[index[j]];
					// A nonpositive/subnormal reciprocal can cross homogeneous W
					// during native-depth clipping; vertex X/Y no longer bounds it.
					if (!std::isnormal(v.z) || v.z <= 0) { bounded = false; break; }
					const double reciprocal = double(m[2][2]) * v.z;
					if (reciprocal < 1.e-12 || reciprocal > 1.e5) { bounded = false; break; }
					const double values[]{v.x, v.y, v.z, 1};
					double screen[2]{};
					for (int axis = 0; axis < 2; ++axis) {
						double value = 0, magnitude = 0;
						for (int k = 0; k < 4; ++k) { const double term = double(m[k][axis]) * values[k]; value += term; magnitude += std::abs(term); }
						// Clipped strips can have very large off-screen vertices.
						// Their rounding margin below remains conservative; only
						// float overflow, including native-depth division, requires
						// the full-screen fallback, not a fixed coordinate cutoff.
						const double safeMagnitude = double(std::numeric_limits<float>::max())
							* std::min(1.0, reciprocal) / 16;
						if (!std::isfinite(value) || !std::isfinite(magnitude) || magnitude > safeMagnitude) { bounded = false; break; }
						// Include raster subpixel rounding and float matrix error.
						const double size = axis == 0 ? frame.width : frame.height;
						const double margin = 4 + magnitude * std::numeric_limits<float>::epsilon() * 16 * size;
						screen[axis] = (value + 1) * size * .5;
						if (axis == 0) { left = std::min(left, screen[axis] - margin); right = std::max(right, screen[axis] + margin); }
						else { top = std::min(top, screen[axis] - margin); bottom = std::max(bottom, screen[axis] + margin); }
					}
				}
				if (!bounded) return whole;
				else {
					const u32 x = u32(std::clamp(std::floor(left), 0.0, double(frame.width)));
					const u32 y = u32(std::clamp(std::floor(top), 0.0, double(frame.height)));
					const u32 endX = u32(std::clamp(std::ceil(right), 0.0, double(frame.width)));
					const u32 endY = u32(std::clamp(std::ceil(bottom), 0.0, double(frame.height)));
					return {x,y,endX-x,endY-y};
				}
}
bool equal(metal_oit::Tile a,metal_oit::Tile b) { return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height; }
int main()
{
	std::mt19937 random(48169);
	Frame f{}; f.width=1920;f.height=1080;
	f.ndc[0][0]=2.f/640;f.ndc[1][1]=-2.f/480;f.ndc[2][2]=1;
	f.ndc[3][0]=-1;f.ndc[3][1]=1;f.ndc[3][3]=1;
	Vertices v;v.data.resize(30);
	for (unsigned fixture=0;fixture<5000;++fixture) {
		for (auto& p:v.data) {
			p.x=float(int(random()%32000)-16000);p.y=float(int(random()%32000)-16000);
			p.z=std::ldexp(float(1+random()%1000)/1000,int(random()%70)-35);
		}
		if(fixture%7==0)v.data[fixture%v.size()].x=std::numeric_limits<float>::infinity();
		if(fixture%11==0)v.data[fixture%v.size()].z=-1;
		if(fixture%13==0)v.data[fixture%v.size()].y=std::numeric_limits<float>::quiet_NaN();
		if(fixture%17==0)v.data[fixture%v.size()].z=std::numeric_limits<float>::denorm_min();
		if(fixture%31==0)v.data[fixture%v.size()].x=1.e30f;
		if(fixture%37==0)v.data[fixture%v.size()].y=std::numeric_limits<float>::max();
		Matrix matrix=f.ndc;
		if(fixture&1) { matrix[0][1]=.004f;matrix[1][0]=-.001f;matrix[2][0]=.03f; }
		if(fixture%3==0)matrix[0][0]=-matrix[0][0];
		if(fixture%19==0)matrix[0][3]=.1f;
		if(fixture%23==0)matrix[2][2]=std::numeric_limits<float>::quiet_NaN();
		Frame frame{matrix,1+random()%7680,1+random()%4320};
		metal_oit::GeometryBounds geometry(matrix,frame.width,frame.height,v.size());
		metal_oit::Tile polygon{};
		std::vector<metal_oit::Tile> triangles;
		for(unsigned i=0;i<32;++i) {
			u32 indices[]{random()%32,random()%32,random()%32};
			const bool naomi2=i%13==0;
			const auto expected=reference(frame,v,indices,naomi2), actual=geometry.triangle(indices,v,naomi2);
			assert(equal(expected,actual));
			if(actual.width && actual.height) {triangles.push_back(actual);polygon=metal_oit::coverageUnion(polygon,actual);}
		}
		// Every contributing primitive must survive polygon selection for every
		// disjoint tile, including zero-area/off-screen primitives in the union.
		for(u32 y=0;y<frame.height;y+=128)for(u32 x=0;x<frame.width;x+=128) {
			const metal_oit::Tile tile{x,y,std::min(128u,frame.width-x),std::min(128u,frame.height-y)};
			const auto poly=metal_oit::intersection(tile,polygon);
			if(!poly.width || !poly.height)for(auto triangle:triangles) {
				const auto overlap=metal_oit::intersection(tile,triangle);assert(!overlap.width || !overlap.height);
			}
		}
	}
	v.data={{1,1,1},{2,2,1},{3,1,1}};v.reads=0;
	metal_oit::GeometryBounds geometry(f.ndc,f.width,f.height,v.size());u32 index[]{0,1,2};
	for(unsigned i=0;i<100000;++i)geometry.triangle(index,v);
	assert(v.reads==3);
	// Reuse the same allocation across logical passes; explicitly reset it
	// when the CPU vertex data or projection changes for a later render.
	v.data[0].x=100; v.data[0].z=1; v.reads=0;
	geometry.reset(f.ndc,f.width,f.height,v.size());
	assert(equal(geometry.triangle(index,v),reference(f,v,index)));
	v.reads=0;
	for(unsigned pass=0;pass<4;++pass)geometry.triangle(index,v);
	assert(v.reads==0);
	v.data[0].z=0;
	geometry.reset(f.ndc,f.width,f.height,v.size());
	assert(equal(geometry.triangle(index,v),{0,0,f.width,f.height}));
	v.data[0].z=1;
	geometry.reset(f.ndc,13,9,v.size(),false);
	assert(equal(geometry.triangle(index,v),{0,0,13,9}));
	// A new render/projection must rebuild the proof, including failed bounds.
	v.data[0].z=0;v.reads=0;
	metal_oit::GeometryBounds invalid(f.ndc,f.width,f.height,v.size());
	for(unsigned i=0;i<100000;++i)assert(equal(invalid.triangle(index,v),{0,0,f.width,f.height}));
	assert(v.reads==3);
	metal_oit::GeometryBounds disabled(f.ndc,f.width,f.height,v.size(),false);
	assert(equal(disabled.triangle(index,v),{0,0,f.width,f.height}));
	puts("PASS: 160000 exact comparisons against old geometry bounds, 5000 polygon/tile coverage fixtures; 100000 shared triangles read only 3 vertices; invalid/projection fallbacks preserved");
}
