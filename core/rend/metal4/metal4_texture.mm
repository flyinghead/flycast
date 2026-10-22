// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_METAL
#include "metal4_texture.h"
#include "metal4_custom_texture.h"
#include "metal4_preload.h"
#include "metal4_upload_scope.h"
#include <algorithm>

namespace {
struct TextureFormat {
	MTLPixelFormat pixelFormat;
	NSUInteger bytesPerPixel;
};
TextureFormat textureFormat(TextureType type)
{
	if (type == TextureType::_8) return {MTLPixelFormatR8Unorm, 1};
	if (type == TextureType::_8888) return {MTLPixelFormatRGBA8Unorm, 4};
	if (@available(macOS 26.0, *)) {
		// Decoder words put red in the high bits. Metal's packed format
		// names describe the same components from low to high bits.
		switch (type) {
		case TextureType::_565: return {MTLPixelFormatB5G6R5Unorm, 2};
		case TextureType::_5551: return {MTLPixelFormatA1BGR5Unorm, 2};
		case TextureType::_4444: return {MTLPixelFormatABGR4Unorm, 2};
		default: break;
		}
	}
	throw RendererException("Unsupported Metal texture format");
}

}

bool MetalTexture::Force32BitTexture(TextureType type) const
{
	switch (type) {
	case TextureType::_565:
	case TextureType::_5551:
	case TextureType::_4444:
		return !MetalContext::Instance()->supportsPackedTextures();
	default:
		return false;
	}
}

std::string MetalTexture::GetId() { return std::to_string((uintptr_t)(__bridge void *)image); }

void MetalTexture::UploadToGPU(int width, int height, const u8 *data, bool mipmapped, bool mipmapsIncluded)
{
	renderTargetSerial = aliasSerial = 0;
	MetalContext *context = MetalContext::Instance();
	const TextureFormat format = textureFormat(tex_type);
	if (format.bytesPerPixel == 2 && !context->supportsPackedTextures())
		throw RendererException("Packed Metal texture supplied on an unsupported GPU");
	NSUInteger mipLevels = 1;
	if (mipmapped)
		for (int dimension = std::max(width, height); dimension > 1; dimension >>= 1) ++mipLevels;
	// Match Vulkan's update of a compatible sampled image. GPU-only writes
	// are ordered after earlier reads; CPU staging remains completion-owned.
	id<MTLTexture> uploaded = image;
	if (sharedPreloadImage || !context->canReuseTextureUpload(uploaded) || uploaded.width != NSUInteger(width)
			|| uploaded.height != NSUInteger(height) || uploaded.pixelFormat != format.pixelFormat
			|| uploaded.mipmapLevelCount != mipLevels)
		uploaded = context->makeTexture(width, height, format.pixelFormat,
			MTLTextureUsageShaderRead, mipmapped);
	metal_upload::Scope updates(context);
	if (mipmapped && mipmapsIncluded)
	{
		const int levels = uploaded.mipmapLevelCount;
		// Flycast's decoded mip chain is ordered smallest to largest.
		for (int level = levels - 1; level >= 0; --level)
		{
			const int w = std::max(1, width >> level), h = std::max(1, height >> level);
			const NSUInteger rowBytes = NSUInteger(w) * format.bytesPerPixel;
			context->uploadTextureLevel(uploaded, level, data, rowBytes);
			data += rowBytes * h;
		}
	}
	else
	{
		context->uploadTextureLevel(uploaded, 0, data, NSUInteger(width) * format.bytesPerPixel);
		if (mipmapped) context->generateMipmaps(uploaded);
	}
	image = uploaded;
	sharedPreloadImage = false;
}

bool MetalTexture::UseGpuCustomTexture()
{
	if (!preloadLookup) return false;
	id<MTLTexture> prepared=preloadLookup(preloadOwner,texture_hash,old_vqtexture_hash,old_texture_hash);
	if (!prepared) return false;
	image=prepared;sharedPreloadImage=true;renderTargetSerial=aliasSerial=0;
	return true;
}

MetalPreloadedTexture::Ptr MetalTexture::preload(const TextureData& texture,bool& deferred)
{
	auto ctx=MetalContext::Instance();
	auto allocated=metal_preload::allocate(ctx->device(),texture,metal_preload::deviceBudget(ctx->device()));
	deferred=allocated.deferred;if(!allocated.image)return {};
	metal_upload::Scope updates(ctx);
	ctx->uploadTextureLevel(allocated.image,0,texture.data.data(),NSUInteger(texture.w)*4);
	ctx->generateMipmaps(allocated.image);
	metal_preload::trackUpload(allocated.image,ctx->textureUploadState());
	return std::make_shared<MetalPreloadedTexture>(allocated.image);
}
bool MetalTexture::preloadUsable(const MetalPreloadedTexture::Ptr& texture)
{
	return texture && metal_preload::usable(texture->image,MetalContext::Instance()->queue());
}
bool MetalTexture::useGpuPreloadedTexture(const MetalPreloadedTexture::Ptr& texture)
{
	if(!preloadUsable(texture))return false;
	image=texture->image;sharedPreloadImage=true;renderTargetSerial=aliasSerial=0;return true;
}

bool MetalTexture::Delete()
{
	if (!BaseTextureCacheData::Delete())
		return false;
	image = nil;
	sharedPreloadImage = false;
	renderTargetSerial = aliasSerial = 0;
	return true;
}

void MetalTextureCache::publishRenderTarget(MetalTexture& texture, FB_W_CTRL_type control, u32 stride, const Rect& clip)
{
	texture.sharedPreloadImage = false;
	texture.renderTargetSerial = texture.aliasSerial = 0;
	renderTargets.erase(texture.startAddress);
	// Reinterpretation is exact for a complete linear 16-bit image. Partial
	// writes, packed rows and differently sized/twiddled views retain decoding.
	if (control.fb_packmode > 3 || (stride && stride != u32(texture.width) * 2)
			|| clip.origin != glm::ivec2(0) || clip.size != glm::ivec2(texture.width, texture.height)) return;
	texture.renderTargetControl = control;
	texture.renderTargetSerial = ++renderSerial;
	const u64 key = (texture.tsp.full & TSPTextureCacheMask.full)
		| (u64(texture.tcw.full & TCWTextureCacheMask.full) << 32);
	renderTargets[texture.startAddress] = key;
	// CPU writes must invalidate a GPU-only source before another format can
	// borrow it. This uses the existing VRAM-write notification mechanism.
	texture.protectVRam();
}

MetalTexture *MetalTextureCache::findRenderTargetAlias(const MetalTexture& texture, TCW tcw)
{
	if (!tcw.ScanOrder || tcw.MipMapped || tcw.VQ_Comp || tcw.PixelFmt > Pixel4444) return nullptr;
	const auto record = renderTargets.find(texture.startAddress);
	if (record == renderTargets.end()) return nullptr;
	const auto entry = cache.find(record->second);
	if (entry == cache.end()) return nullptr;
	auto& source = entry->second;
	if (&source == &texture || !source.renderTargetSerial || source.dirty || !source.image
			|| source.width != texture.width || source.height != texture.height) return nullptr;
	u32 stride = tcw.StrideSel ? (TEXT_CONTROL & 31) * 32 : texture.width;
	if (stride && stride != texture.width) return nullptr;
	return &source;
}

void MetalTextureCache::CollectCleanup()
{
	BaseTextureCache::CollectCleanup();
	for (auto i = renderTargets.begin(); i != renderTargets.end();) {
		const auto source = cache.find(i->second);
		if (source == cache.end() || !source->second.renderTargetSerial || source->second.dirty)
			i = renderTargets.erase(i);
		else ++i;
	}
}
#endif // Apple Silicon Metal 4 slice
