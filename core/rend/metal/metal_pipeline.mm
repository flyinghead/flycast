/*
	Copyright 2025 flyinghead

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

#include "metal_pipeline.h"

#include "metal_context.h"
#include "metal_renderer.h"

MetalPipelineManager::MetalPipelineManager(MetalShaders *shaderManager) {
	this->shaderManager = shaderManager;
}

void MetalPipelineManager::CreateBlitPassPipeline() {
	MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
	[descriptor setLabel:@"Blit Pass"];

	auto attachment = [descriptor colorAttachments][0];
	[attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

	[descriptor setVertexFunction:shaderManager->GetBlitVertexShader()];
	[descriptor setFragmentFunction:shaderManager->GetBlitFragmentShader()];
	if (descriptor.vertexFunction == nil || descriptor.fragmentFunction == nil) {
		ERROR_LOG(RENDERER, "CreateBlitPassPipeline: shader compile failed");
		return;
	}

	NSError *error = nil;
	auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

	if (state == nil) {
		ERROR_LOG(RENDERER, "Failed to create Blit Pipeline State: %s", [[error localizedDescription] UTF8String]);;
	}

	blitPassPipeline = state;
}

void MetalPipelineManager::CreateModVolPipeline(ModVolMode mode, bool naomi2) {
	MTLVertexDescriptor *vertexDesc = nil;
	MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];

	[descriptor setLabel:@"Mod Vol Pass"];

	if (mode == ModVolMode::Final) {
		[descriptor setVertexDescriptor:GetMainVertexInputDescriptor(false, naomi2)];
	}
	else {
		vertexDesc = [[MTLVertexDescriptor alloc] init];

		auto layout = [vertexDesc layouts][30];
		[layout setStride:sizeof(float) * 3];

		auto attribute = [vertexDesc attributes][0];
		[attribute setOffset:0];
		[attribute setBufferIndex:30];
		[attribute setFormat:MTLVertexFormatFloat3];

		[descriptor setVertexDescriptor:vertexDesc];
	}

	auto attachment = [descriptor colorAttachments][0];
	[attachment setBlendingEnabled:mode == ModVolMode::Final];
	[attachment setSourceRGBBlendFactor:MTLBlendFactorSourceAlpha];
	[attachment setDestinationRGBBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[attachment setRgbBlendOperation:MTLBlendOperationAdd];
	[attachment setSourceAlphaBlendFactor:MTLBlendFactorSourceAlpha];
	[attachment setDestinationAlphaBlendFactor:MTLBlendFactorOneMinusSourceAlpha];
	[attachment setAlphaBlendOperation:MTLBlendOperationAdd];
	[attachment setWriteMask:mode != ModVolMode::Final ? MTLColorWriteMaskNone : MTLColorWriteMaskAll];
	[attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

	[descriptor setDepthAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
	[descriptor setStencilAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];

	MetalModVolShaderParams shaderParams { naomi2, !settings.platform.isNaomi2() && config::NativeDepthInterpolation };
	[descriptor setVertexFunction:shaderManager->GetModVolVertexShader(shaderParams)];
	[descriptor setFragmentFunction:shaderManager->GetModVolFragmentShader(!settings.platform.isNaomi2() && config::NativeDepthInterpolation)];
	if (descriptor.vertexFunction == nil || descriptor.fragmentFunction == nil) {
		ERROR_LOG(RENDERER, "CreateModVolPipeline: shader compile failed");
		return;
	}

	NSError *error = nil;
	auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

	if (state == nullptr) {
		ERROR_LOG(RENDERER, "Failed to create Depth Render Pipeline State: %s", [[error localizedDescription] UTF8String]);
	}

	modVolPipelines[hash(mode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthPassPipeline(bool naomi2)
{
	MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
	[descriptor setLabel:@"Depth Pass"];
	[descriptor setVertexDescriptor:GetMainVertexInputDescriptor(false, false)];

	auto attachment = [descriptor colorAttachments][0];
	[attachment setBlendingEnabled:false];
	[attachment setSourceRGBBlendFactor:MTLBlendFactorZero];
	[attachment setDestinationRGBBlendFactor:MTLBlendFactorZero];
	[attachment setRgbBlendOperation:MTLBlendOperationAdd];
	[attachment setSourceAlphaBlendFactor:MTLBlendFactorZero];
	[attachment setDestinationAlphaBlendFactor:MTLBlendFactorZero];
	[attachment setAlphaBlendOperation:MTLBlendOperationAdd];
	[attachment setWriteMask:MTLColorWriteMaskNone];
	// Even with writeMask None, the color attachment's pixelFormat MUST match the
	// render pass's framebuffer (BGRA8Unorm) or binding asserts: "render pipeline's
	// pixelFormat (MTLPixelFormatInvalid) does not match the framebuffer's". The
	// depth-pass shares the main color pass, so set it like the other pipelines.
	[attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

	// REQUIRED: this pipeline's fragment shader writes depth, so the descriptor
	// MUST declare the depth/stencil attachment format — otherwise Metal refuses
	// to build it ("depthAttachmentPixelFormat is not valid and shader writes to
	// depth"), returns nil, and the later setRenderPipelineState(nil) asserts/
	// crashes at boot for any game that uses a depth pass (e.g. Virtua Cop).
	// The ModVol and main-draw pipelines already set these; this one was missing.
	[descriptor setDepthAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
	[descriptor setStencilAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];

	MetalModVolShaderParams shaderParams { naomi2, !settings.platform.isNaomi2() && config::NativeDepthInterpolation };
	[descriptor setVertexFunction:shaderManager->GetModVolVertexShader(shaderParams)];
	[descriptor setFragmentFunction:shaderManager->GetModVolFragmentShader(!settings.platform.isNaomi2() && config::NativeDepthInterpolation)];;
	if (descriptor.vertexFunction == nil || descriptor.fragmentFunction == nil) {
		ERROR_LOG(RENDERER, "CreateDepthPassPipeline: shader compile failed");
		return;
	}

	NSError *error = nil;
	auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

	if (state == nil) {
		ERROR_LOG(RENDERER, "Failed to create Depth Render Pipeline State: %s", [[error localizedDescription] UTF8String]);
	}

	depthPassPipelines[hash(naomi2)] = state;
}

void MetalPipelineManager::CreatePipeline(u32 listType, bool sortTriangles, const PolyParam &pp, int gpuPalette, bool dithering, bool area1) {
	MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
	[descriptor setLabel:@"Main Draw"];
	[descriptor setVertexDescriptor:GetMainVertexInputDescriptor(true, pp.isNaomi2())];

	u32 src = pp.tsp.SrcInstr;
	u32 dst = pp.tsp.DstInstr;

	auto attachment = [descriptor colorAttachments][0];
	[attachment setBlendingEnabled:true];
	[attachment setSourceRGBBlendFactor:GetBlendFactor(src, true)];
	[attachment setDestinationRGBBlendFactor:GetBlendFactor(dst, false)];
	[attachment setRgbBlendOperation:MTLBlendOperationAdd];
	[attachment setSourceAlphaBlendFactor:GetBlendFactor(src, true)];
	[attachment setDestinationAlphaBlendFactor:GetBlendFactor(dst, false)];
	[attachment setAlphaBlendOperation:MTLBlendOperationAdd];
	[attachment setWriteMask:MTLColorWriteMaskAll];
	[attachment setPixelFormat:MTLPixelFormatBGRA8Unorm];

	[descriptor setDepthAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];
	[descriptor setStencilAttachmentPixelFormat:MTLPixelFormatDepth32Float_Stencil8];

	bool divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;

	MetalVertexShaderParams vertParams = {};
	vertParams.gouraud = pp.pcw.Gouraud == 1;
	vertParams.naomi2 = pp.isNaomi2();
	vertParams.divPosZ = divPosZ;
	vertParams.area1 = area1;

	MetalFragmentShaderParams fragParams = {};
	fragParams.alphaTest = listType == ListType_Punch_Through;
	fragParams.bumpmap = pp.tcw.PixelFmt == PixelBumpMap;
	fragParams.clamping = pp.tsp.ColorClamp;
	fragParams.insideClipTest = (pp.tileclip >> 28) == 3;
	fragParams.fog = config::Fog ? pp.tsp.FogCtrl : 2;
	fragParams.gouraud = pp.pcw.Gouraud;
	fragParams.ignoreTexAlpha = pp.tsp.IgnoreTexA || pp.tcw.PixelFmt == Pixel565;
	fragParams.offset = pp.pcw.Offset;
	fragParams.shaderInstr = pp.tsp.ShadInstr;
	fragParams.texture = pp.pcw.Texture;
	fragParams.trilinear = pp.pcw.Texture && pp.tsp.FilterMode > 1 && listType != ListType_Punch_Through && pp.tcw.MipMapped == 1;
	fragParams.useAlpha = pp.tsp.UseAlpha;
	fragParams.palette = gpuPalette;
	fragParams.divPosZ = divPosZ;
	fragParams.dithering = dithering;

	id<MTLFunction> vertexFn = shaderManager->GetVertexShader(vertParams);
	id<MTLFunction> fragmentFn = shaderManager->GetFragmentShader(fragParams);
	if (vertexFn == nil || fragmentFn == nil) {
		ERROR_LOG(RENDERER, "CreatePipeline: shader compile failed");
		return;
	}
	[descriptor setVertexFunction:vertexFn];
	[descriptor setFragmentFunction:fragmentFn];

	NSError *error = nil;
	auto state = [MetalContext::Instance()->GetDevice() newRenderPipelineStateWithDescriptor:descriptor error:&error];

	if (state == nullptr) {
		ERROR_LOG(RENDERER, "Failed to create Render Pipeline State: %s", [[error localizedDescription] UTF8String]);
		// Don't cache a nil pipeline: caching it poisons the slot permanently
		// (GetPipeline's nullptr check would re-trigger CreatePipeline, but storing
		// nil here means the draw site gets nil and setRenderPipelineState(nil)
		// hard-asserts). Leave the slot empty so a later frame can retry, and let
		// the caller skip the draw this frame.
		return;
	}

	pipelines[hash(listType, sortTriangles, &pp, gpuPalette, dithering, area1)] = state;
}

void MetalPipelineManager::CreateModVolDepthStencilState(ModVolMode mode, bool naomi2) {
	MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
	[descriptor setDepthWriteEnabled:false];
	[descriptor setDepthCompareFunction:mode == ModVolMode::Xor || mode == ModVolMode::Or ? MTLCompareFunctionGreater : MTLCompareFunctionAlways];

	MTLStencilDescriptor *stencilDescriptor = [[MTLStencilDescriptor alloc] init];
	switch (mode)
	{
	case ModVolMode::Xor:
		[stencilDescriptor setStencilFailureOperation:MTLStencilOperationKeep];
		[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationInvert];
		[stencilDescriptor setDepthFailureOperation:MTLStencilOperationKeep];
		[stencilDescriptor setStencilCompareFunction:MTLCompareFunctionAlways];
		[stencilDescriptor setReadMask:0];
		[stencilDescriptor setWriteMask:2];
		break;
	case ModVolMode::Or:
		[stencilDescriptor setStencilFailureOperation:MTLStencilOperationKeep];
		[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationReplace];
		[stencilDescriptor setDepthFailureOperation:MTLStencilOperationKeep];
		[stencilDescriptor setStencilCompareFunction:MTLCompareFunctionAlways];
		[stencilDescriptor setReadMask:2];
		[stencilDescriptor setWriteMask:2];
		break;
	case ModVolMode::Inclusion:
		[stencilDescriptor setStencilFailureOperation:MTLStencilOperationZero];
		[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationReplace];
		[stencilDescriptor setDepthFailureOperation:MTLStencilOperationZero];
		[stencilDescriptor setStencilCompareFunction:MTLCompareFunctionLessEqual];
		[stencilDescriptor setReadMask:3];
		[stencilDescriptor setWriteMask:3];
		break;
	case ModVolMode::Exclusion:
		[stencilDescriptor setStencilFailureOperation:MTLStencilOperationZero];
		[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationKeep];
		[stencilDescriptor setDepthFailureOperation:MTLStencilOperationZero];
		[stencilDescriptor setStencilCompareFunction:MTLCompareFunctionEqual];
		[stencilDescriptor setReadMask:3];
		[stencilDescriptor setWriteMask:3];
		break;
	case ModVolMode::Final:
		[stencilDescriptor setStencilFailureOperation:MTLStencilOperationZero];
		[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationZero];
		[stencilDescriptor setDepthFailureOperation:MTLStencilOperationZero];
		[stencilDescriptor setStencilCompareFunction:MTLCompareFunctionEqual];
		[stencilDescriptor setReadMask:0x81];
		[stencilDescriptor setWriteMask:3];
		break;
	}

	[descriptor setFrontFaceStencil:stencilDescriptor];
	[descriptor setBackFaceStencil:stencilDescriptor];

	auto state = [MetalContext::Instance()->GetDevice() newDepthStencilStateWithDescriptor:descriptor];

	modVolStencilStates[hash(mode, naomi2)] = state;
}

void MetalPipelineManager::CreateDepthPassDepthStencilState(bool naomi2) {
	MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
	[descriptor setLabel:@"Sorted Depth Pass"];
	[descriptor setDepthWriteEnabled:true];
	[descriptor setDepthCompareFunction:MTLCompareFunctionGreaterEqual];

	auto state = [MetalContext::Instance()->GetDevice() newDepthStencilStateWithDescriptor:descriptor];

	depthPassDepthStencilStates[hash(naomi2)] = state;
}

void MetalPipelineManager::CreateDepthStencilState(u32 listType, bool sortTriangles, bool shadowed, const PolyParam &pp, int stencilArea) {
	MTLDepthStencilDescriptor *descriptor = [[MTLDepthStencilDescriptor alloc] init];
	if (shadowed)
		[descriptor setLabel:@"Main Shadowed Depth-Stencil State"];
	else
		[descriptor setLabel:@"Main Depth-Stencil State"];

	MTLCompareFunction compareFunction;
	if (listType == ListType_Punch_Through || sortTriangles) {
		compareFunction = MTLCompareFunctionGreaterEqual;
	} else {
		compareFunction = depthOps[pp.isp.DepthMode];
	}

	bool depthWriteEnabled;
	if (sortTriangles) {
		depthWriteEnabled = false;
	} else {
		// Z Write Disable seems to be ignored for punch-through.
		// Fixes Worms World Party, Bust-a-Move 4 and Re-Volt
		if (listType == ListType_Punch_Through) {
			depthWriteEnabled = true;
		} else {
			depthWriteEnabled = !pp.isp.ZWriteDis;
		}
	}

	MTLStencilDescriptor *stencilDescriptor = [[MTLStencilDescriptor alloc] init];
	[stencilDescriptor setStencilFailureOperation:MTLStencilOperationKeep];
	[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationKeep];

	if (stencilArea != 0) {
		// Two-volume poly, one parameter set per draw: pass only inside (1) or only
		// outside (2) the modifier-volume mark. The inside draw of an opaque/PT poly
		// repaints pixels the first draw already wrote (so depth compares >=) and
		// clears the mark so the Final darkening pass leaves its colour alone.
		[stencilDescriptor setStencilCompareFunction:stencilArea == 1 ? MTLCompareFunctionEqual : MTLCompareFunctionNotEqual];
		[stencilDescriptor setReadMask:0x81];
		if (stencilArea == 1 && shadowed) {
			[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationZero];
			[stencilDescriptor setWriteMask:0x81];
			compareFunction = MTLCompareFunctionGreaterEqual;
		} else {
			[stencilDescriptor setWriteMask:0];
		}
	} else if (shadowed) {
		[stencilDescriptor setDepthStencilPassOperation:MTLStencilOperationReplace];
		[stencilDescriptor setStencilCompareFunction:MTLCompareFunctionAlways];
		[stencilDescriptor setReadMask:0];
		[stencilDescriptor setWriteMask:0x80];
	}

	[descriptor setDepthCompareFunction:compareFunction];
	[descriptor setDepthWriteEnabled:depthWriteEnabled];

	if (shadowed || stencilArea != 0) {
		[descriptor setBackFaceStencil:stencilDescriptor];
		[descriptor setFrontFaceStencil:stencilDescriptor];
	}

	auto state = [MetalContext::Instance()->GetDevice() newDepthStencilStateWithDescriptor:descriptor];

	depthStencilStates[hash(listType, sortTriangles, shadowed, &pp, stencilArea)] = state;
}
