/*
	Copyright 2025 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "metal_texture.h"
#include <vector>
#include <algorithm>

// Only Apple GPUs support the packed 16-bit pixel formats A1BGR5/B5G6R5/ABGR4
// that the Dreamcast 16-bit texture types map to. Allocating such a texture on
// any other GPU aborts in Metal validation. Expand those to RGBA8 when the device
// doesn't support them. Channel order matches how each packed format samples on
// Apple GPUs, so the shader output is identical.
static inline u32 metalSimExpand16ToRGBA8(u16 p, TextureType tt)
{
	// `p` is NOT raw Dreamcast ARGB — flycast's TexCache has already converted the
	// texel into the GPU-format layout that the device's native Metal format expects.
	// Those layouts (confirmed by flycast's Vulkan renderer + the MoltenVK Metal↔VK
	// equivalence) are:
	//   _565  -> R5G6B5      (Metal B5G6R5Unorm == VK R5G6B5)   : R[15:11] G[10:5] B[4:0]
	//   _5551 -> R5G5B5A1    (Metal A1BGR5Unorm == VK R5G5B5A1) : R[15:11] G[10:6] B[5:1] A[0]
	//   _4444 -> R4G4B4A4    (Metal ABGR4Unorm  == VK R4G4B4A4) : R[15:12] G[11:8] B[7:4] A[3:0]
	// Decode each to RGBA8 (memory byte order R,G,B,A) to match what the device
	// produces. (Earlier this used ARGB1555/ARGB4444 with alpha in the HIGH bits,
	// which read alpha from red's MSB on _5551 -> wrong colors + speckled 1-bit alpha.)
	u32 r, g, b, a;
	switch (tt) {
	case TextureType::_565:   // R5G6B5: R[15:11] G[10:5] B[4:0]
		r = (p >> 11) & 0x1f; g = (p >> 5) & 0x3f; b = p & 0x1f;
		return (r << 3 | r >> 2) | ((g << 2 | g >> 4) << 8) | ((b << 3 | b >> 2) << 16) | (0xFFu << 24);
	case TextureType::_5551:  // R5G5B5A1: R[15:11] G[10:6] B[5:1] A[0]
		r = (p >> 11) & 0x1f; g = (p >> 6) & 0x1f; b = (p >> 1) & 0x1f; a = p & 1;
		return (r << 3 | r >> 2) | ((g << 3 | g >> 2) << 8) | ((b << 3 | b >> 2) << 16) | ((a ? 0xFFu : 0u) << 24);
	default:                  // _4444 R4G4B4A4: R[15:12] G[11:8] B[7:4] A[3:0]
		r = (p >> 12) & 0xf; g = (p >> 8) & 0xf; b = (p >> 4) & 0xf; a = p & 0xf;
		return (r << 4 | r) | ((g << 4 | g) << 8) | ((b << 4 | b) << 16) | ((a << 4 | a) << 24);
	}
}

void MetalTexture::UploadToGPU(int width, int height, const u8 *data, bool mipmapped, bool mipmapsIncluded)
{
	if (!MetalContext::Instance()->SupportsPacked16BitFormats()
			&& (tex_type == TextureType::_5551 || tex_type == TextureType::_565 || tex_type == TextureType::_4444))
	{
		// Total 16-bit source bytes (mirrors the dataSize math below for 16-bit).
		u32 srcBytes = (u32)width * height * 2;
		if (mipmapsIncluded) {
			int w = width / 2; u32 size = srcBytes / 4;
			while (w) { srcBytes += ((size + 3) >> 2) << 2; size /= 4; w /= 2; }
		}
		const u32 pixelCount = srcBytes / 2;
		std::vector<u32> rgba(pixelCount);
		const u16 *src16 = (const u16 *)data;
		for (u32 i = 0; i < pixelCount; i++)
			rgba[i] = metalSimExpand16ToRGBA8(src16[i], tex_type);

		if (width != (int)this->width || height != (int)this->height
				|| MTLPixelFormatRGBA8Unorm != this->format || this->texture == nil)
			Init(width, height, MTLPixelFormatRGBA8Unorm, pixelCount * 4, mipmapped, mipmapsIncluded);

		const u32 *conv = rgba.data();
		if (mipmapLevels > 1 && mipmapsIncluded)
		{
			// Mirror SetImage's included-mipmap level walk, but RGBA8 (4 bpp).
			u32 elem = 0;
			for (u32 i = 0; i < mipmapLevels; i++) {
				const u32 levelPixels = 1u << (2 * i);
				u32 mipLevel = mipmapLevels - i - 1;
				u32 mipWidth = std::max<u32>((u32)(texture.width >> mipLevel), 1u);
				u32 mipHeight = std::max<u32>((u32)(texture.height >> mipLevel), 1u);
				MTLRegion region = MTLRegionMake2D(0, 0, mipWidth, mipHeight);
				[texture replaceRegion:region mipmapLevel:mipLevel withBytes:(conv + elem) bytesPerRow:mipWidth * 4];
				elem += levelPixels;
			}
		}
		else
		{
			MTLRegion region = MTLRegionMake2D(0, 0, texture.width, texture.height);
			[texture replaceRegion:region mipmapLevel:0 withBytes:conv bytesPerRow:(u32)texture.width * 4];
			if (mipmapLevels > 1 && mipmapped && !mipmapsIncluded)
				GenerateMipmaps();
		}
		return;
	}

	MTLPixelFormat format = MTLPixelFormatInvalid;
	u32 dataSize = width * height * 2;
	switch (tex_type)
	{
	case TextureType::_5551:
		if (@available(macOS 11.0, *))
			format = MTLPixelFormatA1BGR5Unorm;
		break;
	case TextureType::_565:
		if (@available(macOS 11.0, *))
			format = MTLPixelFormatB5G6R5Unorm;
		break;
	case TextureType::_4444:
		if (@available(macOS 11.0, *))
			format = MTLPixelFormatABGR4Unorm;
		break;
	case TextureType::_8888:
		format = MTLPixelFormatRGBA8Unorm;
		dataSize *= 2;
		break;
	case TextureType::_8:
		format = MTLPixelFormatR8Unorm;
		dataSize /= 2;
		break;
	}
	if (mipmapsIncluded)
	{
		int w = width / 2;
		u32 size = dataSize / 4;
		while (w)
		{
			dataSize += ((size + 3) >> 2) << 2;		// offset must be a multiple of 4
			size /= 4;
			w /= 2;
		}
	}

	if (width != this->width || height != this->height
			|| format != this->format || this->texture == nil)
		Init(width, height, format, dataSize, mipmapped, mipmapsIncluded);

	SetImage(dataSize, data, mipmapped && !mipmapsIncluded);
}

void MetalTexture::Init(u32 width, u32 height, MTLPixelFormat format, u32 dataSize, bool mipmapped, bool mipmapsIncluded)
{
	this->width = width;
	this->height = height;
	this->format = format;
	mipmapLevels = 1;
	if (mipmapped)
		mipmapLevels += floor(log2(std::max(width, height)));

	MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];

	[desc setWidth:width];
	[desc setHeight:height];
	[desc setPixelFormat:format];
	[desc setMipmapLevelCount:mipmapLevels];
	[desc setStorageMode:MTLStorageModeShared];
	[desc setUsage:MTLTextureUsageShaderRead];

	auto device = MetalContext::Instance()->GetDevice();

	texture = [device newTextureWithDescriptor:desc];
}

void MetalTexture::SetImage(u32 srcSize, const void *srcData, bool genMipmaps) {
	u32 bpp;
	switch (tex_type) {
	case TextureType::_8888:
		bpp = 4;
		break;
	case TextureType::_8:
		bpp = 1;
		break;
	default:
		bpp = 2;
		break;
	}

	if (mipmapLevels > 1 && !genMipmaps)
	{
		// Walk the packed (smallest-first) mip chain and upload each level.
		// _8888 used to be excluded here and fell through to the single
		// base-level replaceRegion below, which uploaded the ENTIRE packed mip
		// chain as the base image -> doubled / too-small / garbled textures
		// (RE and Aerowings 2 menus use mipmapped 8888 textures). Using bpp for
		// the per-level source size makes it correct for both 8888 (4) and
		// 16-bit (2), matching Vulkan's staging mip-copy path (texture.cpp:451).
		u8 *src = (u8 *)srcData;

		for (u32 i = 0; i < mipmapLevels; i++) {
			const u32 size = (1 << (2 * i)) * bpp;

			u32 mipLevel = mipmapLevels - i - 1;
			u32 mipWidth = (u32)std::max(texture.width >> mipLevel, 1ul);
			u32 mipHeight = (u32)std::max(texture.height >> mipLevel, 1ul);

			MTLRegion region = MTLRegionMake2D(0, 0, mipWidth, mipHeight);
			[texture replaceRegion:region
					   mipmapLevel:mipLevel
						 withBytes:src
					   bytesPerRow:mipWidth * bpp];

			src += size;
		}
	}
	else
	{
		u32 rowBytes = (u32)(texture.width * bpp);

		MTLRegion region = MTLRegionMake2D(0, 0, texture.width, texture.height);
		[texture replaceRegion:region
				   mipmapLevel:0
					 withBytes:srcData
				   bytesPerRow:rowBytes];

		if (mipmapLevels > 1 && genMipmaps) {
			GenerateMipmaps();
		}
	}
}

void MetalTexture::GenerateMipmaps()
{
	verify((bool)commandBuffer);
	[commandBuffer setLabel:@"Mipmap Generation"];

	id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

	// Was a hand-rolled level loop ported from Vulkan (vulkan/texture.cpp:505),
	// which uses vkCmdBlitImage — a SCALING blit: src rect (mipWidth, mipHeight),
	// dst rect (mipWidth/2, mipHeight/2), eLinear. The port kept the src size but
	// used -copyFromTexture:, which does NOT scale, so each level got a full-size
	// source region written into a half-size destination. Metal API validation
	// asserts on it; with validation off the command buffer completes clean and
	// every mip level holds a CROP of the top-left corner of the level above
	// instead of a downsample.
	// -generateMipmapsForTexture: is the real equivalent of Vulkan's scaling blit
	// (also a linear filter). Verified under MTL_DEBUG_LAYER against Init()'s exact
	// descriptor: StorageModeShared + ShaderRead only — no RenderTarget usage needed.
	[blitEncoder generateMipmapsForTexture:texture];

	[blitEncoder endEncoding];
}

void MetalTexture::deferDeleteResource(MetalFlightManager *manager)
{
	class ResourceDeleter : public MetalDeletable
	{
	public:
		ResourceDeleter(MetalTexture *texture)
		{
			std::swap(this->texture, texture->texture);
			std::swap(this->readOnlyTexture, texture->readOnlyTexture);
		}

		~ResourceDeleter() override {
			texture = nil;
			readOnlyTexture = nil;
		}

	private:
		id<MTLTexture> texture = nil;
		id<MTLTexture> readOnlyTexture;
	};
	manager->addToFlight(new ResourceDeleter(this));
}

void MetalTexture::CreateReadOnlyCopy(id<MTLCommandBuffer> commandBuffer)
{
	if (!texture || readOnlyTexture)
		return;

	MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];
	[desc setWidth:texture.width];
	[desc setHeight:texture.height];
	[desc setPixelFormat:texture.pixelFormat];
	[desc setMipmapLevelCount:texture.mipmapLevelCount];
	[desc setStorageMode:MTLStorageModePrivate];
	[desc setUsage:MTLTextureUsageShaderRead];

	readOnlyTexture = [texture.device newTextureWithDescriptor:desc];
	[readOnlyTexture setLabel:@"RTT Read-Only Copy"];

	id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
	[blitEncoder copyFromTexture:texture
					 sourceSlice:0
					 sourceLevel:0
					sourceOrigin:MTLOriginMake(0, 0, 0)
					  sourceSize:MTLSizeMake(texture.width, texture.height, 1)
					   toTexture:readOnlyTexture
				destinationSlice:0
				destinationLevel:0
			   destinationOrigin:MTLOriginMake(0, 0, 0)];

	for (NSUInteger level = 1; level < texture.mipmapLevelCount; level++) {
		NSUInteger mipWidth = MAX(texture.width >> level, 1);
		NSUInteger mipHeight = MAX(texture.height >> level, 1);

		[blitEncoder copyFromTexture:texture
						 sourceSlice:0
						 sourceLevel:level
						sourceOrigin:MTLOriginMake(0, 0, 0)
						  sourceSize:MTLSizeMake(mipWidth, mipHeight, 1)
						   toTexture:readOnlyTexture
					destinationSlice:0
					destinationLevel:level
				   destinationOrigin:MTLOriginMake(0, 0, 0)];
	}

	[blitEncoder endEncoding];
}

MetalSamplers::MetalSamplers() = default;
MetalSamplers::~MetalSamplers() {
	term();
}

void MetalTextureCache::Cleanup()
{
	std::vector<u64> list;

	u32 TargetFrame = std::max((u32)120, FrameCount) - 120;

	for (const auto& [id, texture] : cache)
	{
		if (texture.dirty && texture.dirty < TargetFrame)
			list.push_back(id);

		if (list.size() > 5)
			break;
	}

	for (u64 id : list)
	{
		if (clearTexture(&cache[id]))
			cache.erase(id);
	}
}