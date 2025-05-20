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
#pragma once
#include "rend/TexCache.h"
#include "metal_context.h"
#include <unordered_map>
#include <Metal/Metal.h>

class MetalTexture final : public BaseTextureCacheData
{
public:
    MetalTexture(TSP tsp = {}, TCW tcw = {}) : BaseTextureCacheData(tsp, tcw) {}
    id<MTLTexture> texture;


    std::string GetId() override { return std::to_string([texture gpuResourceID]._impl); }
    void UploadToGPU(int width, int height, const u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded = false) override;
    bool Delete() override;
};

class MetalSamplers
{
public:
    explicit MetalSamplers();
    ~MetalSamplers();

    static const u32 TSP_Mask = 0x7ef00;

    void term() {
        for (auto &[u, samp] : samplers) {
            [samp release];
        }

        samplers.clear();
    }

    id<MTLSamplerState> GetSampler(const PolyParam& poly, bool punchThrough, bool texture1 = false) {
        TSP tsp = texture1 ? poly.tsp1 : poly.tsp;
        if (poly.texture != nullptr && poly.texture->gpuPalette)
            tsp.FilterMode = 0;
        else if (config::TextureFiltering == 1)
            tsp.FilterMode = 0;
        else if (config::TextureFiltering == 2)
            tsp.FilterMode = 1;
        return GetSampler(tsp, punchThrough);
    }

    id<MTLSamplerState> GetSampler(TSP tsp, bool punchThrough = false) {
        const u32 hash = (tsp.full & TSP_Mask) | punchThrough;	// MipMapD, FilterMode, ClampU, ClampV, FlipU, FlipV
        id<MTLSamplerState> sampler = samplers[hash];

        if (!sampler) {
            auto desc = [[MTLSamplerDescriptor alloc] init];

            if (tsp.FilterMode != 0) {
                if (punchThrough) {
                    [desc setMinFilter:MTLSamplerMinMagFilterLinear];
                    [desc setMagFilter:MTLSamplerMinMagFilterLinear];
                    [desc setMipFilter:MTLSamplerMipFilterNearest];
                } else {
                    [desc setMinFilter:MTLSamplerMinMagFilterLinear];
                    [desc setMagFilter:MTLSamplerMinMagFilterLinear];
                    [desc setMipFilter:MTLSamplerMipFilterLinear];
                }
            }
            else {
                [desc setMinFilter:MTLSamplerMinMagFilterNearest];
                [desc setMagFilter:MTLSamplerMinMagFilterNearest];
                [desc setMipFilter:MTLSamplerMipFilterNearest];
            }

            auto sRepeat = tsp.ClampU ? MTLSamplerAddressModeClampToEdge : tsp.FlipU ? MTLSamplerAddressModeMirrorRepeat : MTLSamplerAddressModeRepeat;
            auto tRepeat = tsp.ClampV ? MTLSamplerAddressModeClampToEdge : tsp.FlipV ? MTLSamplerAddressModeMirrorRepeat : MTLSamplerAddressModeRepeat;

            [desc setSAddressMode:sRepeat];
            [desc setTAddressMode:tRepeat];
            [desc setRAddressMode:tRepeat];
            [desc setCompareFunction:MTLCompareFunctionNever];
            [desc setMaxAnisotropy:config::AnisotropicFiltering];

            sampler = [MetalContext::Instance()->GetDevice() newSamplerStateWithDescriptor:desc];

            [desc release];

            samplers.emplace(hash, sampler).first->second;
        }

        return sampler;
    }

private:
    std::unordered_map<u32, id<MTLSamplerState>> samplers;
};

class MetalTextureCache final : public BaseTextureCache<MetalTexture>
{

};