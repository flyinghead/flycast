/*
	Copyright 2026 flyinghead

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
#include "types.h"
#include "wsi/egl.h"
#include "cfg/option.h"
#include "jni_util.h"
#ifdef SWAPPY

#define __eglext_h_ 1	// avoid multiply defined errors
#include <swappy/swappyGL.h>
#include <swappy/swappyGL_extra.h>

class SwappyGLGC : public EGLGraphicsContext
{
public:
	SwappyGLGC(void *window, void *display);
	~SwappyGLGC();
	void swap() override;

private:
	void changeSwapInterval();
	void activate(bool enable);

	bool swappyAvailable = false;
	bool enabled = false;
};

extern jobject g_activity;

void EGLGraphicsContext::Create(void *window, void *display) {
	new SwappyGLGC(window, display);
}

SwappyGLGC::SwappyGLGC(void *window, void *display)
	: EGLGraphicsContext(window, display)
{
	activate(config::FramePacing);
}

SwappyGLGC::~SwappyGLGC() {
	activate(false);
}

void SwappyGLGC::activate(bool enable)
{
	if (this->enabled == enable)
		return;
	this->enabled = enable;
	if (enable)
	{
		SwappyGL_init(jni::env(), g_activity);
		swappyAvailable = SwappyGL_isEnabled();
		if (swappyAvailable)
		{
			SwappyGL_setWindow((ANativeWindow *)window);
			SwappyGL_setSwapIntervalNS(SWAPPY_SWAP_60FPS);
			SwappyGL_setAutoSwapInterval(false);
			SwappyGL_setAutoPipelineMode(false);
		}
	}
	else {
		SwappyGL_destroy();
		swappyAvailable = false;
	}
}

void SwappyGLGC::swap()
{
	activate(config::FramePacing);
	do_swap_automation();
	changeSwapInterval();
	if (swapOnVSync && swappyAvailable)
		SwappyGL_swap(display, surface);
	else
		eglSwapBuffers(display, surface);
}

void SwappyGLGC::changeSwapInterval()
{
	if (!swappyAvailable) {
		EGLGraphicsContext::changeSwapInterval();
		return;
	}
	if (!gameSwapIntervalChanged
			&& swapOnVSync == (!settings.input.fastForwardMode && config::VSync))
		return;
	gameSwapIntervalChanged = false;
	const bool oldSwapOnVSync = swapOnVSync;
	swapOnVSync = (!settings.input.fastForwardMode && config::VSync);
	if (swapOnVSync)
	{
		if (!oldSwapOnVSync)
			// put swappy back in control
			eglSwapInterval(display, 1);
		currentSwapInterval = gameSwapInterval;
		if (swappyAvailable) {
			SwappyGL_setSwapIntervalNS(SWAPPY_SWAP_60FPS * currentSwapInterval);
			NOTICE_LOG(RENDERER, "Swap interval changed to %d (EGL max %d)", currentSwapInterval, maxSwapInterval);
		}
	}
	else {
		// No way to disable swappy so use EGL to disable sync wait
		eglSwapInterval(display, 0);
	}
}

#endif	// SWAPPY
