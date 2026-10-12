// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal_context.h"
#include "rend/TexCache.h"

class API_AVAILABLE(macos(26.0)) MetalTexture final : public BaseTextureCacheData
{
public:
	MetalTexture() : BaseTextureCacheData({}, {}, 0) {}
	MetalTexture(TSP tsp, TCW tcw, int area) : BaseTextureCacheData(tsp, tcw, area) {}
	MetalTexture(MetalTexture&&) = default;
	std::string GetId() override;
	bool Force32BitTexture(TextureType type) const override;
	void UploadToGPU(int width, int height, const u8 *data, bool mipmapped, bool mipmapsIncluded = false) override;
	bool Delete() override;
#ifdef __OBJC__
	id<MTLTexture> image = nil;
#endif
};
