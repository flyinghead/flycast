// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_METAL
#include "metal_texture.h"
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
	if (!context->canReuseTextureUpload(uploaded) || uploaded.width != NSUInteger(width)
			|| uploaded.height != NSUInteger(height) || uploaded.pixelFormat != format.pixelFormat
			|| uploaded.mipmapLevelCount != mipLevels)
		uploaded = context->makeTexture(width, height, format.pixelFormat,
			MTLTextureUsageShaderRead, mipmapped);
	struct Updates {
		MetalContext *context;
		explicit Updates(MetalContext *context) : context(context) { context->beginTextureUpdates(); }
		~Updates() { context->endTextureUpdates(); }
	} updates(context);
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
}

bool MetalTexture::Delete()
{
	if (!BaseTextureCacheData::Delete())
		return false;
	image = nil;
	return true;
}
#endif // Apple Silicon Metal 4 slice
