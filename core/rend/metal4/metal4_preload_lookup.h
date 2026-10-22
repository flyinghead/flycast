// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>

namespace metal_preload {
// Preserve current/VQ/old-hash preference after partial GPU admission. A
// catalogued higher-priority hash must load on demand instead of accidentally
// taking an already-resident older-hash replacement. No payloads are copied.
template<class Map,class HasReplacement,class CanUse>
typename Map::mapped_type findResident(const Map& images,uint32_t current,uint32_t oldVq,uint32_t old,
	HasReplacement&& hasReplacement,CanUse&& canUse)
{
	const std::array<uint32_t,3> hashes{current,oldVq,old};
	for(unsigned i=0;i<hashes.size();++i) {
		const auto hash=hashes[i];
		if(i && (!hash || hash==current || (i==2 && hash==oldVq))) continue;
		const auto found=images.find(hash);
		if(found!=images.end())return canUse(found->second)?found->second:typename Map::mapped_type{};
		if(hasReplacement(hash))return {};
	}
	return {};
}
template<class Map,class HasReplacement>
typename Map::mapped_type findResident(const Map& images,uint32_t current,uint32_t oldVq,uint32_t old,
	HasReplacement&& hasReplacement)
{
	return findResident(images,current,oldVq,old,hasReplacement,[](const auto&){return true;});
}
}
