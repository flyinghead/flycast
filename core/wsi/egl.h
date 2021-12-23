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
#pragma once
#include <GLES32/gl32.h>
#include <GLES32/gl2ext.h>
#ifndef GLES2
#include "gl32funcs.h"
#else
extern "C" void load_gles_symbols();
#endif
#include "gl_context.h"

#define USE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>

class EGLGraphicsContext : public GLGraphicsContext
{
public:
	~EGLGraphicsContext() { term(); }

	bool init();
	void term() override;
	void swap();

private:
	bool makeCurrent();

	EGLDisplay display = EGL_NO_DISPLAY;
	EGLSurface surface = EGL_NO_SURFACE;
	EGLContext context = EGL_NO_CONTEXT;
#ifdef TARGET_PANDORA
	int fbdev = -1;
#endif
	bool swapOnVSync = false;
};

extern EGLGraphicsContext theGLContext;
