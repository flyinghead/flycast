/*
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
// Per-pixel storage for Metal Order-Independent Transparency.
// Port of core/rend/vulkan/oit/oit_buffer.h's OITBuffers.
//
//   pixelBuffer    - linked-list fragments (color + depth + next index), one slot
//                    per translucent fragment, sized by config::PixelBufferSize.
//   pixelCounter   - a single atomic_uint, the next free index into pixelBuffer.
//                    Reset to 0 each frame with a blit fill.
//   abufferPointer - width x height head pointers, or a sentinel for "no fragments".
//                    Reallocated when the framebuffer grows.
//
// All three are MTLResourceStorageModePrivate, matching Vulkan's eDeviceLocal.
#pragma once
#include "../metal_apple.h"

#include "types.h"
#include "cfg/option.h"
#include "../metal_context.h"

#include <algorithm>

class MetalOITBuffers
{
public:
	/// Allocate / reallocate per-pixel storage to accommodate at least the
	/// given dimensions. Idempotent on repeat calls with the same or
	/// smaller dimensions. The framebuffer resize handler should call this
	/// before any OIT draw activity in the new size.
	void Init(int width, int height)
	{
		if (width <= maxWidth && height <= maxHeight)
			return;
		maxWidth = std::max(maxWidth, width);
		maxHeight = std::max(maxHeight, height);

		auto device = MetalContext::Instance()->GetDevice();
		const MTLResourceOptions opts = MTLResourceStorageModePrivate;

		if (pixelBuffer == nil)
		{
			// Cap to a sane upper bound. Vulkan used VkPhysicalDeviceLimits::
			// maxMemoryAllocationSize; Metal exposes
			// MTLDevice.maxBufferLength which is the equivalent ceiling.
			pixelBufferSize = (u64)config::PixelBufferSize;
			const u64 cap = (u64)[device maxBufferLength];
			const u64 sz  = std::min(pixelBufferSize, cap);
			pixelBuffer = [device newBufferWithLength:sz options:opts];
			[pixelBuffer setLabel:@"Flycast OIT pixel buffer"];
		}
		if (pixelCounter == nil)
		{
			pixelCounter = [device newBufferWithLength:4 options:opts];
			[pixelCounter setLabel:@"Flycast OIT pixel counter"];
		}

		// Head-pointer storage: one u32 per pixel. Reallocate any time the
		// framebuffer grows — the previous buffer is dropped and the GPU
		// will release it once nothing else references it. Caller is
		// responsible for inserting a barrier / waiting for the GPU to
		// drain prior work that touched the old buffer before resize.
		abufferPointer = [device newBufferWithLength:(maxWidth * maxHeight * sizeof(u32))
											  options:opts];
		[abufferPointer setLabel:@"Flycast OIT abuffer pointer"];

		firstFrameAfterInit = true;
	}

	/// Per-frame housekeeping: re-allocate the pixel buffer if the user
	/// changed config::PixelBufferSize between frames. Equivalent to
	/// Vulkan's OnNewFrame. Called from the renderer's per-frame entry.
	void OnNewFrame()
	{
		firstFrameAfterInit = false;
		if (pixelBufferSize != (u64)config::PixelBufferSize)
		{
			pixelBufferSize = (u64)config::PixelBufferSize;
			auto device = MetalContext::Instance()->GetDevice();
			const u64 cap = (u64)[device maxBufferLength];
			const u64 sz  = std::min(pixelBufferSize, cap);
			pixelBuffer = [device newBufferWithLength:sz
											  options:MTLResourceStorageModePrivate];
			[pixelBuffer setLabel:@"Flycast OIT pixel buffer"];
		}
	}

	/// Zero the per-frame pixel counter. Vulkan does this via a buffer
	/// copy from a pre-staged 4-byte reset buffer; on Metal we use the
	/// blit encoder's fillBuffer: directly which avoids the staging
	/// buffer entirely.
	void ResetPixelCounter(id<MTLBlitCommandEncoder> blit)
	{
		if (pixelCounter != nil)
			[blit fillBuffer:pixelCounter range:NSMakeRange(0, 4) value:0];
	}

	void Term()
	{
		pixelBuffer = nil;
		pixelCounter = nil;
		abufferPointer = nil;
		maxWidth = 0;
		maxHeight = 0;
		pixelBufferSize = 0;
		firstFrameAfterInit = false;
	}

	bool isFirstFrameAfterInit() const { return firstFrameAfterInit; }

	id<MTLBuffer> getPixelBuffer()    const { return pixelBuffer; }
	id<MTLBuffer> getPixelCounter()   const { return pixelCounter; }
	id<MTLBuffer> getAbufferPointer() const { return abufferPointer; }

	int getMaxWidth()  const { return maxWidth; }
	int getMaxHeight() const { return maxHeight; }

private:
	id<MTLBuffer> pixelBuffer    = nil;
	id<MTLBuffer> pixelCounter   = nil;
	id<MTLBuffer> abufferPointer = nil;

	bool   firstFrameAfterInit = false;
	int    maxWidth            = 0;
	int    maxHeight           = 0;
	u64    pixelBufferSize     = 0;
};
