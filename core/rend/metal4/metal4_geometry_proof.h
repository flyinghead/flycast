// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace metal_geometry {
constexpr float ColourUVCoordinateLimit = 4.f;
struct Proof {
	float depth = 0;
	bool colourCoordinates = false;
};

// The same conservative DC proof is valid for either depth interpolation mode.
// Naomi2 and material/texture eligibility are checked by the caller. UV is read
// only when an actual consumer requests it; it never changes the depth result.
template<class Matrix,class Indices,class Vertices>
Proof prove(const Matrix& m,const Indices& indices,const Vertices& vertices,
	uint32_t first,uint32_t count,bool colourCoordinates)
{
	if (count<3 || first>indices.size() || count>indices.size()-first) return {};
	// The reciprocal must be a positive scale of input Z. Other projections
	// retain depth(any), rather than assuming perspective cancellation.
	if (m[0][2]!=0 || m[1][2]!=0 || m[3][2]!=0
		|| m[0][3]!=0 || m[1][3]!=0 || m[2][3]!=0 || m[3][3]!=1
		|| !std::isnormal(m[2][2]) || m[2][2]<=0) return {};
	bool boundedUV=colourCoordinates;
	double minimum=std::numeric_limits<double>::infinity(),maximum=0;
	for (std::size_t i=first;i<std::size_t(first)+count;++i) {
		const uint32_t index=indices[i];
		if (index==~0u) continue;
		if (index>=vertices.size()) return {};
		const auto& v=vertices[index];
		// Metal can flush a subnormal operand before multiplication; a valid
		// CPU double product alone does not prove positive GPU depth.
		if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isnormal(v.z)) return {};
		if (boundedUV && (!std::isfinite(v.u) || !std::isfinite(v.v)
			|| std::abs(v.u)>ColourUVCoordinateLimit || std::abs(v.v)>ColourUVCoordinateLimit)) boundedUV=false;
		const double z=double(m[2][2])*v.z;
		if (z<1.e-12 || z>1.e5) return {};
		for (int axis=0;axis<2;++axis) {
			// Preserve the existing cancellation/clipping margins. The tighter
			// extent is needed only by the optional colour-UV consumer.
			const double extent=std::abs(double(m[0][axis])*v.x)
				+std::abs(double(m[1][axis])*v.y)+std::abs(double(m[2][axis])*v.z)
				+std::abs(double(m[3][axis]));
			if (!std::isfinite(extent) || extent>65536) return {};
			if (extent>4) boundedUV=false;
		}
		minimum=std::min(minimum,z);maximum=std::max(maximum,z);
	}
	if (maximum==0 || maximum/minimum>65536) return {};
	// Monotonic log depth of screen-linear positive Z: inflate by 12.5%
	// plus a depth margin for matrix, reciprocal, raster and log rounding.
	return {float(std::log1p(100000.0*maximum*1.125)/(std::log(2.0)*34.0)+0x1p-16),boundedUV};
}

// Fixed-size, allocation-free memoization. Collisions replace an entry; they
// can only cost a rescan, never return another range's proof. Lifetime is one
// immutable projection/index/vertex snapshot; reset before each Render.
class ProofCache {
public:
	static constexpr std::size_t Capacity = 1024;
	static_assert((Capacity&(Capacity-1))==0,"Proof cache capacity must be a power of two");
private:
	struct Entry {
		uint64_t range = 0, generation = 0;
		Proof proof;
		bool uvEvaluated = false;
	};
	std::array<Entry,Capacity> entries{};
	uint64_t generation = 1;
public:
	struct Statistics { uint64_t hits = 0, evaluations = 0; } statistics;
	void reset()
	{
		if (++generation==0) { entries.fill({});generation=1; }
		statistics={};
	}
	template<class Evaluate> Proof get(uint32_t first,uint32_t count,bool needUV,Evaluate&& evaluate)
	{
		const uint64_t range=(uint64_t(first)<<32)|count;
		uint64_t hash=range^(range>>33);
		hash*=0xff51afd7ed558ccdULL;hash^=hash>>33;
		auto& entry=entries[std::size_t(hash)&(entries.size()-1)];
		if (entry.generation==generation && entry.range==range && (!needUV || entry.uvEvaluated))
			++statistics.hits;
		else {
			++statistics.evaluations;
			const Proof proof=evaluate(needUV);
			// Publish only after evaluation succeeds. Every rejected depth proof
			// also rejects colour UV, independently of whether UV was requested.
			entry={range,generation,proof,needUV || proof.depth==0};
		}
		Proof result=entry.proof;
		if (!needUV) result.colourCoordinates=false;
		return result;
	}
};
}
