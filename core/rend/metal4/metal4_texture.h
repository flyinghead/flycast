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
	bool uploadCustomTexture(const PreparedCustomTexture& texture, bool mipmapped) override;
	bool useGpuPreloadedTexture(const GpuPreloadedTexture::Ptr& texture) override;
	static CustomTextureCapabilities customCapabilities();
	static GpuPreloadedTexture::Ptr preload(const PreparedCustomTexture& texture, bool& deferred);
	static bool preloadUsable(const GpuPreloadedTexture::Ptr& texture);
#ifdef __OBJC__
	id<MTLTexture> image = nil;
	// Native RTT contents and format aliases share a generation, not stale VRAM.
	u64 renderTargetSerial = 0, aliasSerial = 0;
	FB_W_CTRL_type renderTargetControl{};
	// A preloaded image is shared by arbitrary cache entries. Never upload
	// ordinary texture data into it, even after dropping the base-class owner.
	bool sharedPreloadImage = false;
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
