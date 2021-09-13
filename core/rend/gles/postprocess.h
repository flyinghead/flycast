/*
	Copyright 2020 flyinghead

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
#include "gles.h"

class PostProcessor
{
public:
	void term();
	void render(GLuint output_fbo);
	GLuint getFramebuffer(int width, int height);

private:
	void init();

	GLuint texture = 0;
	GLuint framebuffer = 0;
	GLuint depthBuffer = 0;
	GLuint vertexBuffer = 0;
	GLuint vertexArray = 0;
	float width = 0;
	float height = 0;
};

extern PostProcessor postProcessor;
