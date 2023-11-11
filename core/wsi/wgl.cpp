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
#if defined(_WIN32) && !defined(USE_SDL) && !defined(LIBRETRO)
#include "types.h"
#include "wgl.h"

#include <glad/wgl.h>

void CreateMainWindow();

WGLGraphicsContext theGLContext;

bool WGLGraphicsContext::init()
{
	if (ourOpenGLRenderingContext != NULL)
		// Already initialized
		return true;

	instance = this;
	CreateMainWindow();
	PIXELFORMATDESCRIPTOR pfd =
	{
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
			PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
			32,                        //Colordepth of the framebuffer.
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			0,                        //Number of bits for the depthbuffer
			0,                        //Number of bits for the stencilbuffer
			0,                        //Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
	};

	int  letWindowsChooseThisPixelFormat;
	letWindowsChooseThisPixelFormat = ChoosePixelFormat((HDC)display, &pfd);
	SetPixelFormat((HDC)display,letWindowsChooseThisPixelFormat, &pfd);

	HGLRC tempOpenGLContext = wglCreateContext((HDC)display);
	wglMakeCurrent((HDC)display, tempOpenGLContext);

	gladLoaderLoadWGL((HDC) display);

	int attribs[] =
	{
		WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};

	ourOpenGLRenderingContext = wglCreateContextAttribsARB((HDC)display, 0, attribs);

	if (!ourOpenGLRenderingContext)
	{
		INFO_LOG(RENDERER, "OpenGL 4.3 not supported");
		// Try Gl 3.1
		attribs[1] = 3;
		attribs[3] = 1;
		ourOpenGLRenderingContext = wglCreateContextAttribsARB((HDC)display, 0, attribs);
	}

	bool rv = true;

	if (ourOpenGLRenderingContext)
		wglMakeCurrent((HDC)display, ourOpenGLRenderingContext);
	else
		rv = false;

	wglDeleteContext(tempOpenGLContext);

	if (rv) {
		rv = (gladLoadGL((GLADloadfunc) wglGetProcAddress) != 0) && GLAD_GL_VERSION_3_1;
	}

	RECT r;
	GetClientRect((HWND)window, &r);
	settings.display.width = r.right - r.left;
	settings.display.height = r.bottom - r.top;

	if (rv)
		postInit();

	return rv;
}

void WGLGraphicsContext::swap()
{
	do_swap_automation();
	wglSwapLayerBuffers((HDC)display, WGL_SWAP_MAIN_PLANE);
}

void WGLGraphicsContext::term()
{
	preTerm();
	if (ourOpenGLRenderingContext != NULL)
	{
		wglMakeCurrent((HDC)display, NULL);
		wglDeleteContext(ourOpenGLRenderingContext);
		ourOpenGLRenderingContext = NULL;
	}
}

#endif
