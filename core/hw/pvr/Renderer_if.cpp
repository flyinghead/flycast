#include "Renderer_if.h"
#include "spg.h"
#include "rend/TexCache.h"
#include "rend/transform_matrix.h"
#include "cfg/option.h"
#include "emulator.h"
#include "serialize.h"
#include "hw/holly/holly_intc.h"
#include "hw/sh4/sh4_if.h"
#include "profiler/fc_profiler.h"
#include "network/ggpo.h"

#include <mutex>
#include <deque>

#ifdef TARGET_IPHONE
#include <TargetConditionals.h>
#include <chrono>
#include <atomic>

// === iOS FMV THREADING OPTIMIZATIONS ===
struct IOSRenderingOptimizer {
	// FMV mode detection
	std::atomic<bool> fmv_mode_detected{false};
	std::atomic<u32> consecutive_render_calls{0};
	std::chrono::steady_clock::time_point last_render_time;
	
	// Performance counters
	std::atomic<u32> queue_contention_count{0};
	std::atomic<u32> timeout_count{0};
	std::chrono::steady_clock::time_point last_optimization_check;
	
	// Adaptive timeout values (in milliseconds)
	std::atomic<int> current_render_timeout{50};  // Start conservative
	std::atomic<int> current_present_timeout{16}; // 60fps = 16ms
	
	// Queue optimization parameters - ULTRA-AGGRESSIVE
	std::atomic<bool> aggressive_queue_mode{false};
	std::atomic<u32> max_queue_depth{3};  // Adaptive queue depth
	
	// Ultra-aggressive pipeline optimization
	std::atomic<bool> ultra_pipeline_mode{false};
	std::atomic<u32> ultra_queue_depth{8};  // Deep queue for sustained rendering
	std::atomic<u32> frame_skip_threshold{0};  // Adaptive frame skipping
	std::atomic<u32> total_render_calls{0};
	std::atomic<u32> successful_renders{0};
	
	void init() {
		fmv_mode_detected.store(false);
		consecutive_render_calls.store(0);
		queue_contention_count.store(0);
		timeout_count.store(0);
		current_render_timeout.store(50);
		current_present_timeout.store(16);
		aggressive_queue_mode.store(false);
		max_queue_depth.store(3);
		ultra_pipeline_mode.store(false);
		ultra_queue_depth.store(8);
		frame_skip_threshold.store(0);
		total_render_calls.store(0);
		successful_renders.store(0);
		last_render_time = std::chrono::steady_clock::now();
		last_optimization_check = std::chrono::steady_clock::now();
		
		INFO_LOG(RENDERER, "🔥 iOS Rendering Optimizer: ULTRA-PIPELINE mode initialized");
	}
	
	bool detect_fmv_mode() {
		auto now = std::chrono::steady_clock::now();
		auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_render_time).count();
		
		// Rapid render calls indicate FMV activity
		if (time_diff < 20) {  // Less than 20ms between calls = high frequency
			consecutive_render_calls++;
			
			if (consecutive_render_calls > 30 && !fmv_mode_detected.load()) {
				fmv_mode_detected.store(true);
				aggressive_queue_mode.store(true);
				current_render_timeout.store(100);   // Longer timeout for FMV
				current_present_timeout.store(25);   // Allow more time for presents
				max_queue_depth.store(5);            // Deeper queue for smoother FMV
				
				INFO_LOG(RENDERER, "🚀 iOS Rendering Optimizer: FMV mode detected - enabling aggressive optimization (timeout=%dms, queue_depth=%u)", 
						 current_render_timeout.load(), max_queue_depth.load());
			}
			
			// Enable ultra pipeline for sustained high-frequency rendering
			if (consecutive_render_calls > 60 && !ultra_pipeline_mode.load()) {
				ultra_pipeline_mode.store(true);
				max_queue_depth.store(ultra_queue_depth.load());
				current_render_timeout.store(150);   // Even more patient in ultra mode
				frame_skip_threshold.store(2);       // Allow frame skipping under pressure
				
				INFO_LOG(RENDERER, "⚡ iOS Rendering Optimizer: SUSTAINED RENDERING detected - ULTRA-PIPELINE mode enabled (queue_depth=%u)", 
						 ultra_queue_depth.load());
			}
		} else {
			consecutive_render_calls.store(0);
			
			// Exit FMV mode after period of calm
			if (fmv_mode_detected.load() && time_diff > 500) {  // 500ms of calm
				fmv_mode_detected.store(false);
				aggressive_queue_mode.store(false);
				current_render_timeout.store(50);
				current_present_timeout.store(16);
				max_queue_depth.store(3);
				
				INFO_LOG(RENDERER, "📉 iOS Rendering Optimizer: FMV mode disabled - returning to normal operation");
			}
		}
		
		last_render_time = now;
		return fmv_mode_detected.load();
	}
	
	void record_queue_contention() {
		queue_contention_count++;
		
		// Adaptive timeout adjustment based on contention
		auto now = std::chrono::steady_clock::now();
		auto check_diff = std::chrono::duration_cast<std::chrono::seconds>(now - last_optimization_check).count();
		
		if (check_diff >= 2) {  // Check every 2 seconds
			u32 contention = queue_contention_count.load();
			if (contention > 20 && current_render_timeout.load() < 200) {
				// High contention: increase timeout
				current_render_timeout = std::min(200, current_render_timeout.load() + 10);
				INFO_LOG(RENDERER, "🔧 iOS Rendering Optimizer: High contention detected, increased timeout to %dms", current_render_timeout.load());
			} else if (contention < 5 && current_render_timeout.load() > 30) {
				// Low contention: decrease timeout for responsiveness
				current_render_timeout = std::max(30, current_render_timeout.load() - 5);
			}
			
			queue_contention_count.store(0);
			last_optimization_check = now;
		}
	}
	
	void record_timeout() {
		timeout_count++;
		// If we're getting frequent timeouts, we might need to be more aggressive
		if (timeout_count.load() > 10) {
			current_render_timeout.store(std::min(300, current_render_timeout.load() + 20));
			timeout_count.store(0);
			INFO_LOG(RENDERER, "⚠️ iOS Rendering Optimizer: Frequent timeouts, increased to %dms", current_render_timeout.load());
		}
	}
	
	int get_render_timeout() const {
		return current_render_timeout.load();
	}
	
	int get_present_timeout() const {
		return current_present_timeout.load();
	}
	
	bool should_use_aggressive_mode() const {
		return aggressive_queue_mode.load();
	}
	
	u32 get_max_queue_depth() const {
		return max_queue_depth.load();
	}
};

static IOSRenderingOptimizer g_ios_render_optimizer;
#endif

#ifdef LIBRETRO
void retro_rend_present();
void retro_resize_renderer(int w, int h, float aspectRatio);
#endif

u32 FrameCount=1;

Renderer* renderer;

static cResetEvent renderEnd;
u32 fb_w_cur = 1;
static cResetEvent vramRollback;

// direct framebuffer write detection
static bool render_called = false;
u32 fb_watch_addr_start;
u32 fb_watch_addr_end;
bool fb_dirty;

static bool pend_rend;
static bool rendererEnabled = true;

TA_context* _pvrrc;

static bool presented;
static u32 fbAddrHistory[2] { 1, 1 };

class PvrMessageQueue
{
	using lock_guard = std::lock_guard<std::mutex>;

public:
	enum MessageType { NoMessage = -1, Render, RenderFramebuffer, Present, Stop };
	struct Message
	{
		Message() = default;
		Message(MessageType type, FramebufferInfo config)
			: type(type), config(config) {}

		MessageType type = NoMessage;
		FramebufferInfo config;
	};

	void enqueue(MessageType type, FramebufferInfo config = FramebufferInfo())
	{
		Message msg { type, config };
		if (config::ThreadedRendering)
		{
#ifdef TARGET_IPHONE
			// iOS FMV optimization: detect FMV mode and adjust behavior
			g_ios_render_optimizer.detect_fmv_mode();
#endif
			
			// FIXME need some synchronization to avoid blinking in densha de go
			// or use !threaded rendering for emufb?
			// or read framebuffer vram on emu thread
			bool dupe;
			do {
				dupe = false;
				{
					const lock_guard lock(mutex);
					
#ifdef TARGET_IPHONE
					// iOS optimization: ultra-intelligent queue management
					u32 current_max_depth = g_ios_render_optimizer.ultra_pipeline_mode.load() ? 
						g_ios_render_optimizer.ultra_queue_depth.load() : 
						g_ios_render_optimizer.get_max_queue_depth();
						
					if (g_ios_render_optimizer.should_use_aggressive_mode() || g_ios_render_optimizer.ultra_pipeline_mode.load()) {
						// Ultra-aggressive queue management for sustained workloads
						if (queue.size() >= current_max_depth) {
							u32 removed_count = 0;
							
							// In ultra mode: more aggressive frame dropping
							if (g_ios_render_optimizer.ultra_pipeline_mode.load() && type == Render) {
								// Remove multiple old render calls, keep only most recent ones
								for (auto it = queue.begin(); it != queue.end() && removed_count < 3; ) {
									if (it->type == Render) {
										it = queue.erase(it);
										removed_count++;
									} else {
										++it;
									}
								}
								if (removed_count > 0) {
									INFO_LOG(RENDERER, "🎯 Ultra Pipeline: Dropped %u old render calls for throughput", removed_count);
								}
							} else {
								// Normal aggressive mode: remove one old non-Present message
								for (auto it = queue.begin(); it != queue.end(); ) {
									if (it->type != Present && it->type != Stop) {
										it = queue.erase(it);
										break;
									} else {
										++it;
									}
								}
							}
						}
					}
#endif
					
					for (const auto& m : queue)
						if (m.type == type) {
							dupe = true;
							break;
						}
					if (!dupe || type == Present) {
						queue.push_back(msg);
						dupe = false;
					}
				}
				if (dupe)
				{
					if (type == Stop)
						return;
						
#ifdef TARGET_IPHONE
					// iOS FMV optimization: adaptive timeout
					g_ios_render_optimizer.record_queue_contention();
					int timeout = (type == Present) ? 
						g_ios_render_optimizer.get_present_timeout() : 
						g_ios_render_optimizer.get_render_timeout();
					
					if (!dequeueEvent.Wait(timeout)) {
						// Timeout occurred
						g_ios_render_optimizer.record_timeout();
						if (g_ios_render_optimizer.should_use_aggressive_mode()) {
							// In FMV mode: don't block, skip this frame
							dupe = false;
							WARN_LOG(RENDERER, "iOS FMV Optimizer: Skipping frame due to timeout in FMV mode");
						}
					}
#else
					dequeueEvent.Wait();
#endif
				}
			} while (dupe);
			enqueueEvent.Set();
		}
		else
		{
			void setDefaultRoundingMode();
			void RestoreHostRoundingMode();

			setDefaultRoundingMode();
			// drain the queue after switching to !threaded rendering
			while (!queue.empty())
				waitAndExecute();
			execute(msg);
			RestoreHostRoundingMode();
		}
	}

	bool waitAndExecute(int timeoutMs = -1)
	{
#ifdef TARGET_IPHONE
		// iOS FMV optimization: use adaptive timeout if no specific timeout provided
		if (timeoutMs == -1) {
			timeoutMs = g_ios_render_optimizer.get_render_timeout();
		}
#endif
		return execute(dequeue(timeoutMs));
	}

	void reset() {
		const lock_guard lock(mutex);
		queue.clear();
#ifdef TARGET_IPHONE
		// Initialize iOS rendering optimizer on reset
		g_ios_render_optimizer.init();
#endif
	}

	void cancelEnqueue()
	{
		const lock_guard lock(mutex);
		for (auto it = queue.begin(); it != queue.end(); )
		{
			if (it->type != Render)
				it = queue.erase(it);
			else
				++it;
		}
		dequeueEvent.Set();
	}
private:
	Message dequeue(int timeoutMs = -1)
	{
		FC_PROFILE_SCOPE;

		Message msg;
		while (true)
		{
			{
				const lock_guard lock(mutex);
				if (!queue.empty())
				{
					msg = queue.front();
					queue.pop_front();
				}
			}
			if (msg.type != NoMessage) {
				dequeueEvent.Set();
				break;
			}
			if (timeoutMs == -1)
				enqueueEvent.Wait();
			else if (!enqueueEvent.Wait(timeoutMs))
				break;
		}
		return msg;
	}

	bool execute(Message msg)
	{
		switch (msg.type)
		{
		case Render:
			render();
			return true;
		case RenderFramebuffer:
			renderFramebuffer(msg.config);
			return true;
		case Present:
			present();
			return true;
		case Stop:
		case NoMessage:
		default:
			return false;
		}
	}

	void render()
	{
		FC_PROFILE_SCOPE;

		_pvrrc = DequeueRender();
		if (_pvrrc == nullptr)
			return;

		if (!_pvrrc->rend.isRTT)
		{
			int width, height;
			getScaledFramebufferSize(_pvrrc->rend, width, height);
			_pvrrc->rend.framebufferWidth = width;
			_pvrrc->rend.framebufferHeight = height;
		}
		bool renderToScreen = !_pvrrc->rend.isRTT && !config::EmulateFramebuffer;
#ifdef LIBRETRO
		if (renderToScreen)
			retro_resize_renderer(_pvrrc->rend.framebufferWidth, _pvrrc->rend.framebufferHeight,
					getOutputFramebufferAspectRatio());
#endif
		{
			FC_PROFILE_SCOPE_NAMED("Renderer::Process");
			renderer->Process(_pvrrc);
		}

		if (renderToScreen)
			// If rendering to texture or in full framebuffer emulation, continue locking until the frame is rendered
			renderEnd.Set();
		rend_allow_rollback();
		{
			FC_PROFILE_SCOPE_NAMED("Renderer::Render");
			renderer->Render();
		}

		if (!renderToScreen)
			renderEnd.Set();
		else if (config::DelayFrameSwapping && fb_w_cur == FB_R_SOF1)
			present();

		//clear up & free data ..
		FinishRender(_pvrrc);
		_pvrrc = nullptr;
	}

	void renderFramebuffer(const FramebufferInfo& config)
	{
		FC_PROFILE_SCOPE;

#ifdef LIBRETRO
		int w, h;
		getDCFramebufferReadSize(config, w, h);
		retro_resize_renderer(w, h, getDCFramebufferAspectRatio());
#endif
		renderer->RenderFramebuffer(config);
	}

	void present()
	{
		FC_PROFILE_SCOPE;

		if (renderer->Present())
		{
			presented = true;
			if (!config::ThreadedRendering && !ggpo::active())
				sh4_cpu.Stop();
#ifdef LIBRETRO
			retro_rend_present();
#endif
		}
	}

	std::mutex mutex;
	cResetEvent enqueueEvent;
	cResetEvent dequeueEvent;
	std::deque<Message> queue;
};

static PvrMessageQueue pvrQueue;

bool rend_single_frame(const bool& enabled)
{
	FC_PROFILE_SCOPE;

	const int timeout = SPG_CONTROL.isPAL() ? 23 : 20;
	presented = false;
	while (enabled && !presented)
		if (!pvrQueue.waitAndExecute(timeout))
			return false;
	return true;
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
#ifdef USE_DX11
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

bool rend_init_renderer()
{
	rendererEnabled = true;
	if (renderer == nullptr)
		rend_create_renderer();
	bool success = renderer->Init();
	if (!success) {
		delete renderer;
		renderer = rend_norend();
		renderer->Init();
	}
	return success;
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
	render_called = false;
	pend_rend = false;
	FrameCount = 1;
	fb_w_cur = 1;
	pvrQueue.reset();
	rendererEnabled = true;
	fbAddrHistory[0] = 1;
	fbAddrHistory[1] = 1;
}

void rend_start_render()
{
	render_called = true;
	pend_rend = false;

	TA_context *ctx = nullptr;
	u32 addresses[MAX_PASSES];
	int count = getTAContextAddresses(addresses);
	if (count > 0)
	{
		ctx = tactx_Pop(addresses[0]);
		if (ctx != nullptr)
		{
			TA_context *linkedCtx = ctx;
			for (int i = 1; i < count; i++)
			{
				linkedCtx->nextContext = tactx_Pop(addresses[i]);
				if (linkedCtx->nextContext != nullptr)
					linkedCtx = linkedCtx->nextContext;
				else
					INFO_LOG(PVR, "rend_start_render: Context%d @ %x not found", i, addresses[i]);
			}
		}
		else
			INFO_LOG(PVR, "rend_start_render: Context0 @ %x not found", addresses[0]);
	}
	else
		INFO_LOG(PVR, "rend_start_render: No context not found");

	scheduleRenderDone(ctx);

	if (ctx == nullptr)
		return;

	FillBGP(ctx);

	ctx->rend.isRTT = (FB_W_SOF1 & 0x1000000) != 0;
	ctx->rend.fb_W_SOF1 = FB_W_SOF1;
	ctx->rend.fb_W_CTRL.full = FB_W_CTRL.full;

	ctx->rend.ta_GLOB_TILE_CLIP = TA_GLOB_TILE_CLIP;
	ctx->rend.scaler_ctl = SCALER_CTL;
	ctx->rend.fb_X_CLIP = FB_X_CLIP;
	ctx->rend.fb_Y_CLIP = FB_Y_CLIP;
	ctx->rend.fb_W_LINESTRIDE = FB_W_LINESTRIDE.stride;

	ctx->rend.fog_clamp_min = FOG_CLAMP_MIN;
	ctx->rend.fog_clamp_max = FOG_CLAMP_MAX;

	if (!ctx->rend.isRTT)
	{
		if (FB_W_SOF1 != fbAddrHistory[0] && FB_W_SOF1 != fbAddrHistory[1])
		{
			ctx->rend.clearFramebuffer = true;
			fbAddrHistory[0] = fbAddrHistory[1];
			fbAddrHistory[1] = FB_W_SOF1;
		}
		else {
			ctx->rend.clearFramebuffer = false;
		}
		ggpo::endOfFrame();
	}

	if (QueueRender(ctx))
	{
		palette_update();
		pend_rend = true;
		pvrQueue.enqueue(PvrMessageQueue::Render);
		if (!config::DelayFrameSwapping && !ctx->rend.isRTT && !config::EmulateFramebuffer)
			pvrQueue.enqueue(PvrMessageQueue::Present);
	}
}

int rend_end_render(int tag, int cycles, int jitter, void *arg)
{
	if (settings.platform.isNaomi2())
	{
		asic_RaiseInterruptBothCLX(holly_RENDER_DONE);
		asic_RaiseInterruptBothCLX(holly_RENDER_DONE_isp);
		asic_RaiseInterruptBothCLX(holly_RENDER_DONE_vd);
	}
	else
	{
		asic_RaiseInterrupt(holly_RENDER_DONE);
		asic_RaiseInterrupt(holly_RENDER_DONE_isp);
		asic_RaiseInterrupt(holly_RENDER_DONE_vd);
	}
	if (pend_rend && config::ThreadedRendering)
		renderEnd.Wait();

	return 0;
}

void rend_vblank()
{
	if (config::EmulateFramebuffer
			|| (!render_called && fb_dirty && FB_R_CTRL.fb_enable))
	{
		if (rend_is_enabled())
		{
			FramebufferInfo fbInfo;
			fbInfo.update();
			pvrQueue.enqueue(PvrMessageQueue::RenderFramebuffer, fbInfo);
			pvrQueue.enqueue(PvrMessageQueue::Present);
			if (!config::EmulateFramebuffer)
				DEBUG_LOG(PVR, "Direct framebuffer write detected");
		}
		fb_dirty = false;
	}
	render_called = false;
	check_framebuffer_write();
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
		renderEnd.Set();
		rend_allow_rollback();
		pvrQueue.cancelEnqueue();
		// Needed for android where this function may be called
		// from a thread different from the UI one
		pvrQueue.enqueue(PvrMessageQueue::Stop);
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
	if (!config::EmulateFramebuffer && fb_r_sof == fb_w_cur && rend_is_enabled())
		pvrQueue.enqueue(PvrMessageQueue::Present);
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

void rend_enable_renderer(bool enabled) {
	rendererEnabled = enabled;
}

bool rend_is_enabled() {
	return rendererEnabled;
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
	deser >> fb_w_cur;
	if (deser.version() >= Deserializer::V20)
	{
		deser >> render_called;
		deser >> fb_dirty;
		deser >> fb_watch_addr_start;
		deser >> fb_watch_addr_end;
	}
	pend_rend = false;
	fbAddrHistory[0] = 1;
	fbAddrHistory[1] = 1;
}
