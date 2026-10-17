// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>

namespace metal_oit {
// OIT vertices already carry (polygon << 17) + vertex - firstVertex.
// Prove the subtraction cannot borrow and the vertex offset cannot carry
// into the polygon bits before recovering metadata from that exact sequence.
inline bool sequenceCarriesPolygon(uint32_t polygon, uint32_t firstVertex, uint32_t vertex)
{
	return polygon < (1u << 15) && vertex >= firstVertex && vertex - firstVertex < (1u << 17);
}
}
