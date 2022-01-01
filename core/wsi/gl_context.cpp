/*
    Created on: Oct 19, 2019

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
#include "gl_context.h"

#include "rend/gles/opengl_driver.h"
#include "rend/gui.h"

void GLGraphicsContext::findGLVersion()
{
	while (true)
		if (glGetError() == GL_NO_ERROR)
			break;
	glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
	if (glGetError() == GL_INVALID_ENUM)
		majorVersion = 2;
	else
	{
		glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
	}
	const char *version = (const char *)glGetString(GL_VERSION);
	_isGLES = !strncmp(version, "OpenGL ES", 9);
	INFO_LOG(RENDERER, "OpenGL version: %s", version);
}

void GLGraphicsContext::postInit()
{
	instance = this;
	findGLVersion();
#ifndef LIBRETRO
	gui_init();
	imguiDriver = std::unique_ptr<ImGuiDriver>(new OpenGLDriver());
#endif
}

void GLGraphicsContext::preTerm()
{
#ifndef LIBRETRO
	imguiDriver.reset();
	gui_term();
#endif
	instance = nullptr;
}

std::string GLGraphicsContext::getDriverName() {
	return (const char *)glGetString(GL_RENDERER);
}

std::string GLGraphicsContext::getDriverVersion() {
	return (const char *)glGetString(GL_VERSION);
}
