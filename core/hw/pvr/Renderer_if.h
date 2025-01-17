#pragma once
#include "types.h"
#include "ta_ctx.h"
#include "emulator.h"
#include <vector>

extern u32 FrameCount;

bool rend_init_renderer();
void rend_term_renderer();
void rend_vblank();
void rend_start_render();
int rend_end_render(int tag, int cycles, int jitter, void *arg);
void rend_cancel_emu_wait();
bool rend_single_frame(const bool& enabled);
void rend_swap_frame(u32 fb_r_sof1);
void rend_set_fb_write_addr(u32 fb_w_sof1);
void rend_reset();
void rend_disable_rollback();
void rend_start_rollback();
void rend_allow_rollback();
void rend_enable_renderer(bool enabled);
bool rend_is_enabled();
void rend_serialize(Serializer& ser);
void rend_deserialize(Deserializer& deser);
static void rend_updatePalette();
static void rend_updateFogTable();

///////
extern TA_context* _pvrrc;

#define pvrrc (_pvrrc->rend)

struct FramebufferInfo
{
	void update()
	{
		fb_r_size.full = FB_R_SIZE.full;
		fb_r_ctrl.full = FB_R_CTRL.full;
		spg_control.full = SPG_CONTROL.full;
		spg_status.full = SPG_STATUS.full;
		fb_r_sof1 = FB_R_SOF1;
		fb_r_sof2 = FB_R_SOF2;
		vo_control.full = VO_CONTROL.full;
		vo_border_col.full = VO_BORDER_COL.full;
	}

	FB_R_SIZE_type fb_r_size;
	FB_R_CTRL_type fb_r_ctrl;
	SPG_CONTROL_type spg_control;
	SPG_STATUS_type spg_status;
	u32 fb_r_sof1;
	u32 fb_r_sof2;
	VO_CONTROL_type vo_control;
	VO_BORDER_COL_type vo_border_col;
};

struct Renderer
{
	Renderer() {
		EventManager::listen(EmuEvent::Terminate, onEvent, this);
		EventManager::listen(EmuEvent::LoadState, onEvent, this);
	}
	virtual ~Renderer() {
		EventManager::unlisten(EmuEvent::Terminate, onEvent, this);
		EventManager::unlisten(EmuEvent::LoadState, onEvent, this);
	}

	virtual bool Init() = 0;
	virtual void Term() = 0;

	virtual void Process(TA_context *ctx) = 0;
	virtual bool Render() = 0;
	virtual void RenderFramebuffer(const FramebufferInfo& info) = 0;
	virtual bool RenderLastFrame() { return false; }
	// Get the last rendered frame pixel data in RGB format
	// The returned image is rotated and scaled (upward orientation and square pixels)
	// If both width and height are zero, the internal render resolution will be used.
	// Otherwise either width or height will be used as the maximum width or height respectively.
	virtual bool GetLastFrame(std::vector<u8>& data, int& width, int& height) { return false; }

	virtual bool Present() { return true; }

	virtual BaseTextureCacheData *GetTexture(TSP tsp, TCW tcw) { return nullptr; }

protected:
	bool resetTextureCache = false;
	bool clearLastFrame = false;
	bool updatePalette = true;
	bool updateFogTable = true;

private:
	static void onEvent(EmuEvent event, void *arg)
	{
		Renderer *renderer = static_cast<Renderer*>(arg);
		renderer->resetTextureCache = true;
		renderer->updatePalette = true;
		renderer->updateFogTable = true;
		if (event == EmuEvent::Terminate)
			renderer->clearLastFrame = true;
	}
	friend void rend_updatePalette();
	friend void rend_updateFogTable();
};

extern Renderer* renderer;

extern u32 fb_watch_addr_start;
extern u32 fb_watch_addr_end;
extern bool fb_dirty;

void check_framebuffer_write();
static inline void rend_updatePalette() {
	if (renderer != nullptr)
		renderer->updatePalette = true;
}
static inline void rend_updateFogTable() {
	if (renderer != nullptr)
		renderer->updateFogTable = true;
}
