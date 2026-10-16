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

#include "metal_renderer.h"
#include "hw/pvr/ta.h"
#include "hw/pvr/Renderer_if.h"
#include "rend/texconv.h"

bool BaseMetalRenderer::BaseInit()
{
	texCommandPool.Init();
	fbCommandPool.Init();

	return true;
}

void BaseMetalRenderer::Term()
{
	WaitIdle();
	MetalContext::Instance()->PresentFrame(nil, MTLViewport {}, 0);
	texCommandPool.Term();
	fbCommandPool.Term();
	textureCache.Clear();
	fogTexture = nil;
	paletteTexture = nil;
	framebufferTextures.clear();
	framebufferTexIndex = 0;
	shaderManager.term();
}

BaseTextureCacheData *BaseMetalRenderer::GetTexture(TSP tsp, TCW tcw, int area)
{
	MetalTexture* tf = textureCache.getTextureCacheData(tsp, tcw, area);

	if (tf->NeedsUpdate()) {
		tf->SetCommandBuffer(texCommandBuffer);

		if (!tf->Update())
		{
			tf->SetCommandBuffer(nil);
			return nullptr;
		}
	}
	else if (tf->IsCustomTextureAvailable()) {
		// TODO
		tf->SetCommandBuffer(texCommandBuffer);
	}
	tf->SetCommandBuffer(nil);
	textureCache.SetInFlight(tf);

	return tf;
}

void BaseMetalRenderer::Process(TA_context *ctx)
{
	// This runs on the emu/render thread, which has no autorelease pool of its
	// own — without one, every autoreleased ObjC temporary from the texture
	// uploads below accumulates for the lifetime of the thread. Draw() already
	// pools; Process/RenderFramebuffer/PresentFrame need the same treatment.
	@autoreleasepool {
	rendContext = &ctx->rend;
	if (!ctx->rend.isRTT) {
		framebufferRendered = false;
		if (!config::EmulateFramebuffer)
			clearLastFrame = false;
	}
	if (resetTextureCache) {
		textureCache.Clear();
		resetTextureCache = false;
	}

	texCommandPool.BeginFrame();
	textureCache.SetCurrentIndex(texCommandPool.GetIndex());
	textureCache.Cleanup();

	texCommandBuffer = texCommandPool.Allocate();

	ta_parse(ctx, true);

	// TODO can't update fog or palette twice in multi render
	CheckFogTexture();
	CheckPaletteTexture();
	texCommandBuffer = nil;
	} // @autoreleasepool
}

void BaseMetalRenderer::RenderFramebuffer(const FramebufferInfo &info)
{
	@autoreleasepool {
	framebufferTexIndex = (framebufferTexIndex + 1) % 3;

	if (framebufferTextures.size() != 3)
		framebufferTextures.resize(3);

	std::unique_ptr<MetalTexture>& curTexture = framebufferTextures[framebufferTexIndex];
	if (!curTexture)
	{
		curTexture = std::make_unique<MetalTexture>();
		curTexture->tex_type = TextureType::_8888;
	}

	fbCommandPool.BeginFrame();
	id<MTLCommandBuffer> fbCommandBuffer = fbCommandPool.Allocate();

	curTexture->SetCommandBuffer(fbCommandBuffer);
	if (info.fb_r_ctrl.fb_enable == 0 || info.vo_control.blank_video == 1)
	{
		// Video output disabled
		u8 rgba[]{ (u8)info.vo_border_col._red, (u8)info.vo_border_col._green, (u8)info.vo_border_col._blue, 255 };
		curTexture->UploadToGPU(1, 1, rgba, false);
	}
	else
	{
		int width;
		int height;
		ReadFramebuffer<RGBAPacker>(info, framebufferScratch, width, height);

		curTexture->UploadToGPU(width, height, (u8*)framebufferScratch.data(), false);
	}
	curTexture->SetCommandBuffer(nil);
	fbCommandBuffer = nil;
	fbCommandPool.EndFrame();
	framebufferRendered = true;
	clearLastFrame = false;
	} // @autoreleasepool
}

void BaseMetalRenderer::WaitIdle()
{
//    [commandBuffer waitUntilCompleted];
//    commandBuffer = nil;
}

void BaseMetalRenderer::CheckFogTexture() {

	if (!fogTexture)
	{
		fogTexture = std::make_unique<MetalTexture>();
		fogTexture->tex_type = TextureType::_8;
		updateFogTable = true;
	}
	if (!updateFogTable || !config::Fog)
		return;
	updateFogTable = false;
	u8 texData[256];
	MakeFogTexture(texData);

	fogTexture->SetCommandBuffer(texCommandBuffer);
	fogTexture->UploadToGPU(128, 2, texData, false);
	fogTexture->SetCommandBuffer(nil);
}

void BaseMetalRenderer::CheckPaletteTexture() {

	if (!paletteTexture)
	{
		paletteTexture = std::make_unique<MetalTexture>();
		paletteTexture->tex_type = TextureType::_8888;
	}
	else if (!updatePalette)
		return;
	updatePalette = false;

	paletteTexture->SetCommandBuffer(texCommandBuffer);
	paletteTexture->UploadToGPU(1024, 1, (u8 *)palette32_ram, false);
	paletteTexture->SetCommandBuffer(nil);
}

bool BaseMetalRenderer::presentFramebuffer()
{
	if (framebufferTexIndex >= (int)framebufferTextures.size())
		return false;
	MetalTexture *fbTexture = framebufferTextures[framebufferTexIndex].get();
	if (fbTexture == nullptr)
		return false;

	MTLViewport viewport = { 0, 0, (float)fbTexture->GetTexture().width, (float)fbTexture->GetTexture().height, 1.0, 0 };

	MetalContext::Instance()->PresentFrame(fbTexture->GetTexture(), viewport,
										   getDCFramebufferAspectRatio());
	return true;
}

// NOTE: 'final' removed so OITMetalRenderer (in oit/oit_renderer.mm) can
// subclass and override the transparency-handling methods incrementally.
// The base class otherwise stays the regular Metal renderer for non-OIT
// games and serves as a fallback for everything OIT hasn't ported yet.
class MetalRenderer : public BaseMetalRenderer
{
public:
	bool Init() override
	{
		NOTICE_LOG(RENDERER, "MetalRenderer::Init");

		// Returning false lets rend_init_renderer() fall back to rend_norend()
		if (MetalContext::Instance() == nullptr) {
			WARN_LOG(RENDERER, "MetalRenderer::Init: Metal GPU context unavailable, skipping");
			return false;
		}

		textureDrawer.Init(&samplerManager, &shaderManager, &textureCache);
		textureDrawer.SetCommandPool(&texCommandPool);

		screenDrawer.Init(&samplerManager, &shaderManager, viewport);
		screenDrawer.SetCommandPool(&texCommandPool);
		BaseInit();
		emulateFramebuffer = config::EmulateFramebuffer;

		return true;
	}

	void Term() override
	{
		NOTICE_LOG(RENDERER, "MetalRenderer::Term");
		WaitIdle();
		texCommandPool.Term();
		screenDrawer.Term();
		textureDrawer.Term();
		samplerManager.term();
		BaseMetalRenderer::Term();
	}

	void Process(TA_context* ctx) override
	{
		if (emulateFramebuffer != config::EmulateFramebuffer)
		{
			screenDrawer.EndRenderPass();
			WaitIdle();
			screenDrawer.Term();
			screenDrawer.Init(&samplerManager, &shaderManager, viewport);
			// BaseInit(screenDrawer.GetRenderPass());
			emulateFramebuffer = config::EmulateFramebuffer;
		}
		else if (ctx->rend.isRTT) {
			screenDrawer.EndRenderPass();
		}
		BaseMetalRenderer::Process(ctx);
	}

	bool Render() override
	{
		MetalDrawer *drawer;
		if (rendContext->isRTT)
			drawer = &textureDrawer;
		else {
			resize(rendContext->framebufferWidth, rendContext->framebufferHeight);
			drawer = &screenDrawer;
		}
		drawer->setRendContext(rendContext);

		drawer->Draw(fogTexture.get(), paletteTexture.get());
		if (config::EmulateFramebuffer || rendContext->isRTT)
			// delay ending the render pass in case of multi render
			drawer->EndRenderPass();
		else if (config::ThreadedRendering)
		{
			// GPU-completion-before-RENDER_DONE invariant.
			//
			// In threaded mode the SH4 thread blocks in rend_end_render() on
			// renderEnd until PvrMessageQueue::render() calls renderEnd.Set()
			// immediately after this Render() returns. For SCREEN renders the
			// command buffer is only ENCODED here — the commit happens later
			// in Present()/EndRenderPass() and is async — so without this the
			// SH4 thread would be released (and games like Sonic Adventure,
			// Resident Evil 2 and Unreal Tournament read VRAM/framebuffer on
			// the holly_RENDER_DONE interrupt) while the GPU is still drawing,
			// yielding garbage or boot failures.
			//
			// End the pass now (commits the screen command buffer via
			// commandPool->EndFrame()) and block on the just-committed
			// buffer's MTLSharedEvent until the GPU finishes, mirroring the
			// RTT path's EndFrameAndWait(). The subsequent Present() path is
			// re-entrant: screenDrawer.PresentFrame()->EndRenderPass() is a
			// no-op (renderPassStarted is now false) and frameRendered stays
			// set, so the frame is still composited and presented normally.
			//
			// Only the threaded path is affected; with ThreadedRendering=false
			// the SH4 thread never waits on renderEnd so behavior is unchanged.
			// The RTT path above is untouched (it already waits).
			drawer->EndRenderPass();
			texCommandPool.WaitForLastFrame();
		}

		return !rendContext->isRTT;
	}

	bool Present() override
	{
		if (clearLastFrame)
			return false;
		if (config::EmulateFramebuffer || framebufferRendered)
			return presentFramebuffer();
		else
			return screenDrawer.PresentFrame();
	}

protected:
	void resize(int w, int h) override
	{
		if ((u32)w == viewport.width && (u32)h == viewport.height)
			return;
		BaseMetalRenderer::resize(w, h);
		// WaitIdle();
		screenDrawer.Init(&samplerManager, &shaderManager, viewport);
	}

private:
	MetalSamplers samplerManager;
	MetalScreenDrawer screenDrawer;
	MetalTextureDrawer textureDrawer;
	bool emulateFramebuffer = false;
};

Renderer* rend_Metal()
{
	return new MetalRenderer();
}

// ---------------------------------------------------------------------------
// Order-Independent Transparency (per-pixel sorting) Metal renderer.
// Port of core/rend/vulkan/oit: translucent fragments are collected into a
// per-pixel linked list, then sorted and blended in a final resolve pass.
// ---------------------------------------------------------------------------

#include "oit/metal_oit_buffer.h"
#include "oit/metal_oit_pipeline.h"
#include "oit/metal_oit_drawer.h"

// The OIT frame is waited on before presenting (see Render() below).
//
// MetalCommandPool::Init creates its OWN command queue, and
// MetalContext::PresentFrame builds its command buffer from MetalContext's
// queue. Metal does not order work across separate queues, and nothing
// synchronized the two, so the present could sample the color attachment while
// the OIT frame was still writing it. The resolve, which composites the whole
// translucent layer, is the LAST thing written each frame, so losing that race
// yields the opaque-only image: background intact, translucent layer missing
// for that frame. That was the strobing BIOS logo and the flickering texture
// layer over 3D scenes, and it hit OIT far harder than the regular renderer
// because collect+resolve makes the GPU frame long enough to lose the race
// often. Waiting removes the flicker.
//
// The bounded CPU wait in WaitForLastFrame (200ms timeout) is used rather than
// a GPU-side encodeWaitForEvent: a cross-queue GPU wait on an event that never
// gets signalled would hang the GPU, and the pool has a known phantom-event
// path. Costs a CPU/GPU serialization point per frame on the OIT path only.

class OITMetalRenderer : public MetalRenderer
{
public:
	bool Init() override
	{
		if (!MetalRenderer::Init())
			return false;
		if (viewport.width > 0 && viewport.height > 0)
			oitBuffers.Init((int)viewport.width, (int)viewport.height);

		// The OIT pipeline manager + MSL library are owned by
		// MetalOITScreenDrawer (its screenPipelineManager). No standalone
		// copy needed — that would just compile the same MSL twice.
		oitScreenDrawer.Init(&shaderManager, &oitBuffers, viewport, &oitSamplers);
		oitScreenDrawer.SetCommandPool(&texCommandPool);
		oitTextureDrawer.Init(&shaderManager, &oitBuffers, &textureCache, &oitSamplers);
		oitTextureDrawer.SetCommandPool(&texCommandPool);
		oitEmulateFramebuffer = config::EmulateFramebuffer;

		return true;
	}

	void Term() override
	{
		oitScreenDrawer.Term();
		oitTextureDrawer.Term();
		oitSamplers.term();
		oitBuffers.Term();
		MetalRenderer::Term();
	}

	void Process(TA_context* ctx) override
	{
		if (oitEmulateFramebuffer != config::EmulateFramebuffer)
		{
			oitScreenDrawer.EndFrame();
			WaitIdle();
			oitScreenDrawer.Term();
			oitScreenDrawer.Init(&shaderManager, &oitBuffers, viewport, &oitSamplers);
			oitScreenDrawer.SetCommandPool(&texCommandPool);
			oitEmulateFramebuffer = config::EmulateFramebuffer;
		}
		else if (ctx->rend.isRTT) {
			oitScreenDrawer.EndFrame();
		}
		BaseMetalRenderer::Process(ctx);
	}

	bool Render() override
	{
		if (viewport.width <= 0 || viewport.height <= 0)
			return MetalRenderer::Render();

		if (rendContext->isRTT) {
			oitTextureDrawer.setRendContext(rendContext);
			oitTextureDrawer.NewFrame();
			oitTextureDrawer.Draw(fogTexture.get(), paletteTexture.get());
			oitTextureDrawer.EndFrame();
			return false;
		}

		oitScreenDrawer.setRendContext(rendContext);
		oitScreenDrawer.NewFrame();
		oitScreenDrawer.Draw(fogTexture.get(), paletteTexture.get());
		// EndFrame/PresentFrame happen in Present() below — matches the
		// non-OIT lifecycle where Render only fills the command buffer.
		//
		// Same GPU-completion-before-RENDER_DONE invariant as
		// MetalRenderer::Render() above, and the cross-queue present fix
		// described at the top of this file. Unconditional: the OIT screen
		// drawer shares texCommandPool, so end the frame now (commits via
		// commandPool->EndFrame()) and block on the committed buffer's
		// MTLSharedEvent before returning. That both keeps renderEnd.Set() in
		// PvrMessageQueue::render() from releasing the SH4 thread early and
		// stops MetalContext's present queue sampling the attachment mid-write.
		// Present()->PresentFrame() is re-entrant here: its EndFrame() is a
		// no-op (frameStarted now false) and frameRendered stays set, so the
		// frame still presents.
		oitScreenDrawer.EndFrame();
		texCommandPool.WaitForLastFrame();
		return true;
	}

	bool Present() override
	{
		if (clearLastFrame)
			return false;
		if (config::EmulateFramebuffer || framebufferRendered)
			return presentFramebuffer();
		return oitScreenDrawer.PresentFrame();
	}

protected:
	MetalOITBuffers oitBuffers;
	MetalSamplers oitSamplers;
	MetalOITScreenDrawer oitScreenDrawer;
	MetalOITTextureDrawer oitTextureDrawer;
	bool oitEmulateFramebuffer = false;
};

Renderer* rend_OITMetal()
{
	return new OITMetalRenderer();
}
