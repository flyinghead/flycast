// Native PowerVR2 renderer for Metal 4. SPDX-License-Identifier: GPL-2.0-or-later
#ifdef USE_METAL
#include "metal_texture.h"
#include "metal_shaders.h"
#include "metal_oit_tiling.h"
#include "metal_modifier_clip.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/sorter.h"
#include "rend/transform_matrix.h"
#include "ui/imgui_driver.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <map>
#include <array>
#include <cmath>
#include <chrono>
#include <atomic>
#include <limits>
#include <new>
#include "ui/gui.h"
#ifdef FLYCAST_FRAME_CAPTURE_TESTS
#include "emulator.h"
#include "../../../tests/metal/metal_frame_capture.inc"
#endif

namespace {
// Light models are uploaded verbatim and bound through Metal 4 GPU addresses.
// MSL PackedLight uses scalar padding to match the emulator's 112-byte record.
static_assert(sizeof(N2Light) == 112 && offsetof(N2Light, direction) == 16
	&& offsetof(N2Light, position) == 32 && offsetof(N2Light, parallel) == 48
	&& offsetof(N2Light, diffuse) == 64 && offsetof(N2Light, specular) == 72
	&& offsetof(N2Light, attnDistA) == 80 && offsetof(N2Light, distAttnMode) == 96,
	"Metal Naomi2 light field offsets");
static_assert(sizeof(N2LightModel) == 1888 && offsetof(N2LightModel, ambientBase) == 1792
	&& offsetof(N2LightModel, ambientOffset) == 1824 && offsetof(N2LightModel, ambientMaterialBase) == 1856
	&& offsetof(N2LightModel, ambientMaterialOffset) == 1864 && offsetof(N2LightModel, lightCount) == 1872
	&& offsetof(N2LightModel, useBaseOver) == 1876 && offsetof(N2LightModel, bumpId1) == 1880
	&& offsetof(N2LightModel, bumpId2) == 1884, "Metal Naomi2 light-model field offsets");
constexpr u32 Texture = 1 << 0, Offset = 1 << 1, Gouraud = 1 << 2, Bump = 1 << 3,
	NativeDepth = 1 << 4, Naomi2 = 1 << 5, TwoVolumes = 1 << 6, AlphaTest = 1 << 7,
	ClipInside = 1 << 8, FogEnabled = 1 << 9, TrilinearWeight = 1 << 10,
	IgnoreAlpha0 = 1 << 11, IgnoreAlpha1 = 1 << 12;
constexpr u32 ColorShadow = 1 << 13, LightOff = 1 << 14, DeferDither = 1 << 15;
constexpr u32 ColourUV = 1u << 18;
constexpr float ColourUVCoordinateLimit = 4.f;
constexpr u32 ShadingTspMask = (3u << 6) | (31u << 19);
enum class PolyPass { Direct, Depth, Color0, OIT, DepthOnly };
enum class RenderAttachments { ColorDepth, DepthOnly, None };
bool needsOITColor(u32 tsp0, u32 tsp1, bool twoVolumes)
{
	return !(tsp0 & (1u << 25)) || (twoVolumes && !(tsp1 & (1u << 25)));
}
bool needsOITColor(const MetalPolyUniforms& p)
{
	return needsOITColor(p.tsp0, p.tsp1, (p.flags & TwoVolumes) != 0);
}
bool hasTwoVolumes(const PolyParam& poly)
{
	return poly.tsp1.full != ~0u || poly.tcw1.full != ~0u;
}
bool activeTwoVolumes(const PolyParam& poly, PolyPass pass, bool colorModifiers, bool oitModifiers)
{
	// Standard rendering shades area 0 and dims it with a final modifier
	// quad. Only per-pixel color/OIT consumers can select area 1.
	return hasTwoVolumes(poly) && ((pass == PolyPass::Color0 && colorModifiers)
		|| (pass == PolyPass::OIT && oitModifiers && poly.pcw.Shadow));
}
bool needsLights(const PolyParam& poly, bool twoVolumes, bool color0, bool color1)
{
	return poly.isNaomi2() && !(poly.pcw.Texture && poly.tcw.PixelFmt == PixelBumpMap)
		&& ((color0 && !poly.constantColor[0]) || (twoVolumes && color1 && !poly.constantColor[1]));
}
const MTLCompareFunction depthOps[] = {
	MTLCompareFunctionNever, MTLCompareFunctionLess, MTLCompareFunctionEqual, MTLCompareFunctionLessEqual,
	MTLCompareFunctionGreater, MTLCompareFunctionNotEqual, MTLCompareFunctionGreaterEqual, MTLCompareFunctionAlways
};
struct FlightTicket
{
	dispatch_semaphore_t semaphore;
	bool submitted = false;
	~FlightTicket() { if (!submitted) dispatch_semaphore_signal(semaphore); }
};
MTLBlendFactor blendFactor(u32 mode, bool source, bool alpha = false)
{
	const MTLBlendFactor factors[] = { MTLBlendFactorZero, MTLBlendFactorOne,
		source ? (alpha ? MTLBlendFactorDestinationAlpha : MTLBlendFactorDestinationColor) : (alpha ? MTLBlendFactorSourceAlpha : MTLBlendFactorSourceColor),
		source ? (alpha ? MTLBlendFactorOneMinusDestinationAlpha : MTLBlendFactorOneMinusDestinationColor) : (alpha ? MTLBlendFactorOneMinusSourceAlpha : MTLBlendFactorOneMinusSourceColor),
		MTLBlendFactorSourceAlpha, MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorDestinationAlpha, MTLBlendFactorOneMinusDestinationAlpha };
	return factors[mode & 7];
}
MTL4RenderPipelineColorAttachmentDescriptor *polygonBlend(TSP tsp, bool color)
{
	MTL4RenderPipelineColorAttachmentDescriptor *blend = [[MTL4RenderPipelineColorAttachmentDescriptor alloc] init];
	blend.writeMask = color ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
	blend.blendingState = color ? MTL4BlendStateEnabled : MTL4BlendStateDisabled;
	blend.sourceRGBBlendFactor = blendFactor(tsp.SrcInstr, true);
	blend.sourceAlphaBlendFactor = blendFactor(tsp.SrcInstr, true, true);
	blend.destinationRGBBlendFactor = blendFactor(tsp.DstInstr, false);
	blend.destinationAlphaBlendFactor = blendFactor(tsp.DstInstr, false, true);
	return blend;
}

class MetalRenderer final : public Renderer
{
	const bool perPixel;
	rend_context *rc = nullptr;
	BaseTextureCache<MetalTexture> textureCache;
	MetalTexture fog, palette, framebuffer;
	id<MTLTexture> screen, screenDepth, depth, shadowDepth, shadowStencil, emptyStencil, target, white;
	id<MTLBuffer> vertices, indices, modifiers, lights, nodes, oitColors1, heads, counter, oitPolygons, frameUniforms;
	std::vector<u32> modifierTriangleOffsets;
	std::vector<metal_modifier::Triangle> clippedModifierTriangles;
	id<MTLBuffer> emptyLights, emptyNaomi2, naomi2;
	std::vector<NSUInteger> lightOffsets;
	static constexpr NSUInteger unusedLight = ~NSUInteger(0);
	enum : size_t { N2Opaque, N2PunchThrough, N2Translucent, N2Modifiers, N2TrModifiers, N2BankCount };
	struct Naomi2Offsets {
		NSUInteger base = 0;
		// Mixed PVR/Naomi2 lists need a small CPU indirection table. Dense
		// Naomi2 lists retain the original constant-time arithmetic lookup.
		std::vector<NSUInteger> mixed;
		NSUInteger at(size_t index) const {
			return mixed.empty() ? base + index * sizeof(MetalNaomi2Uniforms) : mixed[index];
		}
		void clear() { base = 0; mixed.clear(); }
	};
	std::array<Naomi2Offsets, N2BankCount> naomi2Offsets;
	struct ResolvePipeline {
		id<MTLComputePipelineState> state = nil;
		MTLSize threads = MTLSizeMake(1, 1, 1);
	};
	ResolvePipeline resolve, finish;
	std::map<u32, ResolvePipeline> resolvePipelines;
	id<MTLDepthStencilState> disabledDepth;
	id<MTLRenderPipelineState> scalePipeline;
	MTLVertexDescriptor *vertexLayout, *dcVertexLayout, *depthLayout, *modLayout;
	std::map<u64, id<MTLRenderPipelineState>> pipelines;
	std::map<u64, id<MTLRenderPipelineState>> materialPipelines;
	struct PipelineSelection {
		u64 key = 0;
		bool material = false;
		id<MTLRenderPipelineState> state = nil;
	} pipelineSelection;
	struct PipelineResults {
		std::atomic<bool> active{true};
		std::mutex mutex;
		std::vector<std::pair<u64, id<MTLRenderPipelineState>>> ready;
	};
	std::shared_ptr<PipelineResults> pipelineResults;
	// Polygon depth state has eight comparisons and two independent booleans.
	std::array<id<MTLDepthStencilState>, 8 * 2 * 2> depths{};
	std::array<id<MTLDepthStencilState>, 4> modifierDepths{};
	id<MTLDepthStencilState> finalModifierDepth = nil;
	// Three address modes per axis; nearest, linear/nearest-mip, or
	// linear/linear-mip with each of the sixteen anisotropy levels.
	std::array<id<MTLSamplerState>, 3 * 3 * 18> samplers{};
	TransformMatrix matrices;
	MetalFrameUniforms frame{};
	static constexpr NSUInteger frameUniformStride = (sizeof(MetalFrameUniforms) + 31) & ~NSUInteger(31);
	bool framebufferRendered = false;
	double presentationVideoTime = 0;
	bool screenRendered = false;
	bool colorModifiers = false, oitModifiers = false;
	struct PassState {
		bool sortedFragments, colorModifiers, oitModifiers, resetOITHeads;
	};
	std::vector<PassState> passStates;
	size_t shadowDepthPassCount = 0;
	bool frameTimings = false;
	u64 profilingFrame = 0;
	u64 profilingProcess = 0;
	u32 profilingBoundEligible = 0, profilingBoundActive = 0;
	u32 profilingUVEligible = 0, profilingUVActive = 0;
	struct FrameResources {
		dispatch_semaphore_t available = dispatch_semaphore_create(1);
		id<MTLBuffer> counter;
		id<MTLBuffer> vertices, indices, modifiers, lights, oitPolygons, naomi2, frameUniforms;
		id<MTLTexture> shadowDepth, shadowStencil, rasterColor;
		id<MTLTexture> screenOutput, rttReadbackOutput, framebufferScale;
		std::array<id<MTLTexture>, 2> resolveScratch;
		std::vector<id<MTLBuffer>> polyUniformPages;
		size_t polyUniformCursor = 0;
	};
	std::array<FrameResources, MetalContext::RenderBatchSlots> frameResources;
	size_t nextFrameResource = 0, currentFrameResource = 0;
	struct RenderAbortState { bool canceled = false; };
	std::shared_ptr<RenderAbortState> renderAbort = std::make_shared<RenderAbortState>();
	void recoverCanceledRender()
	{
		if (!renderAbort->canceled) return;
		renderAbort->canceled = false;
		resetTextureCache = updateFogTable = updatePalette = true;
		// A canceled batch may include earlier renders whose image was already
		// recorded as pending. Keep the previously presented image available.
		screen = screenDepth = target = nil;
		screenRendered = false;
	}
	struct DrawState {
		id<MTLRenderPipelineState> pipeline = nil;
		id<MTLDepthStencilState> depth = nil;
		id<MTLBuffer> geometry = nil;
		id<MTLBuffer> lightBuffer = nil;
		NSUInteger lightOffset = 0;
		id<MTLBuffer> naomi2Buffer = nil;
		NSUInteger naomi2Offset = 0;
		id<MTLBuffer> vertexPolyUniformBuffer = nil, fragmentPolyUniformBuffer = nil;
		u32 vertexPolyFlags = 0, vertexPolySources = 0, vertexPolySequence = 0;
		std::array<id<MTLTexture>, 6> textures{};
		std::array<id<MTLSamplerState>, 2> samplers{};
		MTLScissorRect scissor{};
		MTLCullMode cull = MTLCullModeNone;
		u32 stencil = ~0u;
		MetalPolyUniforms uniforms{};
		bool scissorValid = false, cullValid = false, uniformsValid = false, oitBound = false, trModifiersBound = false;
		bool shadingTexturesBound = false;
	} drawState;
	Rect baseScissor{};
	metal_oit::Tile oitRasterTile{};
	bool discardInitialColor = false;
	bool backgroundColorCleared = false, initialColorClear = false;
	bool skipBackgroundDepth = false;
	MTLClearColor backgroundClearColor{};
#ifdef METAL_RENDERER_TESTS
	bool disableBackgroundClear = false;
	bool disableDepthBounds = false;
	bool disableColourUV = false;
	bool disableOrderedModifiers = false;
#endif
	float conservativeDepthBound(const PolyParam& poly, u32 first, u32 count, bool *colourCoordinates = nullptr) const
	{
		if (colourCoordinates) *colourCoordinates = false;
#ifdef METAL_RENDERER_TESTS
		if (disableDepthBounds) return 0;
#endif
		if (poly.isNaomi2() || count < 3 || first > rc->idx.size() || count > rc->idx.size() - first)
			return 0;
		const auto& m = frame.ndc;
		// The PVR reciprocal must be a positive scale of input Z. Other
		// projections retain depth(any), including every Naomi2 projection.
		if (m[0][2] != 0 || m[1][2] != 0 || m[3][2] != 0
				|| m[0][3] != 0 || m[1][3] != 0 || m[2][3] != 0 || m[3][3] != 1
				|| !std::isnormal(m[2][2]) || m[2][2] <= 0) return 0;
		bool boundedUV = colourCoordinates != nullptr;
		double minimum = std::numeric_limits<double>::infinity(), maximum = 0;
		for (size_t i = first; i < size_t(first) + count; ++i) {
			const u32 index = rc->idx[i];
			if (index == ~0u) continue;
			if (index >= rc->verts.size()) return 0;
			const auto& v = rc->verts[index];
			// Metal may flush subnormal operands to zero before multiplying.
			// A positive CPU double product cannot prove positive GPU Z then.
			if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isnormal(v.z)) return 0;
			if (boundedUV && (!std::isfinite(v.u) || !std::isfinite(v.v)
					|| std::abs(v.u) > ColourUVCoordinateLimit || std::abs(v.v) > ColourUVCoordinateLimit)) boundedUV = false;
			const double z = double(m[2][2]) * v.z;
			if (z < 1.e-12 || z > 1.e5) return 0;
			// Limit cancellation and clipping/interpolation ranges. The bound
			// has a wide margin rather than relying on exact CPU/GPU rounding.
			for (int axis = 0; axis < 2; ++axis) {
				const double extent = std::abs(double(m[0][axis]) * v.x)
					+ std::abs(double(m[1][axis]) * v.y) + std::abs(double(m[2][axis]) * v.z)
					+ std::abs(double(m[3][axis]));
				if (!std::isfinite(extent) || extent > 65536) return 0;
				if (extent > 4) boundedUV = false;
			}
			minimum = std::min(minimum, z); maximum = std::max(maximum, z);
		}
		if (maximum == 0 || maximum / minimum > 65536) return 0;
		if (colourCoordinates) *colourCoordinates = boundedUV;
		// Both interpolation modes reconstruct a positive screen-linear Z.
		// log2(1 + 100000*Z) is increasing, so max(vertex Z) bounds its
		// covered fragments. Inflate Z by 12.5% and add a depth margin for
		// matrix, reciprocal, raster and logarithm rounding at either end.
		return float(std::log1p(100000.0 * maximum * 1.125) / (std::log(2.0) * 34.0) + 0x1p-16);
	}
	bool allowsColourUV(const PolyParam& poly, const MetalPolyUniforms& p) const
	{
#ifdef METAL_RENDERER_TESTS
		if (disableColourUV) return false;
#endif
		// Apple Silicon and Metal 4 capabilities are enforced by MetalContext.
		constexpr u32 excluded = NativeDepth | Naomi2 | TwoVolumes | AlphaTest | Bump | (3u << 16);
		if (!(p.flags & Texture) || (p.flags & excluded) || !(p.tsp0 & (1u << 19))) return false;
		const int filtering = config::TextureFiltering;
		if (filtering == 1 || (filtering != 2 && poly.tsp.FilterMode == 0)
				|| config::AnisotropicFiltering > 1) return false;
		// Copy or convex alpha blending cannot directly amplify RGB error.
		if (!((poly.tsp.SrcInstr == 1 && poly.tsp.DstInstr == 0)
				|| (poly.tsp.SrcInstr == 4 && poly.tsp.DstInstr == 5)
				|| (poly.tsp.SrcInstr == 6 && poly.tsp.DstInstr == 7))) return false;
		const auto *texture = static_cast<const MetalTexture *>(poly.texture);
		if (!texture || texture->gpuPalette || !texture->image
				|| texture->image.width > 512 || texture->image.height > 512) return false;
		// Packed Dreamcast formats and RGBA8 all sample normalized RGB.
		// Indexed, integer, floating-point and sRGB images keep exact UV.
		switch (texture->image.pixelFormat) {
		case MTLPixelFormatRGBA8Unorm:
		case MTLPixelFormatB5G6R5Unorm:
		case MTLPixelFormatA1BGR5Unorm:
		case MTLPixelFormatABGR4Unorm:
			return true;
		default:
			return false;
		}
	}
	bool replacesFramebufferColor() const
	{
		// The first opaque strip may replace every sample before any color
		// consumer runs. Partial, blended or depth-tested images still need
		// their preceding framebuffer, even when clearFramebuffer is false.
		if (rc->render_passes.empty() || rc->render_passes.front().op_count == 0
				|| rc->global_param_op.empty() || frame.width == 0 || frame.height == 0
				|| baseScissor.origin.x > 0 || baseScissor.origin.y > 0
				|| int64_t(baseScissor.origin.x) + baseScissor.size.x < frame.width
				|| int64_t(baseScissor.origin.y) + baseScissor.size.y < frame.height) return false;
		const PolyParam& poly = rc->global_param_op.front();
		if (poly.count != 4 || poly.first > rc->idx.size() || rc->idx.size() - poly.first < 4
				|| poly.isp.DepthMode != 7 || poly.isp.CullMode > 1 || poly.tileclip != 0
				|| poly.isNaomi2() || hasTwoVolumes(poly)
				|| poly.tsp.SrcInstr != 1 || poly.tsp.DstInstr != 0) return false;
		// Limit the proof to an axis-aligned DC transform with constant W/Z.
		// Rotated or projected geometry retains the original copy/load path.
		const auto& m = frame.ndc;
		if (m[1][0] != 0 || m[2][0] != 0 || m[0][1] != 0 || m[2][1] != 0
				|| m[0][2] != 0 || m[1][2] != 0 || m[0][3] != 0 || m[1][3] != 0
				|| m[2][3] != 0 || m[3][3] != 1) return false;
		std::array<glm::dvec2, 4> corners;
		double errorX = 0, errorY = 0;
		float z = 0;
		for (size_t i = 0; i < 4; ++i) {
			const u32 index = rc->idx[poly.first + i];
			if (index >= rc->verts.size()) return false; // Includes strip restarts.
			const auto& v = rc->verts[index];
			if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z)
					|| (i != 0 && v.z != z)) return false;
			z = v.z;
			const double depth = double(m[2][2]) * z + m[3][2];
			if (!(depth >= 1e-20 && depth <= 1e5)) return false;
			const double x = double(m[0][0]) * v.x, y = double(m[1][1]) * v.y;
			if (!std::isfinite(x) || !std::isfinite(y) || std::abs(x) > 1e6 || std::abs(y) > 1e6
					|| !std::isfinite(m[3][0]) || !std::isfinite(m[3][1])
					|| std::abs(m[3][0]) > 1e6 || std::abs(m[3][1]) > 1e6) return false;
			corners[i] = {x + m[3][0], -(y + m[3][1])};
			// Cover both fused/unfused vertex arithmetic and the native-depth
			// divide/perspective divide. Leave half a pixel of extra coverage
			// beyond the outer sample centers; large/cancelling inputs fail safe.
			constexpr double epsilon = std::numeric_limits<float>::epsilon() * 32;
			errorX = std::max(errorX, epsilon * (std::abs(x) + std::abs(double(m[3][0])) + 1));
			errorY = std::max(errorY, epsilon * (std::abs(y) + std::abs(double(m[3][1])) + 1));
		}
		const auto& a = corners[0]; const auto& b = corners[1];
		const auto& c = corners[2]; const auto& d = corners[3];
		return a.x == c.x && b.x == d.x && a.y == b.y && c.y == d.y
			&& a.x + errorX <= -1 + .5 / frame.width && b.x - errorX >= 1 - .5 / frame.width
			&& a.y - errorY >= 1 - .5 / frame.height && c.y + errorY <= -1 + .5 / frame.height;
	}
	bool prepareBackgroundClear()
	{
#ifdef METAL_RENDERER_TESTS
		if (disableBackgroundClear) return false;
#endif
		if (!replacesFramebufferColor() || frame.dithering
				|| (perPixel && !passStates.empty() && passStates.front().colorModifiers)) return false;
		const PolyParam& poly = rc->global_param_op.front();
		// A clear replaces only the constant color calculation. Keep the
		// original geometry/depth/stencil draw, including disabled Z writes.
		// Textures, clamp, shadow and active fog remain ordinary fragments.
		if (poly.pcw.Texture || poly.pcw.Shadow || poly.tsp.ColorClamp
				|| (config::Fog && poly.tsp.FogCtrl != 2
					&& !(poly.tsp.FogCtrl == 1 && !poly.pcw.Offset))) return false;
		const auto& color = rc->verts[rc->idx[poly.first]].col;
		for (size_t i = 1; i < 4; ++i)
			if (memcmp(color, rc->verts[rc->idx[poly.first + i]].col, sizeof(color)) != 0) return false;
		backgroundClearColor = MTLClearColorMake(color[0] / 255.0, color[1] / 255.0,
			color[2] / 255.0, poly.tsp.UseAlpha ? color[3] / 255.0 : 1);
		return true;
	}
	struct TileClipState {
		u32 key = 0;
		TileClipping mode = TileClipping::Off;
		MTLScissorRect scissor{};
		glm::vec4 inside{};
		bool valid = false, clipping = false, drawable = false;
	} tileClipState;

	MetalContext *context() const { return MetalContext::Instance(); }
	id<MTLBuffer> upload(const void *data, size_t size)
	{
		if (size == 0) return nil;
		id<MTLBuffer> buffer = [context()->device() newBufferWithBytes:data length:size options:MTLResourceStorageModeShared];
		if (!buffer) throw RendererException("Metal geometry buffer allocation failed");
		return buffer;
	}
	id<MTLBuffer> reserveBuffer(size_t size, id<MTLBuffer> __strong& storage,
		MTLResourceOptions options = MTLResourceStorageModeShared)
	{
		if (size == 0) return nil;
		// Keep capacity across renders, as Vulkan's buffer pools do. CPU-writable
		// owners acquire completed frame slots; GPU-only storage uses explicit Metal 4 queue barriers.
		if (!storage || storage.length < size) {
			const NSUInteger maximum = context()->device().maxBufferLength;
			if (size > maximum) throw RendererException("Metal buffer exceeds the device buffer limit");
			NSUInteger capacity = std::min<NSUInteger>(4096, maximum);
			while (capacity < size) capacity = capacity <= maximum / 2 ? capacity * 2 : maximum;
			id<MTLBuffer> allocated = [context()->device() newBufferWithLength:capacity options:options];
			if (!allocated) throw RendererException("Metal buffer allocation failed");
			storage = allocated;
		}
		return storage;
	}
	id<MTLBuffer> upload(const void *data, size_t size, id<MTLBuffer> __strong& storage)
	{
		id<MTLBuffer> buffer = reserveBuffer(size, storage);
		if (buffer) memcpy(buffer.contents, data, size);
		return buffer;
	}
	id<MTLTexture> attachment(id<MTLTexture> __strong& storage, MTLPixelFormat format,
		MTLTextureUsage usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget)
	{
		if (!storage || storage.width != frame.width || storage.height != frame.height)
			storage = context()->makeTexture(frame.width, frame.height, format, usage);
		return storage;
	}
	bool renderSamplesTexture(id<MTLTexture> image) const
	{
		// Include zero-count polygons: sorted triangles can reference them
		// independently. Keep both areas conservative, even if shading later
		// proves one inactive. Never rewrite feedback sampled by this render.
		for (const auto *list : {&rc->global_param_op, &rc->global_param_pt, &rc->global_param_tr}) {
			for (const auto& poly : *list) {
				if (!poly.pcw.Texture) continue;
				const auto *area0 = static_cast<const MetalTexture *>(poly.texture);
				const auto *area1 = static_cast<const MetalTexture *>(poly.texture1);
				if ((area0 && area0->image == image) || (area1 && area1->image == image)) return true;
			}
		}
		return false;
	}
	id<MTLTexture> outputTexture(FrameResources& resources, MTLTextureUsage usage, id<MTLTexture> resolveInput = nil)
	{
		if (!rc)
			return context()->makeTexture(frame.width, frame.height, MTLPixelFormatRGBA8Unorm, usage);
		if (rc->isRTT && !config::RenderToTextureBuffer) {
			// Match Vulkan's compatible RTT-image reuse. Cache output may
			// escape beyond frame slots, so preserve current feedback/display
			// readers and the resolve input instead of recycling a slot image.
			int width, height;
			getPvrFramebufferSize(*rc, width, height);
			MetalTexture *texture = textureCache.getRTTexture(rc->fb_W_SOF1 & VRAM_MASK,
				rc->fb_W_CTRL.fb_packmode, width, height);
			id<MTLTexture> output = texture->image;
			if (context()->canReuseTextureWrites(output) && output != screen && output != resolveInput
					&& output.pixelFormat == MTLPixelFormatRGBA8Unorm
					&& output.width == frame.width && output.height == frame.height
					&& output.mipmapLevelCount == 1 && (output.usage & usage) == usage
					&& !renderSamplesTexture(output))
				return output;
			return context()->makeTexture(frame.width, frame.height, MTLPixelFormatRGBA8Unorm, usage);
		}
		// CPU RTT output ends with readback/VRAM conversion. Its completion-
		// owned slot must never escape into the arbitrary-lifetime texture cache.
		auto& output = rc->isRTT ? resources.rttReadbackOutput : resources.screenOutput;
		// Preserve the pending framebuffer, GUI's retained image, and any
		// current resolve input. Older submitted reads are ordered by explicit Metal 4
		// barriers on the same queue; this image is never CPU-written.
		if (!output || output == screen || output == context()->lastFrame() || output == target
				|| output.width != frame.width || output.height != frame.height || (output.usage & usage) != usage)
			output = context()->makeTexture(frame.width, frame.height, MTLPixelFormatRGBA8Unorm, usage);
		return output;
	}

	id<MTLSamplerState> samplerState(size_t u, size_t v, bool linear, bool linearMip, NSUInteger anisotropy)
	{
		// Share states by their resolved descriptor for both polygon sampling
		// and fixed framebuffer resampling. MipMapD stays a shader bias.
		const size_t filter = linearMip ? 1 + anisotropy : linear ? 1 : 0;
		const size_t key = (u * 3 + v) * 18 + filter;
		auto& state = samplers[key];
		if (!state) {
			static constexpr MTLSamplerAddressMode address[] = {
				MTLSamplerAddressModeRepeat, MTLSamplerAddressModeMirrorRepeat, MTLSamplerAddressModeClampToEdge
			};
			MTLSamplerDescriptor *sd = [[MTLSamplerDescriptor alloc] init];
			sd.supportArgumentBuffers = YES;
			sd.minFilter = sd.magFilter = linear ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
			sd.mipFilter = linearMip ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest;
			sd.sAddressMode = address[u]; sd.tAddressMode = address[v];
			sd.maxAnisotropy = anisotropy;
			state = [context()->device() newSamplerStateWithDescriptor:sd];
			if (!state) throw RendererException("Metal sampler allocation failed");
		}
		return state;
	}
	id<MTLSamplerState> sampler(TSP tsp, bool punchThrough, bool gpuPalette, bool textured = true)
	{
		// Untextured shaders still get valid argument bindings, but cannot
		// consume TSP addressing/filtering. Share one nearest/repeat placeholder.
		const int filtering = textured ? config::TextureFiltering.get() : 1;
		const bool linear = !gpuPalette && filtering != 1 && (filtering == 2 || tsp.FilterMode != 0);
		const bool linearMip = linear && !punchThrough;
		const NSUInteger anisotropy = linearMip ? std::clamp(config::AnisotropicFiltering.get(), 1, 16) : 1;
		const size_t u = textured ? (tsp.ClampU ? 2 : tsp.FlipU ? 1 : 0) : 0;
		const size_t v = textured ? (tsp.ClampV ? 2 : tsp.FlipV ? 1 : 0) : 0;
		return samplerState(u, v, linear, linearMip, anisotropy);
	}

	bool setClip(FCMetal4RenderEncoder * encoder, u32 tileclip, MetalPolyUniforms& poly)
	{
		// CPU clip results belong to this render's matrices, independently of
		// encoder bindings. Consecutive polygons/modifiers commonly share them.
		auto& cached = tileClipState;
		const bool clipping = config::Clipping;
		if (!cached.valid || cached.key != tileclip || cached.clipping != clipping) {
			Rect clip;
			cached.mode = matrices.getTileClip(tileclip, clip);
			const Rect scissor = cached.mode == TileClipping::Outside ? clip : baseScissor;
			const int x = std::clamp(scissor.origin.x, 0, int(frame.width));
			const int y = std::clamp(scissor.origin.y, 0, int(frame.height));
			const int right = std::clamp(scissor.origin.x + scissor.size.x, 0, int(frame.width));
			const int bottom = std::clamp(scissor.origin.y + scissor.size.y, 0, int(frame.height));
			cached.drawable = right > x && bottom > y;
			if (cached.drawable) cached.scissor = MTLScissorRect{(NSUInteger)x, (NSUInteger)y,
				(NSUInteger)(right - x), (NSUInteger)(bottom - y)};
			if (cached.mode == TileClipping::Inside)
				cached.inside = glm::vec4(clip.origin.x, clip.origin.y, clip.origin.x + clip.size.x, clip.origin.y + clip.size.y);
			cached.key = tileclip; cached.clipping = clipping; cached.valid = true;
		}
		if (!cached.drawable) return false;
		MTLScissorRect next = cached.scissor;
		if (oitRasterTile.width) {
			const auto clipped = metal_oit::intersection(oitRasterTile,
				{u32(next.x), u32(next.y), u32(next.width), u32(next.height)});
			if (!clipped.width || !clipped.height) return false;
			next = {clipped.x, clipped.y, clipped.width, clipped.height};
		}
		if (!drawState.scissorValid || next.x != drawState.scissor.x || next.y != drawState.scissor.y
			|| next.width != drawState.scissor.width || next.height != drawState.scissor.height) {
			[encoder setScissorRect:next]; drawState.scissor = next; drawState.scissorValid = true;
		}
		if (cached.mode == TileClipping::Inside) {
			poly.flags |= ClipInside;
			poly.clip = cached.inside;
		}
		return true;
	}

	void matrix(glm::mat4& destination, int index)
	{
		if (index >= 0 && (size_t)index < rc->matrices.size())
			memcpy(glm::value_ptr(destination), rc->matrices[index].mat, sizeof(destination));
		else destination = glm::mat4(1);
	}
	void prepareNaomi2(FrameResources& resources)
	{
		naomi2 = emptyNaomi2;
		for (auto& offsets : naomi2Offsets) offsets.clear();
		// Both ta_add_poly overloads install a default matrix for Naomi2
		// geometry. Ordinary PVR frames need no packing pass or upload here.
		if (rc->matrices.empty()) return;
		constexpr NSUInteger stride = sizeof(MetalNaomi2Uniforms);
		static_assert(stride % 32 == 0, "Metal Naomi2 constant-buffer alignment");
		const size_t maximum = context()->device().maxBufferLength / stride;
		size_t records = 1; // Valid placeholder for every non-Naomi2 draw.
		if (maximum < records) throw RendererException("Metal Naomi2 buffer exceeds the device buffer limit");
		auto select = [&](const auto& list, size_t bank) {
			const size_t count = std::count_if(list.begin(), list.end(),
				[](const auto& pp) { return pp.isNaomi2(); });
			if (count > maximum - records) throw RendererException("Metal Naomi2 buffer exceeds the device buffer limit");
			records += count;
			if (count != 0 && count != list.size()) naomi2Offsets[bank].mixed.resize(list.size(), 0);
		};
		select(rc->global_param_op, N2Opaque);
		select(rc->global_param_pt, N2PunchThrough);
		select(rc->global_param_tr, N2Translucent);
		if (modifiers) {
			select(rc->global_param_mvo, N2Modifiers);
			if (perPixel) select(rc->global_param_mvo_tr, N2TrModifiers);
		}
		// Matrix storage alone does not imply a Naomi2 draw. Keep the valid
		// shared placeholder when all selected lists contain only PVR entries.
		if (records == 1) return;
		// Write directly into the acquired frame slot's shared GPU storage.
		// No temporary CPU array or second full-record copy is needed.
		naomi2 = reserveBuffer(records * stride, resources.naomi2);
		u8 *data = (u8 *)naomi2.contents;
		new (data) MetalNaomi2Uniforms{};
		size_t record = 1;
		auto packPolygons = [&](const std::vector<PolyParam>& list, size_t bank) {
			auto& offsets = naomi2Offsets[bank];
			offsets.base = record * stride;
			for (const auto& pp : list) {
				if (!pp.isNaomi2()) continue;
				if (!offsets.mixed.empty()) offsets.mixed[&pp - list.data()] = record * stride;
				u8 *destination = data + record++ * stride;
				auto& n2 = *new (destination) MetalNaomi2Uniforms{};
				matrix(n2.mv, pp.mvMatrix); matrix(n2.normal, pp.normalMatrix); matrix(n2.projection, pp.projMatrix);
				n2.env = glm::ivec2(pp.envMapping[0], pp.envMapping[1]);
				n2.constantColor = glm::ivec2(pp.constantColor[0], pp.constantColor[1]);
				n2.gloss = glm::vec2(pp.glossCoef[0], pp.glossCoef[1]);
			}
		};
		auto packModifiers = [&](const std::vector<ModifierVolumeParam>& list, size_t bank) {
			auto& offsets = naomi2Offsets[bank];
			offsets.base = record * stride;
			for (const auto& pp : list) {
				if (!pp.isNaomi2()) continue;
				if (!offsets.mixed.empty()) offsets.mixed[&pp - list.data()] = record * stride;
				u8 *destination = data + record++ * stride;
				auto& n2 = *new (destination) MetalNaomi2Uniforms{};
				matrix(n2.mv, pp.mvMatrix); matrix(n2.projection, pp.projMatrix);
			}
		};
		packPolygons(rc->global_param_op, N2Opaque);
		packPolygons(rc->global_param_pt, N2PunchThrough);
		packPolygons(rc->global_param_tr, N2Translucent);
		// Modifier draws require their geometry buffer. With no such consumer,
		// retain the placeholder offsets instead of packing unused matrices.
		if (modifiers) {
			packModifiers(rc->global_param_mvo, N2Modifiers);
			if (perPixel) packModifiers(rc->global_param_mvo_tr, N2TrModifiers);
		}
	}
	void prepareLights(FrameResources& resources)
	{
		lights = nil;
		lightOffsets.clear();
		if (rc->lightModels.empty() || rc->render_passes.empty()) return;
		lightOffsets.assign(rc->lightModels.size(), unusedLight);
		size_t used = 0;
		const size_t maximum = context()->device().maxBufferLength / sizeof(N2LightModel);
		auto select = [&](const PolyParam& poly, bool twoVolumes, bool color0, bool color1) {
			if (!needsLights(poly, twoVolumes, color0, color1) || poly.lightModel < 0
					|| size_t(poly.lightModel) >= lightOffsets.size()) return;
			auto& offset = lightOffsets[poly.lightModel];
			if (offset != unusedLight) return;
			if (used == maximum) throw RendererException("Metal lights exceed the device buffer limit");
			offset = 0; // Selected; assign packed byte offsets below.
			++used;
		};
		if (perPixel) {
			// Use the same ranges and effective volumes as the color/OIT draws.
			// Depth and modifier vertices do not consume light models.
			RenderPass previous{};
			for (size_t i = 0; i < rc->render_passes.size() && used != lightOffsets.size(); ++i) {
				const auto& pass = rc->render_passes[i];
				const auto& state = passStates[i];
				auto selectRange = [&](const std::vector<PolyParam>& list, u32 first, u32 last, PolyPass drawPass) {
					for (u32 j = first; j < last && used != lightOffsets.size(); ++j) {
						const auto& poly = list[j];
						if (poly.count < 3) continue;
						const bool twoVolumes = activeTwoVolumes(poly, drawPass, state.colorModifiers, state.oitModifiers);
						// Source-select applies only to OIT. Color0 shades both
						// active areas irrespective of their accumulator controls.
						const u32 sourceSelect = drawPass == PolyPass::OIT ? 1u << 25 : 0;
						select(poly, twoVolumes, !(poly.tsp.full & sourceSelect), !(poly.tsp1.full & sourceSelect));
					}
				};
				selectRange(rc->global_param_op, previous.op_count, pass.op_count, PolyPass::Color0);
				selectRange(rc->global_param_pt, previous.pt_count, pass.pt_count, PolyPass::Color0);
				selectRange(rc->global_param_tr, previous.tr_count, pass.tr_count,
					state.sortedFragments ? PolyPass::OIT : PolyPass::Color0);
				previous = pass;
			}
		} else {
			// Standard triangle sorting can reference a polygon independently
			// of its original strip count. Keep conservative polygon selection,
			// but its direct color shader can consume only area 0.
			for (const auto *list : {&rc->global_param_op, &rc->global_param_pt, &rc->global_param_tr}) {
				if (used == lightOffsets.size()) break;
				for (const auto& poly : *list) {
					select(poly, false, true, false);
					if (used == lightOffsets.size()) break;
				}
			}
		}
		if (used == 0) return;
		constexpr NSUInteger stride = sizeof(N2LightModel);
		static_assert(stride % 32 == 0, "Metal light constant-buffer alignment");
		if (used == lightOffsets.size()) {
			// Preserve the original contiguous upload when every model is used.
			lights = upload(rc->lightModels.data(), used * stride, resources.lights);
			for (size_t i = 0; i < lightOffsets.size(); ++i) lightOffsets[i] = i * stride;
			return;
		}
		lights = reserveBuffer(used * stride, resources.lights);
		NSUInteger offset = 0;
		for (size_t i = 0; i < lightOffsets.size(); ++i) {
			if (lightOffsets[i] == unusedLight) continue;
			lightOffsets[i] = offset;
			memcpy((u8 *)lights.contents + offset, &rc->lightModels[i], stride);
			offset += stride;
		}
	}
	MetalPolyUniforms uniforms(const PolyParam& pp, u32 listType)
	{
		MetalPolyUniforms p{};
		p.flags = (pp.pcw.Texture ? Texture : 0) | (pp.pcw.Offset ? Offset : 0) | (pp.pcw.Gouraud ? Gouraud : 0)
			| (pp.tcw.PixelFmt == PixelBumpMap ? Bump : 0) | (pp.isNaomi2() ? Naomi2 : 0)
			| (!settings.platform.isNaomi2() && config::NativeDepthInterpolation ? NativeDepth : 0)
			| (hasTwoVolumes(pp) ? TwoVolumes : 0)
			| (listType == ListType_Punch_Through ? AlphaTest : 0) | (config::Fog ? FogEnabled : 0)
			| (pp.tcw.PixelFmt == Pixel565 ? IgnoreAlpha0 : 0)
			| (pp.tcw1.PixelFmt == Pixel565 ? IgnoreAlpha1 : 0);
		p.tsp0 = pp.tsp.full; p.tsp1 = pp.tsp1.full;
		p.trilinear = 1;
		if (pp.pcw.Texture && pp.tsp.FilterMode > 1 && listType != ListType_Punch_Through && pp.tcw.MipMapped) {
			p.trilinear = .25f * (pp.tsp.MipMapD & 3);
			if (pp.tsp.FilterMode == 2) p.trilinear = 1 - p.trilinear;
		}
		// Specialize away the fragment multiply when its factor is exactly
		// one. Keep every non-unit weight dynamic, including zero.
		if (p.trilinear != 1.f) p.flags |= TrilinearWeight;
		p.lodBias = D_Adjust_LoD_Bias[pp.tsp.MipMapD] - 1;
		p.lodBias1 = hasTwoVolumes(pp) ? D_Adjust_LoD_Bias[pp.tsp1.MipMapD] - 1 : p.lodBias;
		if (pp.texture && pp.texture->gpuPalette) {
			// Vulkan treats every non-nearest indexed filter as palette
			// bilinear. FilterMode 3 must not overflow the two-bit flag field.
			int mode = pp.tsp.FilterMode == 0 ? 1 : 2;
			if (config::TextureFiltering == 1) mode = 1;
			else if (config::TextureFiltering == 2) mode = 2;
			p.flags |= u32(mode) << 16;
			p.palette = pp.tcw.PixelFmt == PixelPal4 ? pp.tcw.PalSelect << 4 : (pp.tcw.PalSelect >> 4) << 8;
		}
		if (perPixel && listType == ListType_Translucent && !rc->global_param_tr.empty()) {
			p.polyIndex = u32(&pp - rc->global_param_tr.data());
			p.sequence = p.polyIndex << 17;
			p.sequence -= rc->idx[rc->global_param_tr.front().first];
		}
		return p;
	}
	void normalizePoly(MetalPolyUniforms& p, PolyPass pass) const
	{
		// Blend and sampler controls use the original PolyParam separately.
		// OIT also keeps source-select: an accumulator source needs no shade.
		const u32 sourceSelect = pass == PolyPass::OIT ? 1u << 25 : 0;
		p.tsp0 &= ShadingTspMask | sourceSelect;
		p.tsp1 &= (ShadingTspMask & ~(1u << 21)) | sourceSelect; // Clamp comes from area 0.
		constexpr u32 ignoreTexAlpha = 1u << 19, paletteModes = 3u << 16;
		if (!(p.flags & Texture)) {
			p.tsp0 &= ~(3u << 6);
			p.tsp1 &= ~(3u << 6);
			p.flags &= ~paletteModes;
			p.lodBias = p.lodBias1 = 0;
		}
		if ((p.flags & Texture) && !(p.flags & Bump)) {
			if (p.flags & IgnoreAlpha0) p.tsp0 |= ignoreTexAlpha;
			if (p.flags & IgnoreAlpha1) p.tsp1 |= ignoreTexAlpha;
		} else {
			p.tsp0 &= ~ignoreTexAlpha;
			p.tsp1 &= ~ignoreTexAlpha;
		}
		p.flags &= ~(IgnoreAlpha0 | IgnoreAlpha1);
		if (p.flags & FogEnabled) {
			// Mode 2 never applies fog. Mode 1 also has no effect without
			// an offset color, or for bump maps. Canonicalize these controls
			// before both uniform comparison and material pipeline selection.
			auto usesFog = [&](u32& tsp) {
				u32 mode = (tsp >> 22) & 3;
				if (mode == 1 && (!(p.flags & Offset) || (p.flags & Bump))) {
					tsp = (tsp & ~(3u << 22)) | (2u << 22);
					mode = 2;
				}
				return mode != 2;
			};
			const bool fog0 = usesFog(p.tsp0);
			const bool fog1 = (p.flags & TwoVolumes) && usesFog(p.tsp1);
			if (!fog0 && !fog1) p.flags &= ~FogEnabled;
		}
		if (!(p.flags & FogEnabled)) {
			p.tsp0 &= ~(3u << 22);
			p.tsp1 &= ~(3u << 22);
		}
		if (!(p.flags & TwoVolumes)) { p.tsp1 = 0; p.lodBias1 = 0; }
		if (p.flags & paletteModes) p.lodBias = 0; // Indexed area 0 has no mips.
		else p.palette = 0;
		// Only the buffer-writing OIT fragment/resolve needs polygon identity
		// and sort sequence. Color/depth fragments do not consume this varying.
		if (pass != PolyPass::OIT) p.polyIndex = p.sequence = 0;
	}

	void collectMaterialPipelines()
	{
		std::vector<std::pair<u64, id<MTLRenderPipelineState>>> ready;
		{
			std::lock_guard<std::mutex> lock(pipelineResults->mutex);
			ready.swap(pipelineResults->ready);
		}
		if (!ready.empty()) pipelineSelection = {};
		for (const auto& entry : ready) materialPipelines[entry.first] = entry.second;
	}
	id<MTLRenderPipelineState> pipeline(const PolyParam& pp, PolyPass pass, bool depthAttachment,
		bool alphaTest = false, const MetalPolyUniforms *material = nullptr, bool backgroundDepth = false)
	{
		TSP tsp = pp.tsp;
		const bool depthPass = pass == PolyPass::Depth || pass == PolyPass::DepthOnly;
		const bool colorlessOIT = pass == PolyPass::OIT && material && !needsOITColor(*material);
		const bool positionOnly = (depthPass && !alphaTest) || colorlessOIT || backgroundDepth;
		const bool naomi2Vertex = !positionOnly && pp.isNaomi2();
		bool color = !depthPass && pass != PolyPass::OIT && !backgroundDepth;
		// Standard depth masks share the open color encoder. Only separate
		// per-pixel depth and buffer-writing OIT passes have no color attachment.
		const MTLPixelFormat colorFormat = pass == PolyPass::OIT || (perPixel && depthPass)
			? MTLPixelFormatInvalid : MTLPixelFormatRGBA8Unorm;
		// Colorless passes do not depend on blend instructions. One pipeline
		// covers every opaque/translucent depth mask; punch-through remains
		// separate because its texture and color inputs determine rejection.
		u64 key = (color ? tsp.SrcInstr | (tsp.DstInstr << 3) : 0)
			| (u64(depthPass ? PolyPass::Depth : pass) << 6)
			| (u64(depthAttachment) << 10) | (u64(depthPass && alphaTest) << 11)
			// Bits 12..43 hold material constants; keep vertex families separate.
			| (u64(naomi2Vertex) << 44) | (u64(colorlessOIT) << 45) | (u64(backgroundDepth) << 46);
		MTLVertexDescriptor *fullLayout = naomi2Vertex ? vertexLayout : dcVertexLayout;
		constexpr u32 shadingMask = ShadingTspMask;
		// ColorClamp is controlled by TSP0 for both volumes. Its TSP1 bit
		// cannot create a distinct shader or render-pipeline variant.
		constexpr u32 volume1ShadingMask = shadingMask & ~(1u << 21);
		u64 materialKey = 0;
		u32 flags = 0, tsp0 = 0, tsp1 = 0;
		const bool specialize = material && !positionOnly;
		const bool bounded = specialize && !naomi2Vertex && pass != PolyPass::OIT && material->depthBound > 0;
		if (specialize) {
			flags = material->flags & ((1u << 19) - 1);
			u32 materialShadingMask = shadingMask;
			// These controls are read only inside texture/fog branches. Keep
			// equivalent materials on one specialization when those are off.
			if (!(flags & Texture)) {
				materialShadingMask &= ~((3u << 6) | (1u << 19));
				flags &= ~(IgnoreAlpha0 | IgnoreAlpha1 | (3u << 16));
			}
			if (!(flags & FogEnabled)) materialShadingMask &= ~(3u << 22);
			tsp0 = material->tsp0 & materialShadingMask;
			// Punch-through depth evaluates area 0 only. TSP1 is neither a
			// vertex control nor an input to that fragment entry point.
			tsp1 = !depthPass && (flags & TwoVolumes) ? material->tsp1 & materialShadingMask & volume1ShadingMask : 0;
			// Vulkan resolves IgnoreTexA and RGB565 to one shader control.
			// Encode the same effective OR in each used area's TSP constant,
			// rather than specializing the equivalent format/TSP combinations.
			constexpr u32 ignoreTexAlpha = 1u << 19;
			if ((flags & Texture) && !(flags & Bump)) {
				if (flags & IgnoreAlpha0) tsp0 |= ignoreTexAlpha;
				if (!depthPass && (flags & TwoVolumes) && (flags & IgnoreAlpha1)) tsp1 |= ignoreTexAlpha;
			} else {
				// Bump alpha is computed before the ignore-alpha branch. The
				// untextured path never consumes either texture-alpha control.
				tsp0 &= ~ignoreTexAlpha;
				tsp1 &= ~ignoreTexAlpha;
			}
			flags &= ~(IgnoreAlpha0 | IgnoreAlpha1);
			auto compactTsp = [](u32 value) { return ((value >> 6) & 3) | (((value >> 19) & 31) << 2); };
			// Lower 18 flags and seven shading bits per volume occupy bits 12..43;
			// the fragment-only ColourUV flag uses bit 48, beyond the vertex families.
			// Keep these material constants separate from
			// blend/pass bits. Sampler, palette index and matrices stay uniforms.
			materialKey = key | (u64(flags & ((1u << 18) - 1)) << 12) | (u64(compactTsp(tsp0)) << 30) | (u64(compactTsp(tsp1)) << 37)
				| (u64(bounded) << 47) | (u64((flags & ColourUV) != 0) << 48);
		}
		const u64 selectionKey = specialize ? materialKey : key;
		// Consecutive draws often request the same complete pipeline choice.
		// Keep selection separate from the encoder's actual binding cache:
		// modifier draws may change that binding without changing this result.
		if (pipelineSelection.state && pipelineSelection.key == selectionKey && pipelineSelection.material == specialize)
			return pipelineSelection.state;
		auto remember = [&](id<MTLRenderPipelineState> state) {
			pipelineSelection.key = selectionKey;
			pipelineSelection.material = specialize;
			pipelineSelection.state = state;
			return state;
		};
		if (specialize) {
			auto existing = materialPipelines.find(materialKey);
			if (existing != materialPipelines.end() && existing->second) return remember(existing->second);
		}
		auto& state = pipelines[key];
		if (!state) {
			const auto started = frameTimings ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
			MTL4RenderPipelineColorAttachmentDescriptor *blend = polygonBlend(tsp, color);
			NSString *fragment = colorlessOIT ? @"oit_position_fragment" : pass == PolyPass::OIT ? @"oit_fragment"
				: positionOnly ? @"pvr_position_fragment" : depthPass ? @"pvr_depth_fragment" : @"pvr_fragment";
			NSString *vertex = colorlessOIT ? @"oit_position_vertex" : positionOnly ? @"pvr_position_vertex"
				: naomi2Vertex ? @"pvr_vertex" : @"pvr_dc_vertex";
			state = context()->makePipeline(vertex, fragment,
				positionOnly ? depthLayout : fullLayout,
				colorFormat, depthAttachment, blend);
			if (frameTimings)
				NOTICE_LOG(RENDERER, "Metal timing pipeline %llu: %.3f ms", (unsigned long long)key,
					std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
		}
		if (specialize && materialPipelines.try_emplace(materialKey, nil).second) {
			bool sequence = pass == PolyPass::OIT;
			MetalContext::MaterialConstants constants{flags, tsp0, tsp1, sequence};
			MTL4RenderPipelineColorAttachmentDescriptor *blend = polygonBlend(tsp, color);
			NSString *fragment = pass == PolyPass::OIT ? @"oit_material_fragment"
				: bounded ? (depthPass ? @"pvr_bounded_depth_fragment" : @"pvr_bounded_fragment")
				: depthPass ? @"pvr_material_depth_fragment" : @"pvr_material_fragment";
			const auto results = pipelineResults;
			NSString *vertex = bounded ? @"pvr_dc_bounded_vertex"
				: naomi2Vertex ? @"pvr_material_vertex" : @"pvr_dc_material_vertex";
			context()->makePipelineAsync(vertex, fragment, fullLayout,
				colorFormat, depthAttachment, blend, constants,
				[results, materialKey](id<MTLRenderPipelineState> variant, NSError *error) {
					if (!results->active.load()) return;
					if (!variant) {
						WARN_LOG(RENDERER, "Metal material pipeline %llu: %s; using generic shader",
							(unsigned long long)materialKey, error ? error.localizedDescription.UTF8String : "creation failed");
						return;
					}
					std::lock_guard<std::mutex> lock(results->mutex);
					results->ready.emplace_back(materialKey, variant);
				}, [results] { return results->active.load(); });
		}
		return remember(state);
	}

	id<MTLDepthStencilState> depthState(const PolyParam& pp, u32 list, bool sorted, PolyPass pass)
	{
		MTLCompareFunction compare = sorted || list == ListType_Punch_Through ? MTLCompareFunctionGreaterEqual : depthOps[pp.isp.DepthMode];
		bool write = list == ListType_Punch_Through || !pp.isp.ZWriteDis;
		if (sorted && pass != PolyPass::Depth) write = false;
		if (pass == PolyPass::DepthOnly) { compare = MTLCompareFunctionGreaterEqual; write = true; }
		bool shadow = list != ListType_Translucent && (pass == PolyPass::Depth || pass == PolyPass::Direct);
		// The descriptor depends only on these resolved controls. Pass names
		// need not create duplicate states; stencil references are dynamic.
		const size_t key = size_t(compare) | (size_t(write) << 3) | (size_t(shadow) << 4);
		auto& state = depths[key];
		if (!state) {
			MTLDepthStencilDescriptor *d = [[MTLDepthStencilDescriptor alloc] init];
			d.depthCompareFunction = compare; d.depthWriteEnabled = write;
			if (shadow) {
				MTLStencilDescriptor *s = [[MTLStencilDescriptor alloc] init];
				s.stencilCompareFunction = MTLCompareFunctionAlways;
				s.depthStencilPassOperation = MTLStencilOperationReplace;
				s.readMask = 0; s.writeMask = 0x80;
				d.frontFaceStencil = d.backFaceStencil = s;
			}
			state = [context()->device() newDepthStencilStateWithDescriptor:d];
			if (!state) throw RendererException("Metal polygon depth/stencil state allocation failed");
		}
		return state;
	}

	void bindGeometry(FCMetal4RenderEncoder * e, id<MTLBuffer> buffer)
	{
		if (drawState.geometry != buffer) {
			[e setVertexBuffer:buffer offset:0 atIndex:0]; drawState.geometry = buffer;
		}
	}
	void bindNaomi2(FCMetal4RenderEncoder * e, NSUInteger offset)
	{
		if (drawState.naomi2Buffer != naomi2 || drawState.naomi2Offset != offset) {
			if (naomi2 && drawState.naomi2Buffer == naomi2)
				[e setVertexBufferOffset:offset atIndex:4];
			else
				[e setVertexBuffer:naomi2 offset:offset atIndex:4];
			drawState.naomi2Buffer = naomi2; drawState.naomi2Offset = offset;
		}
	}
	void prepareFrameUniforms(FrameResources& resources)
	{
		// Publish the final frame fields only after makeOITBuffers has filled
		// capacity/maxLayers. Every encoder in this render shares the snapshot.
		const bool intermediate = perPixel && frame.dithering && rc->render_passes.size() > 1;
		frameUniforms = reserveBuffer((intermediate ? frameUniformStride : 0) + sizeof(frame), resources.frameUniforms);
		memcpy(frameUniforms.contents, &frame, sizeof(frame));
		if (intermediate) {
			// A separate aligned record disables intermediate dithering without
			// changing data already referenced by raster/final resolve commands.
			MetalFrameUniforms withoutDither = frame;
			withoutDither.dithering = 0;
			memcpy((u8 *)frameUniforms.contents + frameUniformStride, &withoutDither, sizeof(withoutDither));
		}
	}
	void bindFrame(FCMetal4RenderEncoder * e)
	{
		// Cached bindings belong to this encoder. Modifier draws update them;
		// a new render also needs its own geometry and uniform snapshot, even
		// when continuing a standard screen encoder from the previous render.
		drawState = {};
		[e setViewport:MTLViewport{0, 0, (double)frame.width, (double)frame.height, 0, 1}];
		[e setFrontFacingWinding:MTLWindingCounterClockwise];
		[e setVertexBuffer:frameUniforms offset:0 atIndex:1];
		[e setFragmentBuffer:frameUniforms offset:0 atIndex:1];
		// Geometry, Naomi2 records and shading textures are bound by their
		// first draw. Empty, position-only and modifier encoders need no
		// speculative fog/palette bindings or geometry immediately replaced.
	}
	void bindPoly(FCMetal4RenderEncoder * e, const MetalPolyUniforms& p)
	{
		// Compare all shader inputs, including the second volume's LOD bias,
		// while excluding any alignment padding after the last field.
		constexpr size_t dataBytes = offsetof(MetalPolyUniforms, depthBound) + sizeof(p.depthBound);
		if (drawState.uniformsValid && memcmp(&drawState.uniforms, &p, dataBytes) == 0) return;
		// One CPU record can supply both shader stages. Pages belong to a completed
		// frame slot, and each changed polygon gets a fresh aligned range so
		// later CPU writes cannot overwrite uniforms of an earlier draw.
		// Preserve aligned constant records when binding their Metal 4 GPU addresses.
		constexpr NSUInteger stride = (sizeof(MetalPolyUniforms) + 31) & ~NSUInteger(31);
		constexpr NSUInteger pageBytes = 64 * 1024;
		constexpr NSUInteger recordsPerPage = pageBytes / stride;
		auto& resources = frameResources[currentFrameResource];
		const size_t record = resources.polyUniformCursor;
		const size_t page = record / recordsPerPage;
		if (page == resources.polyUniformPages.size()) {
			id<MTLBuffer> buffer = [context()->device() newBufferWithLength:pageBytes options:MTLResourceStorageModeShared];
			if (!buffer) throw RendererException("Metal polygon uniform buffer allocation failed");
			resources.polyUniformPages.push_back(buffer);
		}
		id<MTLBuffer> buffer = resources.polyUniformPages[page];
		const NSUInteger offset = (record % recordsPerPage) * stride;
		memcpy((u8 *)buffer.contents + offset, &p, sizeof(p));
		++resources.polyUniformCursor;
		// Vertex entries consume only vertex flags, dynamic OIT source-select
		// and sequence from Poly. Keep their preceding immutable record when
		// only fragment shading/clip/palette/LOD changes. Both stages can then
		// retain different pages, so track their buffer bindings independently.
		const u32 vertexFlags = p.flags & MetalVertexMaterialFlags;
		const u32 vertexSources = (p.tsp0 & (1u << 25)) | ((p.tsp1 & (1u << 25)) << 1);
		if (!drawState.vertexPolyUniformBuffer || drawState.vertexPolyFlags != vertexFlags
				|| drawState.vertexPolySources != vertexSources || drawState.vertexPolySequence != p.sequence
				|| drawState.uniforms.depthBound != p.depthBound) {
			if (drawState.vertexPolyUniformBuffer == buffer)
				[e setVertexBufferOffset:offset atIndex:2];
			else
				[e setVertexBuffer:buffer offset:offset atIndex:2];
			drawState.vertexPolyUniformBuffer = buffer;
			drawState.vertexPolyFlags = vertexFlags; drawState.vertexPolySources = vertexSources;
			drawState.vertexPolySequence = p.sequence;
		}
		if (drawState.fragmentPolyUniformBuffer == buffer)
			[e setFragmentBufferOffset:offset atIndex:2];
		else {
			[e setFragmentBuffer:buffer offset:offset atIndex:2];
			drawState.fragmentPolyUniformBuffer = buffer;
		}
		memcpy(&drawState.uniforms, &p, sizeof(p)); drawState.uniformsValid = true;
	}
	void drawPoly(FCMetal4RenderEncoder * e, const PolyParam& pp, u32 list, bool sorted, PolyPass pass, u32 first, u32 count)
	{
		if (count < 3) return;
		if (skipBackgroundDepth && list == ListType_Opaque && &pp == &rc->global_param_op.front()
				&& (pass == PolyPass::Direct || pass == PolyPass::Color0 || pass == PolyPass::Depth)) return;
		const bool backgroundDepth = backgroundColorCleared && list == ListType_Opaque
			&& &pp == &rc->global_param_op.front() && (pass == PolyPass::Direct || pass == PolyPass::Color0);
		const bool depthPositionOnly = (pass == PolyPass::Depth || pass == PolyPass::DepthOnly)
			&& list != ListType_Punch_Through;
		MetalPolyUniforms p{};
		if (depthPositionOnly) {
			p.flags = (pp.isNaomi2() ? Naomi2 : 0)
				| (!settings.platform.isNaomi2() && config::NativeDepthInterpolation ? NativeDepth : 0);
		} else p = uniforms(pp, list);
		if (perPixel) p.flags |= DeferDither;
		if (pass == PolyPass::Color0 && colorModifiers) p.flags |= ColorShadow;
		if (pass == PolyPass::Depth) p.flags |= LightOff;
		// Area 1 cannot be consumed without the corresponding modifier
		// operation. Punch-through depth always shades only area 0.
		if (!activeTwoVolumes(pp, pass, colorModifiers, oitModifiers))
			p.flags &= ~TwoVolumes;
		normalizePoly(p, pass);
		const bool positionOnly = depthPositionOnly || backgroundDepth || (pass == PolyPass::OIT && !needsOITColor(p));
		const bool needsBound = !positionOnly && pass != PolyPass::OIT
			&& (sorted || list == ListType_Punch_Through || pp.isp.DepthMode == 4 || pp.isp.DepthMode == 6);
		const bool colourMaterial = !positionOnly && allowsColourUV(pp, p);
		if (needsBound || colourMaterial) {
			bool coordinates = false;
			const float bound = conservativeDepthBound(pp, first, count, colourMaterial ? &coordinates : nullptr);
			if (needsBound) p.depthBound = bound;
			if (coordinates) p.flags |= ColourUV;
		}
		if (!setClip(e, pp.tileclip, p)) return;
		bindGeometry(e, vertices);
		id<MTLRenderPipelineState> pipe = pipeline(pp, pass, pass != PolyPass::OIT, list == ListType_Punch_Through, &p, backgroundDepth);
		if (frameTimings && p.depthBound > 0) {
			++profilingBoundEligible;
			auto specialized = materialPipelines.find(pipelineSelection.key);
			if (specialized != materialPipelines.end() && specialized->second == pipe) ++profilingBoundActive;
		}
		if (frameTimings && (p.flags & ColourUV)) {
			++profilingUVEligible;
			auto specialized = materialPipelines.find(pipelineSelection.key);
			if (specialized != materialPipelines.end() && specialized->second == pipe) ++profilingUVActive;
		}
		if (drawState.pipeline != pipe) { [e setRenderPipelineState:pipe]; drawState.pipeline = pipe; }
		id<MTLDepthStencilState> ds = pass == PolyPass::OIT ? disabledDepth : depthState(pp, list, sorted, pass);
		if (drawState.depth != ds) { [e setDepthStencilState:ds]; drawState.depth = ds; }
		const u32 stencil = pass == PolyPass::Color0 ? 0x81 : pp.pcw.Shadow ? 0x80 : 0;
		if (drawState.stencil != stencil) { [e setStencilReferenceValue:stencil]; drawState.stencil = stencil; }
		const MTLCullMode cull = pp.isp.CullMode == 3 ? MTLCullModeBack : pp.isp.CullMode == 2 ? MTLCullModeFront : MTLCullModeNone;
		if (!drawState.cullValid || drawState.cull != cull) { [e setCullMode:cull]; drawState.cull = cull; drawState.cullValid = true; }
		bindPoly(e, p);
		NSUInteger n2Offset = 0;
		if (pp.isNaomi2()) {
			const size_t bank = list == ListType_Opaque ? N2Opaque : list == ListType_Punch_Through ? N2PunchThrough : N2Translucent;
			const auto& parameters = list == ListType_Opaque ? rc->global_param_op
				: list == ListType_Punch_Through ? rc->global_param_pt : rc->global_param_tr;
			n2Offset = naomi2Offsets[bank].at(&pp - parameters.data());
		}
		bindNaomi2(e, n2Offset);
		if (!positionOnly) {
			// Punch-through depth also evaluates color/alpha, so it takes this
			// path. Position-only depth/OIT and modifier shaders have no such
			// texture arguments. Keep generic shading arguments valid even
			// when a material specialization removes their actual reads.
			if (!drawState.shadingTexturesBound) {
				[e setFragmentTexture:fog.image atIndex:2];
				[e setFragmentTexture:palette.image atIndex:3];
				drawState.shadingTexturesBound = true;
			}
			const bool textured = (p.flags & Texture) != 0;
			const bool twoVolumes = (p.flags & TwoVolumes) != 0;
			// Source-select survives normalization only in OIT. Match the
			// fragment's color consumers before resolving texture/sampler state.
			const bool color0 = !(p.tsp0 & (1u << 25));
			const bool color1 = twoVolumes && !(p.tsp1 & (1u << 25));
			const bool hasLights = !(p.flags & LightOff)
				&& needsLights(pp, twoVolumes, color0, color1) && lights
				&& pp.lightModel >= 0 && size_t(pp.lightModel) < lightOffsets.size()
				&& lightOffsets[pp.lightModel] != unusedLight;
			id<MTLBuffer> lightBuffer = hasLights ? lights : emptyLights;
			const NSUInteger lightOffset = hasLights ? lightOffsets[pp.lightModel] : 0;
			if (drawState.lightBuffer != lightBuffer || drawState.lightOffset != lightOffset) {
				if (lightBuffer && drawState.lightBuffer == lightBuffer)
					[e setVertexBufferOffset:lightOffset atIndex:3];
				else
					[e setVertexBuffer:lightBuffer offset:lightOffset atIndex:3];
				drawState.lightBuffer = lightBuffer; drawState.lightOffset = lightOffset;
			}
			const bool sample0 = textured && color0, sample1 = textured && color1;
			MetalTexture *t0 = sample0 ? static_cast<MetalTexture *>(pp.texture) : nullptr;
			MetalTexture *t1 = sample1 ? static_cast<MetalTexture *>(pp.texture1) : nullptr;
			const id<MTLTexture> textures[] = {t0 && t0->image ? t0->image : white,
				t1 && t1->image ? t1->image : white};
			id<MTLSamplerState> sampler0 = sample0
				? sampler(pp.tsp, list == ListType_Punch_Through, t0 && t0->gpuPalette) : nil;
			id<MTLSamplerState> sampler1 = sample1
				? sampler(pp.tsp1, list == ListType_Punch_Through, t1 && t1->gpuPalette) : sampler0;
			// Keep every generic shader argument valid. An unused area shares
			// the active area's sampler, or the untextured placeholder when neither samples.
			if (!sampler0) sampler0 = sampler1 ? sampler1 : sampler(pp.tsp, false, false, false);
			if (!sampler1) sampler1 = sampler0;
			const id<MTLSamplerState> textureSamplers[] = {sampler0, sampler1};
			for (NSUInteger i = 0; i < 2; ++i) {
				if (drawState.textures[i] != textures[i]) { [e setFragmentTexture:textures[i] atIndex:i]; drawState.textures[i] = textures[i]; }
				id<MTLSamplerState> ss = textureSamplers[i];
				if (drawState.samplers[i] != ss) { [e setFragmentSamplerState:ss atIndex:i]; drawState.samplers[i] = ss; }
			}
			id<MTLTexture> stencilTexture = pass == PolyPass::Color0 && colorModifiers ? shadowStencil : emptyStencil;
			if (drawState.textures[5] != stencilTexture) { [e setFragmentTexture:stencilTexture atIndex:5]; drawState.textures[5] = stencilTexture; }
		}
		// Position-only OIT still publishes depth/sequence/links for blending.
		if (pass == PolyPass::OIT && !drawState.oitBound) {
			[e setFragmentBuffer:nodes offset:0 atIndex:4]; [e setFragmentBuffer:heads offset:0 atIndex:5];
			[e setFragmentBuffer:counter offset:0 atIndex:6]; [e setFragmentTexture:shadowDepth atIndex:4];
			[e setFragmentBuffer:(oitColors1 ? oitColors1 : counter) offset:0 atIndex:8];
			drawState.oitBound = true;
		}
		if (sorted && !perPixel && !config::PerStripSorting) {
			[e drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:count indexType:MTLIndexTypeUInt32 indexBuffer:indices indexBufferOffset:first * sizeof(u32)];
		} else {
			// Metal strip draws always enable primitive restart. The parser's
			// UInt32 restart markers preserve merged strips in one draw, as in Vulkan.
			[e drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip indexCount:count indexType:MTLIndexTypeUInt32 indexBuffer:indices indexBufferOffset:first * sizeof(u32)];
		}
	}
	void drawList(FCMetal4RenderEncoder * e, const std::vector<PolyParam>& list, u32 first, u32 last, u32 type, bool sorted, PolyPass pass)
	{
		for (u32 i = first; i < last; ++i) drawPoly(e, list[i], type, sorted, pass, list[i].first, list[i].count);
	}

	void drawMod(FCMetal4RenderEncoder * e, const ModifierVolumeParam& pp, int mode, u32 first, u32 count,
		bool translucent, NSUInteger n2Offset)
	{
		MetalPolyUniforms p{};
		p.flags = pp.isNaomi2() ? Naomi2 : 0;
		if (!settings.platform.isNaomi2() && config::NativeDepthInterpolation) p.flags |= NativeDepth;
		// Opaque modifier modes live entirely in depth/stencil state. Only
		// the translucent buffer-writing fragment shader reads this uniform.
		p.modMode = translucent ? mode : 0;
		if (!setClip(e, pp.tileclip, p)) return;
		// Mode is a shader uniform and a dynamic depth/stencil state;
		// it does not change this render-pipeline descriptor.
		const u64 pipelineKey = 0x10000 | u64(translucent);
		auto& pipe = pipelines[pipelineKey];
		if (!pipe) {
			MTL4RenderPipelineColorAttachmentDescriptor *blend = [[MTL4RenderPipelineColorAttachmentDescriptor alloc] init];
			blend.writeMask = MTLColorWriteMaskNone;
			const MTLPixelFormat colorFormat = translucent || perPixel ? MTLPixelFormatInvalid : MTLPixelFormatRGBA8Unorm;
			pipe = context()->makePipeline(@"mod_vertex", translucent ? @"tr_mod_fragment" : @"mod_depth_fragment",
				modLayout, colorFormat, !translucent, blend);
		}
		if (drawState.pipeline != pipe) { [e setRenderPipelineState:pipe]; drawState.pipeline = pipe; }
		bindGeometry(e, modifiers);
		bindPoly(e, p);
		bindNaomi2(e, n2Offset);
		const MTLCullMode cull = pp.isp.CullMode == 3 ? MTLCullModeBack : pp.isp.CullMode == 2 ? MTLCullModeFront : MTLCullModeNone;
		if (!drawState.cullValid || drawState.cull != cull) { [e setCullMode:cull]; drawState.cull = cull; drawState.cullValid = true; }
		if (translucent) {
			if (drawState.depth != disabledDepth) { [e setDepthStencilState:disabledDepth]; drawState.depth = disabledDepth; }
			if (!drawState.trModifiersBound) {
				[e setFragmentBuffer:nodes offset:0 atIndex:4]; [e setFragmentBuffer:heads offset:0 atIndex:5];
				[e setFragmentBuffer:oitPolygons offset:0 atIndex:7];
				drawState.trModifiersBound = true;
			}
			// End/renew encoders between dependent modifier-volume draws at the call site.
		} else {
			auto& state = modifierDepths[mode];
			if (!state) {
				MTLDepthStencilDescriptor *d = [[MTLDepthStencilDescriptor alloc] init];
				d.depthCompareFunction = mode < 2 ? MTLCompareFunctionGreater : MTLCompareFunctionAlways;
				MTLStencilDescriptor *s = [[MTLStencilDescriptor alloc] init];
				s.stencilCompareFunction = mode == 2 ? MTLCompareFunctionLessEqual : mode == 3 ? MTLCompareFunctionEqual : MTLCompareFunctionAlways;
				s.readMask = mode < 2 ? (mode == 1 ? 2 : 0) : 3;
				s.writeMask = mode < 2 ? 2 : 3;
				s.stencilFailureOperation = mode >= 2 ? MTLStencilOperationZero : MTLStencilOperationKeep;
				s.depthFailureOperation = mode >= 2 ? MTLStencilOperationZero : MTLStencilOperationKeep;
				s.depthStencilPassOperation = mode == 0 ? MTLStencilOperationInvert : mode == 3 ? MTLStencilOperationKeep : MTLStencilOperationReplace;
				d.frontFaceStencil = d.backFaceStencil = s;
				state = [context()->device() newDepthStencilStateWithDescriptor:d];
				if (!state) throw RendererException("Metal modifier depth/stencil state allocation failed");
			}
			if (drawState.depth != state) { [e setDepthStencilState:state]; drawState.depth = state; }
			const u32 stencil = mode < 2 ? 2 : 1;
			if (drawState.stencil != stencil) { [e setStencilReferenceValue:stencil]; drawState.stencil = stencil; }
		}
		const u32 clippedFirst = modifierTriangleOffsets.at(first);
		const u32 clippedLast = modifierTriangleOffsets.at(size_t(first) + count);
		if (clippedLast != clippedFirst)
			[e drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:NSUInteger(clippedFirst) * 3
				vertexCount:NSUInteger(clippedLast - clippedFirst) * 3];
	}
	void prepareModifiers(FrameResources& resources, bool needed)
	{
		modifierTriangleOffsets.clear(); clippedModifierTriangles.clear();
		if (!needed) { modifiers = nil; return; }
		static_assert(sizeof(ModTriangle) == sizeof(metal_modifier::Triangle), "Metal modifier triangle layout");
		// Projected/mixed Naomi2 lists retain all triangles verbatim: an
		// inclusion range may consume earlier geometry with this volume's
		// projection, which is selected separately after the upload.
		bool projected = false;
		for (const auto *list : {&rc->global_param_mvo, &rc->global_param_mvo_tr})
			for (const auto& volume : *list) {
				if (volume.first > rc->modtrig.size() || volume.count > rc->modtrig.size() - volume.first)
					throw RendererException("Metal modifier geometry range is invalid");
				projected |= volume.isNaomi2();
			}
		modifierTriangleOffsets.reserve(rc->modtrig.size() + 1);
		clippedModifierTriangles.reserve(rc->modtrig.size());
		const bool nativeDepth = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
		bool clip = true;
#ifdef FLYCAST_FRAME_CAPTURE_TESTS
		clip = std::getenv("FLYCAST_CAPTURE_DISABLE_MODIFIER_CLIP") == nullptr;
#endif
		for (size_t i = 0; i < rc->modtrig.size(); ++i) {
			modifierTriangleOffsets.push_back(clippedModifierTriangles.size());
			metal_modifier::Triangle triangle;
			memcpy(triangle.data(), &rc->modtrig[i], sizeof(triangle));
			if (!clip || projected || !metal_modifier::appendClipped(triangle, glm::value_ptr(frame.ndc),
					clippedModifierTriangles, nativeDepth)) clippedModifierTriangles.push_back(triangle);
		}
		modifierTriangleOffsets.push_back(clippedModifierTriangles.size());
		modifiers = clippedModifierTriangles.empty() ? nil : upload(clippedModifierTriangles.data(),
			clippedModifierTriangles.size() * sizeof(metal_modifier::Triangle), resources.modifiers);
	}
	void drawFinalModifier(FCMetal4RenderEncoder * e)
	{
		if (!config::ModifierVolumes || !modifiers) return;
		// The final quad replaces draw state but never changes fog/palette
		// bindings. Preserve their validity within this same encoder.
		const bool shadingTexturesBound = drawState.shadingTexturesBound;
		drawState = {};
		drawState.shadingTexturesBound = shadingTexturesBound;
		constexpr u64 key = 0x20000;
		auto& pipe = pipelines[key];
		if (!pipe) {
			MTL4RenderPipelineColorAttachmentDescriptor *blend = [[MTL4RenderPipelineColorAttachmentDescriptor alloc] init];
			blend.blendingState = MTL4BlendStateEnabled;
			blend.sourceRGBBlendFactor = blend.sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
			blend.destinationRGBBlendFactor = blend.destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
			pipe = context()->makePipeline(@"mod_vertex", @"mod_fragment", vertexLayout, MTLPixelFormatRGBA8Unorm, true, blend);
		}
		auto& state = finalModifierDepth;
		if (!state) {
			MTLDepthStencilDescriptor *d = [[MTLDepthStencilDescriptor alloc] init];
			d.depthCompareFunction = MTLCompareFunctionAlways;
			MTLStencilDescriptor *s = [[MTLStencilDescriptor alloc] init];
			s.stencilCompareFunction = MTLCompareFunctionEqual; s.readMask = 0x81; s.writeMask = 3;
			s.stencilFailureOperation = s.depthFailureOperation = s.depthStencilPassOperation = MTLStencilOperationZero;
			d.frontFaceStencil = d.backFaceStencil = s;
			state = [context()->device() newDepthStencilStateWithDescriptor:d];
			if (!state) throw RendererException("Metal final modifier stencil state allocation failed");
		}
		MetalPolyUniforms p{};
		if (!settings.platform.isNaomi2() && config::NativeDepthInterpolation) p.flags |= NativeDepth;
		if (!setClip(e, 0, p)) return;
		[e setRenderPipelineState:pipe]; [e setDepthStencilState:state]; [e setStencilReferenceValue:0x81];
		[e setCullMode:MTLCullModeNone]; bindGeometry(e, vertices);
		bindNaomi2(e, 0);
		bindPoly(e, p);
		[e drawIndexedPrimitives:MTLPrimitiveTypeTriangleStrip indexCount:4 indexType:MTLIndexTypeUInt32 indexBuffer:indices indexBufferOffset:0];
	}

	FCMetal4RenderEncoder * encoder(FCMetal4Commands * cb, RenderAttachments attachments,
		bool clearColor = false, bool clearDepth = false, bool clearStencil = false, id<MTLTexture> depthOverride = nil,
		bool preserveDepth = true, bool deferredDepthStore = false)
	{
		MTL4RenderPassDescriptor *pass = [MTL4RenderPassDescriptor new];
		if (attachments == RenderAttachments::ColorDepth) {
			pass.colorAttachments[0].texture = target;
			pass.colorAttachments[0].loadAction = clearColor || initialColorClear ? MTLLoadActionClear
				: discardInitialColor ? MTLLoadActionDontCare : MTLLoadActionLoad;
			discardInitialColor = false; // Only the first color pass can discard.
			pass.colorAttachments[0].storeAction = MTLStoreActionStore;
			pass.colorAttachments[0].clearColor = initialColorClear ? backgroundClearColor
				: rc->isRTT ? MTLClearColorMake(0, 0, 0, 1) : MTLClearColorMake(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1);
			initialColorClear = false;
		}
		if (attachments != RenderAttachments::None) {
			pass.depthAttachment.texture = pass.stencilAttachment.texture = depthOverride ? depthOverride : depth;
			pass.depthAttachment.loadAction = clearDepth ? MTLLoadActionClear : MTLLoadActionLoad;
			// An open standard screen encoder chooses at its eventual boundary:
			// retain depth for another render, or discard it when presenting.
			pass.depthAttachment.storeAction = deferredDepthStore ? MTLStoreActionUnknown
				: preserveDepth ? MTLStoreActionStore : MTLStoreActionDontCare;
			// Per-pixel color attachments have no stencil consumer. Shadow
			// depth needs stored stencil only when opaque modifiers can feed
			// the color shader; OIT's manual test consumes the depth aspect.
			const bool stencilUsed = !perPixel || (depthOverride != nil && colorModifiers);
			pass.stencilAttachment.loadAction = !stencilUsed ? MTLLoadActionDontCare
				: clearStencil ? MTLLoadActionClear : MTLLoadActionLoad;
			// Standard final RTT/VRAM passes have no depth or stencil consumer.
			// Per-pixel shadow stencil can still feed color when depth is discarded.
			const bool preserveStencil = stencilUsed && (perPixel || preserveDepth);
			pass.stencilAttachment.storeAction = deferredDepthStore ? MTLStoreActionUnknown
				: preserveStencil ? MTLStoreActionStore : MTLStoreActionDontCare;
			pass.depthAttachment.clearDepth = 0;
			pass.stencilAttachment.clearStencil = 0;
		} else {
			// Buffer-writing fragment passes still rasterize the full frame
			// without loading/storing a color, depth or stencil attachment.
			pass.renderTargetWidth = frame.width;
			pass.renderTargetHeight = frame.height;
			pass.defaultRasterSampleCount = 1;
		}
		FCMetal4RenderEncoder * e = [cb renderCommandEncoderWithDescriptor:pass];
		if (!e) throw RendererException("Metal render encoder allocation failed");
		bindFrame(e);
		return e;
	}
	bool hasModifierVolumes(const std::vector<ModifierVolumeParam>& list, u32 first, u32 last) const
	{
		// Inspect CPU geometry before deciding whether to upload its GPU buffer.
		if (first == last || !config::ModifierVolumes || rc->modtrig.empty()) return false;
		return std::any_of(list.begin() + first, list.begin() + last,
			[](const ModifierVolumeParam& volume) { return volume.count != 0; });
	}
	bool preparePassStates()
	{
		passStates.clear();
		shadowDepthPassCount = 0;
		if (!perPixel) return false;
		passStates.reserve(rc->render_passes.size());
		bool useOIT = false;
		PassState *lastSorted = nullptr;
		RenderPass previous{};
		for (const auto& pass : rc->render_passes) {
			// drawPoly rejects strips with fewer than three indices before any
			// shader work. Such a range cannot publish an OIT fragment list.
			const bool sorted = pass.autosort && pass.tr_count > previous.tr_count
				&& std::any_of(rc->global_param_tr.begin() + previous.tr_count,
					rc->global_param_tr.begin() + pass.tr_count,
					[](const PolyParam& poly) { return poly.count >= 3; });
			const bool color = hasModifierVolumes(rc->global_param_mvo, previous.mvo_count, pass.mvo_count);
			const bool oit = sorted && (pass.mv_op_tr_shared ? color
				: hasModifierVolumes(rc->global_param_mvo_tr, previous.mvo_tr_count, pass.mvo_tr_count));
			passStates.push_back({sorted, color, oit, false});
			// Shadow depth accumulates opaque/punch-through and preceding
			// translucent depths up to its last OIT/modifier consumer. Later
			// color-only passes use their own depth attachment, never this one.
			if (sorted || color) shadowDepthPassCount = passStates.size();
			if (sorted) {
				// The reserved vector keeps this pointer stable. Only an OIT
				// resolve with a later OIT consumer needs to reset its heads.
				if (lastSorted) lastSorted->resetOITHeads = true;
				lastSorted = &passStates.back();
			}
			useOIT |= sorted;
			previous = pass;
		}
		return useOIT;
	}
	void drawModifiers(FCMetal4Commands * cb, FCMetal4RenderEncoder * __strong& e, const std::vector<ModifierVolumeParam>& list, u32 first, u32 last, bool translucent)
	{
		if (!config::ModifierVolumes || !modifiers) return;
		int base = -1;
		int primaryMode = -1;
		bool orderedModifiers = context()->device().rasterOrderGroupsSupported;
#ifdef METAL_RENDERER_TESTS
		orderedModifiers &= !disableOrderedModifiers;
#endif
		auto endTranslucentEncoder = [&] {
			if (e) { [e endEncoding]; e = nil; }
			primaryMode = -1;
		};
		// Shared translucent volumes use the opaque list's records, too.
		const size_t bank = &list == &rc->global_param_mvo_tr ? N2TrModifiers : N2Modifiers;
		for (u32 i = first; i < last; ++i) {
			const auto& pp = list[i];
			if (!pp.count) continue;
			if (base == -1) base = pp.first;
			const NSUInteger n2Offset = pp.isNaomi2() ? naomi2Offsets[bank].at(i) : 0;
			const int mode = !pp.isp.VolumeLast && pp.isp.DepthMode > 0 ? 1 : 0;
			if (translucent) {
				// Without raster-order groups, only same-mode XORs/ORs commute
				// on each node's stencil bit and can share an encoder.
				// Raster-order groups serialize dependent node accesses for the
				// same pixel across every modifier mode and primitive.
				if (e && !orderedModifiers && primaryMode != mode) endTranslucentEncoder();
				if (!e) { e = encoder(cb, RenderAttachments::None); primaryMode = mode; }
			}
			drawMod(e, pp, mode, pp.first, pp.count, translucent, n2Offset);
			if (pp.isp.DepthMode == 1 || pp.isp.DepthMode == 2) {
				// Inclusion/exclusion consumes preceding stencil results. ROG
				// orders these per-pixel accesses; other GPUs retain boundaries.
				if (translucent && !orderedModifiers) { endTranslucentEncoder(); e = encoder(cb, RenderAttachments::None); }
				drawMod(e, pp, pp.isp.DepthMode == 1 ? 2 : 3, base, pp.first + pp.count - base, translucent, n2Offset);
				base = -1;
				if (translucent && !orderedModifiers) endTranslucentEncoder();
			}
		}
		if (translucent) endTranslucentEncoder();
	}

	ResolvePipeline makeResolvePipeline(NSString *name)
	{
		ResolvePipeline pipeline;
		id<MTLComputePipelineState> state = context()->makeComputePipeline(name);
		const NSUInteger maximum = state.maxTotalThreadsPerThreadgroup;
		const NSUInteger executionWidth = state.threadExecutionWidth;
		const MTLSize limits = context()->device().maxThreadsPerThreadgroup;
		if (!maximum || !executionWidth || !limits.width || !limits.height || !limits.depth)
			throw RendererException("Metal framebuffer compute pipeline has invalid threadgroup limits");
		// Keep adjacent pixels in one SIMD row. Retain the previous
		// 64-thread budget unless one SIMD group itself is wider; large
		// layer arrays must not force the device's maximum group size.
		const NSUInteger width = std::min({executionWidth, maximum, limits.width});
		const NSUInteger budget = std::min(maximum, std::max<NSUInteger>(64, executionWidth));
		const NSUInteger height = std::min(budget / width, limits.height);
		pipeline.threads = MTLSizeMake(width, height, 1);
		pipeline.state = state;
		return pipeline;
	}
	const ResolvePipeline& resolvePipeline(u32 layers, bool resetHeads = false)
	{
		// Bound each thread's sorting array to the configured layer count.
		// Two bounded families keep reset work out of the last OIT pass.
		// Power-of-two layer bounds limit the cache to eighteen variants.
		u32 capacity = 1;
		while (capacity < layers) capacity <<= 1;
		const u32 key = capacity | (u32(resetHeads) << 9);
		auto& pipeline = resolvePipelines[key];
		if (!pipeline.state)
			pipeline = makeResolvePipeline([NSString stringWithFormat:@"oit_resolve_%u%@", capacity, resetHeads ? @"_reset" : @""]);
		return pipeline;
	}
	NSUInteger oitNodeBytes() const
	{
		constexpr size_t stride = MetalOITNodeBudgetStride;
		const size_t maximum = std::min<size_t>(context()->device().maxBufferLength,
			size_t(std::numeric_limits<u32>::max()) * stride);
		const size_t budget = std::min<size_t>(std::max<int64_t>(stride, config::PixelBufferSize), maximum);
		return budget / stride * stride;
	}
	void warmOITResources()
	{
		// Reserve reusable private-buffer storage before emulation starts.
		// Node/head storage is GPU-only and shared by consecutive command
		// buffers on this queue, like Vulkan's OITBuffers. Explicit Metal 4 queue
		// barriers order clears, fragment writes, modifier reads and resolves.
		// Only CPU-written/read counters need separate completion-owned slots.
		// Every consumed linked-node field is written by oitFragment. An
		// accumulator source needs no stored color; area 1 additionally needs
		// an active two-volume modifier. As with later buffer replacements,
		// only heads/counters need initialization;
		// clearing all node capacity would add work and a startup GPU wait.
		const NSUInteger bytes = oitNodeBytes() / MetalOITNodeBudgetStride * sizeof(MetalOITNode);
		nodes = [context()->device() newBufferWithLength:bytes
			options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked];
		if (!nodes) throw RendererException("Metal OIT warmup allocation failed");
		for (auto& resources : frameResources) {
			resources.counter = [context()->device() newBufferWithLength:8 options:MTLResourceStorageModeShared];
			if (!resources.counter) throw RendererException("Metal OIT warmup allocation failed");
		}
		NOTICE_LOG(RENDERER, "Metal OIT node buffer allocated: %llu bytes; optional area-1 colors allocated on demand", (unsigned long long)bytes);
	}
	void makeOITBuffers(FrameResources& resources)
	{
		// A 20-byte node plus optional 4-byte area-1 color keeps the previous
		// capacity and total budget. Separating colors makes linked traversal
		// denser without changing sequence or modifier metadata precision.
		frame.capacity = oitNodeBytes() / MetalOITNodeBudgetStride;
		frame.maxLayers = std::clamp(config::PerPixelLayers.get(), 1, 256);
		resolve = resolvePipeline(frame.maxLayers);
		// Explicit Metal 4 queue barriers order GPU-only storage reuse.
		// Growth replaces the buffer; submitted commands retain the old one.
		// Smaller images reuse capacity, with only their active heads cleared.
		// CPU counter storage still waits for its own completion semaphore.
		const NSUInteger nodeBytes = frame.capacity * sizeof(MetalOITNode);
		// The shader addresses heads with a uint pixel index. Validate before
		// multiplying so neither that index nor the host allocation can wrap.
		const NSUInteger maximumHeads = std::min<NSUInteger>(context()->device().maxBufferLength / sizeof(u32),
			std::numeric_limits<u32>::max());
		if (!frame.width || !frame.height || frame.width > maximumHeads / frame.height)
			throw RendererException("Metal OIT head buffer exceeds the device or shader index limit");
		const NSUInteger headBytes = NSUInteger(frame.width) * frame.height * sizeof(u32);
		if (!nodes || nodes.length != nodeBytes)
			nodes = [context()->device() newBufferWithLength:nodeBytes
				options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked];
		const NSUInteger color1Bytes = frame.capacity * sizeof(u32);
		if (oitColors1 && oitColors1.length != color1Bytes) oitColors1 = nil;
		reserveBuffer(headBytes, heads, MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked);
		if (!resources.counter)
			resources.counter = [context()->device() newBufferWithLength:8 options:MTLResourceStorageModeShared];
		counter = resources.counter;
		if (!nodes || !heads || !counter) throw RendererException("Metal per-pixel buffer allocation failed");
		// Store immutable per-polygon blend/shadow instructions once, as
		// Vulkan does. Completion-owned slots protect the CPU-written table.
		if (rc->global_param_tr.size() > (size_t(1) << 30))
			throw RendererException("Metal OIT polygon index exceeds metadata storage");
		const size_t polygonCount = std::max<size_t>(1, rc->global_param_tr.size());
		if (polygonCount > context()->device().maxBufferLength / sizeof(MetalOITPoly))
			throw RendererException("Metal OIT polygon table exceeds the device buffer limit");
		// The frame slot was acquired before building this table. Pack its
		// immutable records directly into shared GPU storage, avoiding a
		// temporary CPU vector and the following full-table upload copy.
		oitPolygons = reserveBuffer(polygonCount * sizeof(MetalOITPoly), resources.oitPolygons);
		auto *polygonData = static_cast<u8 *>(oitPolygons.contents);
		if (rc->global_param_tr.empty()) new (polygonData) MetalOITPoly{};
		bool needsColors1 = false;
		size_t passIndex = 0;
		for (size_t i = 0; i < rc->global_param_tr.size(); ++i) {
			const PolyParam& poly = rc->global_param_tr[i];
			const bool twoVolumes = hasTwoVolumes(poly);
			if (!needsColors1 && twoVolumes && poly.count >= 3 && !(poly.tsp1.full & (1u << 25))) {
				// Match the OIT shader's actual area-1 color writer. Unsorted,
				// unshadowable and accumulator-source polygons cannot access
				// this stream, even though their immutable table keeps both TSPs.
				while (passIndex < rc->render_passes.size() && i >= rc->render_passes[passIndex].tr_count)
					++passIndex;
				if (passIndex < passStates.size()) {
					const auto& state = passStates[passIndex];
					needsColors1 = state.sortedFragments
						&& activeTwoVolumes(poly, PolyPass::OIT, state.colorModifiers, state.oitModifiers);
				}
			}
			new (polygonData + i * sizeof(MetalOITPoly)) MetalOITPoly{(poly.tsp.full & ~3u) | (poly.pcw.Shadow ? 1u : 0u)
				| (twoVolumes ? 2u : 0u), poly.tsp1.full};
		}
		// Area-1 accumulator sources need no stored color. With no matching
		// modifier, metadata cannot select area 1 either. Keep allocated storage
		// across renders instead of reallocating on later volume transitions.
		if (needsColors1 && !oitColors1) {
			oitColors1 = [context()->device() newBufferWithLength:color1Bytes
				options:MTLResourceStorageModePrivate | MTLResourceHazardTrackingModeUntracked];
			if (!oitColors1) throw RendererException("Metal OIT area-1 color allocation failed");
		}
		// Until area 1 is needed, the counter supplies a valid dummy binding
		// at index 8. Shaders guard every color access; it is never dereferenced.
	}
	void clearOIT(FCMetal4Commands * cb)
	{
		FCMetal4ComputeEncoder * b = [cb transferEncoder];
		if (!b) throw RendererException("Metal OIT clear encoder allocation failed");
		const NSUInteger activeHeadBytes = NSUInteger(frame.width) * frame.height * sizeof(u32);
		[b fillBuffer:heads range:NSMakeRange(0, activeHeadBytes) value:255];
		[b fillBuffer:counter range:NSMakeRange(0, 4) value:0];
		[b endEncoding];
	}
	void resolveFramebuffer(FCMetal4Commands * cb, bool finalPass, FrameResources& resources, bool oit, bool resetHeads = false,
		id<MTLTexture> tiledOutput = nil, const metal_oit::Tile *tile = nullptr, u32 layerBound = 0)
	{
		const bool copyOnly = !oit && !(frame.dithering && finalPass);
		MTLTextureUsage usage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
		// Pooled intermediate images must remain usable by later compute
		// resolves. A final copy needs no shader-write capability.
		if (!copyOnly || !finalPass) usage |= MTLTextureUsageShaderWrite;
		// Intermediate images never escape this render. Screen output is
		// pooled only when no longer retained as the pending or displayed
		// image; RTT output selection also preserves sampled feedback images.
		const size_t scratch = target == resources.resolveScratch[0] ? 1 : 0;
		id<MTLTexture> output = tiledOutput ? tiledOutput : finalPass ? outputTexture(resources, usage, target)
			: attachment(resources.resolveScratch[scratch], MTLPixelFormatRGBA8Unorm, usage);
		if (copyOnly) {
			// No fragment list and no final dither: finish the output with a
			// texture copy, without invoking a sort kernel.
			FCMetal4ComputeEncoder * b = [cb transferEncoder];
			if (!b) throw RendererException("Metal framebuffer finish copy encoder allocation failed");
			[b copyFromTexture:target sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
				sourceSize:MTLSizeMake(frame.width, frame.height, 1) toTexture:output destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
			[b endEncoding];
		} else {
			const ResolvePipeline& pipeline = oit ? resolvePipeline(layerBound ? layerBound : frame.maxLayers, resetHeads) : finish;
			FCMetal4ComputeEncoder * c = [cb computeCommandEncoder];
			if (!c) throw RendererException("Metal framebuffer resolve encoder allocation failed");
			const NSUInteger frameOffset = !finalPass && frame.dithering ? frameUniformStride : 0;
			[c setComputePipelineState:pipeline.state];
			if (tile) {
				// setBytes copies this record into submission-owned aligned storage.
				// Each tile needs its own origin without changing the raster's frame.
				MetalFrameUniforms tiledFrame = frame;
				tiledFrame.oitTile = {tile->x, tile->y, tile->width, tile->height};
				if (!finalPass) tiledFrame.dithering = 0;
				[c setBytes:&tiledFrame length:sizeof(tiledFrame) atIndex:1];
			} else [c setBuffer:frameUniforms offset:frameOffset atIndex:1];
			if (oit) {
				[c setBuffer:nodes offset:0 atIndex:4]; [c setBuffer:heads offset:0 atIndex:5]; [c setBuffer:counter offset:0 atIndex:6];
				[c setBuffer:oitPolygons offset:0 atIndex:7];
				[c setBuffer:(oitColors1 ? oitColors1 : counter) offset:0 atIndex:8];
			}
			[c setTexture:target atIndex:0]; [c setTexture:output atIndex:1];
			[c dispatchThreads:MTLSizeMake(tile ? tile->width : frame.width, tile ? tile->height : frame.height, 1)
				threadsPerThreadgroup:pipeline.threads];
			[c endEncoding];
		}
		if (!tiledOutput) target = output;
	}

	std::vector<metal_oit::Tile> oitTiles(u32 first, u32 last, u32& layerBound) const
	{
		std::vector<metal_oit::Tile> triangles;
		const metal_oit::Tile whole{0, 0, frame.width, frame.height};
		for (u32 p = first; p < last; ++p) {
			const PolyParam& poly = rc->global_param_tr[p];
			// The parser merges strips into an earlier polygon and leaves a
			// zero-count record whose first still refers to the old vertex array.
			// Match drawPoly: these records have no index range to validate.
			if (poly.count < 3) continue;
			if (poly.first > rc->idx.size() || poly.count > rc->idx.size() - poly.first)
				throw RendererException("Metal OIT primitive index range is invalid");
			for (u32 i = 0; i + 2 < poly.count; ++i) {
				const u32 *index = rc->idx.data() + poly.first + i;
				if (index[0] == ~0u || index[1] == ~0u || index[2] == ~0u) continue;
				// Naomi2's shader transforms, or a noncanonical projection, need
				// the full screen bound. Positive Dreamcast W is constant one;
				// native-depth division cancels when forming screen X/Y as well.
				const auto& m = frame.ndc;
				bool bounded = !poly.isNaomi2() && m[0][3] == 0 && m[1][3] == 0 && m[2][3] == 0 && m[3][3] == 1
					&& m[0][2] == 0 && m[1][2] == 0 && m[3][2] == 0 && std::isnormal(m[2][2]) && m[2][2] > 0;
				double left = frame.width, top = frame.height, right = 0, bottom = 0;
				for (int j = 0; j < 3 && bounded; ++j) {
					if (index[j] >= rc->verts.size()) { bounded = false; break; }
					const auto& v = rc->verts[index[j]];
					// A nonpositive/subnormal reciprocal can cross homogeneous W
					// during native-depth clipping; vertex X/Y no longer bounds it.
					if (!std::isnormal(v.z) || v.z <= 0) { bounded = false; break; }
					const double reciprocal = double(m[2][2]) * v.z;
					if (reciprocal < 1.e-12 || reciprocal > 1.e5) { bounded = false; break; }
					const double values[]{v.x, v.y, v.z, 1};
					double screen[2]{};
					for (int axis = 0; axis < 2; ++axis) {
						double value = 0, magnitude = 0;
						for (int k = 0; k < 4; ++k) { const double term = double(m[k][axis]) * values[k]; value += term; magnitude += std::abs(term); }
						if (!std::isfinite(value) || !std::isfinite(magnitude) || magnitude > 65536) { bounded = false; break; }
						// Include raster subpixel rounding and float matrix error.
						const double size = axis == 0 ? frame.width : frame.height;
						const double margin = 4 + magnitude * std::numeric_limits<float>::epsilon() * 16 * size;
						screen[axis] = (value + 1) * size * .5;
						if (axis == 0) { left = std::min(left, screen[axis] - margin); right = std::max(right, screen[axis] + margin); }
						else { top = std::min(top, screen[axis] - margin); bottom = std::max(bottom, screen[axis] + margin); }
					}
				}
				if (!bounded) triangles.push_back(whole);
				else {
					const u32 x = u32(std::clamp(std::floor(left), 0.0, double(frame.width)));
					const u32 y = u32(std::clamp(std::floor(top), 0.0, double(frame.height)));
					const u32 endX = u32(std::clamp(std::ceil(right), 0.0, double(frame.width)));
					const u32 endY = u32(std::clamp(std::ceil(bottom), 0.0, double(frame.height)));
					if (endX > x && endY > y) triangles.push_back({x, y, endX - x, endY - y});
				}
			}
		}
		// At most one fragment per original triangle reaches a pixel. A
		// small pass therefore needs no large thread-local sorting array.
		// This bound never truncates the configured or attainable layer count.
		layerBound = u32(std::max<size_t>(1, std::min<size_t>(frame.maxLayers, triangles.size())));
		return metal_oit::plan(frame.width, frame.height, triangles, frame.capacity);
	}

	id<MTLTexture> scaleTexture(id<MTLTexture> image, int width, int height, FCMetal4Commands * cb = nil,
		id<MTLTexture> __strong *storage = nullptr, bool rotate = false)
	{
		if (!rotate && image.width == (NSUInteger)width && image.height == (NSUInteger)height) return image;
		id<MTLTexture> output = storage ? *storage : nil;
		if (!output || output == image || output.width != (NSUInteger)width || output.height != (NSUInteger)height)
			output = context()->makeTexture(width, height, MTLPixelFormatRGBA8Unorm);
		if (storage) *storage = output;
		const bool submit = !cb;
		if (!cb) cb = [context()->queue() commandBuffer];
		if (!cb) throw RendererException("Metal framebuffer scale command buffer allocation failed");
		MTL4RenderPassDescriptor *p = [MTL4RenderPassDescriptor new];
		p.colorAttachments[0].texture = output; p.colorAttachments[0].loadAction = MTLLoadActionDontCare; p.colorAttachments[0].storeAction = MTLStoreActionStore;
		FCMetal4RenderEncoder * e = [cb renderCommandEncoderWithDescriptor:p];
		if (!e) throw RendererException("Metal framebuffer scale encoder allocation failed");
		[e setRenderPipelineState:scalePipeline]; [e setViewport:MTLViewport{0, 0, (double)width, (double)height, 0, 1}];
		struct alignas(16) QuadUniform { float color[4]; u32 rotate, ignoreAlpha; } u{{1, 1, 1, 1}, u32(rotate), 0};
		[e setVertexBytes:&u length:sizeof(u) atIndex:0]; [e setFragmentBytes:&u length:sizeof(u) atIndex:0];
		[e setFragmentTexture:image atIndex:0];
		// Framebuffer resampling uses Vulkan's fixed linear blit filter.
		// These single-level images need clamp, no anisotropy and no mip blend.
		[e setFragmentSamplerState:samplerState(2, 2, true, false, 1) atIndex:0];
		[e drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4]; [e endEncoding];
		if (submit) context()->submitCommandBuffer(cb);
		return output;
	}

public:
#ifdef METAL_RENDERER_TESTS
	void validateSynthetic();
#endif
	explicit MetalRenderer(bool perPixel) : perPixel(perPixel) {}
	~MetalRenderer() override
	{
		if (pipelineResults) pipelineResults->active.store(false);
	}
	bool Init() override
	{
		try {
		@autoreleasepool {
			if (pipelineResults) pipelineResults->active.store(false);
			materialPipelines.clear();
			pipelineSelection = {};
			pipelineResults = std::make_shared<PipelineResults>();
			frameTimings = config::loadBool("log", "MetalFrameTiming", false);
			profilingProcess = 0;
			static_assert(sizeof(N2Light) == 112 && sizeof(N2LightModel) == 1888, "Metal Naomi2 light ABI");
			N2LightModel empty{};
			emptyLights = upload(&empty, sizeof(empty));
			MetalNaomi2Uniforms emptyN2{};
			emptyNaomi2 = upload(&emptyN2, sizeof(emptyN2));
			naomi2 = emptyNaomi2;
			BaseTextureCacheData::SetDirectXColorOrder(false);
			vertexLayout = [MTLVertexDescriptor vertexDescriptor];
			const MTLVertexFormat formats[] = { MTLVertexFormatFloat3, MTLVertexFormatUChar4Normalized, MTLVertexFormatUChar4Normalized,
				MTLVertexFormatFloat2, MTLVertexFormatUChar4Normalized, MTLVertexFormatUChar4Normalized, MTLVertexFormatFloat2, MTLVertexFormatFloat3 };
			const size_t offsets[] = { offsetof(Vertex, x), offsetof(Vertex, col), offsetof(Vertex, spc), offsetof(Vertex, u),
				offsetof(Vertex, col1), offsetof(Vertex, spc1), offsetof(Vertex, u1), offsetof(Vertex, nx) };
			for (size_t i = 0; i < 8; ++i) { vertexLayout.attributes[i].format = formats[i]; vertexLayout.attributes[i].offset = offsets[i]; }
			vertexLayout.layouts[0].stride = sizeof(Vertex);
			dcVertexLayout = [vertexLayout copy];
			dcVertexLayout.attributes[7].format = MTLVertexFormatInvalid;
			depthLayout = [MTLVertexDescriptor vertexDescriptor];
			depthLayout.attributes[0].format = MTLVertexFormatFloat3;
			depthLayout.attributes[0].offset = offsetof(Vertex, x);
			depthLayout.layouts[0].stride = sizeof(Vertex);
			modLayout = [MTLVertexDescriptor vertexDescriptor];
			modLayout.attributes[0].format = MTLVertexFormatFloat3; modLayout.layouts[0].stride = 3 * sizeof(float);
			if (perPixel) {
				const u32 layers = std::clamp(config::PerPixelLayers.get(), 1, 256);
				resolve = resolvePipeline(layers);
				resolvePipeline(layers, true);
				finish = makeResolvePipeline(@"framebuffer_finish");
			}
			MTLDepthStencilDescriptor *disabled = [[MTLDepthStencilDescriptor alloc] init];
			disabled.depthCompareFunction = MTLCompareFunctionAlways;
			disabledDepth = [context()->device() newDepthStencilStateWithDescriptor:disabled];
			if (!disabledDepth) throw RendererException("Metal disabled depth state allocation failed");
			scalePipeline = context()->makePipeline(@"quad_vertex", @"quad_fragment", nil, MTLPixelFormatRGBA8Unorm, false);
			samplerState(2, 2, true, false, 1);
			white = context()->makeTexture(1, 1, MTLPixelFormatRGBA8Unorm, MTLTextureUsageShaderRead, false, true);
			u32 pixel = ~0u; [white replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&pixel bytesPerRow:4];
			emptyStencil = context()->makeTexture(1, 1, MTLPixelFormatR8Uint, MTLTextureUsageShaderRead, false, true);
			u8 zero = 0; [emptyStencil replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&zero bytesPerRow:1];
			if (perPixel) warmOITResources();
			// Compile the common opaque and alpha-blended pipelines before
			// emulation starts. Lazy compilation can otherwise stall the first
			// animated frames for tens of milliseconds.
			PolyParam warm{};
			warm.init();
			// Warm both vertex families; position-only pipelines still share a key.
			for (bool naomi2 : {false, true}) {
				warm.projMatrix = naomi2 ? 0 : -1;
				for (bool alpha : {false, true}) {
					warm.tsp.SrcInstr = alpha ? 4 : 1;
					warm.tsp.DstInstr = alpha ? 5 : 0;
					if (perPixel) {
						pipeline(warm, PolyPass::Depth, true);
						pipeline(warm, PolyPass::Depth, true, true);
						pipeline(warm, PolyPass::Color0, true);
						pipeline(warm, PolyPass::OIT, false);
					} else {
						pipeline(warm, PolyPass::Direct, true);
						pipeline(warm, PolyPass::DepthOnly, true);
					}
				}
			}
			if (perPixel) {
				MetalPolyUniforms accumulatorSource{};
				accumulatorSource.tsp0 = 1u << 25;
				pipeline(warm, PolyPass::OIT, false, false, &accumulatorSource);
			}
			return true;
		}
		} catch (const std::exception& e) {
			ERROR_LOG(RENDERER, "Metal renderer initialization failed: %s", e.what());
			return false;
		}
	}
	void Term() override
	{
		if (pipelineResults) pipelineResults->active.store(false);
		context()->waitIdle(); context()->setLastFrame(nil, 4.f / 3.f);
		textureCache.Clear(); fog.Delete(); palette.Delete(); framebuffer.Delete();
		pipelines.clear(); depths.fill(nil); modifierDepths.fill(nil); finalModifierDepth = nil;
		samplers.fill(nil); resolvePipelines.clear();
		materialPipelines.clear(); pipelineResults.reset();
		pipelineSelection = {};
		drawState = {};
		tileClipState = {};
		for (auto& resources : frameResources) {
			resources.counter = nil;
			resources.vertices = resources.indices = resources.modifiers = resources.lights = resources.oitPolygons = resources.naomi2 = resources.frameUniforms = nil;
			resources.shadowDepth = resources.shadowStencil = resources.rasterColor = nil;
			resources.screenOutput = resources.rttReadbackOutput = resources.framebufferScale = nil;
			resources.resolveScratch.fill(nil);
			resources.polyUniformPages.clear(); resources.polyUniformCursor = 0;
		}
		nextFrameResource = currentFrameResource = 0;
		naomi2 = emptyNaomi2 = nil;
		for (auto& offsets : naomi2Offsets) offsets.clear();
		frameUniforms = nil;
		screen = screenDepth = depth = shadowDepth = shadowStencil = emptyStencil = target = white = nil; vertices = indices = modifiers = lights = nodes = oitColors1 = heads = counter = emptyLights = oitPolygons = nil;
		resolve = {}; finish = {}; disabledDepth = nil; scalePipeline = nil; rc = nullptr;
		screenRendered = framebufferRendered = false;
		discardInitialColor = false;
		backgroundColorCleared = initialColorClear = false;
		skipBackgroundDepth = false;
		colorModifiers = oitModifiers = false;
		passStates.clear();
		shadowDepthPassCount = 0;
		lightOffsets.clear();
	}
	BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw, int area) override
	{
		MetalTexture *texture = textureCache.getTextureCacheData(tsp, tcw, area);
		if (texture->NeedsUpdate()) { if (!texture->Update()) return nullptr; }
		else if (texture->IsCustomTextureAvailable()) texture->CheckCustomTexture();
		return texture;
	}
	void Process(TA_context *ctx) override
	{
		@autoreleasepool {
			const u64 processNumber = frameTimings ? ++profilingProcess : 0;
			const bool profile = frameTimings && processNumber <= 900;
			const auto started = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
			{
				recoverCanceledRender();
				if (ctx->rend.isRTT || config::EmulateFramebuffer) context()->flushRenderCommands();
				struct TextureUpdates {
					MetalContext *context;
					bool readyForRender = false;
					explicit TextureUpdates(MetalContext *context) : context(context) { context->beginTextureUpdates(); }
					~TextureUpdates() { context->endTextureUpdates(readyForRender); }
				} textureUpdates(context());
				rc = &ctx->rend;
				if (resetTextureCache) { textureCache.Clear(); resetTextureCache = false; }
				textureCache.CollectCleanup();
				if (!rc->isRTT) { framebufferRendered = false; if (!config::EmulateFramebuffer) clearLastFrame = false; }
				if (!rc->isRTT && rc->swapInterval > 0) context()->setSwapInterval(rc->swapInterval);
				ta_parse(ctx, true);
				if (!fog.image || (config::Fog && updateFogTable)) {
					u8 pixels[256]; MakeFogTexture(pixels); fog.tex_type = TextureType::_8;
					fog.UploadToGPU(128, 2, pixels, false); updateFogTable = false;
				}
				if (updatePalette || !palette.image) {
					palette.tex_type = TextureType::_8888; palette.UploadToGPU(1024, 1, (u8 *)palette32_ram, false); updatePalette = false;
				}
				textureUpdates.readyForRender = true;
			}
#ifdef FLYCAST_FRAME_CAPTURE_TESTS
			metal_capture::fingerprint(ctx->rend);
#endif
			// Includes parsing/decoding, cache work and CPU upload preparation,
			// which occur before Render's encoding/slot timing starts.
			if (profile) NOTICE_LOG(RENDERER, "Metal timing process %llu CPU: %.3f ms",
				(unsigned long long)processNumber,
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
		}
	}
	bool Render() override
	{
		@autoreleasepool {
			discardInitialColor = false;
			backgroundColorCleared = initialColorClear = false;
			skipBackgroundDepth = false;
			recoverCanceledRender();
			if (!rc || rc->framebufferWidth == 0 || rc->framebufferHeight == 0) {
				context()->flushTextureUpdates();
				return false;
			}
			// Retain Process uploads and append this render to their command
			// buffer, matching Vulkan's joint upload/render submission.
			auto commands = context()->beginRenderCommands([abort = renderAbort] {
				abort->canceled = true;
			}, !rc->isRTT && !config::EmulateFramebuffer);
			// Publish ready variants once per frame, keeping compiler locks out
			// of every polygon draw and one shader choice stable for that frame.
			collectMaterialPipelines();
			const u64 frameNumber = frameTimings ? ++profilingFrame : 0;
			const bool profile = frameTimings && frameNumber <= 900;
			profilingBoundEligible = profilingBoundActive = 0;
			profilingUVEligible = profilingUVActive = 0;
			const auto started = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
			currentFrameResource = nextFrameResource++ % frameResources.size();
			FrameResources& resources = frameResources[currentFrameResource];
			dispatch_semaphore_t sem = resources.available;
			dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
			const auto encodingStarted = profile ? std::chrono::steady_clock::now() : started;
			FlightTicket ticket{sem};
			resources.polyUniformCursor = 0;
			FCMetal4Commands * cb = commands.buffer();
			// Build all per-render resources before encoding. The Metal 4 submission
			// owner retains bound resources, including prior RTT feedback images.
			frame = {};
			oitRasterTile = {};
			frame.width = rc->framebufferWidth; frame.height = rc->framebufferHeight;
			matrices.CalcMatrices(rc, frame.width, frame.height); frame.ndc = matrices.GetNormalMatrix();
			baseScissor = matrices.getBaseScissor();
			tileClipState = {};
			rc->fog_clamp_min.getRGBAColor(glm::value_ptr(frame.clampMin)); rc->fog_clamp_max.getRGBAColor(glm::value_ptr(frame.clampMax));
			FOG_COL_RAM.getRGBColor(glm::value_ptr(frame.fogRam)); FOG_COL_VERT.getRGBColor(glm::value_ptr(frame.fogVertex));
			frame.fogDensity = FOG_DENSITY.get() * config::ExtraDepthScale;
			frame.alphaRef = (PT_ALPHA_REF & 255) / 255.f; frame.shadeScale = FPU_SHAD_SCALE.scale_factor / 256.f;
			frame.dither = glm::vec4(1);
			frame.dithering = config::EmulateFramebuffer && rc->fb_W_CTRL.fb_dither && rc->fb_W_CTRL.fb_packmode <= 3;
			if (rc->fb_W_CTRL.fb_packmode == 0 || rc->fb_W_CTRL.fb_packmode == 3) frame.dither = glm::vec4(2, 2, 2, 1);
			else if (rc->fb_W_CTRL.fb_packmode == 1) frame.dither = glm::vec4(2, 4, 2, 1);
			setFirstProvokingVertex(*rc);
			vertices = upload(rc->verts.data(), rc->verts.size() * sizeof(Vertex), resources.vertices);
			indices = upload(rc->idx.data(), rc->idx.size() * sizeof(u32), resources.indices);
			const bool useOIT = preparePassStates();
			// Per-pixel modifiers need a selected nonempty color/OIT consumer.
			// Standard passes retain their final quad even for zero-count entries;
			// their cumulative opaque range therefore remains the conservative gate.
			const bool useModifiers = config::ModifierVolumes && !rc->modtrig.empty()
				&& (perPixel ? std::any_of(passStates.begin(), passStates.end(),
					[](const PassState& state) { return state.colorModifiers || state.oitModifiers; })
					: !rc->render_passes.empty() && rc->render_passes.back().mvo_count != 0);
			// Keep the completion-owned pool for later reuse without copying
			// geometry which no selected pass can read in this render.
			prepareModifiers(resources, useModifiers);
			// Shadow depth feeds OIT's manual test; its stencil feeds modifier
			// colors. Without either consumer in this render, skip that whole path.
			const bool useShadowDepth = shadowDepthPassCount != 0;
			const bool useResolve = perPixel && (frame.dithering || useOIT);
			prepareLights(resources);
			prepareNaomi2(resources);
			const bool clear = rc->isRTT || rc->clearFramebuffer || !screen
				|| screen.width != frame.width || screen.height != frame.height;
			discardInitialColor = !clear && replacesFramebufferColor();
			backgroundColorCleared = initialColorClear = prepareBackgroundClear();
			const MTLTextureUsage colorUsage = MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget;
			// Multiple renders can build one pending image before Present, as
			// Vulkan's open screen pass does. Continue that unpublished image
			// directly instead of copying the entire framebuffer each time.
			// GPU writes are ordered on the same queue with explicit Metal 4 barriers;
			// an image handed to the presentation/UI path must remain separate.
			const bool reusePending = !clear && screenRendered && screen != context()->lastFrame()
				&& (screen.usage & colorUsage) == colorUsage;
			if (reusePending) target = screen;
			else target = useResolve && !rc->render_passes.empty() ? attachment(resources.rasterColor, MTLPixelFormatRGBA8Unorm)
				: outputTexture(resources, colorUsage);
			// Vulkan's standard screen render pass remains open across renders
			// until Present (or RTT). Preserve its depth for the same pending
			// image, while starting every subsequently presented frame fresh.
			if (rc->isRTT) screenDepth = nil;
			const bool preserveScreenDepth = !perPixel && !rc->isRTT && !config::EmulateFramebuffer;
			const bool continueDepth = preserveScreenDepth && screenRendered && !rc->clearFramebuffer
				&& screenDepth && screenDepth.width == frame.width && screenDepth.height == frame.height;
			// This GPU-only attachment stays on one queue. Metal 4 barriers order
			// reuse across submissions; screenDepth retains pending continuations.
			depth = continueDepth ? screenDepth : attachment(depth, MTLPixelFormatDepth32Float_Stencil8);
			const bool keepStandardRaster = preserveScreenDepth;
			FCMetal4RenderEncoder * standardEncoder = nil;
			if (keepStandardRaster && !clear && continueDepth && target == screen)
				standardEncoder = commands.continuingRasterEncoder(target, depth);
			else commands.endRasterEncoder();
			if (standardEncoder) {
				// A retained encoder has already loaded its attachments. Keep
				// the ordinary background draw instead of reopening that pass.
				discardInitialColor = backgroundColorCleared = initialColorClear = false;
			}
			// With no Z write and an always-pass test, only the stencil replace
			// remains. A fresh standard stencil, or OIT's first shadow stencil,
			// already contains the zero written by this non-shadow background.
			// Retained standard depth/stencil still requires the original draw.
			skipBackgroundDepth = backgroundColorCleared && rc->global_param_op.front().isp.ZWriteDis
				&& (perPixel || !continueDepth);
			if (useShadowDepth) {
				shadowDepth = attachment(resources.shadowDepth, MTLPixelFormatDepth32Float_Stencil8,
					MTLTextureUsageShaderRead | MTLTextureUsageRenderTarget | MTLTextureUsagePixelFormatView);
				if (!resources.shadowStencil || resources.shadowStencil.parentTexture != shadowDepth)
					resources.shadowStencil = [shadowDepth newTextureViewWithPixelFormat:MTLPixelFormatX32_Stencil8];
				if (!resources.shadowStencil) throw RendererException("Metal shadow stencil view allocation failed");
				shadowStencil = resources.shadowStencil;
			} else shadowDepth = shadowStencil = nil;
			if (!clear && target != screen && !discardInitialColor) {
				FCMetal4ComputeEncoder * b = [cb transferEncoder];
				if (!b) throw RendererException("Metal framebuffer copy encoder allocation failed");
				[b copyFromTexture:screen sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
					sourceSize:MTLSizeMake(frame.width, frame.height, 1) toTexture:target destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)]; [b endEncoding];
			}
			if (useOIT) { makeOITBuffers(resources); memset(counter.contents, 0, 8); }
			prepareFrameUniforms(resources);
			id<MTLBuffer> diagnostics = useOIT ? counter : nil;
			[cb addCompletedHandler:^(FCMetal4Commands * done) {
				if (done.status == FCMetal4StatusError) ERROR_LOG(RENDERER, "Metal render: %s", done.error.localizedDescription.UTF8String);
				if (diagnostics) {
					u32 status = ((u32 *)diagnostics.contents)[1];
					if (status) WARN_LOG(RENDERER, "Metal OIT limit reached: %s%s", status & 1 ? "pixel buffer " : "", status & 2 ? "layers" : "");
				}
				dispatch_semaphore_signal(sem);
			}];
			commands.retainRenderSlot(sem);
			ticket.submitted = true; // Completion or batch cancellation now owns this slot.
			// An empty per-pixel render still initializes the pooled output and
			// depth. Standard empty renders use the shared encoder below.
			if (perPixel && rc->render_passes.empty()) {
				FCMetal4RenderEncoder * e = encoder(cb, RenderAttachments::ColorDepth,
					clear, !continueDepth, !continueDepth, nil, preserveScreenDepth);
				[e endEncoding];
			}
			// Standard passes and compatible subsequent screen renders have no
			// intervening attachment reads. Keep color/depth/stencil in the
			// command owner's encoder until a copy, clear or submission boundary.
			if (!perPixel) {
				if (standardEncoder) bindFrame(standardEncoder);
				else {
					standardEncoder = encoder(cb, RenderAttachments::ColorDepth,
						clear, !continueDepth, !continueDepth, nil, preserveScreenDepth, keepStandardRaster);
					commands.retainRasterEncoder(standardEncoder, target, depth, keepStandardRaster);
				}
			}
			// After the last shadow/OIT consumer, the remaining color passes
			// keep their attachments in one encoder until the final output step.
			FCMetal4RenderEncoder * colorOnlyEncoder = nil;
			RenderPass previous{};
			bool firstOITPass = true;
			for (size_t passIndex = 0; passIndex < rc->render_passes.size(); ++passIndex) {
				const RenderPass& pass = rc->render_passes[passIndex];
				const bool finalPass = passIndex + 1 == rc->render_passes.size();
				const bool sortedFragments = perPixel ? passStates[passIndex].sortedFragments
					: pass.autosort && pass.tr_count > previous.tr_count;
				if (perPixel) {
					// Active shadow stencil is cleared per pass; OIT modifier bits
					// start at zero. Only these selected, nonempty draws can
					// make area 1 or shadow dimming visible to their consumers.
					const auto& state = passStates[passIndex];
					colorModifiers = state.colorModifiers;
					oitModifiers = state.oitModifiers;
				}
				// Intermediate color-only passes can retain their current image.
				// A frame using scratch images still publishes a distinct final image.
				const bool passResolve = useResolve && (sortedFragments || finalPass);
				FCMetal4RenderEncoder * e = standardEncoder;
				if (perPixel) {
					const bool shadowDepthNeeded = passIndex < shadowDepthPassCount;
					const bool retainShadowDepth = passIndex + 1 < shadowDepthPassCount;
					if (shadowDepthNeeded) {
						// Retain preceding depths when a later logical pass has a
						// consumer, including passes which currently have none.
						e = encoder(cb, RenderAttachments::DepthOnly, false, passIndex == 0,
							true, shadowDepth, retainShadowDepth || sortedFragments);
						drawList(e, rc->global_param_op, previous.op_count, pass.op_count, ListType_Opaque, false, PolyPass::Depth);
						drawList(e, rc->global_param_pt, previous.pt_count, pass.pt_count, ListType_Punch_Through, false, PolyPass::Depth);
						drawModifiers(cb, e, rc->global_param_mvo, previous.mvo_count, pass.mvo_count, false);
						[e endEncoding];
					}
					// Initialize color before its first draw, including when the
					// shadow path or the color list is empty.
					// Later color passes load this depth, but the final pass has
					// no consumer. OIT's manual depth test reads shadowDepth instead.
					if (!shadowDepthNeeded && !colorOnlyEncoder)
						colorOnlyEncoder = encoder(cb, RenderAttachments::ColorDepth,
							passIndex == 0 && clear, passIndex == 0, false, nil, false);
					e = shadowDepthNeeded ? encoder(cb, RenderAttachments::ColorDepth, passIndex == 0 && clear,
						passIndex == 0, false, nil, !finalPass) : colorOnlyEncoder;
					drawList(e, rc->global_param_op, previous.op_count, pass.op_count, ListType_Opaque, false, PolyPass::Color0);
					drawList(e, rc->global_param_pt, previous.pt_count, pass.pt_count, ListType_Punch_Through, false, PolyPass::Color0);
					if (!passResolve) {
						// No sorted contribution in this pass: retain its color
						// image without clearing or resolving an empty OIT list.
						drawList(e, rc->global_param_tr, previous.tr_count, pass.tr_count, ListType_Translucent, false, PolyPass::Color0);
						if (shadowDepthNeeded || finalPass) [e endEncoding];
					} else if (sortedFragments) {
						[e endEncoding];
						// Every render initializes independently, including after
						// resize, cancellation or an earlier failed command buffer.
						// Subsequent OIT passes consume the preceding resolve's reset.
						if (firstOITPass) { clearOIT(cb); firstOITPass = false; }
						u32 layerBound;
						const auto tiles = oitTiles(previous.tr_count, pass.tr_count, layerBound);
						const bool tiled = tiles.size() > 1;
						id<MTLTexture> tiledOutput = nil;
						if (tiled) {
							const MTLTextureUsage usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite | MTLTextureUsageRenderTarget;
							const size_t scratch = target == resources.resolveScratch[0] ? 1 : 0;
							tiledOutput = finalPass ? outputTexture(resources, usage, target)
								: attachment(resources.resolveScratch[scratch], MTLPixelFormatRGBA8Unorm, usage);
							if (profile) NOTICE_LOG(RENDERER, "Metal OIT bounded pass: %zu tiles, %u nodes", tiles.size(), frame.capacity);
						}
						for (const auto& tile : tiles) {
							// Scissors split screen coverage, never geometry or sort keys.
							// The same full-frame viewport preserves derivatives and seams.
							oitRasterTile = tiled ? tile : metal_oit::Tile{};
							e = encoder(cb, RenderAttachments::None);
							drawList(e, rc->global_param_tr, previous.tr_count, pass.tr_count, ListType_Translucent, false, PolyPass::OIT);
							[e endEncoding]; e = nil;
							if (pass.mv_op_tr_shared) drawModifiers(cb, e, rc->global_param_mvo, previous.mvo_count, pass.mvo_count, true);
							else drawModifiers(cb, e, rc->global_param_mvo_tr, previous.mvo_tr_count, pass.mvo_tr_count, true);
							// Reclaim nodes after each disjoint tile. Every tile reads
							// the same opaque input and writes its own output pixels.
								resolveFramebuffer(cb, finalPass, resources, true, tiled || passStates[passIndex].resetOITHeads,
									tiledOutput, tiled ? &tile : nullptr, layerBound);
						}
						oitRasterTile = {};
						if (tiledOutput) target = tiledOutput;
					} else {
						// Vulkan's unsorted OIT color subpass also reads the
						// opaque modifier stencil (including volume selection).
						drawList(e, rc->global_param_tr, previous.tr_count, pass.tr_count, ListType_Translucent, false, PolyPass::Color0); [e endEncoding];
						// This pass has no OIT list. Old heads need not be cleared:
						// the finish path cannot read them. The first OIT clear or
						// preceding OIT resolve prepares them for the next OIT pass.
						resolveFramebuffer(cb, finalPass, resources, false);
					}
					// Only a later shadow/OIT consumer can read continuation depth.
					// An empty translucent range cannot update it either.
					if (retainShadowDepth && pass.tr_count > previous.tr_count) {
						e = encoder(cb, RenderAttachments::DepthOnly, false, false, false, shadowDepth);
						drawList(e, rc->global_param_tr, previous.tr_count, pass.tr_count, ListType_Translucent, pass.autosort, PolyPass::Depth);
						[e endEncoding];
					}
				} else {
					drawList(e, rc->global_param_op, previous.op_count, pass.op_count, ListType_Opaque, false, PolyPass::Direct);
					drawList(e, rc->global_param_pt, previous.pt_count, pass.pt_count, ListType_Punch_Through, false, PolyPass::Direct);
					// Vulkan skips the complete modifier path for an empty range.
					// Avoid its final quad and cache reset in unrelated logical passes.
					if (pass.mvo_count > previous.mvo_count) {
						drawModifiers(cb, e, rc->global_param_mvo, previous.mvo_count, pass.mvo_count, false);
						drawFinalModifier(e);
					}
					if (pass.autosort && !config::PerStripSorting) {
						for (u32 i = previous.sorted_tr_count; i < pass.sorted_tr_count; ++i) {
							const auto& tri = rc->sortedTriangles[i];
							drawPoly(e, rc->global_param_tr[tri.polyIndex], ListType_Translucent, true, PolyPass::Direct, tri.first, tri.count);
						}
						if (config::TranslucentPolygonDepthMask && passIndex + 1 < rc->render_passes.size()) {
							for (u32 i = previous.sorted_tr_count; i < pass.sorted_tr_count; ++i) {
								const auto& tri = rc->sortedTriangles[i]; const auto& pp = rc->global_param_tr[tri.polyIndex];
								if (!pp.isp.ZWriteDis) drawPoly(e, pp, ListType_Translucent, true, PolyPass::DepthOnly, tri.first, tri.count);
							}
						}
					} else drawList(e, rc->global_param_tr, previous.tr_count, pass.tr_count, ListType_Translucent, pass.autosort, PolyPass::Direct);
				}
				previous = pass;
			}
			if (standardEncoder && !keepStandardRaster) commands.endRasterEncoder();
			std::unique_ptr<MetalContext::TextureReadback> readback;
			glm::ivec2 framebufferSize{};
			Rect framebufferClip;
			if (rc->isRTT && config::RenderToTextureBuffer) {
				readback = context()->encodeReadTexture(target, cb);
			} else if (!rc->isRTT && config::EmulateFramebuffer) {
				getWriteFBToVramParams(*rc, framebufferSize, framebufferClip);
				id<MTLTexture> scaled = scaleTexture(target, framebufferSize.x, framebufferSize.y, cb, &resources.framebufferScale);
				readback = context()->encodeReadTexture(scaled, cb);
			}
			if (profile) NOTICE_LOG(RENDERER, "Metal timing render %llu slot wait: %.3f ms CPU encode: %.3f ms background clear: %d background depth skipped: %d bounded eligible: %u bounded active: %u colour UV eligible: %u colour UV active: %u", (unsigned long long)frameNumber,
				std::chrono::duration<double, std::milli>(encodingStarted - started).count(),
				std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - encodingStarted).count(), backgroundColorCleared, skipBackgroundDepth,
				profilingBoundEligible, profilingBoundActive, profilingUVEligible, profilingUVActive);
			if (!rc->isRTT && !config::EmulateFramebuffer)
				context()->deferRenderCommands(std::move(commands));
			else commands.commit();
			std::vector<u8> packedPixels;
			const u8 *pixels = readback ? readback->finishForConversion(packedPixels) : nullptr;
			if (rc->isRTT) {
				if (config::RenderToTextureBuffer) {
					WriteTextureToVRam(frame.width, frame.height, pixels, (u16 *)&vram[rc->fb_W_SOF1 & VRAM_MASK], rc->fb_W_CTRL, rc->fb_W_LINESTRIDE * 8, rc->fbClip);
				} else {
					int w, h; getPvrFramebufferSize(*rc, w, h);
					MetalTexture *texture = textureCache.getRTTexture(rc->fb_W_SOF1 & VRAM_MASK, rc->fb_W_CTRL.fb_packmode, w, h);
					texture->image = target; texture->dirty = 0; texture->unprotectVRam();
				}
			} else {
				screen = target;
				screenDepth = preserveScreenDepth ? depth : nil;
				screenRendered = !config::EmulateFramebuffer;
				if (config::EmulateFramebuffer) {
					WriteFramebuffer(framebufferSize.x, framebufferSize.y, pixels, rc->fb_W_SOF1 & VRAM_MASK,
						rc->fb_W_CTRL, rc->fb_W_LINESTRIDE * 8, framebufferClip);
				}
			}
			// Conversion has finished reading the mapped buffer. Only now may
			// another readback acquire and overwrite the pooled storage.
			readback.reset();
			return !rc->isRTT;
		}
	}
	void RenderFramebuffer(const FramebufferInfo& info) override
	{
		@autoreleasepool {
			context()->flushRenderCommands();
			framebuffer.tex_type = TextureType::_8888;
			if (!info.fb_r_ctrl.fb_enable || info.vo_control.blank_video) {
				u8 pixel[] = {(u8)info.vo_border_col._red, (u8)info.vo_border_col._green, (u8)info.vo_border_col._blue, 255};
				framebuffer.UploadToGPU(1, 1, pixel, false);
			} else {
				PixelBuffer<u32> pixels; int width, height; ReadFramebuffer(info, pixels, width, height);
				framebuffer.UploadToGPU(width, height, (u8 *)pixels.data(), false);
			}
			framebufferRendered = true; clearLastFrame = false;
		}
	}
	void SetPresentationTime(double time) override { presentationVideoTime = time; }
	bool Present() override
	{
		recoverCanceledRender();
		// Only a successful screen-image handoff retires its continuation
		// depth. Skipped swaps and GUI-only flushes may precede another render.
		const bool discardDepth = !perPixel && !clearLastFrame && screenRendered && screen
			&& !framebufferRendered && !config::EmulateFramebuffer;
		context()->flushRenderCommands(discardDepth);
		// PVR may request another swap without rendering a new image. Match
		// GL/Vulkan: consume a completed image once, and leave RTT-only work
		// out of the display queue. Old images remain available to the GUI.
		if (clearLastFrame || (!screenRendered && !framebufferRendered)) return false;
		if (framebufferRendered || config::EmulateFramebuffer) {
			if (!framebufferRendered || !framebuffer.image) return false;
			context()->setLastFrame(framebuffer.image, getDCFramebufferAspectRatio(), presentationVideoTime); framebufferRendered = false;
		} else {
			if (!screen) return false;
			context()->setLastFrame(screen, getOutputFramebufferAspectRatio(), presentationVideoTime);
		}
		imguiDriver->setFrameRendered();
		screenRendered = false;
		screenDepth = nil;
		context()->drawOverlays();
		gui_display_osd();
#ifdef FLYCAST_FRAME_CAPTURE_TESTS
		metal_capture::frame(this, presentationVideoTime);
#endif
		return true;
	}
	bool RenderLastFrame() override
	{
		recoverCanceledRender();
		if (clearLastFrame) { context()->setLastFrame(nil, 4.f / 3.f); return false; }
		return context()->lastFrame() != nil;
	}
	bool GetLastFrame(std::vector<u8>& data, int& width, int& height) override
	{
		if (!RenderLastFrame()) return false;
		id<MTLTexture> texture = context()->lastFrame();
		const float aspect = context()->lastFrameAspectRatio();
		if (width < 0 || height < 0 || !std::isfinite(aspect) || aspect <= 0) return false;
		const bool rotate = config::Rotate90;
		// Match Vulkan: an explicit width takes precedence; otherwise use
		// height, or expand the retained image to square output pixels.
		double outWidth = width, outHeight = height;
		// Retain Vulkan's float arithmetic before truncating dimensions; a
		// wider intermediate can turn an integral 4:3 result into one less row.
		if (width > 0) outHeight = width / aspect;
		else if (height > 0) outWidth = aspect * height;
		else {
			outWidth = rotate ? texture.height : texture.width;
			outHeight = rotate ? texture.width : texture.height;
			const double squareWidth = std::floor(aspect * float(outHeight));
			if (outWidth > squareWidth) outHeight = float(outWidth) / aspect;
			else outWidth = squareWidth;
		}
		const double maximum = std::numeric_limits<int>::max();
		if (!std::isfinite(outWidth) || !std::isfinite(outHeight) || outWidth > maximum || outHeight > maximum)
			return false;
		width = std::max(1, int(outWidth)); height = std::max(1, int(outHeight));
		// Scale/rotate before readback, as Vulkan does. A savestate thumbnail
		// must not transfer the full high-resolution image and resample on CPU.
		context()->flushTextureUpdates();
		FCMetal4Commands * cb = [context()->queue() commandBuffer];
		if (!cb) throw RendererException("Metal screenshot command buffer allocation failed");
		id<MTLTexture> output = scaleTexture(texture, width, height, cb, nullptr, rotate);
		auto readback = context()->encodeReadTexture(output, cb);
		context()->submitCommandBuffer(cb);
		std::vector<u8> scratch;
		const u8 *pixels = readback->finishForConversion(scratch);
		const size_t pixelCount = size_t(width) * height;
		data.resize(pixelCount * 3);
		for (size_t i = 0; i < pixelCount; ++i)
			std::copy_n(pixels + i * 4, 3, data.data() + i * 3);
		return true;
	}
};
#ifdef METAL_RENDERER_TESTS
#include "../../../tests/metal/metal_renderer_validation.inc"
#endif
}

Renderer *rend_Metal(bool perPixel) { return new MetalRenderer(perPixel); }

#ifdef METAL_RENDERER_TESTS
void metalValidateRenderer()
{
	RamRegion previousVRAM = vram;
	const auto previousPlatform = settings.platform;
	settings.platform.system = DC_PLATFORM_DREAMCAST;
	settings.platform.vram_size = 8_MB; settings.platform.vram_mask = 8_MB - 1;
	std::vector<u8> testVRAM(8_MB);
	vram.setRegion(testVRAM.data(), testVRAM.size());
	try {
		for (bool perPixel : {false, true}) {
			MetalRenderer native(perPixel);
			if (!native.Init()) throw RendererException("Metal validation initialization failed");
			native.validateSynthetic();
			native.Term();
		}
	} catch (...) { vram = previousVRAM; settings.platform = previousPlatform; throw; }
	vram = previousVRAM;
	settings.platform = previousPlatform;
}
#endif
#endif // Apple Silicon Metal 4 slice
