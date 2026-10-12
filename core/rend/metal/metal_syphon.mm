// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_METAL
#include "metal_syphon.h"
#include "types.h"
#include "cfg/cfg.h"
#import <IOSurface/IOSurface.h>
#import <Syphon/SyphonSubclassing.h>

// Syphon's public subclassing API owns discovery and IOSurface transport.
// Flycast owns GPU encoding, residency and feedback on its native Metal 4 queue.
// No patched SyphonMetalServer or legacy MTLCommandBuffer is required.
API_AVAILABLE(macos(26.0))
@interface FlycastMetalSyphonServer : SyphonServerBase
@property (readonly, strong) id<MTLDevice> device;
- (instancetype)initWithName:(NSString *)name device:(id<MTLDevice>)device;
- (id<MTLTexture>)newFrameImage;
- (void)publishTexture:(id<MTLTexture>)texture onCommands:(FCMetal4Commands *)command;
@end

@implementation FlycastMetalSyphonServer {
	id<MTLDevice> _device;
	id<MTLTexture> _surfaceTexture;
	BOOL _stopped;
	uint64_t _nextFrame, _publishedFrame;
}

- (instancetype)initWithName:(NSString *)name device:(id<MTLDevice>)device
{
	if (!device) return nil;
	if ((self = [super initWithName:name options:nil]))
		_device = device;
	return self;
}

- (id<MTLDevice>)device { return _device; }

- (id<MTLTexture>)newFrameImage
{
	@synchronized (self) {
		return _surfaceTexture;
	}
}

- (void)publishTexture:(id<MTLTexture>)texture onCommands:(FCMetal4Commands *)command
{
	@synchronized (self) {
		if (_stopped || !self.hasClients) return;
		if (!command || command.status != FCMetal4StatusEncoding || command.device != _device
			|| !texture || texture.device != _device)
			throw FlycastException("Invalid Metal 4 Syphon submission");
		// Syphon transports BGRA8 IOSurfaces; scaling/conversion is performed by
		// MetalContext before this copy. Preserve the source's Metal orientation.
		if (texture.pixelFormat != MTLPixelFormatBGRA8Unorm || texture.framebufferOnly
			|| texture.textureType != MTLTextureType2D || texture.sampleCount != 1
			|| texture.width == 0 || texture.height == 0)
			throw FlycastException("Invalid Metal 4 Syphon source texture");

		if (!_surfaceTexture || _surfaceTexture.width != texture.width || _surfaceTexture.height != texture.height) {
			// Old submissions retain their own texture, but may no longer announce
			// this server's replacement surface after a resize.
			_surfaceTexture = nil;
			IOSurfaceRef surface = [self newSurfaceForWidth:texture.width height:texture.height options:nil];
			if (!surface) throw FlycastException("Cannot allocate Metal 4 Syphon surface");
			MTLTextureDescriptor *descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
				width:texture.width height:texture.height mipmapped:NO];
			descriptor.storageMode = MTLStorageModeShared;
			descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
			_surfaceTexture = [_device newTextureWithDescriptor:descriptor iosurface:surface plane:0];
			CFRelease(surface);
			if (!_surfaceTexture) throw FlycastException("Cannot create Metal 4 Syphon surface texture");
			_surfaceTexture.label = @"Flycast Metal 4 Syphon Surface";
		}

		id<MTLTexture> destination = _surfaceTexture;
		const uint64_t frame = ++_nextFrame;
		FCMetal4ComputeEncoder *copy = [command transferEncoder];
		copy.label = @"Metal 4 Syphon Copy";
		// The encoder registers both textures with the submission's residency
		// set and retains them through feedback, including resize and shutdown.
		[copy copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
			sourceSize:MTLSizeMake(texture.width, texture.height, 1) toTexture:destination
			destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
		[copy endEncoding];
		[command addCompletedHandler:^(FCMetal4Commands *done) {
			@synchronized (self) {
				if (done.status == FCMetal4StatusCompleted && !self->_stopped
					&& destination == self->_surfaceTexture && frame > self->_publishedFrame) {
					// Polling Syphon clients detect changes via IOSurfaceGetSeed,
					// independently of frame-notification callbacks. Native Metal 4
					// copies do not update that seed automatically. Mark this shared
					// surface modified only after successful GPU feedback, without a
					// CPU pixel copy or a blocking GPU readback.
					IOSurfaceRef surface = destination.iosurface;
					if (!surface || IOSurfaceLock(surface, kIOSurfaceLockAvoidSync, nullptr) != KERN_SUCCESS)
						throw FlycastException("Cannot lock completed Metal 4 Syphon surface");
					if (IOSurfaceUnlock(surface, kIOSurfaceLockAvoidSync, nullptr) != KERN_SUCCESS)
						throw FlycastException("Cannot mark Metal 4 Syphon surface updated");
					self->_publishedFrame = frame;
					[self publish];
				}
			}
		}];
	}
}

- (void)stop
{
	@synchronized (self) {
		if (_stopped) return;
		_stopped = YES;
		_surfaceTexture = nil;
		[super stop];
	}
}
@end

// Renderer-owned; completion handlers retain the specific server they target,
// so a stopped/recreated exporter cannot receive an old submission's frame.
static FlycastMetalSyphonServer *nativeMetalServer API_AVAILABLE(macos(26.0));

bool os_VideoRoutingMetalHasClients(id<MTLDevice> device)
{
	if (nativeMetalServer && nativeMetalServer.device != device)
		os_VideoRoutingTermMetal();
	const int boardID = config::loadInt("naomi", "BoardId");
	NSString *name = [NSString stringWithFormat:(boardID == 0 ? @"Video Content" : @"Video Content - %d"), boardID];
	if (!nativeMetalServer) {
		nativeMetalServer = [[FlycastMetalSyphonServer alloc] initWithName:name device:device];
		if (!nativeMetalServer) throw FlycastException("Cannot create Metal 4 Syphon server");
	}
	if (![nativeMetalServer.name isEqualToString:name]) nativeMetalServer.name = name;
	return nativeMetalServer.hasClients;
}

void os_VideoRoutingPublishMetalTexture(id<MTLTexture> texture, FCMetal4Commands *command)
{
	[nativeMetalServer publishTexture:texture onCommands:command];
}

void os_VideoRoutingTermMetal()
{
	[nativeMetalServer stop];
	nativeMetalServer = nil;
}

#ifdef METAL_RENDERER_TESTS
NSDictionary *os_VideoRoutingMetalServerDescriptionForTests()
{
	return nativeMetalServer.serverDescription;
}
#endif
#endif // Apple Silicon Metal 4 slice
