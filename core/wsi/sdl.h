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
#include <SDL.h>
#include "types.h"
#ifndef GLES
#include <GL4/gl3w.h>
#else
#include <GLES32/gl32.h>
#include <GLES32/gl2ext.h>
#ifndef GLES2
#include "gl32funcs.h"
#else
extern "C" void load_gles_symbols();
#endif
#endif
#include "gl_context.h"

class SDLGLGraphicsContext : public GLGraphicsContext
{
public:
	bool Init();
	void Term();
	void Swap();
	void SetWindow(SDL_Window *window) { this->window = window; }

private:
	SDL_Window* window = nullptr;
	SDL_GLContext glcontext = nullptr;
	bool swapOnVSync = false;
};

extern SDLGLGraphicsContext theGLContext;
