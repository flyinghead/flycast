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

#include "metal_upscaler.h"

// MetalFX isn't in the iOS and tvOS simulator SDKs
#if __has_include(<MetalFX/MetalFX.h>)
#import <MetalFX/MetalFX.h>
#define HAVE_METALFX 1
#endif

void MetalUpscaler::Init(id<MTLDevice> device)
{
	Term();
	this->device = device;
	supported = false;
#ifdef HAVE_METALFX
	if (@available(macOS 13.0, iOS 16.0, tvOS 16.0, *))
		supported = [MTLFXSpatialScalerDescriptor supportsDevice:device];
#endif
	NOTICE_LOG(RENDERER, "MetalFX spatial upscaling %s", supported ? "supported" : "not supported");
}

void MetalUpscaler::Term()
{
	Reset();
	device = nil;
	supported = false;
}

void MetalUpscaler::Reset()
{
	scaler = nil;
	output = nil;
	inputFormat = MTLPixelFormatInvalid;
	inputWidth = inputHeight = 0;
	outputWidth = outputHeight = 0;
	failed = false;
}

bool MetalUpscaler::createScaler(id<MTLTexture> input, u32 outputWidth, u32 outputHeight)
{
	scaler = nil;
	output = nil;
	inputFormat = input.pixelFormat;
	inputWidth = (u32)input.width;
	inputHeight = (u32)input.height;
	this->outputWidth = outputWidth;
	this->outputHeight = outputHeight;
	failed = true;
#ifdef HAVE_METALFX
	if (@available(macOS 13.0, iOS 16.0, tvOS 16.0, *))
	{
		MTLFXSpatialScalerDescriptor *desc = [[MTLFXSpatialScalerDescriptor alloc] init];
		desc.inputWidth = inputWidth;
		desc.inputHeight = inputHeight;
		desc.outputWidth = outputWidth;
		desc.outputHeight = outputHeight;
		desc.colorTextureFormat = inputFormat;
		desc.outputTextureFormat = inputFormat;
		// The frame is gamma-encoded in a UNorm texture
		desc.colorProcessingMode = MTLFXSpatialScalerColorProcessingModePerceptual;
		id<MTLFXSpatialScaler> fxScaler = [desc newSpatialScalerWithDevice:device];
		if (fxScaler == nil)
		{
			WARN_LOG(RENDERER, "MetalFX: spatial scaler creation failed (%dx%d -> %dx%d)", inputWidth, inputHeight, outputWidth, outputHeight);
			return false;
		}
		if ((input.usage & fxScaler.colorTextureUsage) != fxScaler.colorTextureUsage)
		{
			WARN_LOG(RENDERER, "MetalFX: input texture usage %lx lacks %lx", (unsigned long)input.usage, (unsigned long)fxScaler.colorTextureUsage);
			return false;
		}
		MTLTextureDescriptor *texDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:inputFormat
				width:outputWidth height:outputHeight mipmapped:NO];
		texDesc.usage = fxScaler.outputTextureUsage | MTLTextureUsageShaderRead;
		texDesc.storageMode = MTLStorageModePrivate;
		output = [device newTextureWithDescriptor:texDesc];
		if (output == nil)
			return false;
		[output setLabel:@"MetalFX Output"];
		scaler = fxScaler;
		failed = false;
		NOTICE_LOG(RENDERER, "MetalFX: spatial scaler %dx%d -> %dx%d", inputWidth, inputHeight, outputWidth, outputHeight);
		return true;
	}
#endif
	return false;
}

id<MTLTexture> MetalUpscaler::Upscale(id<MTLCommandBuffer> commandBuffer, id<MTLTexture> input, u32 outputWidth, u32 outputHeight)
{
	if (!supported || input == nil || commandBuffer == nil)
		return nil;
	// Not worth it for a tiny input, such as the 1x1 border color texture used when video output is
	// disabled. Returning early also keeps the current scaler.
	if (input.width < 16 || input.height < 16)
		return nil;
	// Upscaling only
	if (outputWidth < input.width || outputHeight < input.height
			|| (outputWidth == input.width && outputHeight == input.height))
		return nil;
#ifdef HAVE_METALFX
	if (@available(macOS 13.0, iOS 16.0, tvOS 16.0, *))
	{
		if (input.pixelFormat != inputFormat || input.width != inputWidth || input.height != inputHeight
				|| outputWidth != this->outputWidth || outputHeight != this->outputHeight)
			createScaler(input, outputWidth, outputHeight);
		if (failed)
			return nil;

		id<MTLFXSpatialScaler> fxScaler = (id<MTLFXSpatialScaler>)scaler;
		fxScaler.colorTexture = input;
		fxScaler.inputContentWidth = inputWidth;
		fxScaler.inputContentHeight = inputHeight;
		fxScaler.outputTexture = output;
		[fxScaler encodeToCommandBuffer:commandBuffer];
		// Don't keep the frame alive until the next upscale
		fxScaler.colorTexture = nil;

		return output;
	}
#endif
	return nil;
}
