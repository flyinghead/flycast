// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace metal_oit {
struct StorageLayout {
	uint32_t capacity;
	size_t colorOffset;
};
// One physical allocation. An active area-1 stream follows the 20-byte nodes;
// otherwise that reserve is available for additional nodes. Queue barriers
// order changes of interpretation after all readers of the preceding frame.
inline StorageLayout storageLayout(size_t bytes, bool area1)
{
	const size_t count = std::min(bytes / (area1 ? 24 : 20), size_t(std::numeric_limits<uint32_t>::max()));
	return {uint32_t(count), area1 ? count * 20 : 0};
}
}
