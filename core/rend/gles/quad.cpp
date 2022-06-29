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
#include "gles.h"

static const char* VertexShader = R"(
in highp vec3 in_pos;
in mediump vec2 in_uv;
out mediump vec2 vtx_uv;

void main()
{
	vtx_uv = in_uv;
#if ROTATE == 1
	gl_Position = vec4(-in_pos.y, in_pos.x, in_pos.z, 1.0);
#else
	gl_Position = vec4(in_pos, 1.0);
#endif
}
)";

static const char* FragmentShader = R"(
in mediump vec2 vtx_uv;

uniform sampler2D tex;

void main()
{
	gl_FragColor = texture(tex, vtx_uv);
}
)";

static GLuint shader;
static GLuint rot90shader;
static GLuint quadVertexArray;
static GLuint quadBuffer;
static GLuint quadIndexBuffer;

static void setupVertexAttribs()
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

void initQuad()
{
	if (shader == 0)
	{
		OpenGlSource fragmentShader;
		fragmentShader.addSource(PixelCompatShader)
				.addSource(FragmentShader);
		OpenGlSource vertexShader;
		vertexShader.addConstant("ROTATE", 0)
				.addSource(VertexCompatShader)
				.addSource(VertexShader);

		const std::string fragmentGlsl = fragmentShader.generate();
		shader = gl_CompileAndLink(vertexShader.generate().c_str(), fragmentGlsl.c_str());
		GLint tex = glGetUniformLocation(shader, "tex");
		glUniform1i(tex, 0);	// texture 0

		vertexShader.setConstant("ROTATE", 1);
		rot90shader = gl_CompileAndLink(vertexShader.generate().c_str(), fragmentGlsl.c_str());
		tex = glGetUniformLocation(rot90shader, "tex");
		glUniform1i(tex, 0);	// texture 0
	}
#ifndef GLES2
	if (quadVertexArray == 0 && gl.gl_major >= 3)
		glGenVertexArrays(1, &quadVertexArray);
#endif
	if (quadIndexBuffer == 0)
	{
		glGenBuffers(1, &quadIndexBuffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuffer);
		static const GLushort indices[] = { 0, 1, 2, 1, 3 };
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}
	if (quadBuffer == 0)
	{
		glGenBuffers(1, &quadBuffer);
#ifndef GLES2
		if (gl.gl_major >= 3)
		{
			bindVertexArray(quadVertexArray);
			glBindBuffer(GL_ARRAY_BUFFER, quadBuffer);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuffer);
			setupVertexAttribs();
			bindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		}
#endif
	}
	glCheck();
}

void termQuad()
{
	if (quadBuffer != 0)
	{
		glDeleteBuffers(1, &quadBuffer);
		quadBuffer = 0;
	}
	if (quadIndexBuffer != 0)
	{
		glDeleteBuffers(1, &quadIndexBuffer);
		quadIndexBuffer = 0;
	}
	if (quadVertexArray != 0)
	{
		deleteVertexArray(quadVertexArray);
		quadVertexArray = 0;
	}
	if (shader != 0)
	{
		glcache.DeleteProgram(shader);
		shader = 0;
		glcache.DeleteProgram(rot90shader);
		rot90shader = 0;
	}
}

void drawQuad(GLuint texId, bool rotate, bool swapY)
{
	float vertices[4][5] = {
		{ -1.f,  1.f, 1.f, 0.f, (float)!swapY },
		{ -1.f, -1.f, 1.f, 0.f, (float)swapY },
		{  1.f,  1.f, 1.f, 1.f, (float)!swapY },
		{  1.f, -1.f, 1.f, 1.f, (float)swapY },
	};

	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);
	glcache.Disable(GL_CULL_FACE);
	glcache.Disable(GL_BLEND);

	glcache.UseProgram(rotate ? rot90shader : shader);

	glActiveTexture(GL_TEXTURE0);
	glcache.BindTexture(GL_TEXTURE_2D, texId);

	glBindBuffer(GL_ARRAY_BUFFER, quadBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadIndexBuffer);
	if (gl.gl_major < 3)
		setupVertexAttribs();
	else
		bindVertexArray(quadVertexArray);

	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW);
	glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, (GLvoid *)0);
	bindVertexArray(0);
	glCheck();
}
