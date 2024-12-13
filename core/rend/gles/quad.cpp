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
#include "quad.h"

#ifdef __vita__
static const char* VertexShader = R"(
void main(
	float3 in_pos,
	float2 in_uv,
	float2 out vtx_uv : TEXCOORD0,
	float4 out gl_Position : POSITION
) {
	vtx_uv = in_uv;
#if ROTATE == 1
	gl_Position = float4(-in_pos.y, in_pos.x, in_pos.z, 1.0);
#else
	gl_Position = float4(in_pos, 1.0);
#endif
}
)";

static const char* FragmentShader = R"(
uniform sampler2D tex;

float4 main(float2 vtx_uv : TEXCOORD0)
{
	return tex2D(tex, vtx_uv);
}
)";
#else
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
uniform mediump vec4 tint;

void main()
{
	gl_FragColor = tint * texture(tex, vtx_uv);
}
)";
#endif

GlQuadDrawer::GlQuadDrawer()
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
	tintUniform = glGetUniformLocation(shader, "tint");

	vertexShader.setConstant("ROTATE", 1);
	rot90shader = gl_CompileAndLink(vertexShader.generate().c_str(), fragmentGlsl.c_str());
	tex = glGetUniformLocation(rot90shader, "tex");
	glUniform1i(tex, 0);	// texture 0
	rot90TintUniform = glGetUniformLocation(rot90shader, "tint");

	quadIndexBuffer = std::make_unique<GlBuffer>(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
	static const GLushort indices[] = { 0, 1, 2, 1, 3 };
	quadIndexBuffer->update(indices, sizeof(indices));

	quadBuffer = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
	{
		float vertices[4][5] = {
			{ -1.f,  1.f, 1.f, 0.f, 1.f },
			{ -1.f, -1.f, 1.f, 0.f, 0.f },
			{  1.f,  1.f, 1.f, 1.f, 1.f },
			{  1.f, -1.f, 1.f, 1.f, 0.f },
		};
		quadBuffer->update(vertices, sizeof(vertices));
	}
	quadBufferSwapY = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
	{
		float vertices[4][5] = {
			{ -1.f,  1.f, 1.f, 0.f, 0.f },
			{ -1.f, -1.f, 1.f, 0.f, 1.f },
			{  1.f,  1.f, 1.f, 1.f, 0.f },
			{  1.f, -1.f, 1.f, 1.f, 1.f },
		};
		quadBufferSwapY->update(vertices, sizeof(vertices));
	}
	quadBufferCustom = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER);
	glCheck();
}

GlQuadDrawer::~GlQuadDrawer()
{
	quadIndexBuffer.reset();
	quadBuffer.reset();
	quadBufferSwapY.reset();
	quadBufferCustom.reset();

	quadVertexArray.term();
	quadVertexArraySwapY.term();
	quadVertexArrayCustom.term();

	glcache.DeleteProgram(shader);
	shader = 0;
	glcache.DeleteProgram(rot90shader);
	rot90shader = 0;
}

void GlQuadDrawer::draw(GLuint texId, bool rotate, bool swapY, const float *coords, const float *color)
{
	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);
	glcache.Disable(GL_CULL_FACE);

	glcache.UseProgram(rotate ? rot90shader : shader);

	glActiveTexture(GL_TEXTURE0);
	glcache.BindTexture(GL_TEXTURE_2D, texId);

	if (color == nullptr) {
		static constexpr float white[4] { 1.f, 1.f, 1.f, 1.f };
		color = white;
	}
	glUniform4fv(rotate ? rot90TintUniform : tintUniform, 1, color);

	if (coords == nullptr)
	{
		if (swapY)
			quadVertexArraySwapY.bind(quadBufferSwapY.get(), quadIndexBuffer.get());
		else
			quadVertexArray.bind(quadBuffer.get(), quadIndexBuffer.get());
	}
	else
	{
		quadBufferCustom->update(coords, sizeof(float) * 4 * 5);
		quadVertexArrayCustom.bind(quadBufferCustom.get(), quadIndexBuffer.get());
	}

	glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, (GLvoid *)0);
	GlVertexArray::unbind();
	glCheck();
}
