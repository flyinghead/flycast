// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace metal_modifier {
using Vertex = std::array<float, 3>;
using Triangle = std::array<Vertex, 3>;

// DC modifier volumes can contain screen coordinates beyond 1e10. Letting
// the float hardware clipper intersect these edges corrupts their coverage;
// an unmatched XOR then shadows a large, unrelated part of the framebuffer.
// Clip only extreme triangles, in double precision, leaving a full viewport
// of guard band. Preserve winding and interpolate DC reciprocal depth along
// each screen-space edge, for both manual and native depth interpolation.
inline bool appendClipped(const Triangle& triangle, const float *ndc,
	std::vector<Triangle>& output, bool nativeDepth = false)
{
	// DC normal matrices are affine scale/translation or a 90-degree rotation.
	// Naomi2's per-volume projection uses its original vertex path instead.
	if (ndc[8] != 0 || ndc[9] != 0 || ndc[3] != 0 || ndc[7] != 0
			|| ndc[11] != 0 || ndc[15] != 1
			|| !((ndc[1] == 0 && ndc[4] == 0) || (ndc[0] == 0 && ndc[5] == 0)))
		return false;
	using Point = std::array<double, 3>;
	auto distance = [&](const Point& v, int plane) {
		const int row = plane / 2;
		const double coordinate = double(ndc[row]) * v[0] + double(ndc[4 + row]) * v[1]
			+ ndc[12 + row];
		return 2.0 + (plane & 1 ? -coordinate : coordinate);
	};
	std::array<Point, 12> polygon{}, scratch{};
	size_t count = 3;
	bool extreme = false;
	for (size_t i = 0; i < 3; ++i) {
		for (size_t k = 0; k < 3; ++k) {
			if (!std::isfinite(triangle[i][k])) return false;
			polygon[i][k] = triangle[i][k];
		}
		// A negative native clip W requires homogeneous near-plane clipping.
		// Retain that existing GPU path rather than altering its visibility.
		if (nativeDepth && !(double(ndc[2]) * polygon[i][0] + double(ndc[6]) * polygon[i][1]
			+ double(ndc[10]) * polygon[i][2] + ndc[14] > 0)) return false;
		for (int plane = 0; plane < 4; ++plane)
			extreme |= std::abs(distance(polygon[i], plane) - 2.0) > 1024.0;
	}
	if (!extreme) return false;
	for (int plane = 0; plane < 4 && count != 0; ++plane) {
		size_t nextCount = 0;
		bool overflow = false;
		auto append = [&](const Point& v) {
			if (nextCount == 0 || scratch[nextCount - 1] != v) {
				if (nextCount == scratch.size()) overflow = true;
				else scratch[nextCount++] = v;
			}
		};
		Point before = polygon[count - 1];
		double beforeDistance = distance(before, plane);
		for (size_t i = 0; i < count; ++i) {
			const Point& next = polygon[i];
			const double nextDistance = distance(next, plane);
			if ((beforeDistance >= 0) != (nextDistance >= 0)) {
				// Shared edges must produce identical float intersections even
				// when two volumes traverse them in opposite directions.
				Point low = before, high = next;
				if (high < low) std::swap(low, high);
				const double lowDistance = distance(low, plane);
				const double alpha = lowDistance / (lowDistance - distance(high, plane));
				Point intersection;
				for (size_t k = 0; k < 3; ++k)
					intersection[k] = low[k] + alpha * (high[k] - low[k]);
				append(intersection);
			}
			if (nextDistance >= 0) append(next);
			before = next; beforeDistance = nextDistance;
		}
		if (overflow) return false;
		if (nextCount > 1 && scratch[0] == scratch[nextCount - 1]) --nextCount;
		polygon.swap(scratch); count = nextCount;
	}
	for (size_t i = 1; i + 1 < count; ++i) {
		Triangle clipped;
		const size_t corners[] = {0, i, i + 1};
		for (size_t vertex = 0; vertex < 3; ++vertex)
			for (size_t k = 0; k < 3; ++k) clipped[vertex][k] = float(polygon[corners[vertex]][k]);
		output.push_back(clipped);
	}
	return true;
}
} // namespace metal_modifier
