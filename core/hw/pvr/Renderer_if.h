#pragma once
#include "drkPvr.h"
#include "ta_ctx.h"

extern u32 VertexCount;
extern u32 FrameCount;

void rend_init_renderer();
void rend_term_renderer();
void rend_stop_renderer();
void rend_vblank();
void rend_start_render();
void rend_end_render();
void rend_cancel_emu_wait();
bool rend_single_frame();

void rend_set_fb_scale(float x,float y);
void rend_resize(int width, int height);
void rend_text_invl(vram_block* bl);

#ifdef GLuint
GLuint
#else
u32
#endif
GetTexture(TSP tsp,TCW tcw);


///////
extern TA_context* _pvrrc;

#define pvrrc (_pvrrc->rend)

struct Renderer
{
	virtual bool Init()=0;
	
	virtual void Resize(int w, int h)=0;

	virtual void Term()=0;

	virtual bool Process(TA_context* ctx)=0;
	virtual bool Render()=0;
	virtual bool RenderLastFrame() { return false; }

	virtual void Present()=0;

	virtual void DrawOSD(bool clear_screen) { }

	virtual u32 GetTexture(TSP tsp, TCW tcw) { return 0; }
};

extern Renderer* renderer;
extern bool renderer_enabled;	// Signals the renderer thread to exit
extern bool renderer_changed;	// Signals the renderer thread to switch renderer

Renderer* rend_GLES2();
#if !defined(GLES) && HOST_OS != OS_DARWIN
Renderer* rend_GL4();
#endif
Renderer* rend_norend();
Renderer* rend_softrend();

extern u32 fb1_watch_addr_start;
extern u32 fb1_watch_addr_end;
extern u32 fb2_watch_addr_start;
extern u32 fb2_watch_addr_end;
extern bool fb_dirty;

void check_framebuffer_write();
