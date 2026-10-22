// SPDX-License-Identifier: GPL-2.0-or-later
#include "rend/metal4/metal4_oit_storage.h"
#include <cstdio>
#include <stdexcept>

static void check(size_t bytes, bool area1)
{
	const auto layout = metal_oit::storageLayout(bytes, area1);
	const size_t count = layout.capacity, stride = area1 ? 24 : 20;
	if (count * stride > bytes || layout.colorOffset != (area1 ? count * 20 : 0)
			|| (count < UINT32_MAX && (bytes - count * stride) >= stride))
		throw std::runtime_error("OIT layout overlaps, exceeds budget or wastes a complete node");
}
int main()
{
	for (size_t bytes = 0; bytes < 100000; ++bytes)
		for (bool area1 : {false, true}) check(bytes, area1);
	for (size_t bytes : {size_t(1) << 30, size_t(1) << 40, SIZE_MAX})
		for (bool area1 : {false, true}) check(bytes, area1);
	if (metal_oit::storageLayout(240, false).capacity != 12 || metal_oit::storageLayout(240, true).capacity != 10)
		throw std::runtime_error("Unused area-1 reserve is not available for nodes");
	std::puts("PASS OIT storage: 200006 budgets/layouts; no overlap, overflow or budget excess; 12 vs 10 nodes in 240 bytes");
}
