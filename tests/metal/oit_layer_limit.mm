// Run on macOS (including Intel/AMD) with Metal validation:
// clang++ -std=c++17 -fobjc-arc -framework Metal -framework Foundation \
//   tests/metal/oit_layer_limit.mm -o /tmp/oit_layer_limit
// MTL_DEBUG_LAYER=1 /tmp/oit_layer_limit
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "../../core/rend/metal/oit/metal_oit_shaders.h"
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
		id<MTLDevice> device = MTLCreateSystemDefaultDevice();
		require(device != nil, "No Metal device");
		printf("GPU: %s\n", device.name.UTF8String);
		id<MTLCommandQueue> queue = [device newCommandQueue];
		NSString *source = [NSString stringWithUTF8String:GetMetalOITShaderSource().c_str()];
		// Both ordering specializations compile the resolve and modifier-volume shaders.
		for (bool rasterOrder : { false, true }) {
			if (rasterOrder && !device.rasterOrderGroupsSupported)
				continue;
			for (unsigned layers : { 8u, 32u, 64u, 128u }) {
				MTLCompileOptions *options = [MTLCompileOptions new];
				options.preprocessorMacros = @{ @"OIT_RASTER_ORDER": @(rasterOrder), @"OIT_MAX_FRAGMENTS": @(layers) };
				NSError *error = nil;
				id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
				require(library != nil, error.localizedDescription.UTF8String);
				MTLRenderPipelineDescriptor *pipelineDesc = [MTLRenderPipelineDescriptor new];
				pipelineDesc.vertexFunction = [library newFunctionWithName:@"oit_resolve_vertex"];
				pipelineDesc.fragmentFunction = [library newFunctionWithName:@"oit_resolve_fragment"];
				pipelineDesc.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
				id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipelineDesc error:&error];
				require(pipeline != nil, error.localizedDescription.UTF8String);

				// 129 additive red fragments, each contributing exactly one 8-bit step.
				// A hardcoded 32-fragment limit fails the 8/64/128 cases.
				struct Pixel { uint32_t color; float depth; uint32_t seq, next; } pixels[129];
				for (unsigned i = 0; i < 129; ++i)
					pixels[i] = { 0x01000000, i / 129.f, 0, i == 128 ? UINT32_MAX : i + 1 };
				uint32_t head = 0;
				int32_t poly[2] { (1 << 29) | (1 << 26), -1 }; // ONE + ONE
				struct alignas(16) Uniforms {
					uint32_t width = 1, height = 1, count = 129;
					float shade = 1;
					float dither[4] { 1, 1, 1, 1 };
					uint32_t applyDither = 0;
				} uniforms;
				static_assert(sizeof(Uniforms) == 48);
				id<MTLBuffer> pixelBuffer = [device newBufferWithBytes:pixels length:sizeof(pixels) options:MTLResourceStorageModeShared];
				id<MTLBuffer> headBuffer = [device newBufferWithBytes:&head length:sizeof(head) options:MTLResourceStorageModeShared];
				id<MTLBuffer> polyBuffer = [device newBufferWithBytes:poly length:sizeof(poly) options:MTLResourceStorageModeShared];
				MTLTextureDescriptor *textureDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:1 height:1 mipmapped:NO];
				textureDesc.storageMode = MTLStorageModeManaged;
				textureDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
				id<MTLTexture> background = [device newTextureWithDescriptor:textureDesc];
				id<MTLTexture> output = [device newTextureWithDescriptor:textureDesc];
				uint32_t black = 0;
				[background replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&black bytesPerRow:4];
				MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
				pass.colorAttachments[0].texture = output;
				pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
				pass.colorAttachments[0].storeAction = MTLStoreActionStore;
				id<MTLCommandBuffer> commands = [queue commandBuffer];
				id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
				[encoder setRenderPipelineState:pipeline];
				[encoder setFragmentBuffer:pixelBuffer offset:0 atIndex:1];
				[encoder setFragmentBuffer:headBuffer offset:0 atIndex:2];
				[encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:3];
				[encoder setFragmentBuffer:polyBuffer offset:0 atIndex:4];
				[encoder setFragmentTexture:background atIndex:0];
				[encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
				[encoder endEncoding];
				id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
				[blit synchronizeResource:output];
				[blit endEncoding];
				[commands commit];
				[commands waitUntilCompleted];
				require(commands.status == MTLCommandBufferStatusCompleted, commands.error.localizedDescription.UTF8String);
				uint8_t rgba[4];
				[output getBytes:rgba bytesPerRow:4 fromRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0];
				printf("layers=%u rasterOrder=%d red=%u\n", layers, rasterOrder, rgba[0]);
				require(rgba[0] == layers && rgba[1] == 0 && rgba[2] == 0, "Incorrect fragment limit");
			}
		}
	}
}
