/*
	Copyright 2021 flyinghead

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
#if defined(LIBRETRO) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGLES))
#include "gl_context.h"

class LibretroGraphicsContext : public GLGraphicsContext
{
public:
	~LibretroGraphicsContext() {
		preTerm();
	}

	void swap() override {}

	static void Create(void *window, void *display) {
		new LibretroGraphicsContext(window, display);
	}

private:
	LibretroGraphicsContext(void *window, void *display)
		: GLGraphicsContext(window, display)
	{
		postInit();
	}
};
#endif
