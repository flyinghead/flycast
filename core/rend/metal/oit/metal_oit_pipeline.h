/*
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
// Metal OIT pipeline state manager. Port of core/rend/vulkan/oit/oit_pipeline.
//
// Three pipeline types beyond the regular MetalPipelineManager:
//   1. Collect - fs_main(oitMode=true), color write masked off
//   2. Resolve - linked-list walk + per-poly blend (metal_oit_shaders.h)
//   3. TR modvol - linked-list shadow tagging (oit_tr_modvol_fragment)
#pragma once
#include "../metal_apple.h"

#include "types.h"
#include "../metal_context.h"
#include "../metal_pipeline.h"
#include "../metal_shaders.h"
#include "metal_oit_buffer.h"
#include "metal_oit_shaders.h"
#include "hw/pvr/ta_ctx.h"

#include <unordered_map>

/// Whether the OIT render pass keeps the opaque + punch-through depth in tile
/// memory (colour attachment 1, R32Float, memoryless) for the collect's manual
/// depth test, instead of ending the encoder to blit the depth attachment into
/// a sampled copy. Needs programmable blending (a [[color(n)]] fragment input)
/// and memoryless render targets, both Apple2+ in the Metal Feature Set Tables
/// (Apple silicon Macs report the Apple families too; Intel/AMD Macs don't).
/// The Simulator's Metal can't build pipelines that read a render target
/// ("reading from a rendertarget is not supported"), so it takes the copy path.
inline bool MetalOITTileDepthSupported(id<MTLDevice> device)
{
#if TARGET_OS_SIMULATOR
	(void)device;
	return false;
#else
	if (device == nil)
		return false;
	if (@available(macOS 11.0, iOS 13.0, tvOS 13.0, *))
		return [device supportsFamily:MTLGPUFamilyApple2];
	return false;
#endif
}

/// How the translucent modifier-volume draws order their a-buffer accesses.
/// Each volume's XOR/OR marks must land in a pixel's records before the
/// Inclusion/Exclusion sum reads them, and the sum's SHADOW_ACC writes before
/// the next volume and the resolve (all in one render encoder). Vulkan puts a
/// by-region fragment->fragment pipelineBarrier at both points; the Metal
/// equivalent, a render-encoder memoryBarrierWithScope: with fragment stages,
/// is invalid on Apple GPUs ("afterStages can not contain MTLRenderStageFragment
/// on this device" from the validation layer, and no ordering without it).
enum class MetalOITFragmentOrdering
{
	/// The modvol and resolve shaders put the a-buffer in raster order group 0:
	/// overlapping fragments touch it in draw submission order, so no barrier
	/// is needed. Every access is to the shaded pixel's own list, which is
	/// exactly the overlap a raster order group orders.
	RasterOrderGroups,
	/// macOS on a non-Apple GPU (Intel/AMD) without raster order groups, where
	/// the fragment->fragment render-encoder barrier is valid.
	MemoryBarrier,
	/// Apple GPU without raster order groups (A8-A10): end the encoder and open
	/// a new one, which Metal's hazard tracking orders after the first.
	EncoderSplit,
};

inline MetalOITFragmentOrdering MetalOITFragmentOrderingFallback(id<MTLDevice> device)
{
#if TARGET_OS_OSX
	if (device != nil) {
		bool appleGpu = false;
		if (@available(macOS 10.15, *))
			appleGpu = [device supportsFamily:MTLGPUFamilyApple1];
		if (!appleGpu)
			return MetalOITFragmentOrdering::MemoryBarrier;
	}
#else
	(void)device;
#endif
	return MetalOITFragmentOrdering::EncoderSplit;
}

inline MetalOITFragmentOrdering MetalOITFragmentOrderingFor(id<MTLDevice> device)
{
	if (device != nil && device.rasterOrderGroupsSupported)
		return MetalOITFragmentOrdering::RasterOrderGroups;
	return MetalOITFragmentOrderingFallback(device);
}

class MetalOITPipelineManager
{
public:
	explicit MetalOITPipelineManager(MetalShaders *shaderManager, MetalOITBuffers *oitBuffers)
		: shaderManager(shaderManager), oitBuffers(oitBuffers),
		  tileDepth(MetalOITTileDepthSupported(MetalContext::Instance()->GetDevice())) {}

	/// See MetalOITTileDepthSupported. Fixed for the manager's lifetime; the
	/// drawer's regular pipeline manager is built with the same value.
	bool UseTileDepth() const { return tileDepth; }

	/// See MetalOITFragmentOrdering. Settled when Init() compiles the library
	/// (a failed raster-order compile falls back), so it compiles it first.
	MetalOITFragmentOrdering FragmentOrdering()
	{
		Init();
		return fragmentOrdering;
	}

	/// Compile the shared OIT MSL library and cache MTLFunction handles
	/// for collect/resolve. Cheap to call repeatedly; first call does the
	/// MTLLibrary newLibraryWithSource: compile (Metal does shader
	/// compilation lazily, so this can stall the first time it runs).
	void Init()
	{
		if (oitLibrary != nil)
			return;
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();
		if (device == nil) {
			WARN_LOG(RENDERER, "MetalOITPipelineManager::Init: no Metal device");
			return;
		}
		std::string src = GetMetalOITShaderSource();
		NSString *srcNS = [NSString stringWithUTF8String:src.c_str()];
		fragmentOrdering = MetalOITFragmentOrderingFor(device);
		auto compile = [&](bool rasterOrder, NSError **err) {
			MTLCompileOptions *options = [[MTLCompileOptions alloc] init];
			options.preprocessorMacros = @{ @"OIT_RASTER_ORDER" : @(rasterOrder ? 1 : 0) };
			return [device newLibraryWithSource:srcNS options:options error:err];
		};
		NSError *err = nil;
		const bool rasterOrder = fragmentOrdering == MetalOITFragmentOrdering::RasterOrderGroups;
		oitLibrary = compile(rasterOrder, &err);
		if (oitLibrary == nil && rasterOrder) {
			WARN_LOG(RENDERER, "MetalOITPipelineManager::Init: MSL compile with raster order groups failed (%s), retrying without",
					 err.localizedDescription.UTF8String);
			fragmentOrdering = MetalOITFragmentOrderingFallback(device);
			err = nil;
			oitLibrary = compile(false, &err);
		}
		if (oitLibrary == nil) {
			ERROR_LOG(RENDERER, "MetalOITPipelineManager::Init: MSL compile failed: %s",
					  err.localizedDescription.UTF8String);
			return;
		}
		INFO_LOG(RENDERER, "Metal OIT: modifier volume fragment ordering: %s",
				 fragmentOrdering == MetalOITFragmentOrdering::RasterOrderGroups ? "raster order groups"
				 : fragmentOrdering == MetalOITFragmentOrdering::MemoryBarrier ? "memory barrier"
				 : "encoder split");
		collectVertexFn   = nil;
		collectFragmentFn = nil;
		seedOpaqueDepthFn = [oitLibrary newFunctionWithName:@"oit_seed_opaque_depth"];
		resolveVertexFn   = [oitLibrary newFunctionWithName:@"oit_resolve_vertex"];
		resolveFragmentFn = [oitLibrary newFunctionWithName:@"oit_resolve_fragment"];
		if (resolveVertexFn == nil || resolveFragmentFn == nil) {
			ERROR_LOG(RENDERER, "MetalOITPipelineManager::Init: OIT resolve shader entry points missing");
		}
	}

	/// Drop all cached pipelines. Called from Term() and when shader
	/// reload invalidates them.
	void term()
	{
		collectPipelines.clear();
		trDepthPipelines.clear();
		resolvePipeline = nil;
		seedOpaqueDepthPipeline = nil;
		twoVolumeCollectDepthStencil[0] = twoVolumeCollectDepthStencil[1] = nil;
		seedOpaqueDepthFn = nil;
		trModVolPipelines.clear();
		trModVolFragmentFns.clear();
		trModVolDepthStencilState = nil;
		collectVertexFn = nil;
		collectFragmentFn = nil;
		resolveVertexFn = nil;
		resolveFragmentFn = nil;
		oitLibrary = nil;
	}

	/// Collect-pass pipeline for translucent geometry. Cached by the
	/// same polygon-state hash MetalPipelineManager uses, so identical
	/// poly setups share one pipeline state.
	///
	/// area: 0 = regular collect; 1 / 2 = a two-volume poly's second / first
	/// parameter set, stencil-gated inside / outside the modifier-volume mark.
	/// Areas 1 and 2 use the [[early_fragment_tests]] fragment entry point and
	/// must be drawn with GetTwoVolumeCollectDepthStencilState(area).
	id<MTLRenderPipelineState> GetCollectPipeline(u32 listType, bool sortTriangles,
												  const PolyParam &pp, int gpuPalette,
												  bool dithering, int area = 0)
	{
		u64 pipehash = hashCollect(listType, sortTriangles, &pp, gpuPalette, dithering, area);
		const auto it = collectPipelines.find(pipehash);
		if (it != collectPipelines.end() && it->second != nil)
			return it->second;
		CreateCollectPipeline(listType, sortTriangles, pp, gpuPalette, dithering, area);
		return collectPipelines[pipehash];
	}

	/// Depth/stencil state for the two-volume collect draws (area 1 or 2).
	/// Their fragment function runs after early depth/stencil tests, so the
	/// stencil area test rejects fragments before the a-buffer append. The
	/// depth compare is Always: the rasterized position.z isn't the log depth
	/// fs_main computes, and the collect tests depth in the shader.
	id<MTLDepthStencilState> GetTwoVolumeCollectDepthStencilState(int area)
	{
		if (area != 1 && area != 2)
			return nil;
		if (twoVolumeCollectDepthStencil[area - 1] != nil)
			return twoVolumeCollectDepthStencil[area - 1];
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();
		if (device == nil)
			return nil;
		MTLStencilDescriptor *stencil = [[MTLStencilDescriptor alloc] init];
		stencil.stencilCompareFunction    = area == 1 ? MTLCompareFunctionEqual
													  : MTLCompareFunctionNotEqual;
		stencil.stencilFailureOperation   = MTLStencilOperationKeep;
		stencil.depthFailureOperation     = MTLStencilOperationKeep;
		stencil.depthStencilPassOperation = MTLStencilOperationKeep;
		stencil.readMask  = 0x81;
		stencil.writeMask = 0;
		MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
		desc.label                = area == 1 ? @"OIT two-volume collect (inside)"
											  : @"OIT two-volume collect (outside)";
		desc.depthCompareFunction = MTLCompareFunctionAlways;
		desc.depthWriteEnabled    = NO;
		desc.frontFaceStencil     = stencil;
		desc.backFaceStencil      = stencil;
		twoVolumeCollectDepthStencil[area - 1] = [device newDepthStencilStateWithDescriptor:desc];
		return twoVolumeCollectDepthStencil[area - 1];
	}

	/// Tile depth only: full-screen draw writing colour attachment 1 from a
	/// depth copy bound at fragment texture 4 (render passes after the first).
	id<MTLRenderPipelineState> GetSeedOpaqueDepthPipeline()
	{
		if (seedOpaqueDepthPipeline != nil || !tileDepth)
			return seedOpaqueDepthPipeline;
		if (oitLibrary == nil)
			Init();
		if (resolveVertexFn == nil || seedOpaqueDepthFn == nil)
			return nil;
		MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
		desc.label                               = @"OIT seed opaque depth";
		desc.vertexFunction                      = resolveVertexFn;
		desc.fragmentFunction                    = seedOpaqueDepthFn;
		desc.rasterSampleCount                   = 1;
		desc.colorAttachments[0].pixelFormat     = MTLPixelFormatBGRA8Unorm;
		desc.colorAttachments[0].writeMask       = MTLColorWriteMaskNone;
		desc.colorAttachments[1].pixelFormat     = MTLPixelFormatR32Float;
		desc.colorAttachments[1].writeMask       = MTLColorWriteMaskRed;
		desc.depthAttachmentPixelFormat          = MTLPixelFormatDepth32Float_Stencil8;
		desc.stencilAttachmentPixelFormat        = MTLPixelFormatDepth32Float_Stencil8;
		NSError *err = nil;
		seedOpaqueDepthPipeline = [MetalContext::Instance()->GetDevice()
			newRenderPipelineStateWithDescriptor:desc error:&err];
		if (seedOpaqueDepthPipeline == nil)
			ERROR_LOG(RENDERER, "GetSeedOpaqueDepthPipeline failed: %s",
					  err.localizedDescription.UTF8String);
		return seedOpaqueDepthPipeline;
	}

	/// Final / resolve pipeline. Single state: unlike Vulkan's GetFinalPipeline
	/// it has no sortTriangles parameter.
	id<MTLRenderPipelineState> GetResolvePipeline()
	{
		if (resolvePipeline != nil)
			return resolvePipeline;
		CreateResolvePipeline();
		return resolvePipeline;
	}

	/// Translucent depth-only (color write masked off). Used after resolve
	/// on non-final render passes — Vulkan Pass::Depth TR slice.
	id<MTLRenderPipelineState> GetTranslucentDepthPipeline(u32 listType, bool sortTriangles,
														   const PolyParam &pp, int gpuPalette,
														   bool dithering)
	{
		u64 pipehash = hashCollect(listType, sortTriangles, &pp, gpuPalette, dithering);
		const auto it = trDepthPipelines.find(pipehash);
		if (it != trDepthPipelines.end() && it->second != nil)
			return it->second;
		CreateTranslucentDepthPipeline(listType, sortTriangles, pp, gpuPalette, dithering);
		return trDepthPipelines[pipehash];
	}

	/// Translucent modifier-volume pipeline.
	id<MTLRenderPipelineState> GetTrModVolPipeline(ModVolMode mode, int cullMode, bool naomi2)
	{
		u32 pipehash = hashModVol(mode, cullMode, naomi2);
		const auto it = trModVolPipelines.find(pipehash);
		if (it != trModVolPipelines.end() && it->second != nil)
			return it->second;
		CreateTrModVolPipeline(mode, cullMode, naomi2);
		return trModVolPipelines[pipehash];
	}

	/// Depth/stencil disabled — OIT TR modvol only tags the linked list.
	id<MTLDepthStencilState> GetTrModVolDepthStencilState()
	{
		if (trModVolDepthStencilState != nil)
			return trModVolDepthStencilState;
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();
		if (device == nil)
			return nil;
		MTLDepthStencilDescriptor *desc = [[MTLDepthStencilDescriptor alloc] init];
		desc.depthCompareFunction = MTLCompareFunctionAlways;
		desc.depthWriteEnabled    = NO;
		trModVolDepthStencilState = [device newDepthStencilStateWithDescriptor:desc];
		return trModVolDepthStencilState;
	}

private:
	id<MTLFunction> GetTrModVolFragmentFunction(ModVolMode mode, bool divPosZ)
	{
		u32 key = ((u32)mode << 1) | (divPosZ ? 1u : 0u);
		const auto it = trModVolFragmentFns.find(key);
		if (it != trModVolFragmentFns.end() && it->second != nil)
			return it->second;
		if (oitLibrary == nil)
			Init();
		if (oitLibrary == nil)
			return nil;
		MTLFunctionConstantValues *constants = [[MTLFunctionConstantValues alloc] init];
		int mvMode = (int)mode;
		bool div   = divPosZ;
		[constants setConstantValue:&mvMode type:MTLDataTypeInt atIndex:0];
		[constants setConstantValue:&div type:MTLDataTypeBool atIndex:1];
		NSError *err = nil;
		id<MTLFunction> fn = [oitLibrary newFunctionWithName:@"oit_tr_modvol_fragment"
											  constantValues:constants
													   error:&err];
		if (fn == nil) {
			ERROR_LOG(RENDERER, "GetTrModVolFragmentFunction failed (mode=%d): %s",
					  (int)mode, err.localizedDescription.UTF8String);
			return nil;
		}
		trModVolFragmentFns[key] = fn;
		return fn;
	}

	// ---- Pipeline creation
	//
	// Build the collect pipeline against the REAL Metal
	// vertex layout + MetalShaders' specialised vertex function, with
	// fs_main(oitMode=true) as the fragment. Writes are masked off on
	// the colour attachment; the fragment side appends each translucent
	// sample to the OIT per-pixel linked list (see metal_shaders.mm's
	// pp_oit_mode branch at the end of fs_main).
	void CreateCollectPipeline(u32 listType, bool sortTriangles,
							   const PolyParam &pp, int gpuPalette,
							   bool dithering, int area = 0)
	{
		if (shaderManager == nullptr) {
			WARN_LOG(RENDERER, "CreateCollectPipeline: no shader manager");
			return;
		}
		u64 pipehash = hashCollect(listType, sortTriangles, &pp, gpuPalette, dithering, area);
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();

		MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
		desc.label                        = @"OIT Collect";

		// Match MetalPipelineManager::CreatePipeline's vertex descriptor.
		// Layout intentionally duplicated from MetalPipelineManager::
		// GetMainVertexInputDescriptor (which is private) — keeping
		// perfect parity with the regular pipeline's Vertex layout is
		// essential or the collect VS won't interpolate correctly.
		MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];
		auto attrPos = [vertexDesc attributes][0];
		[attrPos setFormat:MTLVertexFormatFloat3];
		[attrPos setOffset:offsetof(Vertex, x)];
		[attrPos setBufferIndex:30];
		auto attrCol = [vertexDesc attributes][1];
		[attrCol setFormat:MTLVertexFormatUChar4Normalized];
		[attrCol setOffset:offsetof(Vertex, col)];
		[attrCol setBufferIndex:30];
		auto attrSpc = [vertexDesc attributes][2];
		[attrSpc setFormat:MTLVertexFormatUChar4Normalized];
		[attrSpc setOffset:offsetof(Vertex, spc)];
		[attrSpc setBufferIndex:30];
		auto attrUv = [vertexDesc attributes][3];
		[attrUv setFormat:MTLVertexFormatFloat2];
		[attrUv setOffset:offsetof(Vertex, u)];
		[attrUv setBufferIndex:30];
		auto attrCol1 = [vertexDesc attributes][4];
		[attrCol1 setFormat:MTLVertexFormatUChar4Normalized];
		[attrCol1 setOffset:offsetof(Vertex, col1)];
		[attrCol1 setBufferIndex:30];
		auto attrSpc1 = [vertexDesc attributes][5];
		[attrSpc1 setFormat:MTLVertexFormatUChar4Normalized];
		[attrSpc1 setOffset:offsetof(Vertex, spc1)];
		[attrSpc1 setBufferIndex:30];
		auto attrUv1 = [vertexDesc attributes][6];
		[attrUv1 setFormat:MTLVertexFormatFloat2];
		[attrUv1 setOffset:offsetof(Vertex, u1)];
		[attrUv1 setBufferIndex:30];
		if (pp.isNaomi2()) {
			auto attrN = [vertexDesc attributes][7];
			[attrN setFormat:MTLVertexFormatFloat3];
			[attrN setOffset:offsetof(Vertex, nx)];
			[attrN setBufferIndex:30];
		}
		auto layout = [vertexDesc layouts][30];
		[layout setStride:sizeof(Vertex)];
		[layout setStepRate:1];
		[layout setStepFunction:MTLVertexStepFunctionPerVertex];
		[desc setVertexDescriptor:vertexDesc];

		bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;

		MetalVertexShaderParams vertParams = {};
		vertParams.gouraud = pp.pcw.Gouraud == 1;
		vertParams.naomi2  = pp.isNaomi2();
		vertParams.divPosZ = divPosZ;
		vertParams.oitCollect = true;
		vertParams.area1 = area == 1;

		MetalFragmentShaderParams fragParams = {};
		fragParams.alphaTest      = listType == ListType_Punch_Through;
		fragParams.bumpmap        = pp.tcw.PixelFmt == PixelBumpMap;
		fragParams.clamping       = pp.tsp.ColorClamp;
		fragParams.insideClipTest = (pp.tileclip >> 28) == 3;
		fragParams.fog            = config::Fog ? pp.tsp.FogCtrl : 2;
		fragParams.gouraud        = pp.pcw.Gouraud;
		fragParams.ignoreTexAlpha = pp.tsp.IgnoreTexA || pp.tcw.PixelFmt == Pixel565;
		fragParams.offset         = pp.pcw.Offset;
		fragParams.shaderInstr    = pp.tsp.ShadInstr;
		fragParams.texture        = pp.pcw.Texture;
		fragParams.trilinear      = pp.pcw.Texture && pp.tsp.FilterMode > 1
								   && listType != ListType_Punch_Through
								   && pp.tcw.MipMapped == 1;
		fragParams.useAlpha       = pp.tsp.UseAlpha;
		fragParams.palette        = gpuPalette;
		fragParams.divPosZ        = divPosZ;
		fragParams.dithering      = dithering;
		fragParams.oitMode        = true;
		fragParams.oitTileDepth   = tileDepth;
		fragParams.earlyFragmentTests = area != 0;

		[desc setVertexFunction:shaderManager->GetVertexShader(vertParams)];
		[desc setFragmentFunction:shaderManager->GetFragmentShader(fragParams)];
		if (desc.vertexFunction == nil || desc.fragmentFunction == nil) {
			ERROR_LOG(RENDERER, "CreateCollectPipeline: shader compile failed");
			return;
		}

		desc.rasterSampleCount            = 1;
		// Colour attachment exists but writes are masked off — the real
		// output is the linked-list buffer write in the fragment shader's
		// pp_oit_mode branch.
		desc.colorAttachments[0].pixelFormat     = MTLPixelFormatBGRA8Unorm;
		desc.colorAttachments[0].writeMask       = MTLColorWriteMaskNone;
		desc.colorAttachments[0].blendingEnabled = NO;
		if (tileDepth) {
			// Read through [[color(1)]]; never written by the collect.
			desc.colorAttachments[1].pixelFormat     = MTLPixelFormatR32Float;
			desc.colorAttachments[1].writeMask       = MTLColorWriteMaskNone;
			desc.colorAttachments[1].blendingEnabled = NO;
		}
		desc.depthAttachmentPixelFormat   = MTLPixelFormatDepth32Float_Stencil8;
		desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

		NSError *err = nil;
		id<MTLRenderPipelineState> state =
			[device newRenderPipelineStateWithDescriptor:desc error:&err];
		if (state == nil) {
			ERROR_LOG(RENDERER, "CreateCollectPipeline failed: %s",
					  err.localizedDescription.UTF8String);
			return;
		}
		collectPipelines[pipehash] = state;
	}

	void CreateTranslucentDepthPipeline(u32 listType, bool sortTriangles,
										const PolyParam &pp, int gpuPalette,
										bool dithering)
	{
		if (shaderManager == nullptr) {
			WARN_LOG(RENDERER, "CreateTranslucentDepthPipeline: no shader manager");
			return;
		}
		u64 pipehash = hashCollect(listType, sortTriangles, &pp, gpuPalette, dithering);
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();

		MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
		desc.label = @"OIT TR Depth";

		MTLVertexDescriptor *vertexDesc = [[MTLVertexDescriptor alloc] init];
		auto attrPos = [vertexDesc attributes][0];
		[attrPos setFormat:MTLVertexFormatFloat3];
		[attrPos setOffset:offsetof(Vertex, x)];
		[attrPos setBufferIndex:30];
		auto attrCol = [vertexDesc attributes][1];
		[attrCol setFormat:MTLVertexFormatUChar4Normalized];
		[attrCol setOffset:offsetof(Vertex, col)];
		[attrCol setBufferIndex:30];
		auto attrSpc = [vertexDesc attributes][2];
		[attrSpc setFormat:MTLVertexFormatUChar4Normalized];
		[attrSpc setOffset:offsetof(Vertex, spc)];
		[attrSpc setBufferIndex:30];
		auto attrUv = [vertexDesc attributes][3];
		[attrUv setFormat:MTLVertexFormatFloat2];
		[attrUv setOffset:offsetof(Vertex, u)];
		[attrUv setBufferIndex:30];
		auto attrCol1 = [vertexDesc attributes][4];
		[attrCol1 setFormat:MTLVertexFormatUChar4Normalized];
		[attrCol1 setOffset:offsetof(Vertex, col1)];
		[attrCol1 setBufferIndex:30];
		auto attrSpc1 = [vertexDesc attributes][5];
		[attrSpc1 setFormat:MTLVertexFormatUChar4Normalized];
		[attrSpc1 setOffset:offsetof(Vertex, spc1)];
		[attrSpc1 setBufferIndex:30];
		auto attrUv1 = [vertexDesc attributes][6];
		[attrUv1 setFormat:MTLVertexFormatFloat2];
		[attrUv1 setOffset:offsetof(Vertex, u1)];
		[attrUv1 setBufferIndex:30];
		if (pp.isNaomi2()) {
			auto attrN = [vertexDesc attributes][7];
			[attrN setFormat:MTLVertexFormatFloat3];
			[attrN setOffset:offsetof(Vertex, nx)];
			[attrN setBufferIndex:30];
		}
		auto layout = [vertexDesc layouts][30];
		[layout setStride:sizeof(Vertex)];
		[layout setStepRate:1];
		[layout setStepFunction:MTLVertexStepFunctionPerVertex];
		[desc setVertexDescriptor:vertexDesc];

		bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;

		MetalVertexShaderParams vertParams = {};
		vertParams.gouraud = pp.pcw.Gouraud == 1;
		vertParams.naomi2  = pp.isNaomi2();
		vertParams.divPosZ = divPosZ;
		vertParams.oitCollect = false;

		MetalFragmentShaderParams fragParams = {};
		fragParams.alphaTest      = listType == ListType_Punch_Through;
		fragParams.bumpmap        = pp.tcw.PixelFmt == PixelBumpMap;
		fragParams.clamping       = pp.tsp.ColorClamp;
		fragParams.insideClipTest = (pp.tileclip >> 28) == 3;
		fragParams.fog            = config::Fog ? pp.tsp.FogCtrl : 2;
		fragParams.gouraud        = pp.pcw.Gouraud;
		fragParams.ignoreTexAlpha = pp.tsp.IgnoreTexA || pp.tcw.PixelFmt == Pixel565;
		fragParams.offset         = pp.pcw.Offset;
		fragParams.shaderInstr    = pp.tsp.ShadInstr;
		fragParams.texture        = pp.pcw.Texture;
		fragParams.trilinear      = pp.pcw.Texture && pp.tsp.FilterMode > 1
								   && listType != ListType_Punch_Through
								   && pp.tcw.MipMapped == 1;
		fragParams.useAlpha       = pp.tsp.UseAlpha;
		fragParams.palette        = gpuPalette;
		fragParams.divPosZ        = divPosZ;
		fragParams.dithering      = dithering;
		fragParams.oitMode        = false;

		[desc setVertexFunction:shaderManager->GetVertexShader(vertParams)];
		[desc setFragmentFunction:shaderManager->GetFragmentShader(fragParams)];
		if (desc.vertexFunction == nil || desc.fragmentFunction == nil) {
			ERROR_LOG(RENDERER, "CreateTranslucentDepthPipeline: shader compile failed");
			return;
		}

		desc.rasterSampleCount            = 1;
		desc.colorAttachments[0].pixelFormat     = MTLPixelFormatBGRA8Unorm;
		desc.colorAttachments[0].writeMask       = MTLColorWriteMaskNone;
		desc.colorAttachments[0].blendingEnabled = NO;
		desc.depthAttachmentPixelFormat   = MTLPixelFormatDepth32Float_Stencil8;
		desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

		NSError *err = nil;
		id<MTLRenderPipelineState> state =
			[device newRenderPipelineStateWithDescriptor:desc error:&err];
		if (state == nil) {
			ERROR_LOG(RENDERER, "CreateTranslucentDepthPipeline failed: %s",
					  err.localizedDescription.UTF8String);
			return;
		}
		trDepthPipelines[pipehash] = state;
	}

	void CreateResolvePipeline()
	{
		if (resolveVertexFn == nil || resolveFragmentFn == nil) {
			WARN_LOG(RENDERER, "CreateResolvePipeline: OIT shaders not compiled");
			return;
		}
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();

		MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
		desc.label                        = @"OIT Resolve";
		desc.vertexFunction               = resolveVertexFn;
		desc.fragmentFunction             = resolveFragmentFn;
		desc.rasterSampleCount            = 1;
		// Resolve writes the composited result to the color buffer. The
		// sorting + blend happens IN the shader, so the pipeline blend
		// is disabled — we want the shader's output verbatim.
		desc.colorAttachments[0].pixelFormat     = MTLPixelFormatBGRA8Unorm;
		desc.colorAttachments[0].writeMask       = MTLColorWriteMaskAll;
		desc.colorAttachments[0].blendingEnabled = NO;
		desc.depthAttachmentPixelFormat   = MTLPixelFormatDepth32Float_Stencil8;
		desc.stencilAttachmentPixelFormat = MTLPixelFormatDepth32Float_Stencil8;

		NSError *err = nil;
		resolvePipeline = [device newRenderPipelineStateWithDescriptor:desc error:&err];
		if (resolvePipeline == nil) {
			ERROR_LOG(RENDERER, "CreateResolvePipeline failed: %s",
					  err.localizedDescription.UTF8String);
		}
	}

	// OIT TR modvol walks the per-pixel linked list and atomically tags
	// seq_num shadow bits (Vulkan OITTranslucentModvolShaderSource).
	void CreateTrModVolPipeline(ModVolMode mode, int cullMode, bool naomi2)
	{
		if (shaderManager == nullptr) {
			WARN_LOG(RENDERER, "CreateTrModVolPipeline: no shader manager");
			return;
		}
		if (mode == ModVolMode::Final)
			return;
		id<MTLDevice> device = MetalContext::Instance()->GetDevice();
		MTLRenderPipelineDescriptor *desc = [[MTLRenderPipelineDescriptor alloc] init];
		[desc setLabel:@"OIT TR ModVol"];

		MTLVertexDescriptor *vd = [[MTLVertexDescriptor alloc] init];
		auto attr = [vd attributes][0];
		[attr setFormat:MTLVertexFormatFloat3];
		[attr setOffset:0];
		[attr setBufferIndex:30];
		auto layout = [vd layouts][30];
		[layout setStride:sizeof(float) * 3];
		[layout setStepRate:1];
		[layout setStepFunction:MTLVertexStepFunctionPerVertex];
		[desc setVertexDescriptor:vd];

		auto attachment = [desc colorAttachments][0];
		[attachment setBlendingEnabled:NO];
		[attachment setWriteMask:MTLColorWriteMaskNone];
		[attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

		[desc setDepthAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
		[desc setStencilAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];

		bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
		MetalModVolShaderParams shaderParams { naomi2, divPosZ };
		id<MTLFunction> fragmentFn = GetTrModVolFragmentFunction(mode, divPosZ);
		if (fragmentFn == nil)
			return;
		[desc setVertexFunction:shaderManager->GetModVolVertexShader(shaderParams)];
		[desc setFragmentFunction:fragmentFn];
		if (desc.vertexFunction == nil) {
			ERROR_LOG(RENDERER, "CreateTrModVolPipeline: vertex shader compile failed");
			return;
		}

		NSError *err = nil;
		id<MTLRenderPipelineState> state =
			[device newRenderPipelineStateWithDescriptor:desc error:&err];
		if (state == nil) {
			ERROR_LOG(RENDERER, "CreateTrModVolPipeline failed: %s",
					  err.localizedDescription.UTF8String);
			return;
		}
		trModVolPipelines[hashModVol(mode, cullMode, naomi2)] = state;
	}

	// ---- Hash helpers ----
	//
	// hashCollect folds in every PolyParam field that affects the
	// compiled fragment-shader specialisation OR the pipeline state
	// (vertex naomi2 flag, gouraud, depth/blend setup, etc.). Identical
	// PolyParam state shares one cached MTLRenderPipelineState — the
	// expensive Metal pipeline compile only runs once per unique
	// combination. Mirrors the keys MetalPipelineManager::hash uses for
	// the regular pipeline cache (see metal_pipeline.h:hash).
	u64 hashCollect(u32 listType, bool sortTriangles, const PolyParam *pp,
					int gpuPalette, bool dithering, int area = 0) const
	{
		u64 h = (u64)listType;
		h = (h << 2) | (u64)(area & 3);
		h = (h << 1) | (sortTriangles ? 1 : 0);
		h = (h << 4) | (gpuPalette & 0xF);
		h = (h << 1) | (dithering ? 1 : 0);
		if (pp != nullptr) {
			h = (h << 1) | ((u64)pp->isNaomi2());
			h = (h << 1) | ((u64)(pp->pcw.Gouraud == 1));
			h = (h << 1) | ((u64)pp->pcw.Texture);
			h = (h << 1) | ((u64)pp->pcw.Offset);
			h = (h << 1) | ((u64)pp->pcw.Shadow);
			h = (h << 3) | ((u64)(pp->tsp.ShadInstr & 0x3));
			h = (h << 3) | ((u64)(pp->tsp.FogCtrl & 0x3));
			h = (h << 1) | ((u64)pp->tsp.UseAlpha);
			h = (h << 1) | ((u64)pp->tsp.ColorClamp);
			h = (h << 1) | ((u64)pp->tsp.IgnoreTexA);
			h = (h << 2) | ((u64)(pp->tcw.PixelFmt == PixelBumpMap));
			h = (h << 2) | ((u64)(pp->tcw.PixelFmt == Pixel565));
			h = (h << 2) | ((u64)(pp->tsp.SrcInstr & 0x7));
			h = (h << 2) | ((u64)(pp->tsp.DstInstr & 0x7));
			h = (h << 2) | ((u64)(pp->isp.CullMode & 0x3));
			h = (h << 2) | ((u64)((pp->tileclip >> 28) & 0x3));
		}
		return h;
	}

	u32 hashModVol(ModVolMode mode, int cullMode, bool naomi2) const
	{
		return ((u32)mode << 16) | ((u32)cullMode << 1) | (naomi2 ? 1 : 0);
	}

	MetalShaders   *shaderManager = nullptr;
	// Vulkan's OITPipelineManager needs this to populate descriptor sets
	// (oit_pipeline.h:140). Metal has no descriptor sets — MetalOITDrawer binds
	// the A-buffer/pixel-counter buffers directly at draw time — so this is kept
	// only for structural parity with the Vulkan class the port mirrors.
	[[maybe_unused]] MetalOITBuffers *oitBuffers = nullptr;

	std::unordered_map<u64, id<MTLRenderPipelineState>> collectPipelines;
	std::unordered_map<u64, id<MTLRenderPipelineState>> trDepthPipelines;
	std::unordered_map<u32, id<MTLRenderPipelineState>> trModVolPipelines;
	std::unordered_map<u32, id<MTLFunction>> trModVolFragmentFns;
	id<MTLDepthStencilState> trModVolDepthStencilState = nil;
	id<MTLRenderPipelineState> resolvePipeline = nil;
	id<MTLRenderPipelineState> seedOpaqueDepthPipeline = nil;
	id<MTLDepthStencilState>   twoVolumeCollectDepthStencil[2] = { nil, nil };
	bool tileDepth = false;
	MetalOITFragmentOrdering fragmentOrdering = MetalOITFragmentOrdering::EncoderSplit;

	// Compiled MSL library + cached function handles. Built once by
	// Init() and reused for every pipeline state. Dropped in term().
	id<MTLLibrary>  oitLibrary        = nil;
	id<MTLFunction> collectVertexFn   = nil;
	id<MTLFunction> collectFragmentFn = nil;
	id<MTLFunction> resolveVertexFn   = nil;
	id<MTLFunction> resolveFragmentFn = nil;
	id<MTLFunction> seedOpaqueDepthFn = nil;
};
