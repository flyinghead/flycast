// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#import <Metal/Metal.h>
#include "rend/custom_texture/CustomTextureTypes.h"

namespace metal_custom {
API_AVAILABLE(macos(26.0))
inline MTLPixelFormat format(NativeTextureFormat value)
{
	switch (value) {
	case NativeTextureFormat::Rgba8Unorm: return MTLPixelFormatRGBA8Unorm;
	case NativeTextureFormat::Bc1Unorm: return MTLPixelFormatBC1_RGBA;
	case NativeTextureFormat::Bc3Unorm: return MTLPixelFormatBC3_RGBA;
	case NativeTextureFormat::Bc7Unorm: return MTLPixelFormatBC7_RGBAUnorm;
	case NativeTextureFormat::Etc2Rgb8Unorm: return MTLPixelFormatETC2_RGB8;
	case NativeTextureFormat::Etc2Rgba8Unorm: return MTLPixelFormatEAC_RGBA8;
	case NativeTextureFormat::Astc4x4Unorm: return MTLPixelFormatASTC_4x4_LDR;
	case NativeTextureFormat::Astc5x4Unorm: return MTLPixelFormatASTC_5x4_LDR;
	case NativeTextureFormat::Astc5x5Unorm: return MTLPixelFormatASTC_5x5_LDR;
	case NativeTextureFormat::Astc6x5Unorm: return MTLPixelFormatASTC_6x5_LDR;
	case NativeTextureFormat::Astc6x6Unorm: return MTLPixelFormatASTC_6x6_LDR;
	case NativeTextureFormat::Astc8x5Unorm: return MTLPixelFormatASTC_8x5_LDR;
	case NativeTextureFormat::Astc8x6Unorm: return MTLPixelFormatASTC_8x6_LDR;
	case NativeTextureFormat::Astc8x8Unorm: return MTLPixelFormatASTC_8x8_LDR;
	case NativeTextureFormat::Astc10x5Unorm: return MTLPixelFormatASTC_10x5_LDR;
	case NativeTextureFormat::Astc10x6Unorm: return MTLPixelFormatASTC_10x6_LDR;
	case NativeTextureFormat::Astc10x8Unorm: return MTLPixelFormatASTC_10x8_LDR;
	case NativeTextureFormat::Astc10x10Unorm: return MTLPixelFormatASTC_10x10_LDR;
	case NativeTextureFormat::Astc12x10Unorm: return MTLPixelFormatASTC_12x10_LDR;
	case NativeTextureFormat::Astc12x12Unorm: return MTLPixelFormatASTC_12x12_LDR;
	case NativeTextureFormat::Count: return MTLPixelFormatInvalid;
	}
	return MTLPixelFormatInvalid;
}

API_AVAILABLE(macos(26.0))
inline CustomTextureCapabilities capabilities(id<MTLDevice> device)
{
	// The native backend already requires Apple7 and Metal4. BC is an
	// independent capability (not present on every supported Apple GPU).
	auto result = CustomTextureCapabilities::rgbaOnly(CustomTextureBackend::Metal4);
	if (!device) { result.setSupported(NativeTextureFormat::Rgba8Unorm, false); return result; }
	const bool apple = [device supportsFamily:MTLGPUFamilyApple7];
	for (unsigned i = 1; i < unsigned(NativeTextureFormat::Count); ++i) {
		const auto native = NativeTextureFormat(i);
		const bool bc = native == NativeTextureFormat::Bc1Unorm || native == NativeTextureFormat::Bc3Unorm
			|| native == NativeTextureFormat::Bc7Unorm;
		result.setSupported(native, bc ? device.supportsBCTextureCompression : apple);
	}
	return result;
}

API_AVAILABLE(macos(26.0))
inline MTLTextureDescriptor *descriptor(const PreparedCustomTexture& texture, bool mipmapped)
{
	MTLTextureDescriptor *result = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format(texture.nativeFormat)
		width:texture.width height:texture.height mipmapped:NO];
	result.mipmapLevelCount = mipmapped && texture.generateMipmaps
		? mipmapLevelCount(texture.width, texture.height) : texture.levels.size();
	result.storageMode = MTLStorageModePrivate;
	result.hazardTrackingMode = MTLHazardTrackingModeUntracked;
	result.usage = MTLTextureUsageShaderRead;
	// NativeTextureFormat::Bc1Unorm is RGB (Vulkan's BC1_RGB), whereas
	// Metal exposes RGBA BC1. Preserve RGB semantics for transparent blocks.
	if (texture.nativeFormat == NativeTextureFormat::Bc1Unorm)
		result.swizzle = MTLTextureSwizzleChannelsMake(MTLTextureSwizzleRed, MTLTextureSwizzleGreen,
			MTLTextureSwizzleBlue, MTLTextureSwizzleOne);
	return result;
}

API_AVAILABLE(macos(26.0))
inline id<MTLTexture> newTexture(id<MTLDevice> device, const PreparedCustomTexture& texture, bool mipmapped)
{
	validatePreparedCustomTexture(texture);
	if (texture.levels.size() > mipmapLevelCount(texture.width, texture.height)
			|| !capabilities(device).canUpload(texture.nativeFormat, texture.width, texture.height,
				static_cast<uint32_t>(texture.levels.size()))) return nil;
	return [device newTextureWithDescriptor:descriptor(texture, mipmapped)];
}
}
