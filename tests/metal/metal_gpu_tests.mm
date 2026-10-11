// Standalone, ROM-free Metal GPU checks. SPDX-License-Identifier: GPL-2.0-or-later
#import <Metal/Metal.h>
#include "rend/metal/metal_commands.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <array>
#include <vector>
#include <limits>
#include <utility>
using u32 = uint32_t;
#include "rend/metal/metal_shaders.h"
#include "rend/metal/metal_shader_options.h"
#include "rend/metal/metal_oit_tiling.h"

namespace {
struct Vertex {
	float pos[3]; uint8_t base[4], offset[4]; float uv[2];
	uint8_t base1[4], offset1[4]; float uv1[2], normal[3];
};
struct Light {
	float color[4], direction[4], position[4];
	int parallel, routing, dmode, smode, diffuse[2], specular[2];
	float distA, distB, angleA, angleB; int distMode, pad[3];
};
struct Lights {
	Light lights[16]; float base[2][4], offset[2][4];
	int materialBase[2], materialOffset[2], count, baseOver, bump0, bump1;
};
void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void error(NSError *error) { if (error) throw std::runtime_error(error.localizedDescription.UTF8String); }

static constexpr const char FogReferenceShader[] = R"MSL(
// Compare the production lookup against the previous shader expression.
// Enumerating the float bit patterns covers every mantissa and exponent
// boundary of the clamped range, rather than just a few uniform fog colors.
kernel void validate_fog_coordinates(constant uint& base [[buffer(0)]],
    device atomic_uint *mismatches [[buffer(1)]], uint id [[thread_position_in_grid]]) {
    float z = clamp(as_type<float>(base + id), 1.0f, 255.9999f);
    float exponent = floor(log2(z));
    float m = z * 16 / exp2(exponent) - 16;
    float2 reference = float2((floor(m) + exponent * 16 + .5f) / 128, .75f - fract(m) / 2);
    float2 actual = fogCoordinates(z);
    if (!all((as_type<uint2>(reference) == as_type<uint2>(actual)) | (isnan(reference) & isnan(actual))))
        atomic_fetch_add_explicit(mismatches, 1u, memory_order_relaxed);
}
// Retain the frexp implementation as an independent reference for the bit
// decomposition, including raw values outside fogValue's clamped domain.
float2 referenceFrexpFogCoordinates(float z) {
    int exponent;
    float m = frexp(z, exponent) * 32 - 16;
    return float2((floor(m) + (exponent - 1) * 16 + .5f) / 128,
                  .75f - fract(m) / 2);
}
kernel void validate_fog_bit_coordinates(constant uint& base [[buffer(0)]],
    device atomic_uint *mismatches [[buffer(1)]], uint id [[thread_position_in_grid]]) {
    float raw = as_type<float>(base + id);
    for (uint mode = 0; mode < 2; ++mode) {
        float z = mode ? clamp(raw, 1.0f, 255.9999f) : raw;
        float2 reference = referenceFrexpFogCoordinates(z);
        float2 actual = fogCoordinates(z);
        if (!all((as_type<uint2>(reference) == as_type<uint2>(actual)) | (isnan(reference) & isnan(actual))))
            atomic_fetch_add_explicit(mismatches, 1u, memory_order_relaxed);
    }
}
)MSL";

static constexpr const char DivisorReferenceShader[] = R"MSL(
#if FLYCAST_METAL_SHARED_PRECISE_DIVISOR
kernel void validate_reciprocal_correction(device atomic_uint* errors [[buffer(0)]],
 constant uint& base [[buffer(1)]], uint id [[thread_position_in_grid]]) {
 uint bits = base + id, magnitude = bits & 0x7fffffffu;
 // Integer bit bounds independently cover both signs, nonfinite values,
 // subnormals, exact endpoints and every mantissa inside the normal range.
 bool expectedBounded = magnitude >= 0x2f800000u && magnitude <= 0x4f800000u;
 float b = as_type<float>(bits); auto d = prepareDivisor(b);
 bool mismatch = d.bounded != expectedBounded;
 if (expectedBounded) mismatch |= as_type<uint>(d.reciprocal) != as_type<uint>(1.0f / b);
 else mismatch |= as_type<uint>(d.reciprocal) != 0;
 if (mismatch) atomic_fetch_add_explicit(errors,1u,memory_order_relaxed);
}
float referenceDepth(float z, constant Poly& p) {
 float w = flag(p, 4) ? 100000.0f / z : 100000.0f * z;
 return log2(1 + max(w, -0.999999f)) / 34.0f;
}
int depthOrder(float a, float b) {return a < b ? -1 : a > b ? 1 : 0;}
kernel void validate_depth_correction(device atomic_uint* stats [[buffer(0)]],
 constant Poly& p [[buffer(1)]], uint id [[thread_position_in_grid]]) {
 uint bits = id * 256u;
 float z = as_type<float>(bits), next = as_type<float>(bits + 1u);
 float ref = referenceDepth(z,p), got = pvrDepth(z,p);
 if (!(isnan(ref) && isnan(got)) && as_type<uint>(ref) != as_type<uint>(got))
  atomic_fetch_add_explicit(stats,1u,memory_order_relaxed);
 if (z > 0 && isfinite(z) && isfinite(next)) {
  float refNext = referenceDepth(next,p), gotNext = pvrDepth(next,p);
  if (depthOrder(ref,refNext) != depthOrder(got,gotNext))
   atomic_fetch_add_explicit(stats+1,1u,memory_order_relaxed);
  if (flag(p,4) ? gotNext > got : gotNext < got)
   atomic_fetch_add_explicit(stats+2,1u,memory_order_relaxed);
 }
}
uint hashWord(uint h) {h=h*747796405u+2891336453u;h=((h>>((h>>28)+4u))^h)*277803737u;return (h>>22)^h;}
uint boundedBits(uint h) {return (h & 0x80000000u) | (0x2f800000u+(h%0x20000001u));}
uint ordered(float f) {uint b=as_type<uint>(f);return b&0x80000000u ? ~b:b|0x80000000u;}
void record(float a,float b,float ref,float got,device atomic_uint* stats,device float4* examples) {
 if(!(isnan(ref)&&isnan(got)) && as_type<uint>(ref)!=as_type<uint>(got)) {
  uint index=atomic_fetch_add_explicit(stats,1u,memory_order_relaxed);
  if(index<16) examples[index]=float4(a,b,ref,got);
  if(isfinite(ref)&&isfinite(got)) {uint x=ordered(ref),y=ordered(got);atomic_fetch_max_explicit(stats+1,x>y?x-y:y-x,memory_order_relaxed);}
  else atomic_fetch_add_explicit(stats+3,1u,memory_order_relaxed);
  if(ref==0 && got==0) atomic_fetch_add_explicit(stats+4,1u,memory_order_relaxed);
 }
}
kernel void validate_shared_divisions(device atomic_uint* stats [[buffer(0)]],device float4* examples [[buffer(1)]],constant uint& bounded [[buffer(2)]],uint id [[thread_position_in_grid]]) {
 uint h=hashWord(id);uint eligible=0;
 for(uint i=0;i<64;i++) {
  h=hashWord(h);float b=as_type<float>(bounded ? boundedBits(h):h);auto d=prepareDivisor(b);
  float4 a;
  for(uint j=0;j<4;j++) {h=hashWord(h);a[j]=as_type<float>(bounded ? boundedBits(h):h);}
  float4 ref=a/b,got=correctedDivide(a,d);
  float4 pairs=float4(correctedDivide(a.xy,d),correctedDivide(a.zw,d));
  float3 triple=correctedDivide(a.xyz,d);
  for(uint j=0;j<4;j++) {
   eligible+=d.bounded&&all((a==0)|((abs(a)>=0x1p-32f)&(abs(a)<=0x1p32f)));
   record(a[j],b,ref[j],got[j],stats,examples);
   record(a[j],b,ref[j],pairs[j],stats,examples);
   if(j<3) record(a[j],b,ref[j],triple[j],stats,examples);
  }
 }
 atomic_fetch_add_explicit(stats+2,eligible,memory_order_relaxed);
}
kernel void validate_shared_division_edges(device const float2* pairs [[buffer(0)]],device atomic_uint* stats [[buffer(1)]],device float4* examples [[buffer(2)]],uint id [[thread_position_in_grid]]) {
 float2 pair=pairs[id];auto d=prepareDivisor(pair.y);
 float4 a=float4(pair.x,-pair.x,0.0f,-0.0f),ref=a/pair.y,got=correctedDivide(a,d);
 float4 pairs2=float4(correctedDivide(a.xy,d),correctedDivide(a.zw,d));
 float3 triple=correctedDivide(a.xyz,d);
 record(pair.x,pair.y,pair.x/pair.y,correctedDivide(pair.x,d),stats,examples);
 for(uint j=0;j<4;j++) {
  record(a[j],pair.y,ref[j],got[j],stats,examples);
  record(a[j],pair.y,ref[j],pairs2[j],stats,examples);
  if(j<3) record(a[j],pair.y,ref[j],triple[j],stats,examples);
 }
}
#endif
)MSL";

class GPU {
public:
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	FCMetal4Queue * queue;
	bool sharedDivisorEnabled = false;
	id<MTLLibrary> library;
	id<MTL4Compiler> compiler;
	id<MTLRenderPipelineState> direct, capture, mod;
	id<MTLComputePipelineState> resolve;
	id<MTLSamplerState> nearest;
	id<MTLTexture> tex0, tex1, fog, palette, stencil, depth;
	id<MTLBuffer> nodes, heads, counter, polygons, colors1;
	MetalFrameUniforms frame{};
	MetalNaomi2Uniforms naomi2{};
	Lights lights{};
	GPU(bool relaxedDivisions)
	{
		require(device != nil, "No Metal GPU available");
		require([device supportsFamily:MTLGPUFamilyMetal4], "GPU does not support Metal 4");
		queue = [[FCMetal4Queue alloc] initWithDevice:device];
		MTLCompileOptions *options = metalShaderOptions(device, relaxedDivisions);
		printf("Scoped relaxed divisions: %s\n", [(NSNumber *)options.preprocessorMacros[@"FLYCAST_METAL_RELAXED_DIVISIONS"] boolValue] ? "enabled" : "disabled");
		sharedDivisorEnabled = [(NSNumber *)options.preprocessorMacros[@"FLYCAST_METAL_SHARED_PRECISE_DIVISOR"] boolValue];
		NSError *err = nil;
		NSString *source = [[NSString stringWithUTF8String:MetalShaderSource]
			stringByAppendingString:[NSString stringWithUTF8String:FogReferenceShader]];
		source = [source stringByAppendingString:[NSString stringWithUTF8String:DivisorReferenceShader]];
		MTL4CompilerDescriptor *compilerDescriptor = [MTL4CompilerDescriptor new];
		compiler = [device newCompilerWithDescriptor:compilerDescriptor error:&err]; error(err);
		MTL4LibraryDescriptor *libraryDescriptor = [MTL4LibraryDescriptor new];
		libraryDescriptor.source = source; libraryDescriptor.options = options;
		library = [compiler newLibraryWithDescriptor:libraryDescriptor error:&err]; error(err);
		MTLVertexDescriptor *layout = [MTLVertexDescriptor vertexDescriptor];
		const MTLVertexFormat formats[] = { MTLVertexFormatFloat3, MTLVertexFormatUChar4Normalized, MTLVertexFormatUChar4Normalized, MTLVertexFormatFloat2,
			MTLVertexFormatUChar4Normalized, MTLVertexFormatUChar4Normalized, MTLVertexFormatFloat2, MTLVertexFormatFloat3 };
		const size_t offsets[] = {offsetof(Vertex,pos),offsetof(Vertex,base),offsetof(Vertex,offset),offsetof(Vertex,uv),
			offsetof(Vertex,base1),offsetof(Vertex,offset1),offsetof(Vertex,uv1),offsetof(Vertex,normal)};
		for (int i = 0; i < 8; ++i) { layout.attributes[i].format = formats[i]; layout.attributes[i].offset = offsets[i]; }
		layout.layouts[0].stride = sizeof(Vertex);
		direct = pipeline(@"pvr_vertex", @"pvr_fragment", layout, true, true);
		capture = pipeline(@"pvr_vertex", @"oit_fragment", layout, false, false);
		MTLVertexDescriptor *modLayout = [MTLVertexDescriptor vertexDescriptor];
		modLayout.attributes[0].format = MTLVertexFormatFloat3; modLayout.layouts[0].stride = sizeof(Vertex);
		mod = pipeline(@"mod_vertex", @"tr_mod_fragment", modLayout, false, false);
		setResolveLayers(32);
		// Compile every entry point, including the presentation/UI paths.
		pipeline(@"mod_vertex", @"mod_fragment", modLayout, true, false);
		pipeline(@"quad_vertex", @"quad_fragment", nil, false, true);
		pipeline(@"overlay_vertex", @"overlay_fragment", nil, false, true);
		MTLVertexDescriptor *ui = [MTLVertexDescriptor vertexDescriptor];
		ui.attributes[0].format = ui.attributes[1].format = MTLVertexFormatFloat2;
		ui.attributes[1].offset = 8; ui.attributes[2].format = MTLVertexFormatUChar4Normalized; ui.attributes[2].offset = 16; ui.layouts[0].stride = 20;
		pipeline(@"ui_vertex", @"ui_fragment", ui, false, true);
		MTLSamplerDescriptor *s = [[MTLSamplerDescriptor alloc] init];
		s.supportArgumentBuffers = YES;
		s.sAddressMode = s.tAddressMode = MTLSamplerAddressModeClampToEdge; nearest = [device newSamplerStateWithDescriptor:s];
		uint8_t blue[] = {0,0,255,128}, zero = 0;
		tex0 = texture(MTLPixelFormatRGBA8Unorm, 1, 1, blue, 4); tex1 = tex0;
		fog = texture(MTLPixelFormatR8Unorm, 1, 1, &zero, 1);
		std::array<uint8_t,4096> pal{}; pal[2*4+1] = 255; pal[2*4+3] = 255;
		palette = texture(MTLPixelFormatRGBA8Unorm, 1024, 1, pal.data(), 4096);
		stencil = texture(MTLPixelFormatR8Uint, 1, 1, &zero, 1);
		depth = texture(MTLPixelFormatDepth32Float_Stencil8);
		nodes = [device newBufferWithLength:sizeof(MetalOITNode) * 64 options:MTLResourceStorageModeShared];
		polygons = [device newBufferWithLength:sizeof(MetalOITPoly) * 2 options:MTLResourceStorageModeShared];
		colors1 = [device newBufferWithLength:sizeof(u32) * 64 options:MTLResourceStorageModeShared];
		heads = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
		counter = [device newBufferWithLength:8 options:MTLResourceStorageModeShared];
		frame.ndc = glm::mat4(1); frame.clampMax = glm::vec4(1); frame.dither = glm::vec4(2,2,2,1);
		frame.fogDensity = 1; frame.shadeScale = .25f; frame.width = frame.height = 1; frame.capacity = 64; frame.maxLayers = 32;
		printf("Metal 4 GPU: %s\n", device.name.UTF8String);
	}
	void validateFogCoordinates()
	{
		NSError *err = nil;
		auto state = compute(@"validate_fog_coordinates"); error(err);
		auto mismatches = [device newBufferWithLength:sizeof(u32) options:MTLResourceStorageModeShared];
		*(u32 *)mismatches.contents = 0;
		for (u32 base : {0x3f800000u, 0x40000000u, 0x40800000u, 0x41000000u,
			0x41800000u, 0x42000000u, 0x42800000u, 0x43000000u,
			0x00000000u, 0x80000000u, 0x7f000000u, 0x7f800000u}) {
			auto command = [queue commandBuffer];
			auto encoder = [command computeCommandEncoder];
			[encoder setComputePipelineState:state]; [encoder setBytes:&base length:sizeof(base) atIndex:0];
			[encoder setBuffer:mismatches offset:0 atIndex:1];
			[encoder dispatchThreads:MTLSizeMake(0x800000,1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
			[encoder endEncoding]; [command commit]; [command waitUntilCompleted]; error(command.error);
		}
		require(*(u32 *)mismatches.contents == 0, "Fog lookup differs from the reference");
		printf("PASS fog coordinates: exhaustive clamped range and special-value bands\n");
	}
	void validateFogBitCoordinates()
	{
		NSError *err = nil;
		auto state = compute(@"validate_fog_bit_coordinates"); error(err);
		auto mismatches = [device newBufferWithLength:sizeof(u32) options:MTLResourceStorageModeShared];
		for (uint64_t first = 0; first < (uint64_t(1) << 32); first += 0x800000) {
			@autoreleasepool {
				// Check each band separately so the count cannot wrap to zero.
				*(u32 *)mismatches.contents = 0;
				u32 base = u32(first);
				auto command = [queue commandBuffer];
				auto encoder = [command computeCommandEncoder];
				[encoder setComputePipelineState:state]; [encoder setBytes:&base length:sizeof(base) atIndex:0];
				[encoder setBuffer:mismatches offset:0 atIndex:1];
				[encoder dispatchThreads:MTLSizeMake(0x800000,1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
				[encoder endEncoding]; [command commit]; [command waitUntilCompleted]; error(command.error);
				require(*(u32 *)mismatches.contents == 0, "Fog bit lookup differs from frexp");
			}
		}
		printf("PASS fog bit coordinates: all 2^32 raw and clamped inputs, NaNs by class\n");
	}

	void validateReciprocalCorrection()
	{
		if (!sharedDivisorEnabled) return;
		NSError *err = nil;
		auto state = compute(@"validate_reciprocal_correction"); error(err);
		auto errors = [device newBufferWithLength:4 options:MTLResourceStorageModeShared];
		for (u32 chunk = 0; chunk < 256; ++chunk) {
			u32 base = chunk << 24; *(u32 *)errors.contents = 0;
			auto command = [queue commandBuffer]; auto encoder = [command computeCommandEncoder];
			[encoder setComputePipelineState:state]; [encoder setBuffer:errors offset:0 atIndex:0];
			[encoder setBytes:&base length:sizeof(base) atIndex:1];
			[encoder dispatchThreads:MTLSizeMake(1u << 24,1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
			[encoder endEncoding]; [command commit]; [command waitUntilCompleted]; error(command.error);
			require(*(u32 *)errors.contents == 0, "Reciprocal correction changes precise bits or bounds");
		}
		printf("PASS reciprocal correction: all 4294967296 input patterns, including 1073741826 bounded denominators\n");
	}
	void validateDepthCorrection()
	{
		if (!sharedDivisorEnabled) return;
		NSError *err = nil;
		auto state = compute(@"validate_depth_correction"); error(err);
		auto stats = [device newBufferWithLength:16 options:MTLResourceStorageModeShared];
		for (u32 native : {0u, 1u << 4}) {
			memset(stats.contents, 0, 16); MetalPolyUniforms p{}; p.flags = native;
			auto command = [queue commandBuffer]; auto encoder = [command computeCommandEncoder];
			[encoder setComputePipelineState:state]; [encoder setBuffer:stats offset:0 atIndex:0];
			[encoder setBytes:&p length:sizeof(p) atIndex:1];
			[encoder dispatchThreads:MTLSizeMake(1u << 24,1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
			[encoder endEncoding]; [command commit]; [command waitUntilCompleted]; error(command.error);
			u32 *values = (u32 *)stats.contents;
			require(values[0] == 0 && values[1] == 0 && values[2] == 0, "Depth correction changes precise values or ordering");
			printf("PASS depth correction: 16777216 distributed patterns and adjacent order, native=%u\n", native ? 1 : 0);
		}
	}
	void validateSharedDivisor()
	{
		if (!sharedDivisorEnabled) return;
		NSError *err = nil;
		auto state = compute(@"validate_shared_divisions"); error(err);
		auto stats = [device newBufferWithLength:32 options:MTLResourceStorageModeShared];
		auto examples = [device newBufferWithLength:16 * 4 * sizeof(float) options:MTLResourceStorageModeShared];
		for (u32 bounded : {0u, 1u}) {
			memset(stats.contents, 0, 32);
			auto command = [queue commandBuffer]; auto encoder = [command computeCommandEncoder];
			[encoder setComputePipelineState:state]; [encoder setBuffer:stats offset:0 atIndex:0];
			[encoder setBuffer:examples offset:0 atIndex:1]; [encoder setBytes:&bounded length:sizeof(bounded) atIndex:2];
			[encoder dispatchThreads:MTLSizeMake(1u << 18,1,1) threadsPerThreadgroup:MTLSizeMake(256,1,1)];
			[encoder endEncoding]; [command commit]; [command waitUntilCompleted]; error(command.error);
			require(*(u32 *)stats.contents == 0, "Shared division differs from precise division");
			printf("PASS shared division: 67108864 input quotients, float2/3/4, bounded=%u, float4 optimized=%u\n", bounded, ((u32 *)stats.contents)[2]);
		}
		// Cartesian products include both signed zeros, subnormals, infinities,
		// NaNs and values immediately inside/outside the optimized interval.
		const u32 edge[] = {0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,
			0x2f7fffffu,0x2f800000u,0x2f800001u,0x3cf5c28fu,0x3dcccccdu,0x3e99999au,
			0x3f333333u,0x3f800000u,0x3f800001u,0x3ffffffeu,0x3fffffffu,0x40000000u,
			0x4f7fffffu,0x4f800000u,0x4f800001u,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc00000u,0xffc00000u};
		constexpr NSUInteger n = sizeof(edge) / sizeof(edge[0]);
		auto pairs = [device newBufferWithLength:n * n * 2 * sizeof(u32) options:MTLResourceStorageModeShared];
		u32 *bits = (u32 *)pairs.contents;
		for (u32 a : edge) for (u32 b : edge) { *bits++ = a; *bits++ = b; }
		memset(stats.contents, 0, 32);
		state = compute(@"validate_shared_division_edges"); error(err);
		auto command = [queue commandBuffer]; auto encoder = [command computeCommandEncoder];
		[encoder setComputePipelineState:state]; [encoder setBuffer:pairs offset:0 atIndex:0];
		[encoder setBuffer:stats offset:0 atIndex:1]; [encoder setBuffer:examples offset:0 atIndex:2];
		[encoder dispatchThreads:MTLSizeMake(n*n,1,1) threadsPerThreadgroup:MTLSizeMake(128,1,1)];
		[encoder endEncoding]; [command commit]; [command waitUntilCompleted]; error(command.error);
		require(*(u32 *)stats.contents == 0, "Shared division changes an edge case");
		printf("PASS shared division: %lu edge pairs and mixed float2/3/4 vectors including signed zeros and nonfinite inputs\n", (unsigned long)(n*n));
	}
	MTL4FunctionDescriptor *function(NSString *name, MTLFunctionConstantValues *values = nil)
	{
		MTL4LibraryFunctionDescriptor *base = [MTL4LibraryFunctionDescriptor new];
		base.library = library; base.name = name;
		if (!values) return base;
		MTL4SpecializedFunctionDescriptor *specialized = [MTL4SpecializedFunctionDescriptor new];
		specialized.functionDescriptor = base; specialized.constantValues = values;
		return specialized;
	}
	id<MTLComputePipelineState> compute(NSString *name)
	{
		MTL4ComputePipelineDescriptor *descriptor = [MTL4ComputePipelineDescriptor new];
		descriptor.computeFunctionDescriptor = function(name);
		NSError *err = nil;
		auto state = [compiler newComputePipelineStateWithDescriptor:descriptor compilerTaskOptions:nil error:&err];
		error(err); require(state != nil, "Metal 4 compute pipeline missing"); return state;
	}
	void setResolveLayers(u32 layers)
	{
		NSError *err = nil;
		resolve = compute([NSString stringWithFormat:@"oit_resolve_%u", layers]);
	}
	id<MTLRenderPipelineState> pipeline(NSString *vs, NSString *fs, MTLVertexDescriptor *layout, bool depth, bool color, const MetalPolyUniforms* material = nullptr)
	{
		MTL4RenderPipelineDescriptor *p = [MTL4RenderPipelineDescriptor new];
        if (material) {
            auto values = [[MTLFunctionConstantValues alloc] init]; bool sequence = false;
            [values setConstantValue:&material->flags type:MTLDataTypeUInt atIndex:0];
            [values setConstantValue:&material->tsp0 type:MTLDataTypeUInt atIndex:1];
            [values setConstantValue:&material->tsp1 type:MTLDataTypeUInt atIndex:2];
            [values setConstantValue:&sequence type:MTLDataTypeBool atIndex:3];
            NSError* err = nil;
            auto vertexValues = [values copy];
            if ([vs isEqualToString:@"pvr_material_vertex"] || [vs isEqualToString:@"pvr_dc_material_vertex"]
                    || [vs isEqualToString:@"pvr_dc_bounded_vertex"]) {
                const u32 flags = metalVertexFunctionFlags(material->flags, material->tsp0, material->tsp1), zero = 0;
                [vertexValues setConstantValue:&flags type:MTLDataTypeUInt atIndex:0];
                [vertexValues setConstantValue:&zero type:MTLDataTypeUInt atIndex:1];
                [vertexValues setConstantValue:&zero type:MTLDataTypeUInt atIndex:2];
            }
            p.vertexFunctionDescriptor = function(vs, vertexValues);
            const u32 fragmentFlags = material->flags & MetalFragmentMaterialFlags;
            [values setConstantValue:&fragmentFlags type:MTLDataTypeUInt atIndex:0];
            p.fragmentFunctionDescriptor = function(fs, values);
        } else { p.vertexFunctionDescriptor = function(vs); p.fragmentFunctionDescriptor = function(fs); }
        p.vertexDescriptor = layout;
		p.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm; p.colorAttachments[0].writeMask = color ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
		(void)depth; // Depth/stencil formats are supplied by the Metal 4 render pass.
		NSError *err = nil; auto result = [compiler newRenderPipelineStateWithDescriptor:p compilerTaskOptions:nil error:&err]; error(err); return result;
	}
	id<MTLTexture> texture(MTLPixelFormat format, int width = 1, int height = 1, const void *data = nullptr, int row = 0)
	{
		auto d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:width height:height mipmapped:NO];
		d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsagePixelFormatView;
		if (format == MTLPixelFormatDepth32Float_Stencil8) d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsagePixelFormatView;
		d.storageMode = data ? MTLStorageModeShared : MTLStorageModePrivate;
		auto t = [device newTextureWithDescriptor:d]; require(t != nil, "Texture allocation failed");
		if (data) [t replaceRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0 withBytes:data bytesPerRow:row];
		return t;
	}
	std::array<Vertex,3> triangle(std::array<uint8_t,4> color, float z)
	{
		std::array<Vertex,3> v{};
		const float positions[3][2] = {{-1,-1},{3,-1},{-1,3}};
		for (int i = 0; i < 3; ++i) {
			v[i].pos[0] = positions[i][0]; v[i].pos[1] = positions[i][1]; v[i].pos[2] = z;
			memcpy(v[i].base, color.data(), 4); v[i].uv[0] = v[i].uv[1] = .5f;
			v[i].base1[1] = v[i].base1[3] = 255; v[i].normal[2] = 1;
		}
		return v;
	}
	void bind(FCMetal4RenderEncoder * e, const MetalPolyUniforms& p, const std::array<Vertex,3>& v)
	{
		[e setViewport:MTLViewport{0,0,1,1,0,1}];
		[e setVertexBytes:v.data() length:sizeof(v) atIndex:0];
		[e setVertexBytes:&frame length:sizeof(frame) atIndex:1]; [e setFragmentBytes:&frame length:sizeof(frame) atIndex:1];
		[e setVertexBytes:&p length:sizeof(p) atIndex:2]; [e setFragmentBytes:&p length:sizeof(p) atIndex:2];
		[e setVertexBytes:&lights length:sizeof(lights) atIndex:3];
		[e setVertexBytes:&naomi2 length:sizeof(naomi2) atIndex:4];
		[e setFragmentTexture:tex0 atIndex:0]; [e setFragmentTexture:tex1 atIndex:1]; [e setFragmentTexture:fog atIndex:2];
		[e setFragmentTexture:palette atIndex:3]; [e setFragmentTexture:depth atIndex:4]; [e setFragmentTexture:stencil atIndex:5];
		[e setFragmentSamplerState:nearest atIndex:0]; [e setFragmentSamplerState:nearest atIndex:1];
		[e setFragmentBuffer:nodes offset:0 atIndex:4]; [e setFragmentBuffer:heads offset:0 atIndex:5]; [e setFragmentBuffer:counter offset:0 atIndex:6];
		[e setFragmentBuffer:polygons offset:0 atIndex:7];
		[e setFragmentBuffer:colors1 offset:0 atIndex:8];
	}
	MTL4RenderPassDescriptor *pass(id<MTLTexture> output, bool useDepth, float clearDepth = 0)
	{
		auto p = [MTL4RenderPassDescriptor new];
		p.colorAttachments[0].texture = output; p.colorAttachments[0].loadAction = MTLLoadActionClear; p.colorAttachments[0].storeAction = MTLStoreActionStore;
		p.colorAttachments[0].clearColor = MTLClearColorMake(0,0,0,1);
		if (useDepth) {
			p.depthAttachment.texture = p.stencilAttachment.texture = depth;
			p.depthAttachment.loadAction = p.stencilAttachment.loadAction = MTLLoadActionClear;
			p.depthAttachment.storeAction = p.stencilAttachment.storeAction = MTLStoreActionStore;
			p.depthAttachment.clearDepth = clearDepth;
		}
		return p;
	}
	std::array<uint8_t,4> read(id<MTLTexture> output, FCMetal4Commands * cb)
	{
		auto buffer = [device newBufferWithLength:256 options:MTLResourceStorageModeShared];
		auto b = [cb transferEncoder];
		[b copyFromTexture:output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(1,1,1)
			toBuffer:buffer destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:256];
		[b endEncoding]; [cb commit]; [cb waitUntilCompleted]; error(cb.error);
		std::array<uint8_t,4> result; memcpy(result.data(), buffer.contents, 4); return result;
	}
	std::array<uint8_t,4> draw(MetalPolyUniforms p, std::array<Vertex,3> v, float clearDepth = 0)
	{
		auto output = texture(MTLPixelFormatRGBA8Unorm); auto cb = [queue commandBuffer];
		auto e = [cb renderCommandEncoderWithDescriptor:pass(output,true,clearDepth)];
		[e setRenderPipelineState:direct]; bind(e,p,v);
		auto d = [[MTLDepthStencilDescriptor alloc] init]; d.depthCompareFunction = MTLCompareFunctionGreater; d.depthWriteEnabled = YES;
		[e setDepthStencilState:[device newDepthStencilStateWithDescriptor:d]];
		[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [e endEncoding]; return read(output,cb);
	}
	std::array<uint8_t,4> oit(MetalPolyUniforms back, MetalPolyUniforms front, bool shadow = false, u32 expectedStatus = 0, bool equalDepth = false)
	{
		back.polyIndex = 0; front.polyIndex = 1;
		auto table = static_cast<MetalOITPoly *>(polygons.contents);
		table[0] = {back.tsp0 | u32(shadow) | (u32((back.flags & (1 << 6)) != 0) << 1), back.tsp1};
		table[1] = {front.tsp0 | u32(shadow) | (u32((front.flags & (1 << 6)) != 0) << 1), front.tsp1};
		memset(counter.contents,0,8); *(u32 *)heads.contents = ~0u;
		auto output = texture(MTLPixelFormatRGBA8Unorm), final = texture(MTLPixelFormatRGBA8Unorm); auto cb = [queue commandBuffer];
		// Establish the opaque depth buffer independently of the capture pass.
		auto e = [cb renderCommandEncoderWithDescriptor:pass(output,true)]; [e endEncoding];
		auto p = pass(output,false); p.colorAttachments[0].loadAction = MTLLoadActionLoad;
		e = [cb renderCommandEncoderWithDescriptor:p]; [e setRenderPipelineState:capture];
		bind(e,front,triangle({0,0,255,128},2)); [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
		bind(e,back,triangle({255,0,0,128},equalDepth ? 2 : 1)); [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [e endEncoding];
		if (shadow) for (u32 mode : {1u,2u}) {
			e = [cb renderCommandEncoderWithDescriptor:p]; [e setRenderPipelineState:mod];
			MetalPolyUniforms mv{}; mv.modMode = mode;
			bind(e,mv,triangle({0,0,0,0},3)); [e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [e endEncoding];
		}
		auto c = [cb computeCommandEncoder]; [c setComputePipelineState:resolve];
		[c setBytes:&frame length:sizeof(frame) atIndex:1]; [c setBuffer:nodes offset:0 atIndex:4]; [c setBuffer:heads offset:0 atIndex:5]; [c setBuffer:counter offset:0 atIndex:6];
		[c setBuffer:polygons offset:0 atIndex:7]; [c setBuffer:colors1 offset:0 atIndex:8];
		[c setTexture:output atIndex:0]; [c setTexture:final atIndex:1];
		[c dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)]; [c endEncoding];
		auto result = read(final,cb); require(((u32 *)counter.contents)[1] == expectedStatus,"Unexpected OIT limit status"); return result;
	}
	std::array<uint8_t,4> resolveFullList(u32 layers, u32 limit, u32 expectedStatus)
	{
		std::vector<MetalOITNode> list(layers);
		for (u32 i = 0; i < layers; ++i) {
			// Reverse depth order forces insertion sorting across the whole array.
			list[i].color0 = i == 0 ? 0xff0000ff : 0xffff0000;
			list[i].depth = layers - i;
			list[i].next = i + 1 < layers ? i + 1 : ~0u;
			list[i].sequence = i;
		}
		return resolveList(list, layers, limit, expectedStatus);
	}
	void validateTiledOIT()
	{
		constexpr u32 width = 17, height = 9;
		frame.width = width; frame.height = height;
		depth = texture(MTLPixelFormatDepth32Float_Stencil8, width, height);
		stencil = texture(MTLPixelFormatR8Uint, width, height);
		heads = [device newBufferWithLength:width * height * sizeof(u32) options:MTLResourceStorageModeShared];
		static std::vector<std::vector<uint8_t>> preciseReference;
		const bool collect = preciseReference.empty();
		size_t fixture = 0;
		for (bool shadow : {false, true}) for (bool volume : {false,true}) for (bool dither : {false, true}) for (bool clipped : {false,true}) for (bool textured : {false,true}) {
			std::vector<uint8_t> reference;
			for (u32 layers : {32u,2u}) for (u32 capacity : {2 * width * height, 27u}) {
				auto reset = compute([NSString stringWithFormat:@"oit_resolve_%u_reset",layers]);
				frame.capacity = capacity; frame.dithering = dither;
				nodes = [device newBufferWithLength:capacity * sizeof(MetalOITNode) options:MTLResourceStorageModeShared];
				colors1 = [device newBufferWithLength:capacity * sizeof(u32) options:MTLResourceStorageModeShared];
				memset(heads.contents, 255, heads.length); memset(counter.contents, 0, 8);
				MetalPolyUniforms back{}, front{};
				back.trilinear = front.trilinear = 1;
				back.tsp0 = front.tsp0 = (1u << 20) | (2u << 22) | (4u << 29) | (5u << 26);
				back.sequence = 1; front.sequence = 2; front.polyIndex = 1;
				if (clipped) { back.flags |= 1u << 8; back.clip = {3,1,11,7}; }
				if (textured) { back.flags |= 1; front.flags |= 1; }
				if (volume) { back.flags |= 1u << 6; front.flags |= 1u << 6; back.tsp1 = front.tsp1 = back.tsp0; }
				auto table = static_cast<MetalOITPoly *>(polygons.contents);
				table[0] = {back.tsp0 | u32(shadow) | (volume ? 2u : 0u),back.tsp1}; table[1] = {front.tsp0 | u32(shadow) | (volume ? 2u : 0u),front.tsp1};
				auto input = texture(MTLPixelFormatRGBA8Unorm,width,height), output = texture(MTLPixelFormatRGBA8Unorm,width,height);
				auto cb = [queue commandBuffer];
				auto e = [cb renderCommandEncoderWithDescriptor:pass(input,true)]; [e endEncoding];
				const auto tiles = metal_oit::plan(width,height,{{0,0,width,height},{0,0,width,height}},capacity);
				for (const auto& tile : tiles) {
					frame.oitTile = {tile.x,tile.y,tile.width,tile.height};
					auto p = pass(input,false); p.colorAttachments[0].loadAction = MTLLoadActionLoad;
					e = [cb renderCommandEncoderWithDescriptor:p]; [e setRenderPipelineState:capture];
					for (bool near : {true,false}) {
						bind(e,near ? front : back,triangle(near ? std::array<uint8_t,4>{0,0,255,128} : std::array<uint8_t,4>{255,0,0,128},near ? 2 : 1));
						[e setViewport:MTLViewport{0,0,width,height,0,1}];
						[e setScissorRect:MTLScissorRect{tile.x,tile.y,tile.width,tile.height}];
						[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
					}
					[e endEncoding];
					if (shadow) for (u32 mode : {1u,2u}) {
						e = [cb renderCommandEncoderWithDescriptor:p]; [e setRenderPipelineState:mod];
						MetalPolyUniforms mv{}; mv.modMode = mode;
						bind(e,mv,triangle({0,0,0,0},3));
						[e setViewport:MTLViewport{0,0,width,height,0,1}];
						[e setScissorRect:MTLScissorRect{tile.x,tile.y,tile.width,tile.height}];
						[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3]; [e endEncoding];
					}
					auto c = [cb computeCommandEncoder]; [c setComputePipelineState:reset];
					auto tiledFrame = frame; tiledFrame.oitTile = {tile.x,tile.y,tile.width,tile.height};
					[c setBytes:&tiledFrame length:sizeof(tiledFrame) atIndex:1];
					[c setBuffer:nodes offset:0 atIndex:4]; [c setBuffer:heads offset:0 atIndex:5]; [c setBuffer:counter offset:0 atIndex:6];
					[c setBuffer:polygons offset:0 atIndex:7]; [c setBuffer:colors1 offset:0 atIndex:8];
					[c setTexture:input atIndex:0]; [c setTexture:output atIndex:1];
					[c dispatchThreads:MTLSizeMake(tile.width,tile.height,1) threadsPerThreadgroup:MTLSizeMake(4,2,1)]; [c endEncoding];
				}
				auto readback = [device newBufferWithLength:height * 256 options:MTLResourceStorageModeShared];
				auto b = [cb transferEncoder];
				[b copyFromTexture:output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(width,height,1)
					toBuffer:readback destinationOffset:0 destinationBytesPerRow:256 destinationBytesPerImage:height*256];
				[b endEncoding]; [cb commit]; [cb waitUntilCompleted]; error(cb.error);
				require(((u32 *)counter.contents)[0] == 0 && ((u32 *)counter.contents)[1] == 0,"Tiled OIT allocator/diagnostics were not reset");
				for (u32 i = 0; i < width * height; ++i) require(((u32 *)heads.contents)[i] == ~0u,"Tiled OIT left a stale head");
				std::vector<uint8_t> pixels(width*height*4);
				for (u32 y = 0; y < height; ++y) memcpy(pixels.data()+y*width*4,(uint8_t *)readback.contents+y*256,width*4);
				if (reference.empty()) {
					reference = pixels;
					if (collect) preciseReference.push_back(pixels);
					else require(preciseReference.at(fixture) == pixels,"Relaxed divisions change precise-reference pixels");
				}
				else require(reference == pixels,"Tiled OIT changes blending, modifier shadows, dither or tile seams");
			}
			++fixture;
		}
		frame.oitTile = {};
		printf("PASS tiled OIT: exact full-frame pixels with a 27-node buffer, 2/32-layer arrays, reset/reuse, modifiers, dithering, clipping, textures, odd tile edges\n");
	}
	std::array<uint8_t,4> resolveKeys(std::initializer_list<std::pair<float,u32>> keys)
	{
		std::vector<MetalOITNode> list;
		for (auto key : keys) {
			const u32 index = list.size();
			// Distinct red values reveal the final fragment after source-copy
			// compositing. Fully opaque colors avoid tolerance hiding an error.
			list.push_back({0xff000000u | (index + 1), key.first, index + 1, key.second, 0});
		}
		list.back().next = ~0u;
		return resolveList(list, 32, list.size(), 0);
	}
	std::array<uint8_t,4> resolveList(const std::vector<MetalOITNode>& list, u32 layers, u32 limit, u32 expectedStatus)
	{
		static_cast<MetalOITPoly *>(polygons.contents)[0] = {1u << 29, 0};
		auto source = [device newBufferWithBytes:list.data() length:list.size() * sizeof(MetalOITNode) options:MTLResourceStorageModeShared];
		setResolveLayers(layers);
		const u32 oldLimit = frame.maxLayers;
		frame.maxLayers = limit;
		*(u32 *)heads.contents = 0; memset(counter.contents, 0, 8);
		uint8_t zero[4]{};
		auto input = texture(MTLPixelFormatRGBA8Unorm, 1, 1, zero, 4), output = texture(MTLPixelFormatRGBA8Unorm);
		auto cb = [queue commandBuffer];
		auto c = [cb computeCommandEncoder];
		[c setComputePipelineState:resolve]; [c setBytes:&frame length:sizeof(frame) atIndex:1];
		[c setBuffer:source offset:0 atIndex:4]; [c setBuffer:heads offset:0 atIndex:5]; [c setBuffer:counter offset:0 atIndex:6];
		[c setBuffer:polygons offset:0 atIndex:7]; [c setBuffer:colors1 offset:0 atIndex:8];
		[c setTexture:input atIndex:0]; [c setTexture:output atIndex:1];
		[c dispatchThreads:MTLSizeMake(1,1,1) threadsPerThreadgroup:MTLSizeMake(1,1,1)]; [c endEncoding];
		auto result = read(output, cb);
		frame.maxLayers = oldLimit;
		require(((u32 *)counter.contents)[1] == expectedStatus, "Unexpected full-list OIT status");
		return result;
	}
};
void expect(const char *name, std::array<uint8_t,4> actual, std::array<uint8_t,4> expected, int tolerance = 1)
{
	for (int i = 0; i < 4; ++i) if (abs(int(actual[i]) - int(expected[i])) > tolerance) {
		printf("FAIL %s: got %u,%u,%u,%u expected %u,%u,%u,%u\n",name,actual[0],actual[1],actual[2],actual[3],expected[0],expected[1],expected[2],expected[3]);
		throw std::runtime_error(name);
	}
	printf("PASS %s\n",name);
}
}

#include "metal_naomi2_gpu_validation.inc"


// Cross-material comparisons against the original precise-divide path.
// Reference results are collected with the production optimization macros off.
void validateAlphaConsumers(GPU& g, bool optimized,
    std::vector<std::array<uint8_t,4>>& reference)
{
    auto savedDirect = g.direct; auto savedFog = g.fog; auto savedFrame = g.frame;
    auto savedTexture0 = g.tex0; auto savedTexture1 = g.tex1;
    // Fractional texture channels exercise perspective correction and RGB arithmetic.
    uint8_t mixed0[] = {43,137,211,128}, mixed1[] = {197,83,149,79};
    g.tex0 = g.texture(MTLPixelFormatRGBA8Unorm,1,1,mixed0,4);
    g.tex1 = g.texture(MTLPixelFormatRGBA8Unorm,1,1,mixed1,4);
    uint8_t fogByte = 192; g.fog = g.texture(MTLPixelFormatR8Unorm,1,1,&fogByte,1);
    g.frame.fogRam = glm::vec4(.2f,.6f,.4f,0);
    g.frame.fogVertex = glm::vec4(.7f,.1f,.3f,0); g.frame.alphaRef = .5f;
    auto vertices = g.triangle({90,140,210,128},1);
    const float depths[] = {.3f,1.1f,2.7f};
    for (unsigned i=0;i<3;++i) {
        vertices[i].pos[2]=depths[i]; vertices[i].base[3]=uint8_t(64+64*i);
        vertices[i].offset[0]=32; vertices[i].offset[1]=96; vertices[i].offset[2]=160;
        vertices[i].offset[3]=uint8_t(32+64*i);
        vertices[i].uv[0]=vertices[i].uv[1]=.5f;
        vertices[i].uv1[0]=vertices[i].uv1[1]=.5f;
        vertices[i].base1[3]=uint8_t(192-32*i); vertices[i].offset1[3]=uint8_t(160-32*i);
        vertices[i].offset1[0]=64; vertices[i].offset1[1]=128; vertices[i].offset1[2]=192;
    }
    auto layout = [MTLVertexDescriptor vertexDescriptor];
    const MTLVertexFormat formats[] = {MTLVertexFormatFloat3,MTLVertexFormatUChar4Normalized,
        MTLVertexFormatUChar4Normalized,MTLVertexFormatFloat2,MTLVertexFormatUChar4Normalized,
        MTLVertexFormatUChar4Normalized,MTLVertexFormatFloat2,MTLVertexFormatFloat3};
    const size_t offsets[] = {offsetof(Vertex,pos),offsetof(Vertex,base),offsetof(Vertex,offset),
        offsetof(Vertex,uv),offsetof(Vertex,base1),offsetof(Vertex,offset1),offsetof(Vertex,uv1),offsetof(Vertex,normal)};
    for (unsigned i=0;i<8;++i) {layout.attributes[i].format=formats[i];layout.attributes[i].offset=offsets[i];}
    layout.layouts[0].stride=sizeof(Vertex);
    size_t index=0, specializations=0;
    auto compare = [&](std::array<uint8_t,4> actual) {
        if (!optimized) reference.push_back(actual);
        else {
            require(index < reference.size(), "Missing alpha-consumer reference");
            auto expected = reference[index];
            for (unsigned c=0;c<3;++c)
                require(abs(int(actual[c])-int(expected[c]))<=1, "Alpha-consumer RGB or discard changed");
            require(actual[3]==expected[3], "Alpha-consumer output alpha changed");
        }
        ++index;
    };
    for (unsigned texture=0;texture<2;++texture)
    for (unsigned offset=0;offset<2;++offset)
    for (unsigned bump=0;bump<2;++bump)
    for (unsigned fog=0;fog<4;++fog)
    for (unsigned alpha=0;alpha<2;++alpha)
    for (unsigned instruction=0;instruction<4;++instruction) {
        MetalPolyUniforms p{}; p.trilinear=1;
        p.flags=(1u<<2)|(1u<<9)|texture|(offset<<1)|(bump<<3);
        p.tsp0=(alpha<<20)|(instruction<<6)|(fog<<22);
        g.direct=savedDirect; compare(g.draw(p,vertices));
        // Check every used/missing alpha combination in actual material variants.
        if (texture && offset && bump==(instruction&1)) {
            g.direct=g.pipeline(@"pvr_material_vertex",@"pvr_material_fragment",layout,true,true,&p);
            compare(g.draw(p,vertices)); ++specializations;
        }
        p.flags|=1u<<7; g.direct=savedDirect; compare(g.draw(p,vertices));
    }
    uint8_t shadow=0x81; [g.stencil replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&shadow bytesPerRow:1];
    for (unsigned bump=0;bump<2;++bump) {
        MetalPolyUniforms p{}; p.trilinear=1;
        p.flags=1u|2u|4u|(bump<<3)|(1u<<6)|(1u<<9)|(1u<<13);
        p.tsp0=(2u<<6)|(2u<<22); p.tsp1=(1u<<20)|(3u<<6)|(1u<<22);
        g.direct=savedDirect; compare(g.draw(p,vertices));
        g.direct=g.pipeline(@"pvr_material_vertex",@"pvr_material_fragment",layout,true,true,&p);
        compare(g.draw(p,vertices)); ++specializations;
    }
    shadow=0; [g.stencil replaceRegion:MTLRegionMake2D(0,0,1,1) mipmapLevel:0 withBytes:&shadow bytesPerRow:1];
    if (optimized) require(index==reference.size(), "Alpha-consumer fixture count differs");
    g.direct=savedDirect;g.fog=savedFog;g.frame=savedFrame;g.tex0=savedTexture0;g.tex1=savedTexture1;
    printf("PASS alpha consumers: %lu rendered fixtures, %lu material specializations, exact alpha/discard, RGB <=1 byte (%s)\n",
        (unsigned long)index,(unsigned long)specializations,optimized?"optimized":"precise reference");
}

int main(int argc, char **argv)
{
	@autoreleasepool { try {
		if (argc > 1 && strcmp(argv[1], "--oit-tiles-only") == 0) {
			for (bool relaxed : {false,true}) { GPU g(relaxed); g.validateTiledOIT(); }
			return 0;
		}
		std::vector<std::array<uint8_t,4>> preciseTexelBoundaries;
		std::vector<bool> preciseAlphaBoundaries;
        std::vector<std::array<uint8_t,4>> alphaConsumerReference;
		for (bool relaxedDivisions : {false, true}) {
		GPU g(relaxedDivisions); MetalPolyUniforms p{}; p.trilinear = 1; p.tsp0 = (1 << 20) | (2 << 22);
		g.validateFogCoordinates();
		g.validateFogBitCoordinates();
		g.validateSharedDivisor();
		g.validateDepthCorrection();
		g.validateReciprocalCorrection();
        validateAlphaConsumers(g,relaxedDivisions,alphaConsumerReference);
		// At the center sample the screen-space weights are 1/2, 1/4,
		// 1/4. Non-power-of-two reciprocal depths give perspective weights
		// .15, .275, .675 (sum 1.1), with an alpha of 104.727/255.
		auto perspective = g.triangle({255,0,0,128},1);
		perspective[0].pos[2] = .3f;
		perspective[1].pos[2] = 1.1f; perspective[1].base[0] = 0; perspective[1].base[1] = 255; perspective[1].base[3] = 192;
		perspective[2].pos[2] = 2.7f; perspective[2].base[0] = 0; perspective[2].base[2] = 255; perspective[2].base[3] = 64;
		for (u32 nativeDepth : {0u, 1u << 4}) {
			p.flags = (1 << 2) | nativeDepth;
			expect("non-power-of-two perspective colors",g.draw(p,perspective),{35,64,156,105});
			p.flags |= 1 << 7; g.frame.alphaRef = 105.f/255;
			expect("perspective alpha threshold accepts equal byte",g.draw(p,perspective),{35,64,156,255});
			g.frame.alphaRef = 106.f/255;
			expect("perspective alpha threshold rejects next byte",g.draw(p,perspective),{0,0,0,255},0);
			p.flags = 1 | (1 << 2) | nativeDepth;
			uint8_t texels[] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
			auto savedTexture = g.tex0; g.tex0 = g.texture(MTLPixelFormatRGBA8Unorm,2,2,texels,8);
			perspective[0].uv[0] = perspective[0].uv[1] = .1f;
			perspective[1].uv[0] = .9f; perspective[1].uv[1] = .1f;
			perspective[2].uv[0] = .1f; perspective[2].uv[1] = .9f;
			expect("non-power-of-two perspective texture coordinates",g.draw(p,perspective),{0,0,255,255},0);
			g.tex0 = savedTexture;
		}
		// HUD quads can land exactly on nearest-sampler texel boundaries.
		// A one-ULP UV change can select a completely different texel.
		{
			uint8_t texels[] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
			auto savedTexture = g.tex0; g.tex0 = g.texture(MTLPixelFormatRGBA8Unorm,2,2,texels,8);
			size_t index = 0;
			for (u32 smooth : {0u, 1u << 2})
			for (float z : {.03f,.1f,.3f,.7f,1.1f,1.3f,2.7f,3.1f,7.1f,13.3f,31.7f,63.3f,127.9f,255.9f,1000.3f,.00001f}) {
				p.flags = 1 | smooth;
				auto actual = g.draw(p,g.triangle({255,255,255,255},z));
				if (!relaxedDivisions) preciseTexelBoundaries.push_back(actual);
				else expect("nearest texel boundary matches precise UV division",actual,preciseTexelBoundaries[index],0);
				++index;
			}
			g.tex0 = savedTexture;
		}
		// Half-byte rounding must preserve the original discard decision.
		p.flags = (1 << 2) | (1 << 7); g.frame.alphaRef = 1.f / 255;
		size_t alphaIndex = 0;
		for (float z : {.03f,.1f,.3f,.7f,1.1f,1.3f,2.7f,3.1f,7.1f,13.3f,31.7f,63.3f,127.9f,255.9f,1000.3f,.00001f}) {
			auto v = g.triangle({255,255,255,0},z); v[2].base[3] = 2;
			bool accepted = g.draw(p,v)[0] > 128;
			if (!relaxedDivisions) preciseAlphaBoundaries.push_back(accepted);
			else {
				require(accepted == preciseAlphaBoundaries[alphaIndex], "alpha half-byte decision changed");
				printf("PASS alpha half-byte discard matches precise division\n");
			}
			++alphaIndex;
		}
		p.flags = 0; g.frame.alphaRef = 0;
		auto red = g.triangle({255,0,0,128},1);
		expect("untextured alpha",g.draw(p,red),{255,0,0,128});
		p.flags = 1;
		const std::array<uint8_t,4> expected[] = {{0,0,255,128},{0,0,0,128},{127,0,128,128},{0,0,0,64}};
		for (u32 instruction = 0; instruction < 4; ++instruction) { p.tsp0 = (1 << 20) | (2 << 22) | (instruction << 6); expect("texture instruction",g.draw(p,red),expected[instruction]); }
		p.tsp0 = (1 << 20) | (2 << 22); p.flags = 1 | (1 << 7); g.frame.alphaRef = .75f;
		expect("alpha rejection",g.draw(p,red),{0,0,0,255}); g.frame.alphaRef = .5f;
		expect("alpha acceptance",g.draw(p,red),{0,0,255,255});
		p.flags = 1 | (1 << 8); p.clip = glm::vec4(0,0,1,1);
		expect("inside tile clip",g.draw(p,red),{0,0,0,255});
		uint8_t index = 2; auto original = g.tex0; g.tex0 = g.texture(MTLPixelFormatR8Unorm,1,1,&index,1);
		for (u32 mode : {1u,2u}) { p.flags = 1 | (mode << 16); expect("GPU palette",g.draw(p,red),{0,255,0,255}); } g.tex0 = original;
		p.flags = 0; expect("depth rejects far surface",g.draw(p,red,.5f),{0,0,0,255});
		expect("log depth accepts near surface",g.draw(p,g.triangle({255,0,0,128},2),.5f),{255,0,0,128});
		p.flags = 1 << 4; expect("native depth interpolation",g.draw(p,g.triangle({255,0,0,128},2),.5f),{255,0,0,128});
		p.flags = 1 << 5; g.naomi2.mv = g.naomi2.normal = g.naomi2.projection = glm::mat4(1); g.naomi2.constantColor = glm::ivec2(1);
		expect("Naomi2 constant material",g.draw(p,red),{255,0,0,128});
		g.naomi2.constantColor = glm::ivec2(0); g.lights.count = 1; g.lights.lights[0].parallel = 1; g.lights.lights[0].direction[2] = 1;
		g.lights.lights[0].color[0] = .25f; g.lights.lights[0].diffuse[0] = 1;
		expect("Naomi2 parallel diffuse light",g.draw(p,red),{128,0,0,128});
		validateNaomi2(g);
		p = {}; p.trilinear = 1; p.tsp0 = (1 << 20) | (2 << 22) | (4u << 29) | (5u << 26); p.sequence = 1;
		auto front = p; front.sequence = 2;
		expect("OIT depth sort and alpha blending",g.oit(p,front),{64,0,128,160});
		p.tsp0 = (1 << 20) | (2 << 22) | (1u << 29) | (1 << 24);
		front.tsp0 = (1 << 20) | (2 << 22) | (1u << 29) | (1 << 25);
		expect("OIT secondary accumulation",g.oit(p,front),{255,0,0,128});
		p.flags = front.flags = 0;
		p.tsp0 = front.tsp0 = (1 << 20) | (2 << 22) | (1u << 29); p.sequence = 1; front.sequence = 2;
		expect("translucent modifier shadow",g.oit(p,front,true),{0,0,64,128});
		p.flags = front.flags = 1 << 6; p.tsp1 = front.tsp1 = p.tsp0;
		expect("two-volume modifier selection",g.oit(p,front,true),{0,255,0,255});
		p.flags = front.flags = 0;
		const std::array<uint8_t,4> sourceFactors[] = {
			{0,0,0,0}, {0,0,255,128}, {0,0,0,64}, {0,0,255,64},
			{0,0,128,64}, {0,0,127,64}, {0,0,128,64}, {0,0,127,64}
		};
		const std::array<uint8_t,4> destinationFactors[] = {
			{0,0,0,0}, {255,0,0,128}, {0,0,0,64}, {255,0,0,64},
			{128,0,0,64}, {127,0,0,64}, {128,0,0,64}, {127,0,0,64}
		};
		for (u32 mode = 0; mode < 8; ++mode) {
			p.tsp0 = (1 << 20) | (2 << 22) | (1u << 29);
			front.tsp0 = (1 << 20) | (2 << 22) | (mode << 29);
			expect("OIT source blend factor",g.oit(p,front),sourceFactors[mode]);
			front.tsp0 = (1 << 20) | (2 << 22) | (mode << 26);
			expect("OIT destination blend factor",g.oit(p,front),destinationFactors[mode]);
		}
		p.tsp0 = front.tsp0 = (1 << 20) | (2 << 22) | (1u << 29);
		p.sequence = 2; front.sequence = 1;
		expect("OIT equal-depth polygon order",g.oit(p,front,false,0,true),{255,0,0,128});
		g.frame.capacity = 1;
		expect("OIT capacity overflow diagnostic",g.oit(p,front,false,1),{0,0,255,128});
		g.frame.capacity = 64; g.frame.maxLayers = 1;
		expect("OIT layer overflow diagnostic",g.oit(p,front,false,2),{255,0,0,128});
		g.frame.maxLayers = 256;
		g.setResolveLayers(256);
		expect("OIT 256-layer configuration",g.oit(p,front),{0,0,255,128});
		for (u32 layers : {1u,2u,4u,8u,16u,32u,64u,128u,256u})
			expect("OIT full specialized sorting list",g.resolveFullList(layers,layers,0),{255,0,0,255});
		expect("OIT non-power-of-two layer cutoff",g.resolveFullList(32,17,2),{255,0,0,255});
		const float nan = std::numeric_limits<float>::quiet_NaN();
		const float inf = std::numeric_limits<float>::infinity();
		expect("OIT middle NaN barrier",g.resolveKeys({{3,0},{nan,0},{1,0},{2,0}}),{4,0,0,255},0);
		expect("OIT leading NaN barrier",g.resolveKeys({{nan,0},{3,0},{1,0},{2,0}}),{2,0,0,255},0);
		expect("OIT trailing NaN barrier",g.resolveKeys({{3,0},{1,0},{nan,0},{2,0}}),{4,0,0,255},0);
		expect("OIT infinity across NaN barrier",g.resolveKeys({{inf,0},{nan,0},{-inf,0},{0,0}}),{4,0,0,255},0);
		expect("OIT signed-zero sequence order",g.resolveKeys({{-0.f,2},{0.f,1}}),{1,0,0,255},0);
		expect("OIT consecutive NaN barriers",g.resolveKeys({{nan,3},{nan,1},{1,2}}),{3,0,0,255},0);
		expect("OIT full unsigned sequence",g.resolveKeys({{1,~0u},{1,1},{1,0x7fffffffu}}),{1,0,0,255},0);
		expect("OIT stable identical keys",g.resolveKeys({{1,0},{1,0}}),{2,0,0,255},0);
		uint8_t denseFog = 255; g.fog = g.texture(MTLPixelFormatR8Unorm,1,1,&denseFog,1);
		g.frame.fogRam = glm::vec4(0,1,0,0); g.frame.fogVertex = glm::vec4(0,0,1,0);
		p = {}; p.trilinear = 1; p.flags = 1 << 9; p.tsp0 = 1 << 20;
		expect("fog LUT color",g.draw(p,red),{0,255,0,128});
		p.tsp0 |= 3 << 22;
		expect("fog LUT replacement",g.draw(p,red),{0,255,0,255});
		p.flags |= 1 << 1; p.tsp0 = (1 << 20) | (1 << 22);
		for (auto& v : red) v.offset[3] = 255;
		expect("per-vertex fog",g.draw(p,red),{0,0,255,128});
		}
		printf("All ROM-free Metal GPU checks passed.\n"); return 0;
	} catch (const std::exception& e) { fprintf(stderr,"%s\n",e.what()); return 1; } }
}
