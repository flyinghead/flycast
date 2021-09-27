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
#include "types.h"

void do_swap_automation();

class GLGraphicsContext
{
public:
	int GetMajorVersion() const { return majorVersion; }
	int GetMinorVersion() const { return minorVersion; }
	bool IsGLES() const { return isGLES; }

protected:
	void PostInit();
	void PreTerm();
	void findGLVersion();

private:
	int majorVersion = 0;
	int minorVersion = 0;
	bool isGLES = false;
};

#if defined(LIBRETRO)

#include "libretro.h"

#elif defined(__APPLE__)

#include "osx.h"

#elif defined(USE_SDL)

#include "sdl.h"

#elif defined(GLES) || defined(__ANDROID__) || defined(__SWITCH__)

#include "egl.h"

#elif defined(_WIN32)

#include "wgl.h"

#elif defined(SUPPORT_X11)

#include "xgl.h"

#else

#error Unsupported window system

#endif
