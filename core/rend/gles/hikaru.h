/*
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
// Sega Hikaru renderer for OpenGL (GL 3+ / GLES 3), used by the GLES and GL4 renderers.
// It draws a hikaru::DrawList with its own shaders, render states and textures (taken from the
// Hikaru texture store), independent of the PVR / Naomi 2 rendering path.
#pragma once
#include "gles.h"
#include "hw/hikaru/hikaru_drawlist.h"

// Draw a frame into the currently bound framebuffer (width x height pixels, with a depth buffer)
void hikaruDrawFrame(const hikaru::DrawList& dl, int width, int height);
// Release the Hikaru shaders, buffers and textures
void hikaruTerm();
