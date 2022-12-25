/*
	Copyright 2022 flyinghead

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
#include "types.h"
#include "rend/TexCache.h"

extern const u32 VQMipPoint[11];
extern const u32 OtherMipPoint[11];

enum PvrDataFormat {
	PvrSquareTwiddled           = 0x01,
	PvrSquareTwiddledMipmaps    = 0x02,
    PvrVQ                       = 0x03,
    PvrVQMipmaps                = 0x04,
    PvrPal4                     = 0x05,
    PvrPal4Mipmaps              = 0x06,
    PvrPal8                     = 0x07,
    PvrPal8Mipmaps              = 0x08,
    PvrRectangle                = 0x09,
    PvrStride                   = 0x0B,
    PvrRectangleTwiddled        = 0x0D,
    PvrSmallVQ                  = 0x10,
    PvrSmallVQMipmaps           = 0x11,
    PvrSquareTwiddledMipmapsAlt = 0x12,
};

static bool pvrParse(const u8 *data, u32 len, u32& width, u32& height, std::vector<u8>& out)
{
	if (len < 16)
		return false;
	const u8 *p = data;
	const u8 *end = data + len;
	if (!memcmp("GBIX", p, 4))
	{
		p += 4;
		u32 idxSize = *(u32 *)p;
		p += 4 + idxSize;
	}
	if (memcmp("PVRT", p, 4))
	{
		WARN_LOG(COMMON, "Invalid PVR file: header not found");
		return false;
	}
	if (end - p < 16) {
		WARN_LOG(COMMON, "Invalid PVR file: too small");
		return false;
	}
	p += 4;
	u32 size = *(u32 *)p;
	p += 4;
	PixelFormat pixelFormat = (PixelFormat)*p++;
	PvrDataFormat imgType = (PvrDataFormat)*p++;
	p += 2;
	width = *(u16 *)p;
	p += 2;
	height = *(u16 *)p;
	p += 2;
	if (width > 1024 || height > 1024) {
		WARN_LOG(COMMON, "Invalid PVR file: wrong texture dimensions: %d x %d", width, height);
		return false;
	}

	::vq_codebook = p;
	TexConvFP32 texConv;
	switch (pixelFormat)
	{
	case Pixel1555:
		if (imgType == PvrSquareTwiddled || imgType == PvrSquareTwiddledMipmaps
				|| imgType == PvrRectangleTwiddled || imgType == PvrSquareTwiddledMipmapsAlt)
			texConv = opengl::tex1555_TW32;
		else if (imgType == PvrVQ || imgType == PvrVQMipmaps)
			texConv = opengl::tex1555_VQ32;
		else if (imgType == PvrRectangle || imgType == PvrStride)
			texConv = opengl::tex1555_PL32;
		else
		{
			WARN_LOG(COMMON, "Unsupported 1555 image type: %d", imgType);
			return false;
		}
		break;

	case Pixel565:
		if (imgType == PvrSquareTwiddled || imgType == PvrSquareTwiddledMipmaps
				|| imgType == PvrRectangleTwiddled || imgType == PvrSquareTwiddledMipmapsAlt)
			texConv = opengl::tex565_TW32;
		else if (imgType == PvrVQ || imgType == PvrVQMipmaps)
			texConv = opengl::tex565_VQ32;
		else if (imgType == PvrRectangle || imgType == PvrStride)
			texConv = opengl::tex565_PL32;
		else
		{
			WARN_LOG(COMMON, "Unsupported 565 image type: %d", imgType);
			return false;
		}
		break;

	case Pixel4444:
		if (imgType == PvrSquareTwiddled || imgType == PvrSquareTwiddledMipmaps
				|| imgType == PvrRectangleTwiddled || imgType == PvrSquareTwiddledMipmapsAlt)
			texConv = opengl::tex4444_TW32;
		else if (imgType == PvrVQ || imgType == PvrVQMipmaps)
			texConv = opengl::tex4444_VQ32;
		else if (imgType == PvrRectangle || imgType == PvrStride)
			texConv = opengl::tex4444_PL32;
		else
		{
			WARN_LOG(COMMON, "Unsupported 4444 image type: %d", imgType);
			return false;
		}
		break;

	default:
		WARN_LOG(COMMON, "Unsupported PVR pixel type: %d", pixelFormat);
		return false;
	}
	DEBUG_LOG(COMMON, "PVR file: size %d pixelFmt %d imgType %d w %d h %d", size, pixelFormat, imgType, width, height);
	u32 texU = 3;
	while (1u << texU < width)
		texU++;
	if (imgType == PvrSquareTwiddledMipmapsAlt)
		// Hardcoding pixel size. Not correct for palette texs
		p += OtherMipPoint[texU] * 2;
	else if (imgType == PvrSquareTwiddledMipmaps)
		// Hardcoding pixel size. Not correct for palette texs
		p += (OtherMipPoint[texU] - 2) * 2;
	else if (imgType == PvrVQMipmaps)
		p += VQMipPoint[texU];

	u32 expectedSize = width * height;
	if (imgType == PvrVQ || imgType == PvrVQMipmaps) {
		expectedSize /= 4;				// 4 pixels per byte
		expectedSize += 256 * 4 * 2;	// VQ codebook size
	}
	else
		expectedSize *= 2;	// 2 bytes per pixel
	if (end - p < expectedSize)
	{
		WARN_LOG(COMMON, "Invalid texture: expected %d bytes, %d remaining", expectedSize, (int)(end - p));
		return false;
	}

	PixelBuffer<u32> pb;
	pb.init(width, height);
	texConv(&pb, p, width, height);
	out.resize(width * height * 4);
	memcpy(out.data(), pb.data(), out.size());

	return true;
}
