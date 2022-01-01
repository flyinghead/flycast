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
#if defined(USE_SDL)
#include <math.h>
#include <algorithm>
#include "gl_context.h"
#include "rend/gui.h"
#include "sdl/sdl.h"
#include "cfg/option.h"

extern "C" int eglGetError();

SDLGLGraphicsContext theGLContext;

bool SDLGLGraphicsContext::init()
{
	instance = this;
#ifdef GLES
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (!sdl_recreate_window(SDL_WINDOW_OPENGL))
		return false;

	SDL_Window * const sdlWindow = (SDL_Window *)window;
	glcontext = SDL_GL_CreateContext(sdlWindow);
#ifndef GLES
	if (glcontext == SDL_GLContext())
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#ifdef __APPLE__
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
		glcontext = SDL_GL_CreateContext(sdlWindow);
	}
#endif
	if (glcontext == SDL_GLContext())
	{
		ERROR_LOG(RENDERER, "Error creating SDL GL context");
		SDL_DestroyWindow(sdlWindow);
		window = nullptr;
		return false;
	}
	SDL_GL_MakeCurrent(sdlWindow, NULL);

	int w, h;
	SDL_GetWindowSize(sdlWindow, &w, &h);
	SDL_GL_GetDrawableSize(sdlWindow, &settings.display.width, &settings.display.height);
	settings.display.pointScale = (float)settings.display.width / w;

	float hdpi, vdpi;
	if (!SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(sdlWindow), nullptr, &hdpi, &vdpi))
		screen_dpi = (int)roundf(std::max(hdpi, vdpi));

	INFO_LOG(RENDERER, "Created SDL Window and GL Context successfully");

	SDL_GL_MakeCurrent(sdlWindow, glcontext);
	// Swap at vsync
	swapOnVSync = config::VSync;
	if (settings.display.refreshRate > 60.f)
		swapInterval = settings.display.refreshRate / 60.f;
	else
		swapInterval = 1;

	SDL_GL_SetSwapInterval(swapOnVSync ? swapInterval : 0);

#ifdef GLES
	load_gles_symbols();
#elif !defined(__APPLE__)
	if (gl3wInit() == -1 || !gl3wIsSupported(3, 0))
	{
		ERROR_LOG(RENDERER, "gl3wInit failed or GL 3.0 not supported");
		return false;
	}
#endif
	postInit();

#ifdef TARGET_UWP
	// Force link with libGLESv2.dll and libEGL.dll
#undef glGetError
	glGetError();
	eglGetError();
#endif

	return true;
}

void SDLGLGraphicsContext::swap()
{
	do_swap_automation();
	if (swapOnVSync == (settings.input.fastForwardMode || !config::VSync))
	{
		swapOnVSync = (!settings.input.fastForwardMode && config::VSync);
		if (settings.display.refreshRate > 60.f)
			swapInterval = settings.display.refreshRate / 60.f;
		else
			swapInterval = 1;
		SDL_GL_SetSwapInterval(swapOnVSync ? swapInterval : 0);
	}
	SDL_GL_SwapWindow((SDL_Window *)window);

	// Check if drawable has been resized
	SDL_GL_GetDrawableSize((SDL_Window *)window, &settings.display.width, &settings.display.height);
#ifdef __SWITCH__
	float newScaling = settings.display.height == 720 ? 1.5f : 1.0f;
	if (newScaling != scaling)
	{
		// Restart the UI to take the new scaling factor into account
		scaling = newScaling;
		gui_term();
		gui_init();
	}
#endif
}

void SDLGLGraphicsContext::term()
{
	preTerm();
	if (glcontext != nullptr)
	{
		SDL_GL_DeleteContext(glcontext);
		glcontext = nullptr;
	}
}

#endif

