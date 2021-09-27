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
#if defined(USE_SDL) && !defined(__APPLE__)
#include <math.h>
#include <algorithm>
#include "gl_context.h"
#include "rend/gui.h"
#include "sdl/sdl.h"
#include "cfg/option.h"

SDLGLGraphicsContext theGLContext;

bool SDLGLGraphicsContext::Init()
{
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
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (!sdl_recreate_window(SDL_WINDOW_OPENGL))
		return false;

	glcontext = SDL_GL_CreateContext(window);
#ifndef GLES
	if (glcontext == SDL_GLContext())
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		glcontext = SDL_GL_CreateContext(window);
	}
#endif
	if (glcontext == SDL_GLContext())
	{
		ERROR_LOG(RENDERER, "Error creating SDL GL context");
		SDL_DestroyWindow(window);
		window = nullptr;
		return false;
	}
	SDL_GL_MakeCurrent(window, NULL);

	SDL_GL_GetDrawableSize(window, &settings.display.width, &settings.display.height);

	float ddpi, hdpi, vdpi;
	if (!SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(window), &ddpi, &hdpi, &vdpi))
		screen_dpi = (int)roundf(std::max(hdpi, vdpi));

	INFO_LOG(RENDERER, "Created SDL Window and GL Context successfully");

	SDL_GL_MakeCurrent(window, glcontext);
#ifndef TEST_AUTOMATION
	// Swap at vsync
	swapOnVSync = config::VSync;
#else
	// Swap immediately
	swapOnVSync = false;
#endif
	swapInterval = 1;
	int displayIndex = SDL_GetWindowDisplayIndex(window);
	if (displayIndex < 0)
		WARN_LOG(RENDERER, "Cannot get the window display index: %s", SDL_GetError());
	else
	{
		SDL_DisplayMode mode{};
		if (SDL_GetDesktopDisplayMode(displayIndex, &mode) == 0) {
			INFO_LOG(RENDERER, "Monitor refresh rate: %d Hz", mode.refresh_rate);
			if (mode.refresh_rate > 100)
				swapInterval = 2;
		}
	}

	SDL_GL_SetSwapInterval(swapOnVSync ? swapInterval : 0);

#ifdef GLES
	load_gles_symbols();
#else
	if (gl3wInit() == -1 || !gl3wIsSupported(3, 0))
	{
		ERROR_LOG(RENDERER, "gl3wInit failed or GL 3.0 not supported");
		return false;
	}
#endif
	PostInit();

	return true;
}

void SDLGLGraphicsContext::Swap()
{
#ifdef TEST_AUTOMATION
	do_swap_automation();
#else
	if (swapOnVSync == (settings.input.fastForwardMode || !config::VSync))
	{
		swapOnVSync = (!settings.input.fastForwardMode && config::VSync);
		SDL_GL_SetSwapInterval(swapOnVSync ? swapInterval : 0);
	}
#endif
	SDL_GL_SwapWindow(window);

	// Check if drawable has been resized
	SDL_GL_GetDrawableSize(window, &settings.display.width, &settings.display.height);
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

void SDLGLGraphicsContext::Term()
{
	PreTerm();
	if (glcontext != nullptr)
	{
		SDL_GL_DeleteContext(glcontext);
		glcontext = nullptr;
	}
}

#endif

