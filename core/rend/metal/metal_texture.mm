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

void MetalTexture::UploadToGPU(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded)
{
    MTLPixelFormat format = MTLPixelFormatInvalid;

    u32 bpp = 2;
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
            bpp = 4;
            format = MTLPixelFormatRGBA8Unorm;
            break;
        case TextureType::_8:
            bpp = 1;
            format = MTLPixelFormatR8Unorm;
            break;
    }

    int mipmapLevels = 1;
    if (mipmapsIncluded)
    {
        mipmapLevels = 0;
        int dim = width;
        while (dim != 0)
        {
            mipmapLevels++;
            dim >>= 1;
        }
    }

    MTLTextureDescriptor *desc = [[MTLTextureDescriptor alloc] init];

    [desc setWidth:width];
    [desc setHeight:height];
    [desc setPixelFormat:format];
    [desc setMipmapLevelCount:mipmapLevels];
    [desc setStorageMode:MTLStorageModeShared];
    [desc setUsage:MTLTextureUsageShaderRead];

    auto device = MetalContext::Instance()->GetDevice();

    texture = [device newTextureWithDescriptor:desc];

    MTLRegion region = { 0, 0, static_cast<NSUInteger>(width), static_cast<NSUInteger>(height) };
    [texture replaceRegion:region mipmapLevel:0 withBytes:temp_tex_buffer bytesPerRow:bpp * width];
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
