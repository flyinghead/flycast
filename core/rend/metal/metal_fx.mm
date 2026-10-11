// SPDX-License-Identifier: GPL-2.0-or-later
#include "metal_fx.h"
#import <MetalFX/MetalFX.h>
#include <atomic>

struct MetalFXUpscaler::Impl
{
	id<MTLDevice> device;
	id<MTL4Compiler> compiler;
	bool supported = false;
	std::shared_ptr<std::atomic<bool>> failed = std::make_shared<std::atomic<bool>>(false);
	struct Slot {
		NSUInteger inputWidth = 0, inputHeight = 0, outputWidth = 0, outputHeight = 0;
		MTLPixelFormat format = MTLPixelFormatInvalid;
		id<MTL4FXSpatialScaler> scaler;
		id<MTLTexture> inputCopy, output;
	};
	std::array<Slot, MetalContext::PresentationSlots> slots;
	// Repeated VSync copies and GUI frames can sample the preceding result
	// instead of encoding the same upscale again. Explicit Metal 4 queue
	// barriers order later slot writes after these reads.
	id<MTLTexture> cachedInput, cachedOutput;
	u64 cachedFrame = 0;
	NSUInteger cachedWidth = 0, cachedHeight = 0;
};

MetalFXUpscaler::MetalFXUpscaler(id<MTLDevice> device, id<MTL4Compiler> compiler) : impl(std::make_unique<Impl>())
{
	impl->device = device;
	impl->compiler = compiler;
	if (@available(macOS 26.0, *)) {
		@try { impl->supported = [MTLFXSpatialScalerDescriptor supportsMetal4FX:device]; }
		@catch (NSException *exception) {
			WARN_LOG(RENDERER, "MetalFX unavailable: %s", exception.reason.UTF8String);
		}
	}
}
MetalFXUpscaler::~MetalFXUpscaler() = default;
bool MetalFXUpscaler::supported() const { return impl->supported; }

void MetalFXUpscaler::releaseSlot(size_t slot)
{
	if (slot < impl->slots.size()) impl->slots[slot] = {};
	impl->cachedInput = impl->cachedOutput = nil;
}

id<MTLTexture> MetalFXUpscaler::upscale(id<MTLTexture> input, FCMetal4Commands * command,
	NSUInteger width, NSUInteger height, size_t index, u64 contentFrame)
{
	if (!impl->supported || impl->failed->load() || !input || !command || index >= impl->slots.size()
			|| command.device != impl->device || input.device != impl->device
			|| input.textureType != MTLTextureType2D || input.sampleCount != 1
			|| (input.pixelFormat != MTLPixelFormatRGBA8Unorm && input.pixelFormat != MTLPixelFormatBGRA8Unorm)
			|| input.width < 2 || input.height < 2 || width < input.width || height < input.height
			|| (width == input.width && height == input.height) || width > 16384 || height > 16384) {
		releaseSlot(index);
		return nil;
	}
	if (impl->cachedInput == input && impl->cachedFrame == contentFrame
			&& impl->cachedWidth == width && impl->cachedHeight == height)
		return impl->cachedOutput;

	auto& slot = impl->slots[index];
	// Cache a rejected descriptor as well: a driver allocation/creation
	// failure must not repeat its compilation and log on every frame.
	if (slot.inputWidth != input.width || slot.inputHeight != input.height
			|| slot.outputWidth != width || slot.outputHeight != height || slot.format != input.pixelFormat) {
		slot = {};
		slot.inputWidth = input.width; slot.inputHeight = input.height;
		slot.outputWidth = width; slot.outputHeight = height; slot.format = input.pixelFormat;
		if (@available(macOS 26.0, *)) {
			@try {
				MTLFXSpatialScalerDescriptor *descriptor = [[MTLFXSpatialScalerDescriptor alloc] init];
				descriptor.inputWidth = input.width; descriptor.inputHeight = input.height;
				descriptor.outputWidth = width; descriptor.outputHeight = height;
				descriptor.colorTextureFormat = input.pixelFormat;
				descriptor.outputTextureFormat = MTLPixelFormatRGBA8Unorm;
				// PVR colors are already perceptual values stored in Unorm.
				// No sRGB view or extra gamma conversion is applied to them.
				descriptor.colorProcessingMode = MTLFXSpatialScalerColorProcessingModePerceptual;
				slot.scaler = [descriptor newSpatialScalerWithDevice:impl->device compiler:impl->compiler];
				if (!slot.scaler) {
					WARN_LOG(RENDERER, "MetalFX scaler unavailable for %llux%llu -> %llux%llu; using normal scaling",
						(unsigned long long)input.width, (unsigned long long)input.height,
						(unsigned long long)width, (unsigned long long)height);
					return nil;
				}
				MTLTextureDescriptor *output = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
					width:width height:height mipmapped:NO];
				output.storageMode = MTLStorageModePrivate;
				output.hazardTrackingMode = MTLHazardTrackingModeUntracked;
				output.usage = slot.scaler.outputTextureUsage | MTLTextureUsageShaderRead;
				slot.output = [impl->device newTextureWithDescriptor:output];
				if (!slot.output) {
					slot.scaler = nil;
					WARN_LOG(RENDERER, "MetalFX output allocation failed; using normal scaling");
					return nil;
				}
			} @catch (NSException *exception) {
				slot.scaler = nil; slot.output = nil;
				WARN_LOG(RENDERER, "MetalFX setup failed: %s; using normal scaling", exception.reason.UTF8String);
				return nil;
			}
		}
	}
	if (!slot.scaler) return nil;
	id<MTLTexture> color = input;
	const MTLTextureUsage required = slot.scaler.colorTextureUsage;
	if ((input.usage & required) != required || input.storageMode != MTLStorageModePrivate) {
		// Keep the emulator's texture usages unchanged. An uncommon scaler
		// requirement gets a compatible private copy rather than touching
		// RTT feedback, VRAM conversion, or the retained original image.
		if (!slot.inputCopy) {
			@try {
				MTLTextureDescriptor *copy = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:input.pixelFormat
					width:input.width height:input.height mipmapped:NO];
				copy.storageMode = MTLStorageModePrivate;
				copy.hazardTrackingMode = MTLHazardTrackingModeUntracked;
				copy.usage = required | MTLTextureUsageShaderRead;
				slot.inputCopy = [impl->device newTextureWithDescriptor:copy];
			} @catch (NSException *) { /* Optional allocation; use normal scaling below. */ }
			if (!slot.inputCopy) {
				slot.scaler = nil; slot.output = nil;
				WARN_LOG(RENDERER, "MetalFX input allocation failed; using normal scaling");
				return nil;
			}
		}
		FCMetal4ComputeEncoder * blit = [command transferEncoder];
		if (!blit) return nil;
		[blit copyFromTexture:input sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
			sourceSize:MTLSizeMake(input.width, input.height, 1) toTexture:slot.inputCopy
			destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
		[blit endEncoding];
		color = slot.inputCopy;
	}
	slot.scaler.colorTexture = color;
	slot.scaler.inputContentWidth = input.width;
	slot.scaler.inputContentHeight = input.height;
	slot.scaler.outputTexture = slot.output;
	[command useResource:color];
	[command useResource:slot.output];
	[command retainObject:slot.scaler];
	[command queueBarrier];
	[slot.scaler encodeToCommandBuffer:command.native];
	[command queueBarrier];
	const auto failed = impl->failed;
	[command addCompletedHandler:^(FCMetal4Commands * done) {
		if (done.status == FCMetal4StatusError && !failed->exchange(true))
			WARN_LOG(RENDERER, "MetalFX disabled after a GPU command error; subsequent frames use normal scaling");
	}];
	impl->cachedInput = input; impl->cachedOutput = slot.output;
	impl->cachedFrame = contentFrame;
	impl->cachedWidth = width; impl->cachedHeight = height;
	return slot.output;
}
