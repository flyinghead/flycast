// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_oit_tiling.h"
#include <array>
#include <cmath>
#include <limits>

namespace metal_oit {
// Union of nonempty primitive coverage; never use this to join individual
// off-screen vertices, whose clamped empty extents can still span a triangle.
inline Tile coverageUnion(Tile a, Tile b)
{
	if (!a.width || !a.height) return b;
	if (!b.width || !b.height) return a;
	const uint32_t x = std::min(a.x, b.x), y = std::min(a.y, b.y);
	return {x, y, uint32_t(std::max(uint64_t(a.x) + a.width, uint64_t(b.x) + b.width) - x),
		uint32_t(std::max(uint64_t(a.y) + a.height, uint64_t(b.y) + b.height) - y)};
}

// Lifetime: one immutable vertex array/projection. Reset before a new render.
// Reused strip vertices (also across passes) are transformed once, including
// failed bounds. Noncanonical projections and Naomi2 stay unbounded.
class GeometryBounds {
	struct Point {
		double left = 0, top = 0, right = 0, bottom = 0;
		enum { Unknown, Bounded, Unbounded } state = Unknown;
	};
	std::vector<Point> points;
	std::array<std::array<float, 4>, 4> matrix;
	Tile whole{};
	bool canonical = false;

	template<class Vertices> const Point *project(uint32_t index, const Vertices& vertices)
	{
		if (index >= vertices.size()) return nullptr;
		auto& p = points[index];
		if (p.state != Point::Unknown) return p.state == Point::Bounded ? &p : nullptr;
		p.state = Point::Unbounded;
		const auto& v = vertices[index];
		if (!std::isnormal(v.z) || v.z <= 0) return nullptr;
		const double reciprocal = double(matrix[2][2]) * v.z;
		if (reciprocal < 1.e-12 || reciprocal > 1.e5) return nullptr;
		const double values[]{v.x, v.y, v.z, 1};
		for (int axis = 0; axis < 2; ++axis) {
			double value = 0, magnitude = 0;
			for (int k = 0; k < 4; ++k) {
				const double term = double(matrix[k][axis]) * values[k];
				value += term; magnitude += std::abs(term);
			}
			// Preserve the original bounds' float overflow and rounding margins,
			// including native-depth division and large off-screen coordinates.
			const double safeMagnitude = double(std::numeric_limits<float>::max()) * std::min(1.0, reciprocal) / 16;
			if (!std::isfinite(value) || !std::isfinite(magnitude) || magnitude > safeMagnitude) return nullptr;
			const double size = axis == 0 ? whole.width : whole.height;
			const double margin = 4 + magnitude * std::numeric_limits<float>::epsilon() * 16 * size;
			const double screen = (value + 1) * size * .5;
			if (axis == 0) { p.left = screen - margin; p.right = screen + margin; }
			else { p.top = screen - margin; p.bottom = screen + margin; }
		}
		p.state = Point::Bounded;
		return &p;
	}
public:
	GeometryBounds() = default;
	template<class Matrix> GeometryBounds(const Matrix& m, uint32_t width, uint32_t height, size_t vertexCount, bool enabled = true)
	{ reset(m,width,height,vertexCount,enabled); }
	template<class Matrix> void reset(const Matrix& m, uint32_t width, uint32_t height, size_t vertexCount, bool enabled = true)
	{
		whole = {0,0,width,height};
		canonical = enabled && m[0][3] == 0 && m[1][3] == 0 && m[2][3] == 0 && m[3][3] == 1
			&& m[0][2] == 0 && m[1][2] == 0 && m[3][2] == 0 && std::isnormal(m[2][2]) && m[2][2] > 0;
		for (int k = 0; k < 4; ++k) for (int axis = 0; axis < 4; ++axis) matrix[k][axis] = m[k][axis];
		points.assign(canonical ? vertexCount : 0,{});
	}
	template<class Vertices> Tile triangle(const uint32_t *index, const Vertices& vertices, bool naomi2 = false)
	{
		if (!canonical || naomi2) return whole;
		const Point *a = project(index[0], vertices), *b = project(index[1], vertices), *c = project(index[2], vertices);
		if (!a || !b || !c) return whole;
		const uint32_t x = uint32_t(std::clamp(std::floor(std::min({a->left, b->left, c->left})), 0.0, double(whole.width)));
		const uint32_t y = uint32_t(std::clamp(std::floor(std::min({a->top, b->top, c->top})), 0.0, double(whole.height)));
		const uint32_t endX = uint32_t(std::clamp(std::ceil(std::max({a->right, b->right, c->right})), 0.0, double(whole.width)));
		const uint32_t endY = uint32_t(std::clamp(std::ceil(std::max({a->bottom, b->bottom, c->bottom})), 0.0, double(whole.height)));
		return {x, y, endX - x, endY - y};
	}
};
}
