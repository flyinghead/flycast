/*
	Copyright 2024 flyinghead

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

class GlQuadDrawer
{
public:
	GlQuadDrawer();
	~GlQuadDrawer();

	// coords is an optional array of 20 floats (4 vertices with x,y,z,u,v each)
	void draw(GLuint texId, bool rotate = false, bool swapY = false, const float *coords = nullptr, const float *color = nullptr);

private:
	class VertexArray final : public GlVertexArray
	{
	protected:
		void defineVtxAttribs() override
		{
			glEnableVertexAttribArray(VERTEX_POS_ARRAY);
			glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)0);
			glEnableVertexAttribArray(VERTEX_UV_ARRAY);
			glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, (void*)(sizeof(float) * 3));
			glDisableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
			glDisableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
			glDisableVertexAttribArray(VERTEX_COL_BASE1_ARRAY);
			glDisableVertexAttribArray(VERTEX_COL_OFFS1_ARRAY);
			glDisableVertexAttribArray(VERTEX_UV1_ARRAY);
		}
	};

	GLuint shader;
	GLint tintUniform;
	GLuint rot90shader;
	GLint rot90TintUniform;
	VertexArray quadVertexArray;
	VertexArray quadVertexArraySwapY;
	std::unique_ptr<GlBuffer> quadBuffer;
	std::unique_ptr<GlBuffer> quadBufferSwapY;
	std::unique_ptr<GlBuffer> quadIndexBuffer;
	std::unique_ptr<GlBuffer> quadBufferCustom;
	VertexArray quadVertexArrayCustom;
};
