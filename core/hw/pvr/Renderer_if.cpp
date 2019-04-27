#include "Renderer_if.h"
#include "ta.h"
#include "hw/pvr/pvr_mem.h"
#include "rend/TexCache.h"
#include "rend/gui.h"

#include "deps/zlib/zlib.h"

#include "deps/crypto/md5.h"

#if FEAT_HAS_NIXPROF
#include "profiler/profiler.h"
#endif

#define FRAME_MD5 0x1
FILE* fLogFrames;
FILE* fCheckFrames;

/*

	rendv3 ideas
	- multiple backends
	  - ESish
	    - OpenGL ES2.0
	    - OpenGL ES3.0
	    - OpenGL 3.1
	  - OpenGL 4.x
	  - Direct3D 10+ ?
	- correct memory ordering model
	- resource pools
	- threaded ta
	- threaded rendering
	- rtts
	- framebuffers
	- overlays


	PHASES
	- TA submition (memops, dma)

	- TA parsing (defered, rend thread)

	- CORE render (in-order, defered, rend thread)


	submition is done in-order
	- Partial handling of TA values
	- Gotchas with TA contexts

	parsing is done on demand and out-of-order, and might be skipped
	- output is only consumed by renderer

	render is queued on RENDER_START, and won't stall the emulation or might be skipped
	- VRAM integrity is an issue with out-of-order or delayed rendering.
	- selective vram snapshots require ta parsing to complete in order with REND_START / REND_END


	Complications
	- For some apis (gles2, maybe gl31) texture allocation needs to happen on the gpu thread
	- multiple versions of different time snapshots of the same texture are required
	- ta parsing vs frameskip logic


	Texture versioning and staging
	 A memory copy of the texture can be used to temporary store the texture before upload to vram
	 This can be moved to another thread
	 If the api supports async resource creation, we don't need the extra copy
	 Texcache lookups need to be versioned


	rendv2x hacks
	- Only a single pending render. Any renders while still pending are dropped (before parsing)
	- wait and block for parse/texcache. Render is async
*/

u32 VertexCount=0;
u32 FrameCount=1;

Renderer* renderer;
static Renderer* fallback_renderer;
bool renderer_enabled = true;	// Signals the renderer thread to exit
bool renderer_changed = false;	// Signals the renderer thread to switch renderer

#if !defined(TARGET_NO_THREADS)
cResetEvent rs, re;
#endif

int max_idx,max_mvo,max_op,max_pt,max_tr,max_vtx,max_modt, ovrn;

static bool render_called = false;
u32 fb1_watch_addr_start;
u32 fb1_watch_addr_end;
u32 fb2_watch_addr_start;
u32 fb2_watch_addr_end;
bool fb_dirty;

TA_context* _pvrrc;
void SetREP(TA_context* cntx);
void killtex();
bool render_output_framebuffer();

void dump_frame(const char* file, TA_context* ctx, u8* vram, u8* vram_ref = NULL) {
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

		for (int i = 0; i < VRAM_SIZE; i++) {
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
	for (int i = 0; i < ctx->tad.render_pass_count; i++) {
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

	fread(id0, 1, 8, fw);

	if (memcmp(id0, "TAFRAME", 7) != 0 || (id0[7] != '3' && id0[7] != '4')) {
		fclose(fw);
		return 0;
	}
	int sizeofPolyParam = sizeof(PolyParam);
	int sizeofVertex = sizeof(Vertex);
	if (id0[7] == '3')
	{
		sizeofPolyParam -= 12;
		sizeofVertex -= 16;
	}

	TA_context* ctx = tactx_Alloc();

	ctx->Reset();

	ctx->tad.Clear();

	fread(&ctx->rend.isRTT, 1, sizeof(ctx->rend.isRTT), fw);
	fread(&t, 1, sizeof(bool), fw);	// Was autosort
	fread(&ctx->rend.fb_X_CLIP.full, 1, sizeof(ctx->rend.fb_X_CLIP.full), fw);
	fread(&ctx->rend.fb_Y_CLIP.full, 1, sizeof(ctx->rend.fb_Y_CLIP.full), fw);

	fread(ctx->rend.global_param_op.Append(), 1, sizeofPolyParam, fw);
	Vertex *vtx = ctx->rend.verts.Append(4);
	for (int i = 0; i < 4; i++)
		fread(vtx + i, 1, sizeofVertex, fw);

	fread(&t, 1, sizeof(t), fw);
	verify(t == VRAM_SIZE);

	vram.UnLockRegion(0, VRAM_SIZE);

	uLongf compressed_size;

	fread(&compressed_size, 1, sizeof(compressed_size), fw);

	u8* gz_stream = (u8*)malloc(compressed_size);
	fread(gz_stream, 1, compressed_size, fw);
	uLongf tl = t;
	verify(uncompress(vram.data, &tl, gz_stream, compressed_size) == Z_OK);
	free(gz_stream);

	fread(&t, 1, sizeof(t), fw);
	fread(&compressed_size, 1, sizeof(compressed_size), fw);
	gz_stream = (u8*)malloc(compressed_size);
	fread(gz_stream, 1, compressed_size, fw);
	tl = t;
	verify(uncompress(ctx->tad.thd_data, &tl, gz_stream, compressed_size) == Z_OK);
	free(gz_stream);

	ctx->tad.thd_data += t;

	if (fread(&t, 1, sizeof(t), fw) > 0) {
		ctx->tad.render_pass_count = t;
		for (int i = 0; i < t; i++) {
			u32 offset;
			fread(&offset, 1, sizeof(offset), fw);
			ctx->tad.render_passes[i] = ctx->tad.thd_root + offset;
		}
	}
	fread(pvr_regs, 1, sizeof(pvr_regs), fw);

	fclose(fw);
    
    return ctx;
}

bool dump_frame_switch = false;

bool rend_frame(TA_context* ctx, bool draw_osd) {
	if (dump_frame_switch) {
		char name[32];
		sprintf(name, "dcframe-%d", FrameCount);
		dump_frame(name, _pvrrc, &vram[0]);
		dump_frame_switch = false;
	}
	bool proc = renderer->Process(ctx);
#if !defined(TARGET_NO_THREADS)
	if (!proc || (!ctx->rend.isRTT && !ctx->rend.isRenderFramebuffer))
		// If rendering to texture, continue locking until the frame is rendered
		re.Set();
#endif

	bool do_swp = proc && renderer->Render();

	if (do_swp && draw_osd)
		renderer->DrawOSD(false);

	return do_swp;
}

bool rend_single_frame()
{
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
			return true;
		}
		else
		{
			if (renderer != NULL)
				renderer->RenderLastFrame();

			if (!rs.Wait(100))
				return false;
		}
#else
		if (gui_is_open())
		{
			gui_display_ui();
			FinishRender(NULL);
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
	bool do_swp = rend_frame(_pvrrc, true);

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
#if FEAT_HAS_SOFTREND
	case 2:
		renderer = rend_softrend();
		break;
#endif
#if !defined(GLES) && HOST_OS != OS_DARWIN
	case 3:
		renderer = rend_GL4();
		fallback_renderer = rend_GLES2();
		break;
#endif
	}
#endif
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
    	printf("Selected renderer initialization failed. Falling back to default renderer.\n");
    	renderer  = fallback_renderer;
    }
}

void rend_term_renderer()
{
	killtex();
	gui_term();
	renderer->Term();
	delete renderer;
	renderer = NULL;
	if (fallback_renderer != NULL)
	{
		delete fallback_renderer;
		fallback_renderer = NULL;
	}
	tactx_Term();
}

void* rend_thread(void* p)
{
	rend_init_renderer();

	//we don't know if this is true, so let's not speculate here
	//renderer->Resize(640, 480);

	while (renderer_enabled)
	{
		if (rend_single_frame())
			renderer->Present();
		if (renderer_changed)
		{
			renderer_changed = false;
			rend_term_renderer();
			rend_create_renderer();
			rend_init_renderer();
		}
	}

	rend_term_renderer();

	return NULL;
}

bool pend_rend = false;

void rend_resize(int width, int height) {
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
		bool is_rtt=(FB_W_SOF1& 0x1000000)!=0 && !ctx->rend.isRenderFramebuffer;
		
		if (fLogFrames || fCheckFrames) {
			MD5Context md5;
			u8 digest[16];

			MD5Init(&md5);
			MD5Update(&md5, ctx->tad.thd_root, ctx->tad.End() - ctx->tad.thd_root);
			MD5Final(digest, &md5);

			if (fLogFrames) {
				fputc(FRAME_MD5, fLogFrames);
				fwrite(digest, 1, 16, fLogFrames);
				fflush(fLogFrames);
			}

			if (fCheckFrames) {
				u8 digest2[16];
				int ch = fgetc(fCheckFrames);

				if (ch == EOF) {
					printf("Testing: TA Hash log matches, exiting\n");
					exit(1);
				}
				
				verify(ch == FRAME_MD5);

				fread(digest2, 1, 16, fCheckFrames);

				verify(memcmp(digest, digest2, 16) == 0);

				
			}

			/*
			u8* dig = digest;
			printf("FRAME: %02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X-%02X\n",
				digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
				digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
				);
			*/
		}

		if (!ctx->rend.Overrun)
		{
			//tactx_Recycle(ctx); ctx = read_frame("frames/dcframe-SoA-intro-tr-autosort");
			//printf("REP: %.2f ms\n",render_end_pending_cycles/200000.0);
			if (!ctx->rend.isRenderFramebuffer)
				FillBGP(ctx);
			
			ctx->rend.isRTT=is_rtt;

			ctx->rend.fb_X_CLIP=FB_X_CLIP;
			ctx->rend.fb_Y_CLIP=FB_Y_CLIP;
			
			ctx->rend.fog_clamp_min = FOG_CLAMP_MIN;
			ctx->rend.fog_clamp_max = FOG_CLAMP_MAX;
			
			max_idx=max(max_idx,ctx->rend.idx.used());
			max_vtx=max(max_vtx,ctx->rend.verts.used());
			max_op=max(max_op,ctx->rend.global_param_op.used());
			max_pt=max(max_pt,ctx->rend.global_param_pt.used());
			max_tr=max(max_tr,ctx->rend.global_param_tr.used());
			
			max_mvo=max(max_mvo,ctx->rend.global_param_mvo.used());
			max_modt=max(max_modt,ctx->rend.modtrig.used());

#if HOST_OS==OS_WINDOWS && 0
			printf("max: idx: %d, vtx: %d, op: %d, pt: %d, tr: %d, mvo: %d, modt: %d, ov: %d\n", max_idx, max_vtx, max_op, max_pt, max_tr, max_mvo, max_modt, ovrn);
#endif
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
		else
		{
			ovrn++;
			printf("WARNING: Rendering context is overrun (%d), aborting frame\n",ovrn);
			tactx_Recycle(ctx);
		}
	}
}


void rend_end_render()
{
#if 1 //also disabled the printf, it takes quite some time ...
	#if HOST_OS!=OS_WINDOWS && !(defined(_ANDROID) || defined(TARGET_PANDORA))
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
}

void rend_vblank()
{
	if (!render_called && fb_dirty && FB_R_CTRL.fb_enable)
	{
		SetCurrentTARC(CORE_CURRENT_CTX);
		ta_ctx->rend.isRenderFramebuffer = true;
		rend_start_render();
		fb_dirty = false;
	}
	render_called = false;
	check_framebuffer_write();
}

void check_framebuffer_write()
{
	u32 fb_size = (FB_R_SIZE.fb_y_size + 1) * (FB_R_SIZE.fb_x_size + FB_R_SIZE.fb_modulus) * 4;
	fb1_watch_addr_start = FB_R_SOF1 & VRAM_MASK;
	fb1_watch_addr_end = fb1_watch_addr_start + fb_size;
	fb2_watch_addr_start = FB_R_SOF2 & VRAM_MASK;
	fb2_watch_addr_end = fb2_watch_addr_start + fb_size;
}

void rend_cancel_emu_wait()
{
	FinishRender(NULL);
#if !defined(TARGET_NO_THREADS)
	re.Set();
#endif
}

