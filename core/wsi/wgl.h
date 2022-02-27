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
#include <windows.h>
#include <GL4/gl3w.h>
#include "gl_context.h"

#define WGL_DRAW_TO_WINDOW_ARB         0x2001
#define WGL_ACCELERATION_ARB           0x2003
#define WGL_SWAP_METHOD_ARB            0x2007
#define WGL_SUPPORT_OPENGL_ARB         0x2010
#define WGL_DOUBLE_BUFFER_ARB          0x2011
#define WGL_PIXEL_TYPE_ARB             0x2013
#define WGL_COLOR_BITS_ARB             0x2014
#define WGL_DEPTH_BITS_ARB             0x2022
#define WGL_STENCIL_BITS_ARB           0x2023
#define WGL_FULL_ACCELERATION_ARB      0x2027
#define WGL_SWAP_EXCHANGE_ARB          0x2028
#define WGL_TYPE_RGBA_ARB              0x202B
#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
#define WGL_CONTEXT_FLAGS_ARB              0x2094

#define		WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define 	WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define 	WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
#define 	WGL_CONTEXT_LAYER_PLANE_ARB   0x2093
#define 	WGL_CONTEXT_FLAGS_ARB   0x2094
#define 	WGL_CONTEXT_DEBUG_BIT_ARB   0x0001
#define 	WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB   0x0002
#define 	ERROR_INVALID_VERSION_ARB   0x2095
#define		WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats,
														int *piFormats, UINT *nNumFormats);
typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);
typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);


class WGLGraphicsContext : public GLGraphicsContext
{
public:
	bool init();
	void term() override;
	void swap();

private:
	HGLRC ourOpenGLRenderingContext = NULL;
};

extern WGLGraphicsContext theGLContext;
