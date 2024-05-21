/*
	Copyright 2018 flyinghead

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
#include "gl4.h"
#include "rend/gles/glcache.h"

#include <memory>

static GLuint pixels_buffer;
static GLuint pixels_pointers;
static GLuint atomic_buffer;
static gl4PipelineShader g_abuffer_final_shader[2];
static gl4PipelineShader g_abuffer_clear_shader;
static gl4PipelineShader g_abuffer_tr_modvol_shaders[ModeCount];
static int maxLayers;
static int64_t pixelBufferSize;
static std::unique_ptr<GlBuffer> g_quadBuffer;
static std::unique_ptr<GlBuffer> g_quadIndexBuffer;

class AQuadVertexArray final : public GlVertexArray
{
protected:
	void defineVtxAttribs() override
	{
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);
	}
};
static AQuadVertexArray g_quadVertexArray;

static const char *final_shader_source = R"(
layout(binding = 0) uniform sampler2D tex;
uniform float shade_scale_factor;
#if DITHERING == 1
uniform vec4 ditherColorMax;
#endif

out vec4 FragColor;

uint pixel_list[MAX_PIXELS_PER_FRAGMENT];


int fillAndSortFragmentArray(ivec2 coords)
{
	uint idx = imageLoad(abufferPointerImg, coords).x;
	if (idx == EOL)
		return 0;
	int count = 1;
	pixel_list[0] = idx;
	idx = pixels[idx].next;
	for (; idx != EOL && count < MAX_PIXELS_PER_FRAGMENT; count++)
	{
		const Pixel p = pixels[idx];
		int j = count - 1;
		Pixel jp = pixels[pixel_list[j]];
		while (j >= 0
			   && (jp.depth > p.depth
				   || (jp.depth == p.depth && getPolyIndex(jp) > getPolyIndex(p))))
		{
			pixel_list[j + 1] = pixel_list[j];
			j--;
			if (j >= 0)
				jp = pixels[pixel_list[j]];
		}
		pixel_list[j + 1] = idx;
		idx = p.next;
	}

	return count;
}

// Blend fragments back-to-front
vec4 resolveAlphaBlend(ivec2 coords) {
	
	// Copy and sort fragments into a local array
	int num_frag = fillAndSortFragmentArray(coords);
	
	vec4 finalColor = texture(tex, gl_FragCoord.xy / textureSize(tex, 0));
	vec4 secondaryBuffer = vec4(0.0); // Secondary accumulation buffer
	
	for (int i = 0; i < num_frag; i++)
	{
		const Pixel pixel = pixels[pixel_list[i]];
		const PolyParam pp = tr_poly_params[getPolyNumber(pixel)];
		bool area1 = false;
		bool shadowed = false;
		if (isShadowed(pixel))
		{
			if (isTwoVolumes(pp))
				area1 = true;
			else
				shadowed = true;
		}
		vec4 srcColor;
		if (getSrcSelect(pp, area1))
			srcColor = secondaryBuffer;
		else
		{
			srcColor = unpackColors(pixel.color);
			if (shadowed)
				srcColor.rgb *= shade_scale_factor;
		}
		vec4 dstColor = getDstSelect(pp, area1) ? secondaryBuffer : finalColor;
		vec4 srcCoef;
		vec4 dstCoef;
		
		int srcBlend = getSrcBlendFunc(pp, area1);
		switch (srcBlend)
		{
			case ZERO:
				srcCoef = vec4(0.0);
				break;
			case ONE:
				srcCoef = vec4(1.0);
				break;
			case OTHER_COLOR:
				srcCoef = finalColor;
				break;
			case INVERSE_OTHER_COLOR:
				srcCoef = vec4(1.0) - dstColor;
				break;
			case SRC_ALPHA:
				srcCoef = vec4(srcColor.a);
				break;
			case INVERSE_SRC_ALPHA:
				srcCoef = vec4(1.0 - srcColor.a);
				break;
			case DST_ALPHA:
				srcCoef = vec4(dstColor.a);
				break;
			case INVERSE_DST_ALPHA:
				srcCoef = vec4(1.0 - dstColor.a);
				break;
		}
		int dstBlend = getDstBlendFunc(pp, area1);
		switch (dstBlend)
		{
			case ZERO:
				dstCoef = vec4(0.0);
				break;
			case ONE:
				dstCoef = vec4(1.0);
				break;
			case OTHER_COLOR:
				dstCoef = srcColor;
				break;
			case INVERSE_OTHER_COLOR:
				dstCoef = vec4(1.0) - srcColor;
				break;
			case SRC_ALPHA:
				dstCoef = vec4(srcColor.a);
				break;
			case INVERSE_SRC_ALPHA:
				dstCoef = vec4(1.0 - srcColor.a);
				break;
			case DST_ALPHA:
				dstCoef = vec4(dstColor.a);
				break;
			case INVERSE_DST_ALPHA:
				dstCoef = vec4(1.0 - dstColor.a);
				break;
		}
		const vec4 result = clamp(dstColor * dstCoef + srcColor * srcCoef, 0.0, 1.0);
		if (getDstSelect(pp, area1))
			secondaryBuffer = result;
		else
			finalColor = result;
	}
#if DITHERING == 1
	float ditherTable[16] = float[](
		 0.9375,  0.1875,  0.75,  0.,   
		 0.4375,  0.6875,  0.25,  0.5,
		 0.8125,  0.0625,  0.875, 0.125,
		 0.3125,  0.5625,  0.375, 0.625	
	);
	float r = ditherTable[int(mod(gl_FragCoord.y, 4.)) * 4 + int(mod(gl_FragCoord.x, 4.))];
	// 31 for 5-bit color, 63 for 6 bits, 15 for 4 bits
	finalColor += r / ditherColorMax;
	// avoid rounding
	finalColor = floor(finalColor * 255.) / 255.;
#endif
	
	return finalColor;
	
}

void main(void)
{
	ivec2 coords = ivec2(gl_FragCoord.xy);
	// Compute and output final color for the frame buffer
	// Visualize the number of layers in use
	//FragColor = vec4(float(fillAndSortFragmentArray(coords)) / MAX_PIXELS_PER_FRAGMENT * 4, 0, 0, 1);
	FragColor = resolveAlphaBlend(coords);

	// Reset pointers
	imageStore(abufferPointerImg, coords, uvec4(EOL));
}
)";

static const char *clear_shader_source = R"(
void main(void)
{
	ivec2 coords = ivec2(gl_FragCoord.xy);

	// Reset pointers
	imageStore(abufferPointerImg, coords, uvec4(EOL));

	// Discard fragment so nothing is written to the framebuffer
	discard;
}
)";

static const char *tr_modvol_shader_source = R"(
in vec3 vtx_uv;

// Must match ModifierVolumeMode enum values
#define MV_XOR		 0
#define MV_OR		 1
#define MV_INCLUSION 2
#define MV_EXCLUSION 3

void main(void)
{
#if MV_MODE == MV_XOR || MV_MODE == MV_OR
	setFragDepth(vtx_uv.z);
#endif
	ivec2 coords = ivec2(gl_FragCoord.xy);
	
	uint idx = imageLoad(abufferPointerImg, coords).x;
	int list_len = 0;
	while (idx != EOL && list_len < MAX_PIXELS_PER_FRAGMENT)
	{
		const Pixel pixel = pixels[idx];
		const PolyParam pp = tr_poly_params[getPolyNumber(pixel)];
		if (getShadowEnable(pp))
		{
#if MV_MODE == MV_XOR
			if (gl_FragDepth >= pixel.depth)
				atomicXor(pixels[idx].seq_num, SHADOW_STENCIL);
#elif MV_MODE == MV_OR
			if (gl_FragDepth >= pixel.depth)
				atomicOr(pixels[idx].seq_num, SHADOW_STENCIL);
#elif MV_MODE == MV_INCLUSION
			uint prev_val = atomicAnd(pixels[idx].seq_num, ~(SHADOW_STENCIL));
			if ((prev_val & (SHADOW_STENCIL|SHADOW_ACC)) == SHADOW_STENCIL)
				pixels[idx].seq_num = bitfieldInsert(pixel.seq_num, 1u, 31, 1);
#elif MV_MODE == MV_EXCLUSION
			uint prev_val = atomicAnd(pixels[idx].seq_num, ~(SHADOW_STENCIL|SHADOW_ACC));
			if ((prev_val & (SHADOW_STENCIL|SHADOW_ACC)) == SHADOW_ACC)
				pixels[idx].seq_num = bitfieldInsert(pixel.seq_num, 1u, 31, 1);
#endif
		}
		idx = pixel.next;
		list_len++;
	}
	
	discard;
}
)";

static const char* VertexShaderSource = R"(
in vec3 in_pos;

void main()
{
	gl_Position = vec4(in_pos, 1.0);
}
)";

static void abufferDrawQuad();

static void compileFinalAndModVolShaders()
{
	if (maxLayers != config::PerPixelLayers)
	{
		maxLayers = config::PerPixelLayers;
		for (auto& shader : g_abuffer_final_shader) {
			glcache.DeleteProgram(shader.program);
			shader.program = 0;
		}
		for (auto& shader : g_abuffer_tr_modvol_shaders) {
			glcache.DeleteProgram(shader.program);
			shader.program = 0;
		}
	}
	if (g_abuffer_final_shader[0].program == 0)
	{
		OpenGl4Source vertexShader;
		vertexShader.addSource(VertexShaderSource);
		for (size_t i = 0; i < std::size(g_abuffer_final_shader); i++)
		{
			OpenGl4Source finalShader;
			finalShader.addConstant("MAX_PIXELS_PER_FRAGMENT", config::PerPixelLayers)
					.addConstant("DITHERING", i)
					.addSource(ShaderHeader)
					.addSource(final_shader_source);
			gl4CompilePipelineShader(&g_abuffer_final_shader[i], finalShader.generate().c_str(), vertexShader.generate().c_str());
		}
	}
	if (g_abuffer_tr_modvol_shaders[0].program == 0)
	{
		OpenGl4Source modVolShader;
		modVolShader.addConstant("MAX_PIXELS_PER_FRAGMENT", config::PerPixelLayers)
			.addConstant("DIV_POS_Z", config::NativeDepthInterpolation)
			.addSource(ShaderHeader)
			.addSource(tr_modvol_shader_source);
		for (int mode = 0; mode < ModeCount; mode++)
		{
			modVolShader.setConstant("MV_MODE", mode);
			g_abuffer_tr_modvol_shaders[mode].pp_Gouraud = false;
			gl4CompilePipelineShader(&g_abuffer_tr_modvol_shaders[mode], modVolShader.generate().c_str(), nullptr);
		}
	}
}

static void makePixelBuffer()
{
	if (pixels_buffer == 0 || pixelBufferSize != config::PixelBufferSize)
	{
		// get the max buffer size
		GLint64 maxSize;
		glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSize);
		pixelBufferSize = config::PixelBufferSize;
		GLsizeiptr pixel_buffer_size = std::min<int64_t>(pixelBufferSize, maxSize);

		// Create the buffer
		if (pixels_buffer == 0)
			glGenBuffers(1, &pixels_buffer);
		// Bind it
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, pixels_buffer);
		// Declare storage
		glBufferData(GL_SHADER_STORAGE_BUFFER, pixel_buffer_size, NULL, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pixels_buffer);
		glCheck();
	}
}

void initABuffer()
{
	if (max_image_width > 0 && max_image_height > 0)
	{
		if (pixels_pointers == 0)
			pixels_pointers = glcache.GenTexture();
		glActiveTexture(GL_TEXTURE4);
		glcache.BindTexture(GL_TEXTURE_2D, pixels_pointers);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, max_image_width, max_image_height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
		glBindImageTexture(4, pixels_pointers, 0, false, 0,  GL_READ_WRITE, GL_R32UI);
		glCheck();
	}

	makePixelBuffer();

	if (atomic_buffer == 0 )
	{
		// Create the buffer
		glGenBuffers(1, &atomic_buffer);
		// Bind it
		glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, atomic_buffer);
		// Declare storage
		glBufferData(GL_ATOMIC_COUNTER_BUFFER, 4, NULL, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, atomic_buffer);
		GLint zero = 0;
		glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLint), &zero);
		glCheck();
	}

	compileFinalAndModVolShaders();
	if (g_abuffer_clear_shader.program == 0)
	{
		OpenGl4Source vertexShader;
		vertexShader.addSource(VertexShaderSource);
		OpenGl4Source clearShader;
		clearShader.addSource(ShaderHeader)
				.addSource(clear_shader_source);
		gl4CompilePipelineShader(&g_abuffer_clear_shader, clearShader.generate().c_str(), vertexShader.generate().c_str());
	}
	if (g_quadBuffer == nullptr)
	{
		g_quadBuffer = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
		static const float vertices[] = {
				-1,  1, 1,
				-1, -1, 1,
				 1,  1, 1,
				 1, -1, 1,
		};
		g_quadBuffer->update(vertices, sizeof(vertices));
	}
	if (g_quadIndexBuffer == nullptr)
	{
		g_quadIndexBuffer = std::make_unique<GlBuffer>(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
		static const GLushort indices[] = { 0, 1, 2, 1, 3 };
		g_quadIndexBuffer->update(indices, sizeof(indices));
	}

	g_quadVertexArray.bind(g_quadBuffer.get(), g_quadIndexBuffer.get());
	g_quadVertexArray.unbind();

	if (pixels_pointers != 0)
	{
		// Clear A-buffer pointers
		glcache.UseProgram(g_abuffer_clear_shader.program);
		gl4ShaderUniforms.Set(&g_abuffer_clear_shader);
		glcache.Disable(GL_DEPTH_TEST);
		glcache.Disable(GL_CULL_FACE);
		glcache.Disable(GL_SCISSOR_TEST);

		abufferDrawQuad();
		glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	}

	glCheck();
}

void termABuffer()
{
	if (pixels_pointers != 0)
	{
		glcache.DeleteTextures(1, &pixels_pointers);
		pixels_pointers = 0;
	}
	if (pixels_buffer != 0)
	{
		glDeleteBuffers(1, &pixels_buffer);
		pixels_buffer = 0;
	}
	if (atomic_buffer != 0)
	{
		glDeleteBuffers(1, &atomic_buffer);
		atomic_buffer = 0;
	}
	g_quadVertexArray.term();
	g_quadBuffer.reset();
	g_quadIndexBuffer.reset();
	for (auto& shader : g_abuffer_final_shader) {
		glcache.DeleteProgram(shader.program);
		shader.program = 0;
	}
	glcache.DeleteProgram(g_abuffer_clear_shader.program);
	g_abuffer_clear_shader.program = 0;
	for (auto& shader : g_abuffer_tr_modvol_shaders) {
		glcache.DeleteProgram(shader.program);
		shader.program = 0;
	}
	maxLayers = 0;
}

void reshapeABuffer(int w, int h)
{
	if (pixels_pointers != 0)
	{
		glcache.DeleteTextures(1, &pixels_pointers);
		pixels_pointers = 0;
	}
	initABuffer();
}

static void abufferDrawQuad()
{
	g_quadVertexArray.bind(g_quadBuffer.get(), g_quadIndexBuffer.get());
	glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, (GLvoid *)0);
	g_quadVertexArray.unbind();
	glCheck();
}

void DrawTranslucentModVols(int first, int count, bool useOpaqueGeom)
{
	if (count == 0 || pvrrc.modtrig.empty())
		return;
	compileFinalAndModVolShaders();
	gl4SetupModvolVBO();

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glcache.BindTexture(GL_TEXTURE_2D, 0);

	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);

	glCheck();

	ModifierVolumeParam* params = useOpaqueGeom ? &pvrrc.global_param_mvo[first] : &pvrrc.global_param_mvo_tr[first];

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

	int mod_base = -1;

	for (int cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		verify(param.first >= 0 && param.first + param.count <= (u32)pvrrc.modtrig.size());

		if (mod_base == -1)
			mod_base = param.first;

		gl4PipelineShader *shader;
		if (!param.isp.VolumeLast && mv_mode > 0)
			shader = &g_abuffer_tr_modvol_shaders[Or];	// OR'ing (open volume or quad)
		else
			shader = &g_abuffer_tr_modvol_shaders[Xor];	// XOR'ing (closed volume)
		glcache.UseProgram(shader->program);
		gl4ShaderUniforms.Set(shader);

		SetCull(param.isp.CullMode); glCheck();

		glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

		glDrawArrays(GL_TRIANGLES, param.first * 3, param.count * 3); glCheck();

		if (mv_mode == 1 || mv_mode == 2)
		{
			//Sum the area
			shader = &g_abuffer_tr_modvol_shaders[mv_mode == 1 ? Inclusion : Exclusion];
			glcache.UseProgram(shader->program);
			gl4ShaderUniforms.Set(shader);

			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
			glDrawArrays(GL_TRIANGLES, mod_base * 3, (param.first + param.count - mod_base) * 3); glCheck();
			mod_base = -1;
		}
	}
	gl4SetupMainVBO();
}

void checkOverflowAndReset()
{
	// Using atomic counter
	GLuint max_pixel_index = 0;
//	glGetBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &max_pixel_index);
////	printf("ABUFFER %d pixels used\n", max_pixel_index);
//	if ((max_pixel_index + 1) * 32 - 1 >= pixel_buffer_size)
//	{
//		GLint64 size;
//		glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &size);
//		if (pixel_buffer_size == size)
//			printf("A-buffer overflow: %d pixels. Buffer size already maxed out\n", max_pixel_index);
//		else
//		{
//			pixel_buffer_size = (GLuint)min(2 * (GLint64)pixel_buffer_size, size);
//
//			printf("A-buffer overflow: %d pixels. Resizing buffer to %d MB\n", max_pixel_index, pixel_buffer_size / 1024 / 1024);
//
//			glBindBuffer(GL_SHADER_STORAGE_BUFFER, pixels_buffer);
//			glBufferData(GL_SHADER_STORAGE_BUFFER, pixel_buffer_size, NULL, GL_DYNAMIC_COPY);
//			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pixels_buffer);
//			glCheck();
//		}
//	}
	// Reset counter
	max_pixel_index = 0;
 	glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0 , sizeof(GLuint), &max_pixel_index);
}

void renderABuffer(bool lastPass)
{
	// Render to output FBO
	compileFinalAndModVolShaders();
	if (lastPass && config::EmulateFramebuffer && pvrrc.fb_W_CTRL.fb_dither && pvrrc.fb_W_CTRL.fb_packmode <= 3)
	{
		glcache.UseProgram(g_abuffer_final_shader[1].program);
		switch (pvrrc.fb_W_CTRL.fb_packmode)
		{
		case 0: // 0555 KRGB 16 bit
		case 3: // 1555 ARGB 16 bit
			gl4ShaderUniforms.ditherColorMax[0] = gl4ShaderUniforms.ditherColorMax[1] = gl4ShaderUniforms.ditherColorMax[2] = 31.f;
			gl4ShaderUniforms.ditherColorMax[3] = 255.f;
			break;
		case 1: // 565 RGB 16 bit
			gl4ShaderUniforms.ditherColorMax[0] = gl4ShaderUniforms.ditherColorMax[2] = 31.f;
			gl4ShaderUniforms.ditherColorMax[1] = 63.f;
			gl4ShaderUniforms.ditherColorMax[3] = 255.f;
			break;
		case 2: // 4444 ARGB 16 bit
			gl4ShaderUniforms.ditherColorMax[0] = gl4ShaderUniforms.ditherColorMax[1]
				= gl4ShaderUniforms.ditherColorMax[2] = gl4ShaderUniforms.ditherColorMax[3] = 15.f;
			break;
		default:
			break;
		}
		gl4ShaderUniforms.Set(&g_abuffer_final_shader[1]);
	}
	else {
		glcache.UseProgram(g_abuffer_final_shader[0].program);
		gl4ShaderUniforms.Set(&g_abuffer_final_shader[0]);
	}

	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_CULL_FACE);
	glcache.Disable(GL_SCISSOR_TEST);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
	glcache.Disable(GL_BLEND);

	abufferDrawQuad();

	glActiveTexture(GL_TEXTURE0);

	makePixelBuffer();
	glCheck();
}
