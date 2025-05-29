/*
    Copyright 2024 flyinghead

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

#include "metal_texture.h"

void MetalTexture::UploadToGPU(int width, int height, const u8 *data, bool mipmapped, bool mipmapsIncluded)
{
    MTLPixelFormat format = MTLPixelFormatInvalid;
    u32 dataSize = width * height * 2;
    switch (tex_type)
    {
    case TextureType::_5551:
        format = MTLPixelFormatA1BGR5Unorm;
        break;
    case TextureType::_565:
        format = MTLPixelFormatB5G6R5Unorm;
        break;
    case TextureType::_4444:
        format = MTLPixelFormatABGR4Unorm;
        break;
    case TextureType::_8888:
        format = MTLPixelFormatRGBA8Unorm;
        dataSize *= 2;
        break;
    case TextureType::_8:
        format = MTLPixelFormatR8Unorm;
        dataSize /= 2;
        break;
    }
    if (mipmapsIncluded)
    {
        int w = width / 2;
        u32 size = dataSize / 4;
        while (w)
        {
            dataSize += ((size + 3) >> 2) << 2;		// offset must be a multiple of 4
            size /= 4;
            w /= 2;
        }
    }

    if (width != this->width || height != this->height
            || format != this->format || this->texture == nil)
        Init(width, height, format, dataSize, mipmapped, mipmapsIncluded);

    SetImage(dataSize, data, mipmapped && !mipmapsIncluded);
}

void MetalTexture::Init(u32 width, u32 height, MTLPixelFormat format, u32 dataSize, bool mipmapped, bool mipmapsIncluded)
{
    this->width = width;
    this->height = height;
    this->format = format;
    mipmapLevels = 1;
    if (mipmapped)
        mipmapLevels += floor(log2(std::max(width, height)));

    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];

    [desc setWidth:width];
    [desc setHeight:height];
    [desc setPixelFormat:format];
    [desc setMipmapLevelCount:mipmapLevels];
    [desc setStorageMode:MTLStorageModeShared];
    [desc setUsage:MTLTextureUsageShaderRead];

    auto device = MetalContext::Instance()->GetDevice();

    texture = [device newTextureWithDescriptor:desc];
}

void MetalTexture::SetImage(u32 srcSize, const void *srcData, bool genMipmaps) {
    u32 bpp;
    switch (tex_type) {
    case TextureType::_8888:
        bpp = 4;
        break;
    case TextureType::_8:
        bpp = 1;
        break;
    default:
        bpp = 2;
        break;
    }

    if (mipmapLevels > 1 && !genMipmaps && tex_type != TextureType::_8888)
    {
        u8 *src = (u8 *)srcData;
        u32 dataOffset = 0;

        for (u32 i = 0; i < mipmapLevels; i++) {
            const u32 size = (1 << (2 * i)) * bpp;

            u32 mipLevel = mipmapLevels - i - 1;
            u32 mipWidth = std::max(texture.width >> mipLevel, 1ul);
            u32 mipHeight = std::max(texture.height >> mipLevel, 1ul);

            MTLRegion region = MTLRegionMake2D(0, 0, mipWidth, mipHeight);
            [texture replaceRegion:region
                       mipmapLevel:mipLevel
                         withBytes:src + dataOffset
                       bytesPerRow:mipWidth * bpp];

            dataOffset += ((size + 3) >> 2) << 2;
        }
    }
    else
    {
        u32 rowBytes = texture.width * bpp;

        MTLRegion region = MTLRegionMake2D(0, 0, texture.width, texture.height);
        [texture replaceRegion:region
                   mipmapLevel:0
                     withBytes:srcData
                   bytesPerRow:rowBytes];

        if (mipmapLevels > 1 && genMipmaps) {
            GenerateMipmaps();
        }
    }
}

void MetalTexture::GenerateMipmaps()
{
    verify((bool)commandBuffer);
    [commandBuffer setLabel:@"Mipmap Generation"];

    id<MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];

    u32 mipWidth = this->width;
    u32 mipHeight = this->height;

    for (u32 i = 1; i < mipmapLevels; i++) {
        u32 srcWidth = mipWidth;
        u32 srcHeight = mipHeight;

        mipWidth = std::max(mipWidth / 2, 1u);
        mipHeight = std::max(mipHeight / 2, 1u);

        MTLOrigin srcOrigin = MTLOriginMake(0, 0, 0);
        MTLSize srcSize = MTLSizeMake(srcWidth, srcHeight, 1);

        MTLOrigin dstOrigin = MTLOriginMake(0, 0, 0);
        MTLSize dstSize = MTLSizeMake(mipWidth, mipHeight, 1);

        [blitEncoder copyFromTexture:texture
                         sourceSlice:0
                         sourceLevel:i - 1
                        sourceOrigin:srcOrigin
                          sourceSize:srcSize
                           toTexture:texture
                    destinationSlice:0
                    destinationLevel:i
                   destinationOrigin:dstOrigin];
    }

    [blitEncoder endEncoding];
}

bool MetalTexture::Delete()
{
    [texture setPurgeableState:MTLPurgeableStateEmpty];
    texture = nil;

    return true;
}

MetalSamplers::MetalSamplers() = default;
MetalSamplers::~MetalSamplers() {
    term();
}
