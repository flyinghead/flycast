// PowerVR2/Naomi2 semantics ported from the GLES and Vulkan shaders.
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstddef>
#include <glm/glm.hpp>

struct alignas(16) MetalFrameUniforms
{
	glm::mat4 ndc;
	glm::vec4 clampMin, clampMax, fogRam, fogVertex, dither;
	float alphaRef, fogDensity, shadeScale;
	u32 width, height, capacity, maxLayers, dithering;
	glm::uvec4 oitTile{0}; // zero extent selects a full-frame resolve
};

struct alignas(16) MetalPolyUniforms
{
	glm::vec4 clip{0};
	u32 tsp0, tsp1, flags, palette, sequence, polyIndex, modMode;
	float trilinear, lodBias, lodBias1, depthBound;
};

// Immutable for every draw of one Naomi2 polygon in the current render.
// Keep these vertex inputs separate from pass-dependent polygon state.
struct alignas(16) MetalNaomi2Uniforms
{
	glm::mat4 mv{0}, normal{0}, projection{0};
	glm::ivec2 env{0}, constantColor{0};
	glm::vec2 gloss{0};
};
static_assert(sizeof(MetalFrameUniforms) == 192, "Metal frame uniform ABI");
static_assert(sizeof(MetalPolyUniforms) == 64, "Metal polygon uniform ABI");
static_assert(sizeof(MetalNaomi2Uniforms) == 224, "Metal Naomi2 uniform ABI");
static_assert(offsetof(MetalFrameUniforms, clampMin) == 64 && offsetof(MetalFrameUniforms, dither) == 128
	&& offsetof(MetalFrameUniforms, alphaRef) == 144 && offsetof(MetalFrameUniforms, width) == 156
	&& offsetof(MetalFrameUniforms, dithering) == 172 && offsetof(MetalFrameUniforms, oitTile) == 176, "Metal frame field offsets");
static_assert(offsetof(MetalPolyUniforms, tsp0) == 16 && offsetof(MetalPolyUniforms, lodBias1) == 52
	&& offsetof(MetalPolyUniforms, depthBound) == 56,
	"Metal polygon field offsets");
static_assert(offsetof(MetalNaomi2Uniforms, normal) == 64 && offsetof(MetalNaomi2Uniforms, projection) == 128
	&& offsetof(MetalNaomi2Uniforms, env) == 192 && offsetof(MetalNaomi2Uniforms, constantColor) == 200
	&& offsetof(MetalNaomi2Uniforms, gloss) == 208, "Metal Naomi2 field offsets");

struct MetalOITNode
{
	u32 color0;
	float depth;
	u32 next, sequence, metadata;
};
struct MetalOITPoly
{
	// Low bits are unused by OIT blending: bit 0 shadow, bit 1 two volumes.
	u32 tsp0, tsp1;
};
static_assert(sizeof(MetalOITNode) == 20, "Metal OIT node ABI");
static_assert(offsetof(MetalOITNode, depth) == 4 && offsetof(MetalOITNode, next) == 8
	&& offsetof(MetalOITNode, sequence) == 12 && offsetof(MetalOITNode, metadata) == 16,
	"Metal OIT node field offsets");
// Keep the existing node capacity within PixelBufferSize, including optional
// area-1 colors. Linked traversal does not fetch that separate color stream.
static constexpr size_t MetalOITNodeBudgetStride = sizeof(MetalOITNode) + sizeof(u32);
static_assert(sizeof(MetalOITPoly) == 8, "Metal OIT polygon ABI");

// Vertex controls before interpolation-layout canonicalization: texture,
// gouraud, bump, native depth, Naomi2, two volumes, and light-off.
// metalVertexFunctionFlags additionally encodes the varying layout.
static constexpr u32 MetalVertexMaterialFlags = (1u << 0) | (1u << 2) | (1u << 3)
	| (1u << 4) | (1u << 5) | (1u << 6) | (1u << 14);

// Fragment shading and MaterialVarying use no Naomi2/light-off controls;
// those affect vertex calculations only. Bit 10 selects non-unit trilinear
// weighting in the fragment; it does not affect the varying layout.
static constexpr u32 MetalFragmentMaterialFlags = ((1u << 19) - 1)
	& ~((1u << 5) | (1u << 14));

static constexpr u32 metalVertexFunctionFlags(u32 flags, u32 tsp0, u32 tsp1)
{
	constexpr u32 mask = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 9);
	constexpr u32 value = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 9);
	const bool linear = (flags & mask) == value && ((tsp0 >> 22) & 3) == 0
		&& (!(flags & (1u << 6)) || ((tsp1 >> 22) & 3) == 0);
	flags &= MetalVertexMaterialFlags;
	if (!(flags & (1u << 5))) flags &= ~((1u << 3) | (1u << 14));
	else if ((flags & (1u << 14)) || !(flags & (1u << 0))) flags &= ~(1u << 3);
	// Encode the varying layout with one additional key state. The vertex
	// function can keep canonical zero TSPs and share all other shading keys.
	if (linear) flags |= (1u << 1) | (1u << 9);
	return flags;
}

// flags: texture, offset, gouraud, bump, nativeDepth, naomi2, twoVolumes,
// alphaTest, clipInside, fogEnabled, trilinearWeight, ignoreAlpha0, ignoreAlpha1.
static constexpr const char MetalShaderSource[] = R"MSL(
#include <metal_stdlib>
using namespace metal;
// Match the literal used by the Vulkan/GL PowerVR bump-map calculation.
// M_PI_F rounds to the adjacent float and can change the generated alpha.
constant float PVR_PI = 3.1415926f;

struct Frame {
    float4x4 ndc;
    float4 clampMin, clampMax, fogRam, fogVertex, dither;
    float alphaRef, fogDensity, shadeScale;
    uint width, height, capacity, maxLayers, dithering;
    uint4 oitTile;
};
struct Poly {
    float4 clip;
    uint tsp0, tsp1, flags, palette, sequence, polyIndex, modMode;
    float trilinear, lodBias, lodBias1, depthBound;
};
struct Naomi2 {
    float4x4 mv, normal, projection;
    int2 env, constantColor;
    float2 gloss;
};
struct Light {
    float4 color, direction, position;
    int parallel, routing, dmode, smode;
    int2 diffuse, specular;
    float attnDistA, attnDistB, attnAngleA, attnAngleB;
    int distAttnMode;
    int3 padding;
};
// Use scalars for padding: int3 has 16-byte alignment in MSL.
struct PackedLight {
    float4 color, direction, position;
    int parallel, routing, dmode, smode;
    int2 diffuse, specular;
    float attnDistA, attnDistB, attnAngleA, attnAngleB;
    int distAttnMode, pad0, pad1, pad2;
};
struct Lights {
    PackedLight lights[16];
    float4 ambientBase[2], ambientOffset[2];
    int2 ambientMaterialBase, ambientMaterialOffset;
    int lightCount, useBaseOver, bumpId0, bumpId1;
};
// GPU addresses in Metal 4 carry no host-side layout conversion. Keep both
// sides of the constant-buffer ABI checked when compiling their own language.
static_assert(sizeof(Frame) == 192, "Metal frame uniform ABI");
static_assert(sizeof(Poly) == 64, "Metal polygon uniform ABI");
static_assert(sizeof(Naomi2) == 224, "Metal Naomi2 uniform ABI");
static_assert(sizeof(PackedLight) == 112, "Metal Naomi2 light ABI");
static_assert(sizeof(Lights) == 1888, "Metal Naomi2 light-model ABI");
struct Input {
    float3 pos [[attribute(0)]];
    float4 base [[attribute(1)]], offset [[attribute(2)]];
    float2 uv [[attribute(3)]];
    float4 base1 [[attribute(4)]], offset1 [[attribute(5)]];
    float2 uv1 [[attribute(6)]];
    float3 normal [[attribute(7)]];
};
// Ordinary PVR polygons have no Naomi2 normal or lighting inputs.
// Keep both volumes' colors/UVs, but exclude the normal from stage input.
struct InputDC {
    float3 pos [[attribute(0)]];
    float4 base [[attribute(1)]], offset [[attribute(2)]];
    float2 uv [[attribute(3)]];
    float4 base1 [[attribute(4)]], offset1 [[attribute(5)]];
    float2 uv1 [[attribute(6)]];
};
Input expandDC(InputDC in) {
    Input out = {};
    out.pos = in.pos; out.base = in.base; out.offset = in.offset;
    out.uv = in.uv; out.base1 = in.base1; out.offset1 = in.offset1; out.uv1 = in.uv1;
    return out;
}
struct Varying {
    float4 position [[position]];
    float4 base, offset, base1, offset1;
    float4 flatBase [[flat]], flatOffset [[flat]], flatBase1 [[flat]], flatOffset1 [[flat]];
    float2 uv, uv1;
    float reciprocal;
    uint sequence [[flat]];
};
// Only the specialized entry points depend on these constants. Keep the
// generic entry points available while their material variants compile.
constant uint MaterialFlags [[function_constant(0)]];
constant uint MaterialTsp0 [[function_constant(1)]];
constant uint MaterialTsp1 [[function_constant(2)]];
constant bool MaterialSequence [[function_constant(3)]];
constant bool MaterialSmooth = (MaterialFlags & (1u << 2)) != 0;
constant bool MaterialFlat = !MaterialSmooth;
constant bool MaterialSmooth1 = MaterialSmooth && (MaterialFlags & (1u << 6)) != 0;
constant bool MaterialFlat1 = MaterialFlat && (MaterialFlags & (1u << 6)) != 0;
constant bool MaterialUV = (MaterialFlags & 1u) != 0;
constant bool MaterialUV1 = MaterialUV && (MaterialFlags & (1u << 6)) != 0;
// Linear interpolation is equivalent when the vertex path fixes W=1.
// Use it for the textured offset/table-fog materials with measured benefit;
// other materials retain their original perspective interpolation.
constant uint LinearMaterialMask = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 9);
constant uint LinearMaterialValue = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 9);
constant bool MaterialManual = (MaterialFlags & LinearMaterialMask) == LinearMaterialValue
    && ((MaterialTsp0 >> 22) & 3u) == 0
    && ((MaterialFlags & (1u << 6)) == 0 || ((MaterialTsp1 >> 22) & 3u) == 0);
constant bool MaterialPerspective = !MaterialManual;
constant bool MaterialSmoothPerspective = MaterialSmooth && MaterialPerspective;
constant bool MaterialSmoothLinear = MaterialSmooth && MaterialManual;
constant bool MaterialSmooth1Perspective = MaterialSmooth1 && MaterialPerspective;
constant bool MaterialSmooth1Linear = MaterialSmooth1 && MaterialManual;
constant bool MaterialUVPerspective = MaterialUV && MaterialPerspective;
constant bool MaterialUVLinear = MaterialUV && MaterialManual;
constant bool MaterialUV1Perspective = MaterialUV1 && MaterialPerspective;
constant bool MaterialUV1Linear = MaterialUV1 && MaterialManual;
struct MaterialVarying {
    float4 position [[position]];
    float4 base [[function_constant(MaterialSmoothPerspective)]];
    float4 linearBase [[center_no_perspective, function_constant(MaterialSmoothLinear)]];
    float4 offset [[function_constant(MaterialSmoothPerspective)]];
    float4 linearOffset [[center_no_perspective, function_constant(MaterialSmoothLinear)]];
    float4 flatBase [[flat, function_constant(MaterialFlat)]];
    float4 flatOffset [[flat, function_constant(MaterialFlat)]];
    float4 base1 [[function_constant(MaterialSmooth1Perspective)]];
    float4 linearBase1 [[center_no_perspective, function_constant(MaterialSmooth1Linear)]];
    float4 offset1 [[function_constant(MaterialSmooth1Perspective)]];
    float4 linearOffset1 [[center_no_perspective, function_constant(MaterialSmooth1Linear)]];
    float4 flatBase1 [[flat, function_constant(MaterialFlat1)]];
    float4 flatOffset1 [[flat, function_constant(MaterialFlat1)]];
    float2 uv [[function_constant(MaterialUVPerspective)]];
    float2 linearUv [[center_no_perspective, function_constant(MaterialUVLinear)]];
    float2 uv1 [[function_constant(MaterialUV1Perspective)]];
    float2 linearUv1 [[center_no_perspective, function_constant(MaterialUV1Linear)]];
    float reciprocal [[function_constant(MaterialPerspective)]];
    float linearReciprocal [[center_no_perspective, function_constant(MaterialManual)]];
    uint sequence [[flat, function_constant(MaterialSequence)]];
};
Varying expandMaterial(MaterialVarying in) {
    Varying out = {};
    out.position = in.position; if (MaterialPerspective) out.reciprocal = in.reciprocal;
    if (MaterialManual) out.reciprocal = in.linearReciprocal;
    if (MaterialSmoothPerspective) { out.base = in.base; out.offset = in.offset; }
    if (MaterialSmoothLinear) { out.base = in.linearBase; out.offset = in.linearOffset; }
    if (MaterialFlat) { out.flatBase = in.flatBase; out.flatOffset = in.flatOffset; }
    if (MaterialSmooth1Perspective) { out.base1 = in.base1; out.offset1 = in.offset1; }
    if (MaterialSmooth1Linear) { out.base1 = in.linearBase1; out.offset1 = in.linearOffset1; }
    if (MaterialFlat1) { out.flatBase1 = in.flatBase1; out.flatOffset1 = in.flatOffset1; }
    if (MaterialUVPerspective) out.uv = in.uv;
    if (MaterialUVLinear) out.uv = in.linearUv;
    if (MaterialUV1Perspective) out.uv1 = in.uv1;
    if (MaterialUV1Linear) out.uv1 = in.linearUv1;
    if (MaterialSequence) out.sequence = in.sequence;
    return out;
}
template<bool Specialized = false> uint materialFlags(constant Poly& p);
template<> uint materialFlags<false>(constant Poly& p) { return p.flags; }
template<> uint materialFlags<true>(constant Poly& p) { return MaterialFlags; }
template<bool Specialized = false> uint materialTsp(constant Poly& p, uint area);
template<> uint materialTsp<false>(constant Poly& p, uint area) { return area ? p.tsp1 : p.tsp0; }
template<> uint materialTsp<true>(constant Poly& p, uint area) { return area ? MaterialTsp1 : MaterialTsp0; }
template<bool Specialized = false> bool flag(constant Poly& p, uint b) {
    return (materialFlags<Specialized>(p) & (1u << b)) != 0;
}

void lightColors(thread float4& base, thread float4& offset, uint volume,
                 float3 position, float3 normal, constant Naomi2& n2, constant Lights& ls,
                 thread float3& reflectDir, thread bool& reflectionReady) {
    if (n2.constantColor[volume]) return;
    float3 diffuse = 0, specular = 0;
    float diffuseAlpha = 0, specularAlpha = 0;
    for (int i = 0; i < min(ls.lightCount, 16); i++) {
        constant PackedLight& l = ls.lights[i];
        // Disabled routes cannot contribute to this volume. Skip their
        // direction, distance and angle calculations as well.
        if (!l.diffuse[volume] && !l.specular[volume]) continue;
        float3 color = l.color.rgb;
        float3 direction = normalize(l.parallel ? l.direction.xyz : l.position.xyz - position);
        if (!l.parallel) {
            if (l.attnDistA != 1 || l.attnDistB != 0) {
                float dist = length(l.position.xyz - position);
                if (l.distAttnMode == 0) dist = 1 / dist;
                color *= clamp(l.attnDistB * dist + l.attnDistA, 0.0f, 1.0f);
            }
            if (l.attnAngleA != 1 || l.attnAngleB != 0)
                color *= clamp((1 - max(0.0f, dot(direction, l.direction.xyz))) * l.attnAngleB + l.attnAngleA, 0.0f, 1.0f);
        }
        float factor = (l.routing & 8) ? -2.0f : 2.0f;
        if (l.diffuse[volume]) {
            float f = factor;
            if (l.dmode == 0) f *= max(dot(normal, direction), 0.0f);
            else if (l.dmode == 1) f *= abs(dot(normal, direction));
            if (l.routing & 4) diffuseAlpha += color.r * f;
            else if (l.routing & 2) specular += color * f * base.rgb;
            else diffuse += color * f * base.rgb;
        }
        if (l.specular[volume]) {
            if (!reflectionReady && (l.smode == 0 || l.smode == 1)) {
                reflectDir = reflect(normalize(position), normal);
                reflectionReady = true;
            }
            float f = factor;
            if (l.smode == 0) f *= clamp(pow(max(dot(direction, reflectDir), 0.0f), n2.gloss[volume]), 0.0f, 1.0f);
            else if (l.smode == 1) f *= clamp(pow(abs(dot(direction, reflectDir)), n2.gloss[volume]), 0.0f, 1.0f);
            if (l.routing & 4) specularAlpha += color.r * f;
            else if (l.routing & 1) specular += color * f * offset.rgb;
            else diffuse += color * f * offset.rgb;
        }
    }
    diffuse += ls.ambientBase[volume].rgb * (ls.ambientMaterialBase[volume] ? base.rgb : float3(1));
    specular += ls.ambientOffset[volume].rgb * (ls.ambientMaterialOffset[volume] ? offset.rgb : float3(1));
    base.rgb = diffuse; offset.rgb = specular;
    base.a += diffuseAlpha; offset.a += specularAlpha;
    if (ls.useBaseOver) offset += max(base - 1.0f, 0.0f);
    base = clamp(base, 0.0f, 1.0f); offset = clamp(offset, 0.0f, 1.0f);
}

template<bool Specialized, bool CanNaomi2 = true> Varying pvrVertex(Input in, uint vertexID,
                                                constant Frame& f, constant Poly& p, constant Lights& ls,
                                                constant Naomi2& n2) {
    Varying out;
    float4 pos = float4(in.pos, 1);
    out.base = in.base; out.offset = in.offset;
    out.base1 = in.base1; out.offset1 = in.offset1;
    bool textured = flag<Specialized>(p, 0);
    bool color0 = !(p.tsp0 & (1u << 25));
    bool color1 = flag<Specialized>(p, 6) && !(p.tsp1 & (1u << 25));
    // Untextured/inactive areas have no defined UV input. Even an unused
    // interpolant must not carry NaN/Inf through Metal's rasterizer. Keep
    // the generic fallback finite without altering any sampled area's UV.
    bool uv0 = textured && color0, uv1 = textured && color1;
    out.uv = uv0 ? in.uv : float2(0);
    out.uv1 = uv1 ? in.uv1 : float2(0);
    if (CanNaomi2 && flag<Specialized>(p, 5)) {
        pos = n2.mv * pos;
        // Position-only OIT depth passes need neither transformed normals,
        // lighting nor environment coordinates (Vulkan LIGHT_ON semantics).
        if (!flag<Specialized>(p, 14)) {
            bool lighting = !flag<Specialized>(p, 3) || !textured;
            // Source-select survives normalization only for OIT. Read the
            // dynamic TSP even in material variants: shading constants omit
            // accumulator controls. Only a consumed area's color/UV needs
            // lighting or environment mapping.
            bool light0 = color0 && lighting && !n2.constantColor.x;
            bool light1 = color1 && lighting && !n2.constantColor.y;
            bool env0 = color0 && textured && n2.env.x;
            bool env1 = color1 && textured && n2.env.y;
            float3 normal = 0;
            // Constant/ambient-only colors and inactive UVs cannot consume
            // normals. Keep ambient shading and untextured color addition.
            if (((light0 || light1) && ls.lightCount > 0) || env0 || env1) {
                float3x3 normalMatrix(n2.normal[0].xyz, n2.normal[1].xyz, n2.normal[2].xyz);
                normal = normalize(normalMatrix * in.normal);
            }
            // Vulkan's n2Uniform.bumpMapping requires Texture && Bump.
            // An unused TCW format must not disable untextured lighting.
            if (lighting) {
                // Both volumes use the same view-space position and normal.
                // Lazily compute their shared reflection once; gloss and
                // light routing remain independent for each volume.
                float3 reflectDir = 0;
                bool reflectionReady = false;
                if (light0) lightColors(out.base, out.offset, 0, pos.xyz, normal, n2, ls, reflectDir, reflectionReady);
                if (light1) lightColors(out.base1, out.offset1, 1, pos.xyz, normal, n2, ls, reflectDir, reflectionReady);
                if (!textured) {
                    if (color0) out.base += out.offset;
                    if (color1) out.base1 += out.offset1;
                }
            }
            if (env0) out.uv = clamp(out.uv + normal.xy * .5f + .5f, 0.0f, 1.0f);
            if (env1) out.uv1 = clamp(out.uv1 + normal.xy * .5f + .5f, 0.0f, 1.0f);
        }
        pos = n2.projection * pos;
        pos = float4(pos.xy / pos.w, 1 / pos.w, 1);
    }
    pos = f.ndc * pos;
    out.flatBase = out.base; out.flatOffset = out.offset;
    out.flatBase1 = out.base1; out.flatOffset1 = out.offset1;
    if (flag<Specialized>(p, 4)) {
        pos /= pos.z;
        out.reciprocal = pos.w;
    } else {
        out.reciprocal = pos.z;
        if (uv0) out.uv *= pos.z;
        if (uv1) out.uv1 *= pos.z;
        out.base *= pos.z; out.offset *= pos.z;
        out.base1 *= pos.z; out.offset1 *= pos.z;
        pos.w = 1;
    }
    // Vulkan has downward clip Y; Metal has upward clip Y.
    pos.y = -pos.y; pos.z = 0;
    out.position = pos;
    out.sequence = p.sequence + vertexID;
    return out;
}

vertex Varying pvr_vertex(Input in [[stage_in]], uint vertexID [[vertex_id]],
                          constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                          constant Lights& ls [[buffer(3)]], constant Naomi2& n2 [[buffer(4)]]) {
    return pvrVertex<false>(in, vertexID, f, p, ls, n2);
}
vertex Varying pvr_dc_vertex(InputDC in [[stage_in]], uint vertexID [[vertex_id]],
                             constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                             constant Lights& ls [[buffer(3)]], constant Naomi2& n2 [[buffer(4)]]) {
    return pvrVertex<false, false>(expandDC(in), vertexID, f, p, ls, n2);
}
MaterialVarying compactMaterial(Varying full) {
    MaterialVarying out;
    out.position = full.position; if (MaterialPerspective) out.reciprocal = full.reciprocal;
    if (MaterialManual) out.linearReciprocal = full.reciprocal;
    if (MaterialSmoothPerspective) { out.base = full.base; out.offset = full.offset; }
    if (MaterialSmoothLinear) { out.linearBase = full.base; out.linearOffset = full.offset; }
    if (MaterialFlat) { out.flatBase = full.flatBase; out.flatOffset = full.flatOffset; }
    if (MaterialSmooth1Perspective) { out.base1 = full.base1; out.offset1 = full.offset1; }
    if (MaterialSmooth1Linear) { out.linearBase1 = full.base1; out.linearOffset1 = full.offset1; }
    if (MaterialFlat1) { out.flatBase1 = full.flatBase1; out.flatOffset1 = full.flatOffset1; }
    if (MaterialUVPerspective) out.uv = full.uv;
    if (MaterialUVLinear) out.linearUv = full.uv;
    if (MaterialUV1Perspective) out.uv1 = full.uv1;
    if (MaterialUV1Linear) out.linearUv1 = full.uv1;
    if (MaterialSequence) out.sequence = full.sequence;
    return out;
}
vertex MaterialVarying pvr_material_vertex(Input in [[stage_in]], uint vertexID [[vertex_id]],
                                           constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                           constant Lights& ls [[buffer(3)]], constant Naomi2& n2 [[buffer(4)]]) {
    return compactMaterial(pvrVertex<true>(in, vertexID, f, p, ls, n2));
}
vertex MaterialVarying pvr_dc_material_vertex(InputDC in [[stage_in]], uint vertexID [[vertex_id]],
                                              constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                              constant Lights& ls [[buffer(3)]], constant Naomi2& n2 [[buffer(4)]]) {
    return compactMaterial(pvrVertex<true, false>(expandDC(in), vertexID, f, p, ls, n2));
}
// Selected only with a host-proven upper bound for positive DC depth.
// Keep the precise reciprocal varying and all material interpolation intact.
vertex MaterialVarying pvr_dc_bounded_vertex(InputDC in [[stage_in]], uint vertexID [[vertex_id]],
                                           constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                           constant Lights& ls [[buffer(3)]], constant Naomi2& n2 [[buffer(4)]]) {
    Varying out = pvrVertex<true, false>(expandDC(in), vertexID, f, p, ls, n2);
    out.position.z = p.depthBound * out.position.w;
    return compactMaterial(out);
}

template<bool Specialized = false> float pvrDepth(float z, constant Poly& p) {
    float w = flag<Specialized>(p, 4) ? 100000.0f / z : 100000.0f * z;
#if FLYCAST_METAL_SHARED_PRECISE_DIVISOR
    // The clamped logarithm is zero, nonfinite, or a normal value in [-20,128].
    // Correct the constant reciprocal product to the original rounded quotient.
    float x = log2(1 + max(w, -0.999999f));
    constexpr float reciprocal = 1.0f / 34.0f;
    float q = x * reciprocal;
    if (!isfinite(x) || x == 0) return q;
    return fma(fma(-34.0f, q, x), reciprocal, q);
#else
    return log2(1 + max(w, -0.999999f)) / 34.0f;
#endif
}
float2 fogCoordinates(float z) {
    // In the clamped normal range, the first four mantissa bits select
    // the fog-table entry and the remaining nineteen give its fraction.
    if (z >= 1.0f && z <= 255.9999f) {
        uint bits = as_type<uint>(z);
        uint index = (bits >> 19) - (127u << 4);
        float fraction = float(bits & 0x7ffffu);
        return float2((float(index) + .5f) * 0x1p-7f,
                      .75f - fraction * 0x1p-20f);
    }
    // Preserve previous behaviour for NaN and out-of-domain callers.
    int exponent;
    float m = frexp(z, exponent) * 32 - 16;
    return float2((floor(m) + (exponent - 1) * 16 + .5f) / 128,
                  .75f - fract(m) / 2);
}
template<bool Specialized = false> float fogValue(float w, constant Frame& f, constant Poly& p, texture2d<float> fog) {
    constexpr sampler s(coord::normalized, address::clamp_to_edge, filter::linear);
    float z = clamp(flag<Specialized>(p, 4) ? f.fogDensity / w : f.fogDensity * w, 1.0f, 255.9999f);
    return fog.sample(s, fogCoordinates(z)).r;
}
float4 paletteColor(float index, constant Poly& p, texture2d<float> palette) {
    return palette.read(uint2(uint(round(index * 255)) + p.palette, 0));
}
#if FLYCAST_METAL_SHARED_PRECISE_DIVISOR
struct SharedDivisor { float denominator, reciprocal; bool bounded; };
SharedDivisor prepareDivisor(float b) {
    // Keep reciprocal, quotient and correction residuals normal.
    bool bounded = abs(b) >= 0x1p-32f && abs(b) <= 0x1p32f;
    float reciprocal = 0;
    if (bounded) {
        // Estimate only the positive, bounded normal magnitude (<= 1 ulp).
        {
#pragma METAL fp math_mode(relaxed)
            reciprocal = 1.0f / abs(b);
        }
        reciprocal = copysign(reciprocal, b);
        uint estimateBits = as_type<uint>(reciprocal);

        // Markstein reciprocal correction; safe math outside the estimate.
        reciprocal = fma(fma(-b, reciprocal, 1.0f), reciprocal, reciprocal);
        // All-ones divisor: the initial estimate is one of two adjacent
        // floats; the odd one is correctly rounded (the exceptional tie).
        if ((as_type<uint>(b) & 0x007fffffu) == 0x007fffffu)
            reciprocal = as_type<float>(estimateBits | 1u);
    }
    return {b, reciprocal, bounded};
}
float correctedDivide(float a, thread const SharedDivisor& d) {
    if (!d.bounded || !(a == 0 || (abs(a) >= 0x1p-32f && abs(a) <= 0x1p32f)))
        return a / d.denominator;
    float q = a * d.reciprocal;
    if (a == 0) return q; // Preserve the sign of zero, including negative divisors.
    // Correctly rounded shared-reciprocal division (no overflow/underflow).
    // Brisebarre/Muller/Raina, Algorithm 1, https://perso.ens-lyon.fr/jean-michel.muller/117197.pdf.
    return fma(fma(-d.denominator, q, a), d.reciprocal, q);
}
float2 correctedDivide(float2 a, thread const SharedDivisor& d) {
    if (!d.bounded || !all((a == 0) | ((abs(a) >= 0x1p-32f) & (abs(a) <= 0x1p32f))))
        return a / d.denominator;
    float2 q = a * d.reciprocal;
    float2 corrected = fma(fma(float2(-d.denominator), q, a), float2(d.reciprocal), q);
    return select(corrected, q, a == 0);
}
float3 correctedDivide(float3 a, thread const SharedDivisor& d) {
    if (!d.bounded || !all((a == 0) | ((abs(a) >= 0x1p-32f) & (abs(a) <= 0x1p32f))))
        return a / d.denominator;
    float3 q = a * d.reciprocal;
    float3 corrected = fma(fma(float3(-d.denominator), q, a), float3(d.reciprocal), q);
    return select(corrected, q, a == 0);
}
float4 correctedDivide(float4 a, thread const SharedDivisor& d) {
    if (!d.bounded || !all((a == 0) | ((abs(a) >= 0x1p-32f) & (abs(a) <= 0x1p32f))))
        return a / d.denominator;
    float4 q = a * d.reciprocal;
    float4 corrected = fma(fma(float4(-d.denominator), q, a), float4(d.reciprocal), q);
    return select(corrected, q, a == 0);
}
#endif
template<bool Specialized = false> float4 shade(Varying in, uint area, constant Frame& f, constant Poly& p, bool shadowDim,
             texture2d<float> tex, sampler ts, texture2d<float> fog, texture2d<float> palette) {
    uint tsp = materialTsp<Specialized>(p, area);
    float4 base = area ? in.base1 : in.base, offset = area ? in.offset1 : in.offset;
    float2 uv = area ? in.uv1 : in.uv;
#if FLYCAST_METAL_RELAXED_DIVISIONS
    // Host-proven bounded DC coordinates, linear RGB-only texture use.
    // Generic fallback, nearest, palette and alpha-dependent samples stay exact.
    const bool colourUV = Specialized && flag<Specialized>(p, 18);
    float2 reconstructedUV = uv;
    if (colourUV) {
#pragma METAL fp math_mode(relaxed)
        reconstructedUV = uv / in.reciprocal;
    }
#else
    const bool colourUV = false;
#endif
#if FLYCAST_METAL_SHARED_PRECISE_DIVISOR
    SharedDivisor divisor = {in.reciprocal, 0, false};
    if (!flag<Specialized>(p, 4) && (flag<Specialized>(p, 2) || flag<Specialized>(p, 0)))
        divisor = prepareDivisor(in.reciprocal);
#endif
    if (!flag<Specialized>(p, 2)) { base = area ? in.flatBase1 : in.flatBase; offset = area ? in.flatOffset1 : in.flatOffset; }
    else if (!flag<Specialized>(p, 4)) {
        // Alpha controls blending/discard; bump offsets also produce alpha.
#if FLYCAST_METAL_SHARED_PRECISE_DIVISOR
        // Reconstruct only alpha components consumed by this material.
        // Keeping unused components out of the UV vector also removes their guards.
        float2 alpha = float2(base.a, offset.a);
        uint fogControl = flag<Specialized>(p, 9) ? (tsp >> 22) & 3 : 2;
        bool baseAlphaNeeded = (tsp & (1u << 20)) && fogControl != 3
            && (!flag<Specialized>(p, 0) || ((tsp >> 6) & 3) >= 2);
        bool offsetAlphaNeeded = (flag<Specialized>(p, 0) && flag<Specialized>(p, 3))
            || (fogControl == 1 && flag<Specialized>(p, 1) && !flag<Specialized>(p, 3));
        if (flag<Specialized>(p, 0) && !colourUV) {
            if (baseAlphaNeeded && offsetAlphaNeeded) {
                float4 alphaUV = correctedDivide(float4(base.a, offset.a, uv), divisor);
                alpha = alphaUV.xy; uv = alphaUV.zw;
            } else if (baseAlphaNeeded) {
                float3 alphaUV = correctedDivide(float3(base.a, uv), divisor);
                alpha.x = alphaUV.x; uv = alphaUV.yz;
            } else if (offsetAlphaNeeded) {
                float3 alphaUV = correctedDivide(float3(offset.a, uv), divisor);
                alpha.y = alphaUV.x; uv = alphaUV.yz;
            } else uv = correctedDivide(uv, divisor);
        } else if (baseAlphaNeeded && offsetAlphaNeeded)
            alpha = correctedDivide(alpha, divisor);
        else if (baseAlphaNeeded) alpha.x = correctedDivide(alpha.x, divisor);
        else if (offsetAlphaNeeded) alpha.y = correctedDivide(alpha.y, divisor);
#if FLYCAST_METAL_RELAXED_DIVISIONS
        if (colourUV) uv = reconstructedUV;
#endif
        if (flag<Specialized>(p, 3)) offset.rgb = correctedDivide(offset.rgb, divisor);
#else
        float2 alpha = float2(base.a, offset.a) / in.reciprocal;
        if (flag<Specialized>(p, 3)) offset.rgb /= in.reciprocal;
#endif
        {
#if FLYCAST_METAL_RELAXED_DIVISIONS
        // Share reciprocal work only across RGB color components, without
        // asserting finite inputs. Coverage, alpha and nearest texel selection stay precise.
#pragma METAL fp math_mode(relaxed)
#endif
            base.rgb /= in.reciprocal;
            if (!flag<Specialized>(p, 3)) offset.rgb /= in.reciprocal;
        }
        base.a = alpha.x; offset.a = alpha.y;
    }
    if (!(tsp & (1u << 20))) base.a = 1;
    uint fogMode = flag<Specialized>(p, 9) ? (tsp >> 22) & 3 : 2;
    if (fogMode == 3) base = float4(f.fogRam.rgb, fogValue<Specialized>(in.reciprocal, f, p, fog));
    if (flag<Specialized>(p, 0)) {
        if (!flag<Specialized>(p, 4)) {
            // Keep exact texel-boundary selection for nearest-filtered HUDs.
#if FLYCAST_METAL_SHARED_PRECISE_DIVISOR
            if (!flag<Specialized>(p, 2)) {
#if FLYCAST_METAL_RELAXED_DIVISIONS
                if (colourUV) uv = reconstructedUV;
                else
#endif
                    uv = correctedDivide(uv, divisor);
            }
#else
            uv /= in.reciprocal;
#endif
        }
        float4 tc;
        uint paletteMode = (materialFlags<Specialized>(p) >> 16) & 3;
        if (!area && paletteMode) {
            if (paletteMode == 1) tc = paletteColor(tex.sample(ts, uv).r, p, palette);
            else {
                float2 size = float2(tex.get_width(), tex.get_height());
                float2 weight = fract(uv * size - .5f);
                // GPU palette textures have one mip level. Gather the four
                // R8 indices together, preserving sampler wrap/clamp modes.
                // Metal returns footprint corners (0,1), (1,1), (1,0), (0,0).
                float4 indices = tex.gather(ts, uv);
                float4 a = paletteColor(indices.w, p, palette);
                float4 b = paletteColor(indices.z, p, palette);
                float4 c = paletteColor(indices.x, p, palette);
                float4 d = paletteColor(indices.y, p, palette);
                tc = mix(mix(a, b, weight.x), mix(c, d, weight.x), weight.y);
            }
        } else tc = tex.sample(ts, uv, bias(area ? p.lodBias1 : p.lodBias));
        if (flag<Specialized>(p, 3)) {
            float s = PVR_PI / 2 * (tc.a * 240 + tc.r * 15) / 255;
            float r = 2 * PVR_PI * (tc.g * 240 + tc.b * 15) / 255;
            tc = float4(1, 1, 1, clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2 * PVR_PI * offset.b), 0.0f, 1.0f));
        } else if ((tsp & (1u << 19)) || flag<Specialized>(p, area ? 12 : 11)) tc.a = 1;
        switch ((tsp >> 6) & 3) {
            case 0: base = tc; break;
            case 1: base.rgb *= tc.rgb; base.a = tc.a; break;
            case 2: base.rgb = mix(base.rgb, tc.rgb, tc.a); break;
            case 3: base *= tc; break;
        }
        if (flag<Specialized>(p, 1) && !flag<Specialized>(p, 3)) base.rgb += offset.rgb;
    }
    if (shadowDim) base.rgb *= f.shadeScale;
    // Vulkan's ColorClamping pipeline constant comes from TSP0 for both
    // volumes; volume selection changes shading/alpha/fog, not this control.
    if (materialTsp<Specialized>(p, 0) & (1u << 21)) base = clamp(base, f.clampMin, f.clampMax);
    if (fogMode == 0) base.rgb = mix(base.rgb, f.fogRam.rgb, fogValue<Specialized>(in.reciprocal, f, p, fog));
    if (fogMode == 1 && flag<Specialized>(p, 1) && !flag<Specialized>(p, 3)) base.rgb = mix(base.rgb, f.fogVertex.rgb, offset.a);
    // Generic fallback keeps its original uniform calculation. Material
    // variants omit weighting when the host guarantees a unit factor.
    if (!Specialized || flag<Specialized>(p, 10)) base *= p.trilinear;
    return base;
}
template<bool Specialized = false> void clipFragment(float2 xy, constant Poly& p) {
    if (flag<Specialized>(p, 8) && all(xy >= p.clip.xy) && all(xy <= p.clip.zw)) discard_fragment();
}
float4 ditherColor(float4 color, uint2 xy, constant Frame& f) {
    constexpr float table[16] = {5,13,7,15,9,1,11,3,6,14,4,12,10,2,8,0};
    float value = table[(xy.y & 3) * 4 + (xy.x & 3)];
    return clamp(floor(color * 255 + float4(value, value, value, 1) / f.dither) / 255, 0.0f, 1.0f);
}
struct Fragment { float4 color [[color(0)]]; float depth [[depth(any)]]; };
struct DepthFragment { float depth [[depth(any)]]; };
struct BoundedFragment { float4 color [[color(0)]]; float depth [[depth(less)]]; };
struct BoundedDepthFragment { float depth [[depth(less)]]; };
struct PositionInput { float3 pos [[attribute(0)]]; };
struct PositionVarying { float4 position [[position]]; float reciprocal; };
// Opaque/translucent depth masks only need transformed position and depth.
// Keep their Naomi2 projection identical to pvr_vertex; modifier volumes
// additionally clamp view-space Z and therefore cannot share this vertex path.
PositionVarying positionVertex(PositionInput in, constant Frame& f, constant Poly& p, constant Naomi2& n2) {
    float4 pos = float4(in.pos, 1);
    if (flag(p, 5)) {
        pos = n2.mv * pos;
        pos = n2.projection * pos;
        pos = float4(pos.xy / pos.w, 1 / pos.w, 1);
    }
    pos = f.ndc * pos;
    float reciprocal;
    if (flag(p, 4)) { pos /= pos.z; reciprocal = pos.w; }
    else { reciprocal = pos.z; pos.w = 1; }
    pos.y = -pos.y; pos.z = 0;
    return {pos, reciprocal};
}
vertex PositionVarying pvr_position_vertex(PositionInput in [[stage_in]],
                                           constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                           constant Naomi2& n2 [[buffer(4)]]) {
    return positionVertex(in, f, p, n2);
}
struct OITPositionVarying {
    float4 position [[position]];
    float reciprocal;
    uint sequence [[flat]];
};
vertex OITPositionVarying oit_position_vertex(PositionInput in [[stage_in]], uint vertexID [[vertex_id]],
                                            constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                            constant Naomi2& n2 [[buffer(4)]]) {
    PositionVarying out = positionVertex(in, f, p, n2);
    return {out.position, out.reciprocal, p.sequence + vertexID};
}
fragment DepthFragment pvr_position_fragment(PositionVarying in [[stage_in]], constant Poly& p [[buffer(2)]]) {
    clipFragment(in.position.xy, p);
    return {pvrDepth(in.reciprocal, p)};
}
// Depth passes need color evaluation only for punch-through rejection. Keep
// its full alpha semantics (texture, palette, bump and shading instruction),
// while avoiding color/fog evaluation for opaque and translucent depth masks.
template<bool Specialized> DepthFragment pvrDepthFragment(Varying in, constant Frame& f, constant Poly& p,
                                    texture2d<float> tex0, texture2d<float> fog,
                                    texture2d<float> palette, sampler s0) {
    clipFragment<Specialized>(in.position.xy, p);
    if (flag<Specialized>(p, 7)) {
        float4 color = shade<Specialized>(in, 0, f, p, false, tex0, s0, fog, palette);
        color.a = round(color.a * 255) / 255;
        if (f.alphaRef > color.a) discard_fragment();
    }
    return {pvrDepth<Specialized>(in.reciprocal, p)};
}
fragment DepthFragment pvr_depth_fragment(Varying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                    texture2d<float> tex0 [[texture(0)]], texture2d<float> fog [[texture(2)]],
                                    texture2d<float> palette [[texture(3)]], sampler s0 [[sampler(0)]]) {
    return pvrDepthFragment<false>(in, f, p, tex0, fog, palette, s0);
}
fragment DepthFragment pvr_material_depth_fragment(MaterialVarying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                    texture2d<float> tex0 [[texture(0)]], texture2d<float> fog [[texture(2)]],
                                    texture2d<float> palette [[texture(3)]], sampler s0 [[sampler(0)]]) {
    return pvrDepthFragment<true>(expandMaterial(in), f, p, tex0, fog, palette, s0);
}
fragment BoundedDepthFragment pvr_bounded_depth_fragment(MaterialVarying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                    texture2d<float> tex0 [[texture(0)]], texture2d<float> fog [[texture(2)]],
                                    texture2d<float> palette [[texture(3)]], sampler s0 [[sampler(0)]]) {
    return {pvrDepthFragment<true>(expandMaterial(in), f, p, tex0, fog, palette, s0).depth};
}
template<bool Specialized> Fragment pvrFragment(Varying in, constant Frame& f, constant Poly& p,
                              texture2d<float> tex0, texture2d<float> tex1,
                              texture2d<float> fog, texture2d<float> palette,
                              texture2d<uint> stencil,
                              sampler s0, sampler s1) {
    clipFragment<Specialized>(in.position.xy, p);
    bool shadow = flag<Specialized>(p, 13) && stencil.read(uint2(in.position.xy)).r == 0x81;
    uint area = shadow && flag<Specialized>(p, 6);
    float4 color = area ? shade<Specialized>(in, 1, f, p, false, tex1, s1, fog, palette) : shade<Specialized>(in, 0, f, p, shadow, tex0, s0, fog, palette);
    if (flag<Specialized>(p, 7)) {
        color.a = round(color.a * 255) / 255;
        if (f.alphaRef > color.a) discard_fragment();
        color.a = 1;
    }
    if (f.dithering && !flag<Specialized>(p, 15)) color = ditherColor(color, uint2(in.position.xy), f);
    return {color, pvrDepth<Specialized>(in.reciprocal, p)};
}
fragment Fragment pvr_fragment(Varying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                              texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]],
                              texture2d<float> fog [[texture(2)]], texture2d<float> palette [[texture(3)]],
                              texture2d<uint> stencil [[texture(5)]],
                              sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]]) {
    return pvrFragment<false>(in, f, p, tex0, tex1, fog, palette, stencil, s0, s1);
}
fragment Fragment pvr_material_fragment(MaterialVarying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                              texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]],
                              texture2d<float> fog [[texture(2)]], texture2d<float> palette [[texture(3)]],
                              texture2d<uint> stencil [[texture(5)]],
                              sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]]) {
    return pvrFragment<true>(expandMaterial(in), f, p, tex0, tex1, fog, palette, stencil, s0, s1);
}
fragment BoundedFragment pvr_bounded_fragment(MaterialVarying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                              texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]],
                              texture2d<float> fog [[texture(2)]], texture2d<float> palette [[texture(3)]],
                              texture2d<uint> stencil [[texture(5)]],
                              sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]]) {
    Fragment out = pvrFragment<true>(expandMaterial(in), f, p, tex0, tex1, fog, palette, stencil, s0, s1);
    return {out.color, out.depth};
}

struct ModInput { float3 pos [[attribute(0)]]; };
struct ModVarying { float4 position [[position]]; float reciprocal; };
vertex ModVarying mod_vertex(ModInput in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                            constant Naomi2& n2 [[buffer(4)]]) {
    float4 pos = float4(in.pos, 1);
    if (flag(p, 5)) {
        pos = n2.mv * pos; pos.z = min(pos.z, -.001f); pos = n2.projection * pos;
        pos = float4(pos.xy / pos.w, 1 / pos.w, 1);
    }
    pos = f.ndc * pos;
    float z = pos.z;
    if (flag(p, 4)) { pos /= pos.z; z = pos.w; }
    else pos.w = 1;
    pos.y = -pos.y; pos.z = 0;
    return {pos, z};
}
fragment Fragment mod_fragment(ModVarying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]]) {
    clipFragment(in.position.xy, p);
    return {float4(0, 0, 0, 1 - f.shadeScale), pvrDepth(in.reciprocal, p)};
}
fragment DepthFragment mod_depth_fragment(ModVarying in [[stage_in]], constant Poly& p [[buffer(2)]]) {
    clipFragment(in.position.xy, p);
    return {pvrDepth(in.reciprocal, p)};
}

struct Node {
    uint color0;
    float depth;
    uint next, sequence;
    // Polygon index occupies bits 2..31. Modifier operations change only
    // the two low stencil/accumulator bits; sequence stays a full uint.
    atomic_uint metadata;
};
struct OITPoly { uint tsp0, tsp1; };
static_assert(sizeof(Node) == 20, "Metal OIT node ABI");
static_assert(sizeof(OITPoly) == 8, "Metal OIT polygon ABI");
uint packColor(float4 c) { uint4 v = uint4(round(clamp(c, 0.0f, 1.0f) * 255)); return v.x | v.y << 8 | v.z << 16 | v.w << 24; }
float4 unpackColor(uint c) { return float4(c & 255, (c >> 8) & 255, (c >> 16) & 255, c >> 24) / 255; }
uint allocateOITNode(constant Frame& f, device atomic_uint *counter) {
    uint index = atomic_fetch_add_explicit(counter, 1u, memory_order_relaxed);
    if (index >= f.capacity) {
        atomic_fetch_or_explicit(counter + 1, 1u, memory_order_relaxed);
        discard_fragment();
        return 0xffffffffu;
    }
    return index;
}
void publishOITNode(device Node& n, uint index, uint2 xy, float z, uint sequence,
                    constant Frame& f, constant Poly& p, device atomic_uint *heads) {
    n.depth = z; n.sequence = sequence;
    atomic_store_explicit(&n.metadata, p.polyIndex << 2, memory_order_relaxed);
    n.next = atomic_exchange_explicit(heads + xy.y * f.width + xy.x, index, memory_order_relaxed);
}
template<bool Specialized> void oitFragment(Varying in, constant Frame& f, constant Poly& p,
                             device Node *nodes, device atomic_uint *heads, device atomic_uint *counter, device uint *colors1,
                             texture2d<float> tex0, texture2d<float> tex1,
                             texture2d<float> fog, texture2d<float> palette, depth2d<float> depth,
                             sampler s0, sampler s1) {
    clipFragment<Specialized>(in.position.xy, p);
    uint2 xy = uint2(in.position.xy);
    float z = pvrDepth<Specialized>(in.reciprocal, p);
    // Discarded helper lanes must continue through implicit texture sampling
    // so neighbouring fragments retain defined derivatives.
    if (z < depth.read(xy)) discard_fragment();
    // Source-select is dynamic even in material variants: their TSP
    // constants contain only shading controls. Resolve uses the original
    // polygon table to select the same accumulator instead of these colors.
    bool color0Needed = !(p.tsp0 & (1u << 25));
    bool color1Needed = flag<Specialized>(p, 6) && !(p.tsp1 & (1u << 25));
    // Area 1 is consumed only after a matching modifier shadows a
    // two-volume polygon. Keep its derivative-dependent shading before
    // allocation/overflow divergence, but do not store a duplicate area 0.
    float4 c0 = 0, c1 = 0;
    if (color0Needed) c0 = shade<Specialized>(in, 0, f, p, false, tex0, s0, fog, palette);
    if (color1Needed) c1 = shade<Specialized>(in, 1, f, p, false, tex1, s1, fog, palette);
    uint index = allocateOITNode(f, counter);
    if (index == 0xffffffffu) return;
    device Node& n = nodes[index];
    if (color0Needed) n.color0 = packColor(c0);
    if (color1Needed) colors1[index] = packColor(c1);
    publishOITNode(n, index, xy, z, in.sequence, f, p, heads);
}
// Both active areas source the secondary accumulator. Preserve raster coverage,
// manual depth and ordering without fetching or shading colors, UVs or lights.
fragment void oit_position_fragment(OITPositionVarying in [[stage_in]], constant Frame& f [[buffer(1)]],
                                   constant Poly& p [[buffer(2)]], device Node *nodes [[buffer(4)]],
                                   device atomic_uint *heads [[buffer(5)]], device atomic_uint *counter [[buffer(6)]],
                                   depth2d<float> depth [[texture(4)]]) {
    clipFragment(in.position.xy, p);
    uint2 xy = uint2(in.position.xy);
    float z = pvrDepth(in.reciprocal, p);
    if (z < depth.read(xy)) { discard_fragment(); return; }
    uint index = allocateOITNode(f, counter);
    if (index == 0xffffffffu) return;
    publishOITNode(nodes[index], index, xy, z, in.sequence, f, p, heads);
}
fragment void oit_fragment(Varying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                             device Node *nodes [[buffer(4)]], device atomic_uint *heads [[buffer(5)]], device atomic_uint *counter [[buffer(6)]],
                             device uint *colors1 [[buffer(8)]],
                             texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]],
                             texture2d<float> fog [[texture(2)]], texture2d<float> palette [[texture(3)]], depth2d<float> depth [[texture(4)]],
                             sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]]) {
    oitFragment<false>(in, f, p, nodes, heads, counter, colors1, tex0, tex1, fog, palette, depth, s0, s1);
}
fragment void oit_material_fragment(MaterialVarying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                             device Node *nodes [[buffer(4)]], device atomic_uint *heads [[buffer(5)]], device atomic_uint *counter [[buffer(6)]],
                             device uint *colors1 [[buffer(8)]],
                             texture2d<float> tex0 [[texture(0)]], texture2d<float> tex1 [[texture(1)]],
                             texture2d<float> fog [[texture(2)]], texture2d<float> palette [[texture(3)]], depth2d<float> depth [[texture(4)]],
                             sampler s0 [[sampler(0)]], sampler s1 [[sampler(1)]]) {
    oitFragment<true>(expandMaterial(in), f, p, nodes, heads, counter, colors1, tex0, tex1, fog, palette, depth, s0, s1);
}
// Each pixel owns a disjoint immutable linked list. Order metadata accesses
// for overlapping modifier fragments without ending the render encoder.
#if FLYCAST_METAL_RASTER_ORDERED_MODIFIERS
#define FLYCAST_MODIFIER_ORDER , raster_order_group(0)
#else
#define FLYCAST_MODIFIER_ORDER
#endif
fragment void tr_mod_fragment(ModVarying in [[stage_in]], constant Frame& f [[buffer(1)]], constant Poly& p [[buffer(2)]],
                                device Node *nodes [[buffer(4) FLYCAST_MODIFIER_ORDER]], device atomic_uint *heads [[buffer(5)]],
                                const device OITPoly *polygons [[buffer(7)]]) {
    clipFragment(in.position.xy, p);
    uint2 xy = uint2(in.position.xy);
    uint index = atomic_load_explicit(heads + xy.y * f.width + xy.x, memory_order_relaxed);
    if (index == 0xffffffffu) return;
    // Like Vulkan's MV_MODE variants, only XOR/OR compare fragment depth.
    // Inclusion/exclusion operate solely on the existing modifier bits.
    float z = 0;
    if (p.modMode < 2) z = pvrDepth(in.reciprocal, p);
    for (uint count = 0; index != 0xffffffffu && count < f.maxLayers; count++) {
        device Node& n = nodes[index];
        uint metadata = atomic_load_explicit(&n.metadata, memory_order_relaxed);
        if (polygons[metadata >> 2].tsp0 & 1u) {
            if (p.modMode == 0 && z >= n.depth) atomic_fetch_xor_explicit(&n.metadata, 1u, memory_order_relaxed);
            else if (p.modMode == 1 && z >= n.depth) atomic_fetch_or_explicit(&n.metadata, 1u, memory_order_relaxed);
            else if (p.modMode == 2) {
                uint previous = atomic_fetch_and_explicit(&n.metadata, ~1u, memory_order_relaxed);
                if ((previous & 3) == 1) atomic_fetch_or_explicit(&n.metadata, 2u, memory_order_relaxed);
            } else if (p.modMode == 3) {
                uint previous = atomic_fetch_and_explicit(&n.metadata, ~3u, memory_order_relaxed);
                if ((previous & 3) == 2) atomic_fetch_or_explicit(&n.metadata, 2u, memory_order_relaxed);
            }
        }
        index = n.next;
    }
}
#undef FLYCAST_MODIFIER_ORDER
float4 blendCoefficient(uint mode, float4 other, float4 src, float4 dst) {
    switch (mode) {
        case 0: return 0; case 1: return 1; case 2: return other; case 3: return 1 - other;
        case 4: return src.a; case 5: return 1 - src.a; case 6: return dst.a; default: return 1 - dst.a;
    }
}
bool oitSortsAfter(const device Node& previous, float depth, uint sequence) {
    float previousDepth = previous.depth;
    return previousDepth > depth || (previousDepth == depth && previous.sequence > sequence);
}
template<uint Layers, bool ResetHeads = false>
void resolveOITPixel(constant Frame& f, device Node *nodes, device atomic_uint *heads,
                     device atomic_uint *counter, const device OITPoly *polygons, const device uint *colors1, texture2d<float, access::read> input,
                     texture2d<float, access::write> output, uint2 xy) {
    if (f.oitTile.z != 0) {
        if (xy.x >= f.oitTile.z || xy.y >= f.oitTile.w) return;
        xy += f.oitTile.xy;
    }
    if (xy.x >= f.width || xy.y >= f.height) return;
    uint list[Layers], count = 0, sortedBegin = 0;
    // The incoming key is already loaded. Keep the sorted suffix's last key
    // locally instead of rereading that Node for every append/search decision.
    float tailDepth = 0;
    uint tailSequence = 0;
    uint index = atomic_load_explicit(heads + xy.y * f.width + xy.x, memory_order_relaxed);
    while (index != 0xffffffffu && count < min(f.maxLayers, Layers)) {
        float depth = nodes[index].depth;
        uint sequence = nodes[index].sequence;
        uint position = count;
        if (isnan(depth)) {
            // Linear insertion never crosses a NaN: comparisons stop at it.
            // Keep that barrier and search only the sorted suffix after it.
            sortedBegin = count + 1;
        } else if (count > sortedBegin && (tailDepth > depth || (tailDepth == depth && tailSequence > sequence))) {
            // The last entry already sorts after this fragment. Find the
            // stable upper bound with logarithmic global Node-key reads;
            // shifting the thread-local indices needs no Node-buffer reads.
            uint first = sortedBegin, last = count - 1;
            while (first < last) {
                uint middle = first + (last - first) / 2;
                if (oitSortsAfter(nodes[list[middle]], depth, sequence)) last = middle;
                else first = middle + 1;
            }
            position = first;
            for (uint j = count; j > position; --j) list[j] = list[j - 1];
        }
        // An insertion before the tail leaves it unchanged. A NaN appends
        // and starts an empty suffix, whose first subsequent key replaces it.
        if (position == count) { tailDepth = depth; tailSequence = sequence; }
        list[position] = index; ++count; index = nodes[index].next;
    }
    if (index != 0xffffffffu) atomic_fetch_or_explicit(counter + 1, 2u, memory_order_relaxed);
    float4 color = input.read(xy), secondary = 0;
    for (uint i = 0; i < count; i++) {
        device Node& n = nodes[list[i]];
        uint metadata = atomic_load_explicit(&n.metadata, memory_order_relaxed);
        uint polygonIndex = metadata >> 2;
        uint tsp0 = polygons[polygonIndex].tsp0;
        bool shadow = (metadata & 2) != 0;
        bool area = shadow && (tsp0 & 2u);
        uint tsp = area ? polygons[polygonIndex].tsp1 : tsp0;
        float4 src = secondary;
        if (!(tsp & (1u << 25))) {
            // Accumulator sources leave their packed color unwritten. Area 1
            // exists only for two-volume polygons; single-volume renders bind
            // a dummy buffer and never access this stream.
            if (area) src = unpackColor(colors1[list[i]]);
            else src = unpackColor(n.color0);
        }
        if (shadow && !area && !(tsp & (1u << 25))) src.rgb *= f.shadeScale;
        float4 dst = (tsp & (1u << 24)) ? secondary : color;
        float4 result = clamp(src * blendCoefficient(tsp >> 29, dst, src, dst) + dst * blendCoefficient((tsp >> 26) & 7, src, src, dst), 0.0f, 1.0f);
        if (tsp & (1u << 24)) secondary = result; else color = result;
    }
    if (f.dithering) color = ditherColor(color, xy, f);
    output.write(color, xy);
    if (ResetHeads) {
        // Like Vulkan's final shader, leave an empty list for the next OIT
        // pass. Encoder hazards order that pass after all resolve threads.
        // A zero-count list was already empty. Only populated heads need
        // clearing; maxLayers and Layers are both at least one.
        if (count != 0)
            atomic_store_explicit(heads + xy.y * f.width + xy.x, 0xffffffffu, memory_order_relaxed);
        // Resolve never consumes the allocator count. One thread resets
        // it without a separate counter-copy pass; counter[1] keeps diagnostics.
        if (all(xy == f.oitTile.xy)) atomic_store_explicit(counter, 0u, memory_order_relaxed);
    }
}

// MSL function constants cannot size a thread-local array. Instantiate a
// bounded set of kernels so the compiler can allocate only the needed storage.
#define OIT_RESOLVE_KERNEL(layers) \
kernel void oit_resolve_##layers(constant Frame& f [[buffer(1)]], device Node *nodes [[buffer(4)]], \
    device atomic_uint *heads [[buffer(5)]], device atomic_uint *counter [[buffer(6)]], \
    const device OITPoly *polygons [[buffer(7)]], const device uint *colors1 [[buffer(8)]], \
    texture2d<float, access::read> input [[texture(0)]], texture2d<float, access::write> output [[texture(1)]], \
    uint2 xy [[thread_position_in_grid]]) { resolveOITPixel<layers>(f, nodes, heads, counter, polygons, colors1, input, output, xy); } \
kernel void oit_resolve_##layers##_reset(constant Frame& f [[buffer(1)]], device Node *nodes [[buffer(4)]], \
    device atomic_uint *heads [[buffer(5)]], device atomic_uint *counter [[buffer(6)]], \
    const device OITPoly *polygons [[buffer(7)]], const device uint *colors1 [[buffer(8)]], \
    texture2d<float, access::read> input [[texture(0)]], texture2d<float, access::write> output [[texture(1)]], \
    uint2 xy [[thread_position_in_grid]]) { resolveOITPixel<layers, true>(f, nodes, heads, counter, polygons, colors1, input, output, xy); }
OIT_RESOLVE_KERNEL(1)
OIT_RESOLVE_KERNEL(2)
OIT_RESOLVE_KERNEL(4)
OIT_RESOLVE_KERNEL(8)
OIT_RESOLVE_KERNEL(16)
OIT_RESOLVE_KERNEL(32)
OIT_RESOLVE_KERNEL(64)
OIT_RESOLVE_KERNEL(128)
OIT_RESOLVE_KERNEL(256)
#undef OIT_RESOLVE_KERNEL

// A pass with no sorted fragments needs only the final framebuffer operation.
// Keep it independent of layer arrays, nodes, heads and polygon metadata.
kernel void framebuffer_finish(constant Frame& f [[buffer(1)]],
    texture2d<float, access::read> input [[texture(0)]],
    texture2d<float, access::write> output [[texture(1)]],
    uint2 xy [[thread_position_in_grid]]) {
    if (xy.x >= f.width || xy.y >= f.height) return;
    float4 color = input.read(xy);
    if (f.dithering) color = ditherColor(color, xy, f);
    output.write(color, xy);
}

struct QuadUniform { float4 color; uint rotate, ignoreAlpha; };
struct QuadVarying { float4 position [[position]]; float2 uv; };
vertex QuadVarying quad_vertex(uint id [[vertex_id]], constant QuadUniform& u [[buffer(0)]]) {
    float2 uv = float2(id & 1, id >> 1);
    float2 pos = float2(uv.x * 2 - 1, 1 - uv.y * 2);
    if (u.rotate) pos = float2(-pos.y, pos.x);
    return {float4(pos, 0, 1), uv};
}
fragment float4 quad_fragment(QuadVarying in [[stage_in]], constant QuadUniform& u [[buffer(0)]], texture2d<float> tex [[texture(0)]], sampler s [[sampler(0)]]) {
    float4 c = tex.sample(s, in.uv);
    if (u.ignoreAlpha) c.a = 1;
    return c * u.color;
}
struct OverlayQuad { float4 rectangle, color; };
struct OverlayVarying { float4 position [[position]]; float2 uv; float4 color [[flat]]; };
vertex OverlayVarying overlay_vertex(uint id [[vertex_id]], uint instance [[instance_id]],
    constant OverlayQuad *quads [[buffer(0)]], constant float2& screen [[buffer(1)]]) {
    float2 uv = float2(id & 1, id >> 1);
    float2 pixel = quads[instance].rectangle.xy + uv * quads[instance].rectangle.zw;
    return {float4(pixel.x * 2 / screen.x - 1, 1 - pixel.y * 2 / screen.y, 0, 1), uv, quads[instance].color};
}
fragment float4 overlay_fragment(OverlayVarying in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler s [[sampler(0)]]) {
    return tex.sample(s, in.uv) * in.color;
}
struct UIInput { float2 pos [[attribute(0)]], uv [[attribute(1)]]; float4 color [[attribute(2)]]; };
struct UIVarying { float4 pos [[position]]; float2 uv; float4 color; };
vertex UIVarying ui_vertex(UIInput in [[stage_in]], constant float4& transform [[buffer(1)]]) {
    return {float4(in.pos * transform.xy + transform.zw, 0, 1), in.uv, in.color};
}
fragment float4 ui_fragment(UIVarying in [[stage_in]], texture2d<float> tex [[texture(0)]], sampler s [[sampler(0)]]) {
    return in.color * tex.sample(s, in.uv);
}
)MSL";
