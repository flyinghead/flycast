// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal4_context.h"
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
	// Native RTT contents and format aliases share a generation, not stale VRAM.
	u64 renderTargetSerial = 0, aliasSerial = 0;
	FB_W_CTRL_type renderTargetControl{};
#endif
};

// RTT lookup uses the same cache entries/cleanup as ordinary textures. Store
// cache keys rather than pointers so collection cannot leave dangling sources.
class API_AVAILABLE(macos(26.0)) MetalTextureCache final : public BaseTextureCache<MetalTexture>
{
public:
	void publishRenderTarget(MetalTexture& texture, FB_W_CTRL_type control, u32 stride, const Rect& clip);
	MetalTexture *findRenderTargetAlias(const MetalTexture& texture, TCW tcw);
	void Clear() { renderTargets.clear(); BaseTextureCache::Clear(); }
	void CollectCleanup();
private:
	std::unordered_map<u32, u64> renderTargets;
	u64 renderSerial = 0;
};
