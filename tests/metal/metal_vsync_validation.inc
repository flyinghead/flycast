// Onscreen checks of the production presentation path. SPDX-License-Identifier: GPL-2.0-or-later
#include "ui/gui.h"
#include <mutex>
#include <algorithm>
#include <numeric>

void MetalContext::validateSyntheticVSync()
{
	struct Restore {
		MetalContext *context;
		bool vsync = config::VSync;
		bool duplicates = config::DupeFrames;
		bool fastForward = settings.input.fastForwardMode;
		GuiState gui = gui_state;
		int interval;
		id<MTLTexture> frame;
		float aspect;
		~Restore() {
			context->waitIdle(); context->impl->testPresentationObserver = {}; config::VSync = vsync; config::DupeFrames = duplicates;
			settings.input.fastForwardMode = fastForward; gui_state = gui;
			context->setSwapInterval(interval); context->setLastFrame(frame, aspect);
		}
	} restore{this, config::VSync, config::DupeFrames, settings.input.fastForwardMode, gui_state, impl->swapInterval, impl->lastFrame, impl->aspectRatio};
	struct Samples {
		std::mutex mutex;
		std::vector<double> times;
		dispatch_group_t pending = dispatch_group_create();
	};
	// Actual display timestamps, not GPU completion or CPU submission times.
	// The window must be visible; skipped drawables report presentedTime == 0.
	for (auto mode : {0, 1, 2, 3, 4, 5, 6, 7}) {
		const char *name = mode == 0 ? "interval-1" : mode == 1 ? "interval-2" : mode == 2 ? "menu-after-interval-2" : mode == 3 ? "vsync-off" : mode == 4 ? "fast-forward" : mode == 5 ? "duplicate-interval-1" : mode == 6 ? "duplicate-interval-2" : "duplicate-menu-after-interval-2";
		const bool synced = mode != 3 && mode != 4;
		config::DupeFrames = mode >= 5;
		config::VSync = mode != 3; settings.input.fastForwardMode = mode == 4;
		gui_state = (mode == 2 || mode == 7) ? GuiState::Main : GuiState::Closed;
		setSwapInterval(mode == 1 || mode == 2 || mode == 6 || mode == 7 ? 2 : 1);
		auto samples = std::make_shared<Samples>();
		const int copies = metal::presentationCount(settings.display.refreshRate, impl->swapInterval, gui_is_open(), config::VSync,
			settings.input.fastForwardMode, config::DupeFrames);
		impl->testPresentationObserver = [samples](id<MTLDrawable> drawable) {
			dispatch_group_enter(samples->pending);
			[drawable addPresentedHandler:^(id<MTLDrawable> displayed) {
				{ std::lock_guard<std::mutex> lock(samples->mutex); samples->times.push_back(displayed.presentedTime); }
				dispatch_group_leave(samples->pending);
			}];
		};
		const double started = CACurrentMediaTime();
		for (int i = 0; i < 90; ++i) {
			@autoreleasepool {
				// Each frame is different, so dropped display updates are observable.
				auto texture = makeTexture(1, 1, MTLPixelFormatRGBA8Unorm, MTLTextureUsageShaderRead, false, true);
				u32 pixel = 0xff808080u + u32(i);
				[texture replaceRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0 withBytes:&pixel bytesPerRow:4];
				setLastFrame(texture, 4.f / 3.f); beginPresentation();
				if (!impl->drawable) throw RendererException("Metal VSync test: no drawable");
				present();
			}
		}
		impl->testPresentationObserver = {};
		waitIdle();
		const double submitted = CACurrentMediaTime() - started;
		const bool complete = dispatch_group_wait(samples->pending, dispatch_time(DISPATCH_TIME_NOW, 3 * NSEC_PER_SEC)) == 0;
		std::vector<double> valid;
		{ std::lock_guard<std::mutex> lock(samples->mutex); for (double t : samples->times) if (t > 0) valid.push_back(t); }
		std::sort(valid.begin(), valid.end());
		std::vector<double> deltas;
		for (size_t i = 1; i < valid.size(); ++i) deltas.push_back((valid[i] - valid[i - 1]) * 1000);
		std::sort(deltas.begin(), deltas.end());
		NOTICE_LOG(RENDERER, "Metal VSync sample %s: submitted %d in %.3f s, visible %zu, callbacks %s, median %.3f ms, min %.3f ms, max %.3f ms",
			name, 90 * copies, submitted, valid.size(), complete ? "complete" : "timeout", deltas.empty() ? 0 : deltas[deltas.size() / 2],
			deltas.empty() ? 0 : deltas.front(), deltas.empty() ? 0 : deltas.back());
		if (impl->layer.displaySyncEnabled != synced) throw RendererException("Metal VSync toggle did not reach the layer");
		// Uncapped submission may intentionally replace frames before scanout.
		// Require nearly every frame only for the synchronized cadence checks.
		if (!complete || valid.size() < (synced ? size_t(80 * copies) : 2u)) {
			WARN_LOG(RENDERER, "Metal VSync SKIP: %s (window occluded or missing timestamps)", name);
			continue;
		}
		if (synced) {
			// This tight loop exercises presentation without the emulator's
			// 60/30 Hz pacing. Ordinary frames have no extra duration limit;
			// both they and explicit copies should follow display refresh here.
			const double refresh = std::isfinite(settings.display.refreshRate) && settings.display.refreshRate > 0
				? settings.display.refreshRate : 60;
			const double expected = 1000 / refresh;
			if (samples->times.size() != size_t(90 * copies)) throw RendererException("Metal duplicate drawable count mismatch");
			if (std::abs(deltas[deltas.size() / 2] - expected) > 1.5 || deltas.back() > 2.1 * expected)
				throw RendererException(std::string("Metal VSync cadence mismatch: ") + name);
		} else if (settings.display.refreshRate >= 100 && submitted >= 1.35) {
			throw RendererException(std::string("Metal uncapped mode retained the 60 FPS limit: ") + name);
		}
		NOTICE_LOG(RENDERER, "Metal VSync PASS: %s", name);
	}
}
