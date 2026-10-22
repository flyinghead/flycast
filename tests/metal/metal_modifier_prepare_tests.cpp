// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_modifier_clip.h"
#include <chrono>
#include <cstdio>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

using namespace metal_modifier;
static void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
static const std::array<float, 16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static const Triangle ordinary{{Vertex{-.7f,-.3f,1}, Vertex{.8f,-.3f,2}, Vertex{.1f,.9f,3}}};
static const Triangle extreme{{Vertex{-1e10f,-1e10f,2}, Vertex{1e10f,-1e10f,2}, Vertex{0,1e10f,2}}};
static const Triangle outside{{Vertex{1e10f,-1e10f,2}, Vertex{2e10f,-1e10f,2}, Vertex{1e10f,1e10f,2}}};

// Original renderer's eager collection, kept independent of the new remapping.
static void reference(const std::vector<Triangle>& input, const float *ndc, bool clip, bool native,
	std::vector<Triangle>& output, std::vector<uint32_t>& offsets)
{
	output.clear(); offsets.clear();
	output.reserve(input.size()); offsets.reserve(input.size() + 1);
	for (const auto& source : input) {
		offsets.push_back(output.size());
		Triangle triangle; std::memcpy(triangle.data(), source.data(), sizeof(triangle));
		if (!clip || !appendClipped(triangle, ndc, output, native)) output.push_back(triangle);
	}
	offsets.push_back(output.size());
}
static void verify(const std::vector<Triangle>& input, const float *ndc, bool clip, bool native,
	std::vector<Triangle>& scratch, std::vector<uint32_t>& offsets)
{
	std::vector<Triangle> expected; std::vector<uint32_t> expectedOffsets;
	reference(input, ndc, clip, native, expected, expectedOffsets);
	const bool changed = prepare(input.data(), input.size(), ndc, clip, native, scratch, offsets);
	const auto& actual = changed ? scratch : input;
	require(offsets == expectedOffsets, "Original volume boundaries changed");
	require(actual.size() == expected.size(), "Output triangle count changed");
	require(actual.empty() || std::memcmp(actual.data(), expected.data(), actual.size() * sizeof(Triangle)) == 0,
		"Vertex/depth/winding bytes changed");
	if (!changed) require(scratch.empty(), "Unchanged geometry was materialized");
	// Inclusion ranges can span multiple prior groups, including empty groups.
	for (size_t first = 0; first < input.size(); ++first)
		for (size_t last : {first, std::min(first + 1, input.size()), input.size()})
			require(offsets[last] - offsets[first] == expectedOffsets[last] - expectedOffsets[first], "Range remap differs");
}
static double median(std::vector<double> values)
{
	std::sort(values.begin(), values.end()); return values[values.size() / 2];
}
static void benchmark(const char *name, const std::vector<Triangle>& input, bool clip)
{
	std::vector<Triangle> eager, lazy; std::vector<uint32_t> oldOffsets, newOffsets;
	std::vector<unsigned char> uploadBytes((input.size() * 8 + 1) * sizeof(Triangle));
	verify(input, identity.data(), clip, false, lazy, newOffsets);
	std::vector<double> oldTimes, newTimes;
	const size_t repetitions = std::max(size_t(1), size_t(16384) / std::max(size_t(1), input.size()));
	for (unsigned pair = 0; pair < 37; ++pair) for (unsigned half = 0; half < 2; ++half) {
		const bool candidate = half == (pair & 1);
		const auto start = std::chrono::steady_clock::now();
		for (size_t iteration = 0; iteration < repetitions; ++iteration) {
			const std::vector<Triangle> *result;
			if (candidate) {
				const bool changed = prepare(input.data(), input.size(), identity.data(), clip, false, lazy, newOffsets);
				result = changed ? &lazy : &input;
			} else {
				reference(input, identity.data(), clip, false, eager, oldOffsets); result = &eager;
			}
			if (!result->empty()) std::memcpy(uploadBytes.data(), result->data(), result->size() * sizeof(Triangle));
			// Make the entire simulated upload observable without timing a checksum.
			asm volatile("" : : "r"(uploadBytes.data()) : "memory");
		}
		const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count() / repetitions;
		if (pair >= 6) (candidate ? newTimes : oldTimes).push_back(us);
	}
	std::printf("PROFILE %s count=%zu clip=%d old_us=%.3f new_us=%.3f ratio=%.4f samples=31\n",
		name, input.size(), clip, median(oldTimes), median(newTimes), median(newTimes) / median(oldTimes));
}
int main(int argc, char **argv)
{
	try {
		std::vector<Triangle> scratch; std::vector<uint32_t> offsets; size_t cases = 0;
		for (bool native : {false, true}) for (bool clip : {false, true}) {
			verify({}, identity.data(), clip, native, scratch, offsets); ++cases;
			for (size_t at = 0; at < 257; ++at) for (const auto& changed : {outside, extreme}) {
				std::vector<Triangle> input(257, ordinary); input[at] = changed;
				verify(input, identity.data(), clip, native, scratch, offsets); ++cases;
			}
			verify(std::vector<Triangle>(257, outside), identity.data(), clip, native, scratch, offsets); ++cases;
		}
		std::mt19937 random(0x4d4f4450);
		for (size_t fixture = 0; fixture < 6000; ++fixture) {
			auto matrix = identity;
			if (fixture % 3 == 0) {matrix[0] = matrix[5] = 0; matrix[1] = .5f; matrix[4] = -2; matrix[12] = .3f;}
			if (fixture % 11 == 0) matrix[11] = .1f;
			std::vector<Triangle> input(random() % 193);
			for (auto& triangle : input) {
				const unsigned kind = random() % 10;
				triangle = kind == 0 ? extreme : kind == 1 ? outside : ordinary;
				if (kind == 2) triangle[0][2] = -1;
				if (kind == 3) triangle[1][0] = std::numeric_limits<float>::quiet_NaN();
				if (kind == 4) triangle[2][1] = std::numeric_limits<float>::infinity();
				if (kind == 5) std::swap(triangle[0], triangle[2]);
			}
			verify(input, matrix.data(), fixture % 7 != 0, fixture % 2, scratch, offsets); ++cases;
		}
		std::vector<Triangle> fresh; std::vector<uint32_t> freshOffsets;
		prepare(&ordinary, 1, identity.data(), true, false, fresh, freshOffsets);
		require(fresh.capacity() == 0, "Unchanged frame allocated triangle storage");
		std::printf("PASS modifier preparation: %zu byte-exact cases, empty/clipped-away/prefix/reuse/native/rotation/projection/nonfinite/range checks\n", cases);
		if (argc > 1 && std::string(argv[1]) == "--profile")
			for (size_t count : {size_t(8), size_t(256), size_t(4096), size_t(32768)}) {
				std::vector<Triangle> input(count, ordinary);
				benchmark("unchanged", input, true); benchmark("projected", input, false);
				input.back() = extreme; benchmark("late-clip", input, true);
				input.back() = ordinary; input.front() = extreme; benchmark("early-clip", input, true);
				std::fill(input.begin(), input.end(), extreme); benchmark("all-clipped", input, true);
			}
	} catch (const std::exception& error) {std::fprintf(stderr, "FAIL %s\n", error.what()); return 1;}
}
