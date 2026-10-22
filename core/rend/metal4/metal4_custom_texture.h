// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <Metal/Metal.h>
#include "rend/CustomTexture.h"
#include <limits>
#include <stdexcept>
namespace metal_custom {
inline void validate(const TextureData& texture) {
	if (texture.w <= 0 || texture.h <= 0 || size_t(texture.w) > std::numeric_limits<size_t>::max()/4/size_t(texture.h)
			|| texture.data.size() != size_t(texture.w)*size_t(texture.h)*4)
		throw std::invalid_argument("Invalid master RGBA replacement texture");
}
API_AVAILABLE(macos(26.0))
inline MTLTextureDescriptor *descriptor(const TextureData& texture, bool mipmapped) {
	validate(texture);
	auto d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
		width:texture.w height:texture.h mipmapped:mipmapped];
	d.storageMode=MTLStorageModePrivate;d.hazardTrackingMode=MTLHazardTrackingModeUntracked;
	d.usage=MTLTextureUsageShaderRead;return d;
}
}
