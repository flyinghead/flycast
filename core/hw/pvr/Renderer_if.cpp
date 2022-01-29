#include "Renderer_if.h"
#include "spg.h"
#include "cheats.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"
#include "cfg/option.h"
#include "network/ggpo.h"
#include "emulator.h"
#include "serialize.h"

#include <mutex>

void retro_rend_present();
#ifndef LIBRETRO
void retro_rend_present()
{
	if (!config::ThreadedRendering)
		sh4_cpu.Stop();
}
#endif

u32 VertexCount=0;
u32 FrameCount=1;

Renderer* renderer;

cResetEvent rs, re;
static bool do_swap;
std::mutex swap_mutex;
u32 fb_w_cur = 1;
static cResetEvent vramRollback;

// direct framebuffer write detection
static bool render_called = false;
u32 fb_watch_addr_start;
u32 fb_watch_addr_end;
bool fb_dirty;

static bool pend_rend;

TA_context* _pvrrc;

static bool rend_frame(TA_context* ctx)
{
	bool proc = renderer->Process(ctx);

	if (!proc || (!ctx->rend.isRTT && !ctx->rend.isRenderFramebuffer))
		// If rendering to texture, continue locking until the frame is rendered
		re.Set();
	rend_allow_rollback();

	return proc && renderer->Render();
}

bool rend_single_frame(const bool& enabled)
{
	do
	{
		if (config::ThreadedRendering && !rs.Wait(50))
			return false;
		if (do_swap)
		{
			do_swap = false;
			if (renderer->Present())
			{
				rs.Set(); // don't miss any render
				retro_rend_present();
				return true;
			}
		}
		if (!enabled)
			return false;

		_pvrrc = DequeueRender();
		if (!config::ThreadedRendering && _pvrrc == nullptr)
			return false;
	}
	while (_pvrrc == nullptr);

	bool frame_rendered = rend_frame(_pvrrc);

	if (frame_rendered)
	{
		{
			std::lock_guard<std::mutex> lock(swap_mutex);
			if (config::DelayFrameSwapping && !_pvrrc->rend.isRenderFramebuffer && fb_w_cur != FB_R_SOF1 && !do_swap)
				// Delay swap
				frame_rendered = false;
			else
				// Swap now
				do_swap = false;
		}
		if (frame_rendered)
		{
			frame_rendered = renderer->Present();
			if (frame_rendered)
				retro_rend_present();
		}
	}

	if (_pvrrc->rend.isRTT)
		re.Set();

	//clear up & free data ..
	FinishRender(_pvrrc);
	_pvrrc = nullptr;

	return frame_rendered;
}

Renderer* rend_GLES2();
Renderer* rend_GL4();
Renderer* rend_norend();
Renderer* rend_Vulkan();
Renderer* rend_OITVulkan();
Renderer* rend_DirectX9();
Renderer* rend_DirectX11();
Renderer* rend_OITDirectX11();

static void rend_create_renderer()
{
#ifdef NO_REND
	renderer	 = rend_norend();
#else
	switch (config::RendererType)
	{
	default:
#ifdef USE_OPENGL
	case RenderType::OpenGL:
		renderer = rend_GLES2();
		break;
#if !defined(GLES) && !defined(__APPLE__)
	case RenderType::OpenGL_OIT:
		renderer = rend_GL4();
		break;
#endif
#endif
#ifdef USE_VULKAN
	case RenderType::Vulkan:
		renderer = rend_Vulkan();
		break;
	case RenderType::Vulkan_OIT:
		renderer = rend_OITVulkan();
		break;
#endif
#ifdef USE_DX9
	case RenderType::DirectX9:
		renderer = rend_DirectX9();
		break;
#endif
#if (defined(_WIN32) && !defined(LIBRETRO)) || defined(HAVE_D3D11)
	case RenderType::DirectX11:
		renderer = rend_DirectX11();
		break;
	case RenderType::DirectX11_OIT:
		renderer = rend_OITDirectX11();
		break;
#endif
	}
#endif
}

void rend_init_renderer()
{
	if (renderer == nullptr)
		rend_create_renderer();
	if (!renderer->Init())
   		die("Renderer initialization failed\n");
}

void rend_term_renderer()
{
	if (renderer != nullptr)
	{
		renderer->Term();
		delete renderer;
		renderer = nullptr;
	}
}

void rend_reset()
{
	FinishRender(DequeueRender());
	do_swap = false;
	render_called = false;
	pend_rend = false;
	FrameCount = 1;
	VertexCount = 0;
	fb_w_cur = 1;
}

void rend_start_render(TA_context *ctx)
{
	render_called = true;
	pend_rend = false;
	if (ctx == nullptr)
	{
		u32 ta_ol_base = getTAContextAddress();
		ctx = tactx_Pop(ta_ol_base);
	}

	// No end of render interrupt when rendering the framebuffer
	if (!ctx || !ctx->rend.isRenderFramebuffer)
		SetREP(ctx);

	if (ctx)
	{
		if (ctx->rend.isRenderFramebuffer)
		{
			ctx->rend.isRTT = false;
			ctx->rend.fb_X_CLIP.min = 0;
			ctx->rend.fb_X_CLIP.max = 639;
			ctx->rend.fb_Y_CLIP.min = 0;
			ctx->rend.fb_Y_CLIP.max = 479;

			ctx->rend.fog_clamp_min.full = 0;
			ctx->rend.fog_clamp_max.full = 0xffffffff;
		}
		else
		{
			FillBGP(ctx);

			ctx->rend.isRTT = (FB_W_SOF1 & 0x1000000) != 0;

			ctx->rend.fb_X_CLIP = FB_X_CLIP;
			ctx->rend.fb_Y_CLIP = FB_Y_CLIP;

			ctx->rend.fog_clamp_min = FOG_CLAMP_MIN;
			ctx->rend.fog_clamp_max = FOG_CLAMP_MAX;
		}

		if (!config::DelayFrameSwapping)
			ggpo::endOfFrame();
		palette_update();
		if (QueueRender(ctx))
		{
			pend_rend = true;
			if (!config::ThreadedRendering)
				rend_single_frame(true);
			else
				rs.Set();
		}
	}
}

void rend_end_render()
{
	if (pend_rend && config::ThreadedRendering)
		re.Wait();
}

void rend_vblank()
{
	if (!render_called && fb_dirty && FB_R_CTRL.fb_enable)
	{
		DEBUG_LOG(PVR, "Direct framebuffer write detected");
		TA_context ctx;
		ctx.Alloc();
		ctx.rend.isRenderFramebuffer = true;
		rend_start_render(&ctx);
		ctx.Free();
		fb_dirty = false;
	}
	render_called = false;
	check_framebuffer_write();
	cheatManager.apply();
	emu.vblank();
}

void check_framebuffer_write()
{
	u32 fb_size = (FB_R_SIZE.fb_y_size + 1) * (FB_R_SIZE.fb_x_size + FB_R_SIZE.fb_modulus) * 4;
	fb_watch_addr_start = (SPG_CONTROL.interlace ? FB_R_SOF2 : FB_R_SOF1) & VRAM_MASK;
	fb_watch_addr_end = fb_watch_addr_start + fb_size;
}

void rend_cancel_emu_wait()
{
	if (config::ThreadedRendering)
	{
		FinishRender(NULL);
		re.Set();
		rend_allow_rollback();
	}
}

void rend_set_fb_write_addr(u32 fb_w_sof1)
{
	if (fb_w_sof1 & 0x1000000)
		// render to texture
		return;
	fb_w_cur = fb_w_sof1;
}

void rend_swap_frame(u32 fb_r_sof)
{
	swap_mutex.lock();
	if (fb_r_sof == fb_w_cur)
	{
		do_swap = true;
		if (config::ThreadedRendering)
			rs.Set();
		else
		{
			swap_mutex.unlock();
			rend_single_frame(true);
			swap_mutex.lock();
		}
		if (config::DelayFrameSwapping)
        	ggpo::endOfFrame();
	}
	swap_mutex.unlock();
}

void rend_disable_rollback()
{
	vramRollback.Reset();
}

void rend_allow_rollback()
{
	vramRollback.Set();
}

void rend_start_rollback()
{
	if (config::ThreadedRendering)
		vramRollback.Wait();
}

void rend_serialize(Serializer& ser)
{
	ser << fb_w_cur;
	ser << render_called;
	ser << fb_dirty;
	ser << fb_watch_addr_start;
	ser << fb_watch_addr_end;
}
void rend_deserialize(Deserializer& deser)
{
	if ((deser.version() >= Deserializer::V12_LIBRETRO && deser.version() < Deserializer::V5) || deser.version() >= Deserializer::V12)
		deser >> fb_w_cur;
	else
		fb_w_cur = 1;
	if (deser.version() >= Deserializer::V20)
	{
		deser >> render_called;
		deser >> fb_dirty;
		deser >> fb_watch_addr_start;
		deser >> fb_watch_addr_end;
	}
	pend_rend = false;
}

void rend_resize_renderer()
{
	if (renderer == nullptr)
		return;
	float hres;
	int vres = config::RenderResolution;
	if (config::Widescreen && !config::Rotate90)
	{
		if (config::SuperWidescreen)
			hres = (float)config::RenderResolution * settings.display.width / settings.display.height;
		else
			hres = config::RenderResolution * 16.f / 9.f;
	}
	else if (config::Rotate90)
	{
		vres = vres * config::ScreenStretching / 100;
		hres = config::RenderResolution * 4.f / 3.f;
	}
	else
	{
		hres = config::RenderResolution * 4.f * config::ScreenStretching / 3.f / 100.f;
	}
	if (!config::Rotate90)
		hres = std::roundf(hres / 2.f) * 2.f;
	DEBUG_LOG(RENDERER, "rend_resize_renderer: %d x %d", (int)hres, vres);
	renderer->Resize((int)hres, vres);
}
