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
#include <GL4/gl3w.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "gl_context.h"

class XGLGraphicsContext : public GLGraphicsContext
{
public:
	~XGLGraphicsContext() { term(); XFree(framebufferConfigs); }

	bool init();
	void term() override;
	void swap();
	bool ChooseVisual(Display* x11Display, XVisualInfo** visual, int* depth);

private:
	GLXContext context;
	GLXFBConfig* framebufferConfigs = nullptr;
	PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = nullptr;
	int (*glXSwapIntervalMESA)(unsigned int interval) = nullptr;
	bool swapOnVSync = false;
};

extern XGLGraphicsContext theGLContext;
