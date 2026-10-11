// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "metal_context.h"

// Presentation-only spatial upscaling. Call after acquiring the completed
// presentation slot and before opening its render encoder.
class MetalFXUpscaler
{
public:
	MetalFXUpscaler(id<MTLDevice> device, id<MTL4Compiler> compiler);
	~MetalFXUpscaler();
	bool supported() const;
	id<MTLTexture> upscale(id<MTLTexture> input, FCMetal4Commands * command,
		NSUInteger width, NSUInteger height, size_t slot, u64 contentFrame) API_AVAILABLE(macos(26.0));
	void releaseSlot(size_t slot);
private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
