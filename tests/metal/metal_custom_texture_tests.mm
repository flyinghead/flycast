// Native formats and mip transfers. SPDX-License-Identifier: GPL-2.0-or-later
#import <Metal/Metal.h>
#include "rend/metal4/metal4_custom_texture.h"
#include "rend/metal4/metal4_commands.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
PreparedCustomTexture payload(NativeTextureFormat format, uint32_t width, uint32_t height, unsigned levels)
{
	PreparedCustomTexture result;
	result.nativeFormat = format; result.width = width; result.height = height;
	uint64_t offset = 0;
	for (unsigned i = 0; i < levels; ++i) {
		auto level = computeMipLayout(format, width, height, offset);
		result.levels.push_back(level); offset += level.byteSize;
		width = std::max(1u, width / 2); height = std::max(1u, height / 2);
	}
	result.bytes.resize(offset);
	return result;
}
}

int main()
{
	@autoreleasepool { try {
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		require(device && [device supportsFamily:MTLGPUFamilyMetal4] && [device supportsFamily:MTLGPUFamilyApple7],
			"Metal4/Apple7 device required");
		FCMetal4Queue *queue = [[FCMetal4Queue alloc] initWithDevice:device];
		const auto caps = metal_custom::capabilities(device);
		require(caps.backend == CustomTextureBackend::Metal4, "Wrong backend identity");
		unsigned formats = 0, mipChecks = 0, rejected = 0;
		for (unsigned f = 0; f < unsigned(NativeTextureFormat::Count); ++f) {
			const auto format = NativeTextureFormat(f);
			if (!caps.supports(format)) continue;
			// Non-block-multiple dimensions and mips smaller than a block.
			auto source = payload(format, 19, 13, 5);
			for (size_t i = 0; i < source.bytes.size(); ++i) source.bytes[i] = uint8_t(i * 17 + f);
			id<MTLTexture> image = metal_custom::newTexture(device, source, true);
			require(image && image.mipmapLevelCount == source.levels.size(), "Native format or mip allocation failed");
			if (format == NativeTextureFormat::Bc1Unorm)
				require(image.swizzle.alpha == MTLTextureSwizzleOne, "BC1 RGB alpha is not opaque");
			std::vector<size_t> offsets;
			size_t length = 0;
			for (const auto& mip : source.levels) {
				length = (length + 255) & ~size_t(255); offsets.push_back(length); length += mip.byteSize;
			}
			id<MTLBuffer> staging = [device newBufferWithLength:length options:MTLResourceStorageModeShared];
			id<MTLBuffer> readback = [device newBufferWithLength:length options:MTLResourceStorageModeShared];
			require(staging && readback, "Transfer buffer allocation failed");
			for (size_t i = 0; i < source.levels.size(); ++i)
				memcpy(static_cast<uint8_t *>(staging.contents) + offsets[i], source.bytes.data() + source.levels[i].byteOffset, source.levels[i].byteSize);
			auto command = [queue commandBuffer];
			auto transfer = [command transferEncoder];
			for (size_t i = 0; i < source.levels.size(); ++i) {
				const auto& mip = source.levels[i];
				[transfer copyFromBuffer:staging sourceOffset:offsets[i] sourceBytesPerRow:mip.rowPitchBytes
					sourceBytesPerImage:0 sourceSize:MTLSizeMake(mip.width,mip.height,1) toTexture:image
					destinationSlice:0 destinationLevel:i destinationOrigin:MTLOriginMake(0,0,0)];
			}
			for (size_t i = 0; i < source.levels.size(); ++i) {
				const auto& mip = source.levels[i];
				[transfer copyFromTexture:image sourceSlice:0 sourceLevel:i sourceOrigin:MTLOriginMake(0,0,0)
					sourceSize:MTLSizeMake(mip.width,mip.height,1) toBuffer:readback destinationOffset:offsets[i]
					destinationBytesPerRow:mip.rowPitchBytes destinationBytesPerImage:0];
			}
			[transfer endEncoding]; [command commit]; [command waitUntilCompleted];
			require(command.status == FCMetal4StatusCompleted, "GPU mip transfer failed");
			for (size_t i = 0; i < source.levels.size(); ++i) {
				const auto& mip = source.levels[i];
				require(memcmp(static_cast<uint8_t *>(readback.contents) + offsets[i], source.bytes.data() + mip.byteOffset, mip.byteSize) == 0,
					"GPU mip contents differ from prepared payload"); ++mipChecks;
			}
			++formats;
		}
		auto rgba = payload(NativeTextureFormat::Rgba8Unorm, 8, 4, 1); rgba.generateMipmaps = true;
		require(metal_custom::newTexture(device, rgba, true).mipmapLevelCount == 4, "Generated RGBA chain length");
		require(metal_custom::newTexture(device, rgba, false).mipmapLevelCount == 1, "Unrequested generated mips");
		auto extra = payload(NativeTextureFormat::Rgba8Unorm, 1, 1, 2);
		require(!metal_custom::newTexture(device, extra, false), "Excess mip count accepted"); ++rejected;
		auto bad = rgba; ++bad.levels[0].rowPitchBytes;
		try { (void)metal_custom::newTexture(device, bad, false); } catch (const std::exception&) { ++rejected; }
		bad = rgba; bad.bytes.pop_back();
		try { (void)metal_custom::newTexture(device, bad, false); } catch (const std::exception&) { ++rejected; }
		bad = rgba; bad.width = 0;
		try { (void)metal_custom::newTexture(device, bad, false); } catch (const std::exception&) { ++rejected; }
		require(rejected == 4, "Invalid payload was not rejected");
		printf("PASS custom formats: %u formats, %u exact GPU mip roundtrips, 4 invalid payloads rejected\n", formats, mipChecks);
		return 0;
	} catch (const std::exception& error) { fprintf(stderr,"FAIL: %s\n",error.what()); return 1; } }
}
