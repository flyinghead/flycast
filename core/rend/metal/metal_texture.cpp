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
    MTL::PixelFormat format = MTL::PixelFormatInvalid;

    u32 bpp = 2;
    switch (tex_type)
    {
        case TextureType::_5551:
            format = MTL::PixelFormatA1BGR5Unorm;
            break;
        case TextureType::_565:
            format = MTL::PixelFormatB5G6R5Unorm;
            break;
        case TextureType::_4444:
            format = MTL::PixelFormatABGR4Unorm;
            break;
        case TextureType::_8888:
            bpp = 4;
            format = MTL::PixelFormatRGBA8Unorm;
            break;
        case TextureType::_8:
            bpp = 1;
            format = MTL::PixelFormatR8Unorm;
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

    MTL::TextureDescriptor *desc = MTL::TextureDescriptor::alloc()->init();

    desc->setWidth(width);
    desc->setHeight(height);
    desc->setPixelFormat(format);
    desc->setMipmapLevelCount(mipmapLevels);
    desc->setStorageMode(MTL::StorageModeShared);
    desc->setUsage(MTL::TextureUsageUnknown);

    auto device = MetalContext::Instance()->GetDevice();

    texture = device->newTexture(desc);
    desc->release();

    MTL::Region region = { 0, 0, static_cast<NS::UInteger>(width), static_cast<NS::UInteger>(height) };
    texture->replaceRegion(region, 0, temp_tex_buffer, bpp * width);
}

bool MetalTexture::Delete()
{
    texture->setPurgeableState(MTL::PurgeableStateEmpty);
    texture->release();
    texture = nullptr;

    return true;
}

MetalSamplers::MetalSamplers() = default;
MetalSamplers::~MetalSamplers() {
    term();
}
