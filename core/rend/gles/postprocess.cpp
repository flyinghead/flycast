/*
	PowerVR2 buffer shader
    Authors: leilei

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
#include "postprocess.h"

#ifdef LIBRETRO
#include <array>

PostProcessor postProcessor;

static const char* VertexShaderSource = R"(
in vec3 in_pos;

void main()
{
	gl_Position = vec4(in_pos, 1.0);
}
)";

static const char* FragmentShaderSource = R"(
#define LUMBOOST 0

#if TARGET_GL == GLES2 || TARGET_GL == GLES3
precision highp float;
#endif

uniform int FrameCount;
uniform sampler2D Texture;
uniform vec2 videoShift;

// compatibility #defines
#define Source Texture
#define TextureSize textureSize(Texture, 0)
#define vTexCoord ((gl_FragCoord.xy + videoShift) / vec2(textureSize(Texture, 0)))

float dithertable[16] = float[](
	16.,4.,13.,1.,   
	8.,12.,5.,9.,
	14.,2.,15.,3.,
	6.,10.,7.,11.		
);

//#pragma parameter INTERLACED "PVR - Interlace smoothing" 1.00 0.00 1.00 1.0
//#pragma parameter VGASIGNAL "PVR - VGA signal loss" 0.00 0.00 1.00 1.0
//#pragma parameter LUMBOOST "PVR - Luminance gain" 0.35 0.00 1.00 0.01

#define LUM_R (76.0/255.0)
#define LUM_G (150.0/255.0)
#define LUM_B (28.0/255.0)

void main()
{
	vec2 texcoord = vTexCoord;
	texcoord.y = 1. - texcoord.y;
	vec2 texcoord2 = vTexCoord;
	texcoord2.y = 1. - texcoord2.y;
	texcoord2.x *= float(TextureSize.x);
	texcoord2.y *= float(TextureSize.y);
	vec4 color = texture(Source, texcoord);
	float fc = mod(float(FrameCount), 2.0);

#if INTERLACED == 1
	// Blend vertically for composite mode
	int taps = int(3);
	float tap = (2.666f/float(taps)) / float(min(TextureSize.y, 720));
	vec2 texcoord4  = vTexCoord;
	texcoord4.y = 1. - texcoord4.y;
	texcoord4.y -= tap * 2.f;
	int bl;
	vec4 ble = vec4(0.0);

	for (bl=0;bl<taps;bl++)
	{
		texcoord4.y += tap;
		ble.rgb += (texture(Source, texcoord4).rgb / float(taps+1));
	}
	color.rgb = (color.rgb / float(taps+1)) + ( ble.rgb );
#endif

#if LUMBOOST == 1
	// Some games use a luminance boost (JSR etc)
	color.rgb += (((color.r * LUM_R) + (color.g * LUM_G) + (color.b * LUM_B)) * LUMBOOST);
#endif

#if DITHERING == 1
	// Dither
	int ditdex = 	int(mod(texcoord2.x, 4.0)) * 4 + int(mod(texcoord2.y, 4.0)); 	
	int yeh = 0;
	float ohyes;
	vec4 how;

	for (yeh=ditdex; yeh<(ditdex+16); yeh++) 	ohyes =  ((((dithertable[yeh-15]) - 1.f) * 0.1));
	color.rb -= (ohyes / 128.);
	color.g -= (ohyes / 128.);
	{
		vec4 reduct;		// 16 bits per pixel (5-6-5)
		reduct.r = 32.;
		reduct.g = 64.;	
		reduct.b = 32.;
		how = color;
  		how = pow(how, vec4(1.0, 1.0, 1.0, 1.0));  	how *= reduct;  	how = floor(how);	how = how / reduct;  	how = pow(how, vec4(1.0, 1.0, 1.0, 1.0));
	}

	color.rb = how.rb;
	color.g = how.g;
#endif

#if VGASIGNAL == 1
	// There's a bit of a precision drop involved in the RGB565ening for VGA
	// I'm not sure why that is. it's exhibited on PVR1 and PVR3 hardware too
	if (mod(color.r*32, 2.0)>0) color.r -= 0.023;
	if (mod(color.g*64, 2.0)>0) color.g -= 0.01;
	if (mod(color.b*32, 2.0)>0) color.b -= 0.023;
#endif

	// RGB565 clamp

	color.rb = floor(color.rb * 32. + 0.5)/32.;
	color.g = floor(color.g * 64. + 0.5)/64.;

#if VGASIGNAL == 1
	// VGA Signal Loss, which probably is very wrong but i tried my best
	int taps = 32;
	float tap = 12.0/taps;
	vec2 texcoord4  = vTexCoord;
	texcoord4.y = 1. - texcoord4.y;
	texcoord4.x = texcoord4.x + (2.0/640.0);
	texcoord4.y = texcoord4.y;
	vec4 blur1 = texture(Source, texcoord4);
	int bl;
	vec4 ble = vec4(0.0);
	for (bl=0;bl<taps;bl++)
	{
		float e = 1;
		if (bl>=3)
			e=0.35;
		texcoord4.x -= (tap  / 640);
		ble.rgb += (texture(Source, texcoord4).rgb * e) / (taps/(bl+1));
	}

	color.rgb += ble.rgb * 0.015;

	//color.rb += (4.0/255.0);
	color.g += (9.0/255.0);
#endif

	FragColor = vec4(color);
} 
)";

class PostProcessShader
{
public:
	static void select(bool dither, bool interlaced, bool vga)
	{
		u32 key = ((int)dither << 2) | ((int)interlaced << 1) | (int)vga;
		if (shaders[key].program == 0)
			shaders[key].compile(dither, interlaced, vga);
		shaders[key].select();
	}
	static void term()
	{
		for (auto& shader : shaders)
		{
			if (shader.program != 0)
			{
				glDeleteProgram(shader.program);
				shader.program = 0;
			}
		}
	}

private:
	void compile(bool dither, bool interlaced, bool vga)
	{
		OpenGlSource vertexShader;
		vertexShader.addSource(VertexCompatShader)
				.addSource(VertexShaderSource);

		OpenGlSource fragmentShader;
		fragmentShader.addConstant("DITHERING", dither)
				.addConstant("INTERLACED", interlaced)
				.addConstant("VGASIGNAL", vga)
				.addSource(PixelCompatShader)
				.addSource(FragmentShaderSource);

		program = gl_CompileAndLink(vertexShader.generate().c_str(), fragmentShader.generate().c_str());

		//setup texture 0 as the input for the shader
		GLint gu = glGetUniformLocation(program, "Texture");
		if (gu != -1)
			glUniform1i(gu, 0);

		frameCountUniform = glGetUniformLocation(program, "FrameCount");
		videoShiftUniform = glGetUniformLocation(program, "videoShift");
	}

	void select()
	{
		glcache.UseProgram(program);
		glUniform1f(frameCountUniform, FrameCount);
		float shift[] = { -gl.ofbo.shiftX, gl.ofbo.shiftY };
		glUniform2fv(videoShiftUniform, 1, shift);
	}

	GLuint program = 0;
	GLint frameCountUniform = -1;
	GLint videoShiftUniform = -1;
	static std::array<PostProcessShader, 8> shaders;
};

std::array<PostProcessShader, 8> PostProcessShader::shaders;

void PostProcessor::VertexArray::defineVtxAttribs()
{
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);
	glDisableVertexAttribArray(3);
}

void PostProcessor::init(int width, int height)
{
	framebuffer = std::make_unique<GlFramebuffer>(width, height, true, true);

	float vertices[] = {
			-1,  1, 1,
			-1, -1, 1,
			 1,  1, 1,
			 1, -1, 1,
	};
	vertexBuffer = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
	vertexBuffer->update(vertices, sizeof(vertices));
	glCheck();
}

void PostProcessor::term()
{
	framebuffer.reset();
	vertexBuffer.reset();
	vertexArray.term();
	PostProcessShader::term();
	glCheck();
}


GLuint PostProcessor::getFramebuffer(int width, int height)
{
	if (framebuffer != nullptr
			&& (width != framebuffer->getWidth() || height != framebuffer->getHeight()))
		term();

	if (framebuffer == nullptr)
		init(width, height);

	return framebuffer->getFramebuffer();
}

void PostProcessor::render(GLuint output_fbo)
{
	glcache.Disable(GL_SCISSOR_TEST);
	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);
	glcache.Disable(GL_CULL_FACE);
	glcache.Disable(GL_BLEND);

	if (!config::PowerVR2Filter)
	{
		// Just handle shifting and Y flipping
		if (gl.gl_major < 3)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
			glViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
			glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			static float vertices[20] = {
				-1.f, -1.f, 1.f, 0.f, 1.f,
				-1.f,  1.f, 1.f, 0.f, 0.f,
				 1.f, -1.f, 1.f, 1.f, 1.f,
				 1.f,  1.f, 1.f, 1.f, 0.f,
			};
			vertices[0] = vertices[5] = -1.f + gl.ofbo.shiftX * 2.f / framebuffer->getWidth();
			vertices[10] = vertices[15] = vertices[0] + 2;
			vertices[1] = vertices[11] = -1.f - gl.ofbo.shiftY * 2.f / framebuffer->getHeight();
			vertices[6] = vertices[16] = vertices[1] + 2;
			glcache.Disable(GL_BLEND);
			drawQuad(framebuffer->getTexture(), false, false, vertices);
		}
		else
		{
#ifndef GLES2
			framebuffer->bind(GL_READ_FRAMEBUFFER);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output_fbo);
			glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
			glBlitFramebuffer(-gl.ofbo.shiftX, -gl.ofbo.shiftY, framebuffer->getWidth() - gl.ofbo.shiftX, framebuffer->getHeight() - gl.ofbo.shiftY,
					0, framebuffer->getHeight(), framebuffer->getWidth(), 0,
					GL_COLOR_BUFFER_BIT, GL_NEAREST);
	    	glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
#endif
		}
		return;
	}

	if (_pvrrc == nullptr)
		// Framebuffer render: no dithering
		PostProcessShader::select(false,
				SPG_CONTROL.interlace,
				FB_R_CTRL.vclk_div == 1 && SPG_CONTROL.interlace == 0);
	else
		PostProcessShader::select(pvrrc.fb_W_CTRL.fb_dither == 1 && pvrrc.fb_W_CTRL.fb_packmode <= 3 && !config::EmulateFramebuffer,
				SPG_CONTROL.interlace,
				FB_R_CTRL.vclk_div == 1 && SPG_CONTROL.interlace == 0);
	vertexArray.bind(vertexBuffer.get());

	glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
	glActiveTexture(GL_TEXTURE0);
	glcache.BindTexture(GL_TEXTURE_2D, framebuffer->getTexture());

	glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	GlVertexArray::unbind();
}
#endif // LIBRETRO
