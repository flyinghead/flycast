#include "Renderer_if.h"
#include "cheats.h"
#include "hw/mem/_vmem.h"
#include "hw/pvr/pvr_mem.h"
#include "oslib/oslib.h"
#include "rend/gui.h"
#include "rend/TexCache.h"
#include "wsi/context.h"

#include <zlib.h>

u32 VertexCount=0;
u32 FrameCount=1;

Renderer* renderer;
static Renderer* fallback_renderer;
bool renderer_enabled = true;	// Signals the renderer thread to exit
int renderer_changed = -1;	// Signals the renderer thread to switch renderer
bool renderer_reinit_requested = false;	// Signals the renderer thread to reinit the renderer

#if !defined(TARGET_NO_THREADS)
cResetEvent rs, re;
#endif
static bool swap_pending;
static bool do_swap;

static bool render_called = false;
u32 fb_watch_addr_start;
u32 fb_watch_addr_end;
bool fb_dirty;

TA_context* _pvrrc;
void SetREP(TA_context* cntx);
static void rend_create_renderer();

static void dump_frame(const char* file, TA_context* ctx, u8* vram, u8* vram_ref = NULL) {
	FILE* fw = fopen(file, "wb");

	//append to it
	fseek(fw, 0, SEEK_END);

	u32 bytes = ctx->tad.End() - ctx->tad.thd_root;

	fwrite("TAFRAME4", 1, 8, fw);

	fwrite(&ctx->rend.isRTT, 1, sizeof(ctx->rend.isRTT), fw);
	u32 zero = 0;
	fwrite(&zero, 1, sizeof(bool), fw);	// Was autosort
	fwrite(&ctx->rend.fb_X_CLIP.full, 1, sizeof(ctx->rend.fb_X_CLIP.full), fw);
	fwrite(&ctx->rend.fb_Y_CLIP.full, 1, sizeof(ctx->rend.fb_Y_CLIP.full), fw);

	fwrite(ctx->rend.global_param_op.head(), 1, sizeof(PolyParam), fw);
	fwrite(ctx->rend.verts.head(), 1, 4 * sizeof(Vertex), fw);

	u32 t = VRAM_SIZE;
	fwrite(&t, 1, sizeof(t), fw);
	
	u8* compressed;
	uLongf compressed_size;
	u8* src_vram = vram;

	if (vram_ref) {
		src_vram = (u8*)malloc(VRAM_SIZE);

		for (u32 i = 0; i < VRAM_SIZE; i++) {
			src_vram[i] = vram[i] ^ vram_ref[i];
		}
	}

	compressed = (u8*)malloc(VRAM_SIZE+16);
	compressed_size = VRAM_SIZE;
	verify(compress(compressed, &compressed_size, src_vram, VRAM_SIZE) == Z_OK);
	fwrite(&compressed_size, 1, sizeof(compressed_size), fw);
	fwrite(compressed, 1, compressed_size, fw);
	free(compressed);

	if (src_vram != vram)
		free(src_vram);

	fwrite(&bytes, 1, sizeof(t), fw);
	compressed = (u8*)malloc(bytes + 16);
	compressed_size = VRAM_SIZE;
	verify(compress(compressed, &compressed_size, ctx->tad.thd_root, bytes) == Z_OK);
	fwrite(&compressed_size, 1, sizeof(compressed_size), fw);
	fwrite(compressed, 1, compressed_size, fw);
	free(compressed);

	fwrite(&ctx->tad.render_pass_count, 1, sizeof(u32), fw);
	for (u32 i = 0; i < ctx->tad.render_pass_count; i++) {
		u32 offset = ctx->tad.render_passes[i] - ctx->tad.thd_root;
		fwrite(&offset, 1, sizeof(offset), fw);
	}

	fwrite(pvr_regs, 1, sizeof(pvr_regs), fw);

	fclose(fw);
}

TA_context* read_frame(const char* file, u8* vram_ref = NULL) {
	
	FILE* fw = fopen(file, "rb");
	if (fw == NULL)
		die("Cannot open frame to display");
	char id0[8] = { 0 };
	u32 t = 0;

	if (fread(id0, 1, 8, fw) != 8) {
		fclose(fw);
		return 0;
	}

	if (memcmp(id0, "TAFRAME", 7) != 0 || (id0[7] != '3' && id0[7] != '4')) {
		fclose(fw);
		return 0;
	}
	u32 sizeofPolyParam = sizeof(PolyParam);
	u32 sizeofVertex = sizeof(Vertex);
	if (id0[7] == '3')
	{
		sizeofPolyParam -= 12;
		sizeofVertex -= 16;
	}

	TA_context* ctx = tactx_Alloc();

	ctx->Reset();

	ctx->tad.Clear();

	verify(fread(&ctx->rend.isRTT, 1, sizeof(ctx->rend.isRTT), fw) == sizeof(ctx->rend.isRTT));
	verify(fread(&t, 1, sizeof(bool), fw) == sizeof(bool));	// Was autosort
	verify(fread(&ctx->rend.fb_X_CLIP.full, 1, sizeof(ctx->rend.fb_X_CLIP.full), fw) == sizeof(ctx->rend.fb_X_CLIP.full));
	verify(fread(&ctx->rend.fb_Y_CLIP.full, 1, sizeof(ctx->rend.fb_Y_CLIP.full), fw) == sizeof(ctx->rend.fb_Y_CLIP.full));

	verify(fread(ctx->rend.global_param_op.Append(), 1, sizeofPolyParam, fw) == sizeofPolyParam);
	Vertex *vtx = ctx->rend.verts.Append(4);
	for (int i = 0; i < 4; i++)
		verify(fread(vtx + i, 1, sizeofVertex, fw) == sizeofVertex);

	verify(fread(&t, 1, sizeof(t), fw) == sizeof(t));
	verify(t == VRAM_SIZE);

	_vmem_unprotect_vram(0, VRAM_SIZE);

	uLongf compressed_size;

	verify(fread(&compressed_size, 1, sizeof(compressed_size), fw) == sizeof(compressed_size));

	u8* gz_stream = (u8*)malloc(compressed_size);
	verify(fread(gz_stream, 1, compressed_size, fw) == compressed_size);
	uLongf tl = t;
	verify(uncompress(vram.data, &tl, gz_stream, compressed_size) == Z_OK);
	free(gz_stream);

	verify(fread(&t, 1, sizeof(t), fw) == sizeof(t));
	verify(fread(&compressed_size, 1, sizeof(compressed_size), fw) == sizeof(compressed_size));
	gz_stream = (u8*)malloc(compressed_size);
	verify(fread(gz_stream, 1, compressed_size, fw) == compressed_size);
	tl = t;
	verify(uncompress(ctx->tad.thd_data, &tl, gz_stream, compressed_size) == Z_OK);
	free(gz_stream);

	ctx->tad.thd_data += t;

	if (fread(&t, 1, sizeof(t), fw) > 0) {
		ctx->tad.render_pass_count = t;
		for (u32 i = 0; i < t; i++) {
			u32 offset;
			verify(fread(&offset, 1, sizeof(offset), fw) == sizeof(offset));
			ctx->tad.render_passes[i] = ctx->tad.thd_root + offset;
		}
	}
	verify(fread(pvr_regs, 1, sizeof(pvr_regs), fw) == sizeof(pvr_regs));

	fclose(fw);
    
    return ctx;
}

bool dump_frame_switch = false;

static bool rend_frame(TA_context* ctx)
{
	if (dump_frame_switch) {
		char name[32];
		sprintf(name, "dcframe-%d", FrameCount);
		dump_frame(name, _pvrrc, &vram[0]);
		dump_frame_switch = false;
	}
	bool proc = renderer->Process(ctx);
	if ((ctx->rend.isRTT || ctx->rend.isRenderFramebuffer) && swap_pending)
	{
		// If there is a frame swap pending, we want to do it now.
		// The current frame "swapping" detection mechanism (using FB_R_SOF1) doesn't work
		// if a RTT frame is rendered in between.
		renderer->Present();
		swap_pending = false;
	}
#if !defined(TARGET_NO_THREADS)
	if (!proc || (!ctx->rend.isRTT && !ctx->rend.isRenderFramebuffer))
		// If rendering to texture, continue locking until the frame is rendered
		re.Set();
#endif

	return proc && renderer->Render();
}

bool rend_single_frame()
{
	if ((u32)renderer_changed != settings.pvr.rend)
	{
		rend_term_renderer();
		SwitchRenderApi(renderer_changed);
		rend_create_renderer();
		rend_init_renderer();
	}
	//wait render start only if no frame pending
	do
	{
		// FIXME not here
		os_DoEvents();
#if !defined(TARGET_NO_THREADS)
		if (gui_is_open() || gui_state == VJoyEdit)
		{
			gui_display_ui();
			if (gui_state == VJoyEdit && renderer != NULL)
				renderer->DrawOSD(true);
			FinishRender(NULL);
			// Use the rendering start event to wait between two frames but save its value
			if (rs.Wait(17))
				rs.Set();
			swap_pending = false;
			return true;
		}
		else
		{
			if (renderer != NULL)
				renderer->RenderLastFrame();

			if (!rs.Wait(100))
				return false;
			if (do_swap)
			{
				do_swap = false;
				renderer->Present();
			}
		}
#else
		if (gui_is_open())
		{
			gui_display_ui();
			FinishRender(NULL);
			swap_pending = false;
			return true;
		}
		if (renderer != NULL)
			renderer->RenderLastFrame();
#endif
		if (!renderer_enabled)
			return false;

		_pvrrc = DequeueRender();
	}
	while (!_pvrrc);
	bool do_swp = rend_frame(_pvrrc);
	swap_pending = settings.rend.DelayFrameSwapping && do_swp && !_pvrrc->rend.isRenderFramebuffer
			&& settings.pvr.rend != 4 && settings.pvr.rend != 5;	// TODO Fix vulkan

#if !defined(TARGET_NO_THREADS)
	if (_pvrrc->rend.isRTT)
		re.Set();
#endif

	//clear up & free data ..
	FinishRender(_pvrrc);
	_pvrrc=0;

	return do_swp;
}

static void rend_create_renderer()
{
#ifdef NO_REND
	renderer	 = rend_norend();
#else
	switch (settings.pvr.rend)
	{
	default:
	case 0:
		renderer = rend_GLES2();
		break;
#if !defined(GLES) && !defined(__APPLE__)
	case 3:
		renderer = rend_GL4();
		fallback_renderer = rend_GLES2();
		break;
#endif
#ifdef USE_VULKAN
	case 4:
		renderer = rend_Vulkan();
		break;
	case 5:
		renderer = rend_OITVulkan();
		break;
#endif
	}
#endif
	renderer_changed = settings.pvr.rend;
}

void rend_init_renderer()
{
	if (renderer == NULL)
		rend_create_renderer();
	if (!renderer->Init())
    {
		delete renderer;
    	if (fallback_renderer == NULL || !fallback_renderer->Init())
    	{
    		if (fallback_renderer != NULL)
    			delete fallback_renderer;
    		die("Renderer initialization failed\n");
    	}
    	INFO_LOG(PVR, "Selected renderer initialization failed. Falling back to default renderer.");
    	renderer  = fallback_renderer;
    	fallback_renderer = NULL;	// avoid double-free
    }
}

void rend_term_renderer()
{
	if (renderer != NULL)
	{
		renderer->Term();
		delete renderer;
		renderer = NULL;
	}
	if (fallback_renderer != NULL)
	{
		delete fallback_renderer;
		fallback_renderer = NULL;
	}
}

void* rend_thread(void* p)
{
	renderer_enabled = true;

	rend_init_renderer();

	//we don't know if this is true, so let's not speculate here
	//renderer->Resize(640, 480);

	while (renderer_enabled)
	{
		if (rend_single_frame())
		{
			if (FB_R_SOF1 == FB_W_SOF1 || !swap_pending)
			{
				renderer->Present();
				swap_pending = false;
			}
		}
		if (renderer_reinit_requested)
		{
			renderer_reinit_requested = false;
			rend_init_renderer();
		}
	}

	rend_term_renderer();

	return NULL;
}

bool pend_rend = false;

void rend_resize(int width, int height)
{
	if (renderer != nullptr)
		renderer->Resize(width, height);
}


void rend_start_render()
{
	render_called = true;
	pend_rend = false;
	TA_context* ctx = tactx_Pop(CORE_CURRENT_CTX);

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

			ctx->rend.fog_clamp_min = 0;
			ctx->rend.fog_clamp_max = 0xffffffff;
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

		if (QueueRender(ctx))
		{
			palette_update();
#if !defined(TARGET_NO_THREADS)
			rs.Set();
#else
			rend_single_frame();
#endif
			pend_rend = true;
		}
	}
}


void rend_end_render()
{
#if 1 //also disabled the printf, it takes quite some time ...
	#if !defined(_WIN32) && !(defined(__ANDROID__) || defined(TARGET_PANDORA))
		//too much console spam.
		//TODO: how about a counter?
		//if (!re.state) printf("Render > Extended time slice ...\n");
	#endif
#endif

	if (pend_rend) {
#if !defined(TARGET_NO_THREADS)
		re.Wait();
#else
		if (renderer != NULL)
			renderer->Present();
#endif
	}
}

void rend_stop_renderer()
{
	renderer_enabled = false;
	tactx_Term();
}

void rend_vblank()
{
	if (!render_called && fb_dirty && FB_R_CTRL.fb_enable)
	{
		DEBUG_LOG(PVR, "Direct framebuffer write detected");
		u32 saved_ctx_addr = PARAM_BASE;
		bool restore_ctx = ta_ctx != NULL;
		PARAM_BASE = 0xF00000;
		SetCurrentTARC(CORE_CURRENT_CTX);
		ta_ctx->Reset();
		ta_ctx->rend.isRenderFramebuffer = true;
		rend_start_render();
		PARAM_BASE = saved_ctx_addr;
		if (restore_ctx)
			SetCurrentTARC(CORE_CURRENT_CTX);
		fb_dirty = false;
	}
	render_called = false;
	check_framebuffer_write();
	cheatManager.Apply();
}

void check_framebuffer_write()
{
	u32 fb_size = (FB_R_SIZE.fb_y_size + 1) * (FB_R_SIZE.fb_x_size + FB_R_SIZE.fb_modulus) * 4;
	fb_watch_addr_start = (SPG_CONTROL.interlace ? FB_R_SOF2 : FB_R_SOF1) & VRAM_MASK;
	fb_watch_addr_end = fb_watch_addr_start + fb_size;
}

void rend_cancel_emu_wait()
{
	FinishRender(NULL);
#if !defined(TARGET_NO_THREADS)
	re.Set();
#endif
}

void rend_swap_frame()
{
	if (swap_pending)
	{
		swap_pending = false;
		do_swap = true;
		rs.Set();
	}
}
