#pragma once

#import <Metal/Metal.h>
#include <TargetConditionals.h>

inline MTLStorageMode metalCpuTextureStorageMode(id<MTLDevice> device)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
	// Intel GPUs also report unified memory, but cannot use shared textures.
	if (![device supportsFamily:MTLGPUFamilyApple1])
		return MTLStorageModeManaged;
#endif
	return MTLStorageModeShared;
}

inline bool metalSupportsPacked16BitFormats(id<MTLDevice> device)
{
#if TARGET_OS_SIMULATOR
	return false;
#else
	if (@available(macOS 11.0, iOS 13.0, tvOS 13.0, *))
		return [device supportsFamily:MTLGPUFamilyApple1];
	return false;
#endif
}

inline void metalSynchronizeTextureForCpu(id<MTLCommandBuffer> commands, id<MTLTexture> texture)
{
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
	if (texture.storageMode == MTLStorageModeManaged)
	{
		id<MTLBlitCommandEncoder> blit = [commands blitCommandEncoder];
		[blit synchronizeResource:texture];
		[blit endEncoding];
	}
#endif
}
