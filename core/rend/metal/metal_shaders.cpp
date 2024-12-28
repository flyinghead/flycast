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
    float4 vtx_base INTERPOLATION;
    float4 vtx_offs INTERPOLATION;
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
