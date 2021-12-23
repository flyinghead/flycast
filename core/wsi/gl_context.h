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
#include "context.h"

#ifdef TEST_AUTOMATION
void do_swap_automation();
#else
static inline void do_swap_automation() {}
#endif

class GLGraphicsContext : public GraphicsContext
{
public:
	int getMajorVersion() const {
		return majorVersion;
	}
	int getMinorVersion() const {
		return minorVersion;
	}
	bool isGLES() const {
		return _isGLES;
	}
	std::string getDriverName() override;
	std::string getDriverVersion() override;

	bool hasPerPixel() override
	{
		return !isGLES()
				&& (getMajorVersion() > 4
						|| (getMajorVersion() == 4 && getMinorVersion() >= 3));
	}

protected:
	void postInit();
	void preTerm();
	void findGLVersion();

private:
	int majorVersion = 0;
	int minorVersion = 0;
	bool _isGLES = false;
};

#if defined(LIBRETRO)

#include "libretro.h"

#elif defined(TARGET_IPHONE)

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
