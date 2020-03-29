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
#include "types.h"
#if defined(SUPPORT_X11) && !defined(USE_SDL)
#include "gl_context.h"

#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#endif
#ifndef GLX_CONTEXT_MINOR_VERSION_ARB
#define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
#endif

XGLGraphicsContext theGLContext;

static int x11_error_handler(Display *, XErrorEvent *)
{
	return 0;
}

bool XGLGraphicsContext::Init()
{
	typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

	glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
	glXCreateContextAttribsARB = (glXCreateContextAttribsARBProc)glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");
	verify(glXCreateContextAttribsARB != 0);
	int context_attribs[] =
	{
			GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
			GLX_CONTEXT_MINOR_VERSION_ARB, 3,
#ifndef NDEBUG
			GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
			GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
			None
	};
	int (*old_handler)(Display *, XErrorEvent *) = XSetErrorHandler(&x11_error_handler);

	context = glXCreateContextAttribsARB(this->display, *framebufferConfigs, 0, True, context_attribs);
	if (!context)
	{
		INFO_LOG(RENDERER, "Open GL 4.3 not supported");
		// Try GL 3.0
		context_attribs[1] = 3;
		context_attribs[3] = 0;
		context = glXCreateContextAttribsARB(this->display, *framebufferConfigs, 0, True, context_attribs);
		if (!context)
		{
			ERROR_LOG(RENDERER, "Open GL 3.0 not supported\n");
			return false;
		}
	}
	XSetErrorHandler(old_handler);
	XSync(this->display, False);

	glXMakeCurrent(this->display, this->window, context);

	screen_width = 640;
	screen_height = 480;
	if (gl3wInit() == -1 || !gl3wIsSupported(3, 1))
		return false;

	PostInit();

	return true;
}

bool XGLGraphicsContext::ChooseVisual(Display* x11Display, XVisualInfo** visual, int* depth)
{
	// Get a matching FB config
	static int visual_attribs[] =
	{
		GLX_X_RENDERABLE    , True,
		GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
		GLX_RENDER_TYPE     , GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
		GLX_RED_SIZE        , 8,
		GLX_GREEN_SIZE      , 8,
		GLX_BLUE_SIZE       , 8,
		GLX_ALPHA_SIZE      , 8,
		GLX_DEPTH_SIZE      , 24,
		GLX_STENCIL_SIZE    , 8,
		GLX_DOUBLEBUFFER    , True,
		//GLX_SAMPLE_BUFFERS  , 1,
		//GLX_SAMPLES         , 4,
		None
	};

	int glx_major, glx_minor;

	// FBConfigs were added in GLX version 1.3.
	if (!glXQueryVersion(x11Display, &glx_major, &glx_minor) ||
			((glx_major == 1) && (glx_minor < 3)) || (glx_major < 1))
	{
		ERROR_LOG(RENDERER, "Invalid GLX version");
		return false;
	}
	const long x11Screen = XDefaultScreen(x11Display);

	int fbcount;
	framebufferConfigs = glXChooseFBConfig(x11Display, x11Screen, visual_attribs, &fbcount);
	if (framebufferConfigs == nullptr)
	{
		ERROR_LOG(RENDERER, "Failed to retrieve a framebuffer config");
		return false;
	}
	INFO_LOG(RENDERER, "Found %d matching FB configs.", fbcount);

	// Get a visual
	XVisualInfo *vi = glXGetVisualFromFBConfig(x11Display, *framebufferConfigs);
	INFO_LOG(RENDERER, "Chosen visual ID = 0x%lx", vi->visualid);

	*depth = vi->depth;
	*visual = vi;

	return true;
}

void XGLGraphicsContext::Swap()
{
#ifdef TEST_AUTOMATION
	do_swap_automation();
#endif
	glXSwapBuffers(display, window);

	Window win;
	int temp;
	unsigned int tempu, new_w, new_h;
	XGetGeometry(display, window, &win, &temp, &temp, &new_w, &new_h, &tempu, &tempu);

	//if resized, clear up the draw buffers, to avoid out-of-draw-area junk data
	if (new_w != screen_width || new_h != screen_height) {
		screen_width = new_w;
		screen_height = new_h;
	}

#if 0
	//handy to debug really stupid render-not-working issues ...

	glcache.ClearColor(0, 0.5, 1, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glXSwapBuffers(display, window);


	glcache.ClearColor(1, 0.5, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glXSwapBuffers(display, window);
#endif
}

void XGLGraphicsContext::Term()
{
	PreTerm();
	if (context)
	{
		glXMakeCurrent(display, None, NULL);
		glXDestroyContext(display, context);
		context = (GLXContext)0;
	}
}

#endif
