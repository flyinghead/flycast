#pragma once
#include "types.h"
#include "ta_ctx.h"

extern u32 VertexCount;
extern u32 FrameCount;

void rend_init_renderer();
void rend_term_renderer();
void rend_vblank();
void rend_start_render();
void rend_end_render();
void rend_cancel_emu_wait();
bool rend_single_frame(const bool& enabled);
void rend_swap_frame(u32 fb_r_sof1);
void rend_set_fb_write_addr(u32 fb_w_sof1);
void rend_reset();
void rend_disable_rollback();
void rend_start_rollback();
void rend_allow_rollback();
void rend_serialize(Serializer& ser);
void rend_deserialize(Deserializer& deser);
void rend_resize_renderer();

///////
extern TA_context* _pvrrc;

#define pvrrc (_pvrrc->rend)

struct Renderer
{
	virtual bool Init()=0;
	virtual ~Renderer() = default;
	
	virtual void Resize(int w, int h)=0;

	virtual void Term()=0;

	virtual bool Process(TA_context* ctx)=0;
	virtual bool Render()=0;
	virtual bool RenderLastFrame() { return false; }

	virtual bool Present() { return true; }

	virtual void DrawOSD(bool clear_screen) { }

	virtual BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) { return nullptr; }
};

extern Renderer* renderer;

extern u32 fb_watch_addr_start;
extern u32 fb_watch_addr_end;
extern bool fb_dirty;

void check_framebuffer_write();
