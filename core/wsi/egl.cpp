/*
    Created on: Oct 18, 2019

	Copyright 2019 flyinghead

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
#include "gl_context.h"

#ifdef USE_EGL
#include "types.h"
#include "cfg/option.h"

#ifdef __ANDROID__
#include <android/native_window.h> // requires ndk r5 or newer
#endif
#ifdef TARGET_PANDORA
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#ifndef FBIO_WAITFORVSYNC
#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif
#endif

EGLGraphicsContext theGLContext;

bool EGLGraphicsContext::makeCurrent()
{
	if (surface == EGL_NO_SURFACE || context == EGL_NO_CONTEXT)
		return false;
	return eglMakeCurrent(display, surface, surface, context);
}

bool EGLGraphicsContext::init()
{
	//try to get a display
	display = eglGetDisplay((EGLNativeDisplayType)display);

	//if failed, get the default display (this will not happen in win32)
	if (display == EGL_NO_DISPLAY)
		display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	// Initialise EGL
	EGLint maj, min;
	if (!eglInitialize(display, &maj, &min))
	{
		ERROR_LOG(RENDERER, "EGL Error: eglInitialize failed");
		return false;
	}

	if (surface == EGL_NO_SURFACE)
	{
		EGLint pi32ConfigAttribs[]  = {
				EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
				EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
				EGL_DEPTH_SIZE, 24,
				EGL_STENCIL_SIZE, 8,
				EGL_RED_SIZE, 8,
				EGL_GREEN_SIZE, 8,
				EGL_BLUE_SIZE, 8,
				EGL_NONE
		};

		int num_config;

		EGLConfig config;
		if (!eglChooseConfig(display, pi32ConfigAttribs, &config, 1, &num_config) || (num_config != 1))
		{
			ERROR_LOG(RENDERER, "EGL Error: eglChooseConfig failed");
			return false;
		}
#ifdef __ANDROID__
		EGLint format;
		if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format))
		{
			ERROR_LOG(RENDERER, "eglGetConfigAttrib() returned error %x", eglGetError());
			return false;
		}
		ANativeWindow_setBuffersGeometry((ANativeWindow *)window, 0, 0, format);
#endif
		surface = eglCreateWindowSurface(display, config, (EGLNativeWindowType)window, NULL);

		if (surface == EGL_NO_SURFACE)
		{
			ERROR_LOG(RENDERER, "EGL Error: eglCreateWindowSurface failed: %x", eglGetError());
			return false;
		}

#ifndef GLES
		bool try_full_gl = true;
		if (!eglBindAPI(EGL_OPENGL_API))
		{
			INFO_LOG(RENDERER, "eglBindAPI(EGL_OPENGL_API) failed: %x", eglGetError());
			try_full_gl = false;
		}
		if (try_full_gl)
		{
			EGLint contextAttrs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
									  EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
									  EGL_NONE };
			context = eglCreateContext(display, config, NULL, contextAttrs);
			if (context != EGL_NO_CONTEXT)
			{
				makeCurrent();
				if (gl3wInit())
					ERROR_LOG(RENDERER, "gl3wInit() failed");
			}
		}
#endif
		if (context == EGL_NO_CONTEXT)
		{
			if (!eglBindAPI(EGL_OPENGL_ES_API))
			{
				ERROR_LOG(RENDERER, "eglBindAPI() failed: %x", eglGetError());
				return false;
			}

			EGLint contextAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2 , EGL_NONE };

			context = eglCreateContext(display, config, NULL, contextAttrs);

			if (context == EGL_NO_CONTEXT)
			{
				ERROR_LOG(RENDERER, "eglCreateContext() failed: %x", eglGetError());
				return false;
			}

#ifdef GLES
// EGL only supports runtime loading with android? TDB
			load_gles_symbols();
#else
			makeCurrent();
			if (gl3wInit())
				INFO_LOG(RENDERER, "gl3wInit() failed");
#endif
		}
	}
	else if (glGetError == NULL)
	{
		// Needed when the context is not created here (Android java mode, iOS)
		// Broken atm
		load_gles_symbols();
	}

	if (!makeCurrent())
	{
		ERROR_LOG(RENDERER, "eglMakeCurrent() failed: %x", eglGetError());
		return false;
	}

	EGLint w,h;
	eglQuerySurface(display, surface, EGL_WIDTH, &w);
	eglQuerySurface(display, surface, EGL_HEIGHT, &h);
	NOTICE_LOG(RENDERER, "eglQuerySurface: %d - %d", w, h);

	settings.display.width = w;
	settings.display.height = h;

#ifdef TARGET_PANDORA
	fbdev = open("/dev/fb0", O_RDONLY);
#else
	swapOnVSync = config::VSync;
	eglSwapInterval(display, (int)swapOnVSync);
#endif

	postInit();

	INFO_LOG(RENDERER, "EGL config: %p, %p, %p %dx%d", context, display, surface, w, h);
	return true;
}

void EGLGraphicsContext::term()
{
	preTerm();
	eglMakeCurrent(display, NULL, NULL, EGL_NO_CONTEXT);
	if (context != EGL_NO_CONTEXT)
		eglDestroyContext(display, context);
	if (surface != EGL_NO_SURFACE)
		eglDestroySurface(display, surface);
#ifdef TARGET_PANDORA
	if (display != EGL_NO_DISPLAY)
		eglTerminate(display);
	if (fbdev >= 0)
		close(fbdev);
	fbdev = -1;
#endif

	context = EGL_NO_CONTEXT;
	surface = EGL_NO_SURFACE;
	display = EGL_NO_DISPLAY;
}

void EGLGraphicsContext::swap()
{
	do_swap_automation();
	if (swapOnVSync == (settings.input.fastForwardMode || !config::VSync))
	{
		swapOnVSync = (!settings.input.fastForwardMode && config::VSync);
		eglSwapInterval(display, (int)swapOnVSync);
	}
	eglSwapBuffers(display, surface);
}
#endif // USE_EGL
