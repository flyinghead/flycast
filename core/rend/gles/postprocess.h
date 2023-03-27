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
#include <memory>

class PostProcessor
{
public:
	void term();
	void render(GLuint output_fbo);
	GLuint getFramebuffer(int width, int height);

private:
	void init(int width, int height);

	class VertexArray final : public GlVertexArray
	{
	protected:
		void defineVtxAttribs() override;
	};

	std::unique_ptr<GlBuffer> vertexBuffer;
	VertexArray vertexArray;
	std::unique_ptr<GlFramebuffer> framebuffer;
	std::unique_ptr<GlBuffer> vertexBufferShifted;
	VertexArray vertexArrayShifted;
};

extern PostProcessor postProcessor;
