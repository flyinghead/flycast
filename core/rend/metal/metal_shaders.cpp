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

#include "metal_shaders.h"

#include "metal_context.h"

static const char VertexShaderSource[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant bool pp_gouraud [[function_constant(0)]];
constant bool div_pos_z [[function_constant(1)]];

struct VertexShaderUniforms
{
    float4x4 ndc_mat;
};

struct VertexIn
{
    float4 in_pos [[attribute(0)]];
    float4 in_base [[attribute(1)]];
    float4 in_offs [[attribute(2)]];
    float2 in_uv [[attribute(3)]];
};

struct VertexOut
{
    // TODO: Interpolation mode
    float4 vtx_base;
    float4 vtx_offs;
    float3 vtx_uv;
    float4 position [[position]];
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant VertexShaderUniforms& uniforms [[buffer(0)]])
{
    float4 vpos = uniforms.ndc_mat * in.in_pos;

    if (div_pos_z) {
        vpos /= vpos.z;
        vpos.z = vpos.w;
    }

    VertexOut out = {};
    out.vtx_base = in.in_base;
    out.vtx_offs = in.in_offs;
    out.vtx_uv = float3(in.in_uv, vpos.z);

    if (pp_gouraud && !div_pos_z) {
        out.vtx_base *= vpos.z;
        out.vtx_offs *= vpos.z;
    }

    if (!div_pos_z) {
        out.vtx_uv.xy *= vpos.z;
        vpos.w = 1.0;
        vpos.z = 0.0;
    }

    out.position = vpos;

    return out;
}
)";

static const char FragmentShaderSource[] = R"(
#include <metal_stdlib>
#define PI 3.1415926

using namespace metal;

constant bool cp_alpha_test [[function_constant(0)]];
constant bool pp_clip_inside [[function_constant(1)]];
constant bool pp_use_alpha [[function_constant(2)]];
constant bool pp_texture [[function_constant(3)]];
constant bool pp_ignore_tex_a [[function_constant(4)]];
constant int pp_shad_instr [[function_constant(5)]];
constant bool pp_offset [[function_constant(6)]];
constant int pp_fog_ctrl [[function_constant(7)]];
constant bool pp_gouraud [[function_constant(8)]];
constant bool pp_bump_map [[function_constant(9)]];
constant bool color_clamping [[function_constant(10)]];
constant bool pp_trilinear [[function_constant(11)]];
constant int pp_palette [[function_constant(12)]];
constant bool div_pos_z [[function_constant(13)]];
constant bool dithering [[function_constant(14)]];

constant bool has_fog_table = pp_fog_ctrl != 2;
constant bool has_palette = pp_palette != 0;

struct FragmentShaderUniforms
{
    float4 color_clamp_min;
    float4 color_clamp_max;
    float4 sp_fog_col_ram;
    float4 sp_fog_col_vert;
    float4 dither_color_max;
    float cp_alpha_test_value;
    float sp_fog_density;
};

struct PushBlock
{
    float4 clip_test;
    float trilinear_alpha;
    float palette_index;
};

struct VertexOut
{
    // TODO: Interpolation mode
    float4 vtx_base;
    float4 vtx_offs;
    float3 vtx_uv;
    float4 position [[position]];
};

struct FragmentOut
{
    float4 color [[color(0)]];
    float depth [[depth(any)]];
};

float fog_mode2(float w, constant FragmentShaderUniforms& uniforms,
                texture2d<float> fog_table, sampler fog_table_sampler)
{
    float z = 0.0;

    if (div_pos_z) {
        z = clamp(uniforms.sp_fog_density / w, 1.0, 255.9999);
    } else {
        z = clamp(uniforms.sp_fog_density * w, 1.0, 255.9999);
    }

    float exp = floor(log2(z));
    float m = z * 16.0 / pow(2.0, exp) - 16.0;
    float idx = floor(m) + exp * 16.0 + 0.5;
    float4 fog_coef = fog_table.sample(fog_table_sampler, float2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
    return fog_coef.r;
}

float4 color_clamp(float4 col, constant FragmentShaderUniforms& uniforms)
{
    if (color_clamping)
    {
        return clamp(col, uniforms.color_clamp_min, uniforms.color_clamp_max);
    } else {
        return col;
    }
}

float4 get_palette_entry(texture2d<float> palette, sampler palette_sampler,
                         float col_idx, constant PushBlock& push_constants)
{
    float2 c = float2(col_idx * 255.0 / 1023.0 + push_constants.palette_index, 0.5);
    return palette.sample(palette_sampler, c);
}

float4 palette_pixel(texture2d<float> texture, sampler texture_sampler,
                     texture2d<float> palette, sampler palette_sampler,
                     float3 coords, constant PushBlock& push_constants)
{
    if (div_pos_z) {
        return get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, coords.xy).r, push_constants);
    } else {
        return get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, float2(coords.xy / coords.z)).r, push_constants);
    }
}

float4 palette_pixel_bilinear(texture2d<float> texture, sampler texture_sampler,
                              texture2d<float> palette, sampler palette_sampler,
                              float3 coords, constant PushBlock& push_constants)
{
    if (div_pos_z) {
        coords.xy /= coords.z;
    }

    float2 tex_size = float2(texture.get_width(), texture.get_height());
    float2 pix_coord = coords.xy * tex_size - 0.5; // Coordinates of top left pixel
    float2 origin_pix_coords = floor(pix_coord);

    float2 sample_uv = (origin_pix_coords + 0.5) / tex_size; // UV coordinates of center of top left pixel

    // Sample from all surrounding texels
    float4 c00 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv).r, push_constants);
    float4 c01 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv, int2(0, 1)).r, push_constants);
    float4 c11 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv, int2(1, 1)).r, push_constants);
    float4 c10 = get_palette_entry(palette, palette_sampler, texture.sample(texture_sampler, sample_uv, int2(1, 0)).r, push_constants);

    float2 weight = pix_coord - origin_pix_coords;

    // Bi-linear mixing
    float4 temp0 = mix(c00, c10, weight.x);
    float4 temp1 = mix(c01, c11, weight.x);
    return mix(temp0, temp1, weight.y);
}

fragment FragmentOut fs_main(VertexOut in [[stage_in]], constant FragmentShaderUniforms& uniforms [[buffer(0)]],
                             constant PushBlock& push_constants [[buffer(1)]],
                             texture2d<float> tex [[texture(0), function_constant(pp_texture)]], sampler tex_sampler [[sampler(0), function_constant(pp_texture)]],
                             texture2d<float> fog_table [[texture(2), function_constant(has_fog_table)]], sampler fog_table_sampler [[sampler(2), function_constant(has_fog_table)]],
                             texture2d<float> palette [[texture(3), function_constant(has_palette)]], sampler palette_sampler [[sampler(3), function_constant(has_palette)]])
{
    // Clip inside the box
    if (pp_clip_inside) {
        if (in.position.x >= push_constants.clip_test.x && in.position.x <= push_constants.clip_test.z
            && in.position.y >= push_constants.clip_test.y && in.position.y <= push_constants.clip_test.w)
            discard_fragment();
    }

    float4 color = in.vtx_base;
    float4 offset = in.vtx_offs;

    if (pp_gouraud && !div_pos_z) {
        color /= in.vtx_uv.z;
        offset /= in.vtx_uv.z;
    }

    if (!pp_use_alpha) {
        color.a = 1.0;
    }

    if (pp_fog_ctrl == 3) {
        color = float4(uniforms.sp_fog_col_ram.rgb, fog_mode2(in.vtx_uv.z, uniforms, fog_table, fog_table_sampler));
    }

    if (pp_texture) {
        float4 tex_col;

        if (pp_palette == 0) {
            if (div_pos_z) {
                tex_col = tex.sample(tex_sampler, in.vtx_uv.xy);
            } else {
                tex_col = tex.sample(tex_sampler, float2(in.vtx_uv.xy / in.vtx_uv.z));
            }
        } else {
            if (pp_palette == 1) {
                tex_col = palette_pixel(tex, tex_sampler, palette, palette_sampler, in.vtx_uv, push_constants);
            } else {
                tex_col = palette_pixel_bilinear(tex, tex_sampler, palette, palette_sampler, in.vtx_uv, push_constants);
            }
        }

        if (pp_bump_map) {
            float s = PI / 2.0 * (tex_col.a * 15.0 * 16.0 + tex_col.r * 15.0) / 255.0;
            float r = 2.0 * PI * (tex_col.g * 15.0 * 16.0 + tex_col.b * 15.0) / 255.0;
            tex_col.a = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0 * PI * offset.b), 0.0, 1.0);
            tex_col.rgb = float3(1.0, 1.0, 1.0);
        } else {
            if (pp_ignore_tex_a)
                tex_col.a = 1.0;
        }

        if (pp_shad_instr == 0) {
            color = tex_col;
        } else if (pp_shad_instr == 1) {
            color.rgb *= tex_col.rgb;
            color.a = tex_col.a;
        } else if (pp_shad_instr == 2) {
            color.rgb = mix(color.rgb, tex_col.rgb, tex_col.a);
        } else if (pp_shad_instr == 3) {
            color *= tex_col;
        }

        if (pp_offset && !pp_bump_map) {
            color.rgb += offset.rgb;
        }
    }

    color = color_clamp(color, uniforms);

    if (pp_fog_ctrl == 0) {
        color.rgb = mix(color.rgb, uniforms.sp_fog_col_ram.rgb, fog_mode2(in.vtx_uv.z, uniforms, fog_table, fog_table_sampler));
    }

    if (pp_fog_ctrl == 1 && pp_offset && !pp_bump_map) {
        color.rgb = mix(color.rgb, uniforms.sp_fog_col_vert.rgb, offset.a);
    }

    if (pp_trilinear)
        color *= push_constants.trilinear_alpha;

    if (cp_alpha_test) {
        color.a = round(color.a * 255.0) / 255.0;
        if (uniforms.cp_alpha_test_value > color.a)
            discard_fragment();
        color.a = 1.0;
    }

    float w;

    if (div_pos_z) {
        w = 100000.0 / in.vtx_uv.z;
    } else {
        w = 100000.0 * in.vtx_uv.z;
    }

    float depth = log2(1.0 + max(w, -0.999999)) / 34.0;

    if (dithering) {
        constexpr float dither_table[16] = {
		    0.9375,  0.1875,  0.75,  0.0,
		    0.4375,  0.6875,  0.25,  0.5,
		    0.8125,  0.0625,  0.875, 0.125,
		    0.3125,  0.5625,  0.375, 0.625
        };

        float r = dither_table[int(fmod(in.position.y, 4.0)) * 4 + int(fmod(in.position.x, 4.0))];
        // 31 for 5-bit color, 63 for 6-bits, 15 for 4 bits
        color += r / uniforms.dither_color_max;
        // Avoid rounding
        color = floor(color * 255.0) / 255.0;
    }

    return FragmentOut { color, depth };
}
)";

static const char BlitShader[] = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 texCoord;
};

vertex VertexOut vs_main(uint vertexID [[vertex_id]]) {
    // Predefined positions and texture coordinates for a full-screen quad
    float4 positions[4] = {
        float4(-1.0, -1.0, 0.0, 1.0), // Bottom-left
        float4( 1.0, -1.0, 0.0, 1.0), // Bottom-right
        float4(-1.0,  1.0, 0.0, 1.0), // Top-left
        float4( 1.0,  1.0, 0.0, 1.0)  // Top-right
    };

    float2 texCoords[4] = {
        float2(0.0, 1.0), // Bottom-left
        float2(1.0, 1.0), // Bottom-right
        float2(0.0, 0.0), // Top-left
        float2(1.0, 0.0)  // Top-right
    };

    VertexOut out;
    out.position = positions[vertexID];
    out.texCoord = texCoords[vertexID];
    return out;
}

fragment float4 fs_main(VertexOut in [[stage_in]],
                              texture2d<float> sourceTexture [[texture(0)]]) {
    constexpr sampler textureSampler(mag_filter::linear, min_filter::linear);
    return sourceTexture.sample(textureSampler, in.texCoord);
}
)";

static const char ModVolShaderSource[] = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

constant bool div_pos_z [[function_constant(1)]];

struct VertexShaderUniforms
{
    float4x4 ndc_mat;
};

struct VertexIn
{
    float4 in_pos [[attribute(0)]];
};

struct VertexOut
{
    float depth;
    float4 position [[position]];
};

struct FragmentOut
{
    float4 color [[color(0)]];
    float depth [[depth(any)]];
};

struct PushBlock
{
    float sp_shader_color;
};

vertex VertexOut vs_main(VertexIn in [[stage_in]], constant VertexShaderUniforms& uniforms [[buffer(0)]]) {
    float4 vpos = uniforms.ndc_mat * in.in_pos;

    VertexOut out = {};

    if (div_pos_z) {
        vpos /= vpos.z;
        vpos.z = vpos.w;
        out.depth = vpos.w;
    } else {
        out.depth = vpos.z;
        vpos.w = 1.0;
        vpos.z = 0.0;
    }

    out.position = vpos;
    return out;
}

fragment FragmentOut fs_main(VertexOut in [[stage_in]],
                             constant PushBlock& push_constants [[buffer(1)]]) {
    FragmentOut out = {};

    float w;

    if (div_pos_z) {
        w = 100000.0 / in.depth;
    } else {
        w = 100000.0 * in.depth;
    }

    out.depth = log2(1.0 + max(w, -0.999999)) / 34.0;
    out.color = float4(0.0, 0.0, 0.0, push_constants.sp_shader_color);
    return out;
}
)";

// TODO: Handle gouraud interpolation
// TODO: N2 Shaders

MetalShaders::MetalShaders() {
    auto device = MetalContext::Instance()->GetDevice();

    NS::Error *error = nullptr;
    fragmentShaderLibrary = device->newLibrary(NS::String::string(FragmentShaderSource, NS::UTF8StringEncoding), nullptr, &error);
    fragmentShaderConstants = MTL::FunctionConstantValues::alloc()->init();

    if (!fragmentShaderLibrary) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    vertexShaderLibrary = device->newLibrary(NS::String::string(VertexShaderSource, NS::UTF8StringEncoding), nullptr, &error);
    vertexShaderConstants = MTL::FunctionConstantValues::alloc()->init();

    if (!vertexShaderLibrary) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    blitShaderLibrary = device->newLibrary(NS::String::string(BlitShader, NS::UTF8StringEncoding), nullptr, &error);

    if (!blitShaderLibrary) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    blitVertexShader = blitShaderLibrary->newFunction(NS::String::string("vs_main", NS::UTF8StringEncoding), nullptr, &error);

    if (!blitVertexShader) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    blitFragmentShader = blitShaderLibrary->newFunction(NS::String::string("fs_main", NS::UTF8StringEncoding), nullptr, &error);

    if (!blitFragmentShader) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    modVolShaderLibrary = device->newLibrary(NS::String::string(ModVolShaderSource, NS::UTF8StringEncoding), nullptr, &error);
    modVolShaderConstants = MTL::FunctionConstantValues::alloc()->init();
}

MTL::Function *MetalShaders::compileShader(const VertexShaderParams &params) {
    vertexShaderConstants->setConstantValue(&params.gouraud, MTL::DataTypeBool, static_cast<NS::UInteger>(0));
    vertexShaderConstants->setConstantValue(&params.divPosZ, MTL::DataTypeBool, 1);

    NS::Error *error = nullptr;

    MTL::Function *function = vertexShaderLibrary->newFunction(NS::String::string("vs_main", NS::UTF8StringEncoding), vertexShaderConstants, &error);

    if (!function) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    return function;
}

MTL::Function *MetalShaders::compileShader(const FragmentShaderParams &params) {
    fragmentShaderConstants->setConstantValue(&params.alphaTest, MTL::DataTypeBool, static_cast<NS::UInteger>(0));
    fragmentShaderConstants->setConstantValue(&params.insideClipTest, MTL::DataTypeBool, 1);
    fragmentShaderConstants->setConstantValue(&params.useAlpha, MTL::DataTypeBool, 2);
    fragmentShaderConstants->setConstantValue(&params.texture, MTL::DataTypeBool, 3);
    fragmentShaderConstants->setConstantValue(&params.ignoreTexAlpha, MTL::DataTypeBool, 4);
    fragmentShaderConstants->setConstantValue(&params.shaderInstr, MTL::DataTypeInt, 5);
    fragmentShaderConstants->setConstantValue(&params.offset, MTL::DataTypeBool, 6);
    fragmentShaderConstants->setConstantValue(&params.fog, MTL::DataTypeInt, 7);
    fragmentShaderConstants->setConstantValue(&params.gouraud, MTL::DataTypeBool, 8);
    fragmentShaderConstants->setConstantValue(&params.bumpmap, MTL::DataTypeBool, 9);
    fragmentShaderConstants->setConstantValue(&params.clamping, MTL::DataTypeBool, 10);
    fragmentShaderConstants->setConstantValue(&params.trilinear, MTL::DataTypeBool, 11);
    fragmentShaderConstants->setConstantValue(&params.palette, MTL::DataTypeInt, 12);
    fragmentShaderConstants->setConstantValue(&params.divPosZ, MTL::DataTypeBool, 13);
    fragmentShaderConstants->setConstantValue(&params.dithering, MTL::DataTypeBool, 14);

    NS::Error *error = nullptr;

    MTL::Function *function = fragmentShaderLibrary->newFunction(NS::String::string("fs_main", NS::UTF8StringEncoding), fragmentShaderConstants, &error);

    if (!function) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    return function;
}

MTL::Function *MetalShaders::compileShader(const ModVolShaderParams &params) {
    modVolShaderConstants->setConstantValue(&params.divPosZ, MTL::DataTypeBool, static_cast<NS::UInteger>(0));

    NS::Error *error = nullptr;

    // TODO: Naomi2 ModVol Frag Shader
    MTL::Function *function = modVolShaderLibrary->newFunction(NS::String::string("vs_main", NS::UTF8StringEncoding), modVolShaderConstants, &error);

    if (!function) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    return function;
}

MTL::Function *MetalShaders::compileShader(bool divPosZ) {
    modVolShaderConstants->setConstantValue(&divPosZ, MTL::DataTypeBool, static_cast<NS::UInteger>(0));

    NS::Error *error = nullptr;

    MTL::Function *function = modVolShaderLibrary->newFunction(NS::String::string("fs_main", NS::UTF8StringEncoding), modVolShaderConstants, &error);

    if (!function) {
        ERROR_LOG(RENDERER, "%s", error->localizedDescription()->utf8String());
        assert(false);
    }

    return function;
}

