// clang++ -std=c++17 -fobjc-arc -framework Metal -framework Foundation \
//   tests/metal/texture_storage.mm -o /tmp/texture_storage
// MTL_DEBUG_LAYER=1 /tmp/texture_storage
#include "../../core/rend/metal/metal_texture_utils.h"
#include <cstdio>
#include <cstdlib>

static void require(bool ok, const char *message)
{
	if (!ok) {
		fprintf(stderr, "%s\n", message);
		exit(1);
	}
}

int main()
{
	@autoreleasepool {
		NSArray<id<MTLDevice>> *devices = MTLCopyAllDevices();
		require(devices.count != 0, "No Metal device");
		for (id<MTLDevice> device in devices)
		{
			MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
				width:4 height:4 mipmapped:NO];
			desc.storageMode = metalCpuTextureStorageMode(device);
			desc.usage = MTLTextureUsageShaderRead;
			id<MTLTexture> source = [device newTextureWithDescriptor:desc];
			desc.usage |= MTLTextureUsageRenderTarget;
			id<MTLTexture> target = [device newTextureWithDescriptor:desc];
			require(source != nil && target != nil, "Texture allocation failed");
			uint32_t pixels[16];
			for (uint32_t& pixel : pixels)
				pixel = 0xff332211;
			[source replaceRegion:MTLRegionMake2D(0, 0, 4, 4) mipmapLevel:0 withBytes:pixels bytesPerRow:16];

			id<MTLCommandQueue> queue = [device newCommandQueue];
			id<MTLCommandBuffer> commands = [queue commandBuffer];
			MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
			pass.colorAttachments[0].texture = target;
			pass.colorAttachments[0].loadAction = MTLLoadActionClear;
			pass.colorAttachments[0].storeAction = MTLStoreActionStore;
			pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 1, 0, 1);
			id<MTLRenderCommandEncoder> render = [commands renderCommandEncoderWithDescriptor:pass];
			[render endEncoding];
			id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
			[blit copyFromTexture:source sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
				sourceSize:MTLSizeMake(4, 4, 1) toTexture:target destinationSlice:0 destinationLevel:0
				destinationOrigin:MTLOriginMake(0, 0, 0)];
			[blit endEncoding];
			metalSynchronizeTextureForCpu(commands, target);
			[commands commit];
			[commands waitUntilCompleted];
			require(commands.status == MTLCommandBufferStatusCompleted, "GPU copy failed");
			uint32_t actual[16] {};
			[target getBytes:actual bytesPerRow:16 fromRegion:MTLRegionMake2D(0, 0, 4, 4) mipmapLevel:0];
			for (uint32_t pixel : actual)
				require(pixel == 0xff332211, "CPU readback did not match uploaded pixels");
			printf("GPU: %s storage=%lu packed16=%d upload/render/copy/readback passed\n",
				device.name.UTF8String, (unsigned long)desc.storageMode, metalSupportsPacked16BitFormats(device));
		}
	}
}
