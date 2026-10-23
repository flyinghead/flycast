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

#pragma once
#include "metal_apple.h"

#include "types.h"

// MetalFX spatial upscaling of the rendered frame to its on-screen size.
// MetalFX needs macOS 13 / iOS 16 / tvOS 16 and a supported GPU. Upscale() returns nil
// whenever it can't be used, and the caller then presents the source texture as before.
class MetalUpscaler
{
public:
	void Init(id<MTLDevice> device);
	void Term();
	// Releases the scaler and its output texture
	void Reset();

	// Encodes the upscale into commandBuffer and returns the upscaled texture,
	// or nil if MetalFX is unavailable or the output isn't larger than the input.
	id<MTLTexture> Upscale(id<MTLCommandBuffer> commandBuffer, id<MTLTexture> input, u32 outputWidth, u32 outputHeight);

	bool IsSupported() const { return supported; }

private:
	bool createScaler(id<MTLTexture> input, u32 outputWidth, u32 outputHeight);

	id<MTLDevice> device = nil;
	bool supported = false;
	// id<MTLFXSpatialScaler>. Typed as id because the protocol needs macOS 13.
	id scaler = nil;
	id<MTLTexture> output = nil;
	// Configuration of the current scaler. Scaler creation is retried only when it changes.
	MTLPixelFormat inputFormat = MTLPixelFormatInvalid;
	u32 inputWidth = 0;
	u32 inputHeight = 0;
	u32 outputWidth = 0;
	u32 outputHeight = 0;
	bool failed = false;
};
