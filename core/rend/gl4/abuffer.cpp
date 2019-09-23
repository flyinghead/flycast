/*
 * abuffer.cpp
 *
 *  Created on: May 26, 2018
 *      Author: raph
 */
#include <cmath>
#include "gl4.h"
#include "rend/gles/glcache.h"

GLuint pixels_buffer;
GLuint pixels_pointers;
GLuint atomic_buffer;
gl4PipelineShader g_abuffer_final_shader;
gl4PipelineShader g_abuffer_final_nosort_shader;
gl4PipelineShader g_abuffer_clear_shader;
gl4PipelineShader g_abuffer_tr_modvol_shaders[ModeCount];
static GLuint g_quadBuffer = 0;
static GLuint g_quadVertexArray = 0;

GLuint pixel_buffer_size = 512 * 1024 * 1024;	// Initial size 512 MB

#define MAX_PIXELS_PER_FRAGMENT "32"

static const char *final_shader_source = SHADER_HEADER "\
#define DEPTH_SORTED %d \n\
#define MAX_PIXELS_PER_FRAGMENT " MAX_PIXELS_PER_FRAGMENT " \n\
 \n\
layout(binding = 0) uniform sampler2D tex; \n\
uniform highp float shade_scale_factor; \n\
 \n\
out vec4 FragColor; \n\
 \n\
uint pixel_list[MAX_PIXELS_PER_FRAGMENT]; \n\
 \n\
 \n\
int fillAndSortFragmentArray(ivec2 coords) \n\
{ \n\
	// Load fragments into a local memory array for sorting \n\
	uint idx = imageLoad(abufferPointerImg, coords).x; \n\
	int count = 0; \n\
	for (; idx != EOL && count < MAX_PIXELS_PER_FRAGMENT; count++) \n\
	{ \n\
		const Pixel p = pixels[idx]; \n\
		int j = count - 1; \n\
		Pixel jp = pixels[pixel_list[j]]; \n\
#if DEPTH_SORTED == 1 \n\
		while (j >= 0 \n\
			   && (jp.depth > p.depth \n\
				   || (jp.depth == p.depth && getPolyNumber(jp) > getPolyNumber(p)))) \n\
#else \n\
		while (j >= 0 && getPolyNumber(jp) > getPolyNumber(p)) \n\
#endif \n\
		{ \n\
			pixel_list[j + 1] = pixel_list[j]; \n\
			j--; \n\
			jp = pixels[pixel_list[j]]; \n\
		} \n\
		pixel_list[j + 1] = idx; \n\
		idx = p.next; \n\
	} \n\
	return count; \n\
} \n\
 \n\
// Blend fragments back-to-front \n\
vec4 resolveAlphaBlend(ivec2 coords) { \n\
	 \n\
	// Copy and sort fragments into a local array \n\
	int num_frag = fillAndSortFragmentArray(coords); \n\
	 \n\
	vec4 finalColor = texture(tex, gl_FragCoord.xy / textureSize(tex, 0)); \n\
	vec4 secondaryBuffer = vec4(0.0); // Secondary accumulation buffer \n\
	float depth = 1.0; \n\
	 \n\
	bool do_depth_test = false; \n\
	for (int i = 0; i < num_frag; i++) \n\
	{ \n\
		const Pixel pixel = pixels[pixel_list[i]]; \n\
		const PolyParam pp = tr_poly_params[getPolyNumber(pixel)]; \n\
#if DEPTH_SORTED != 1 \n\
		const float frag_depth = pixel.depth; \n\
		if (do_depth_test) \n\
		{ \n\
			switch (getDepthFunc(pp)) \n\
			{ \n\
			case 0:		// Never \n\
				continue; \n\
			case 1:		// Less \n\
				if (frag_depth >= depth) \n\
					continue; \n\
				break; \n\
			case 2:		// Equal \n\
				if (frag_depth != depth) \n\
					continue; \n\
				break; \n\
			case 3:		// Less or equal \n\
				if (frag_depth > depth) \n\
					continue; \n\
				break; \n\
			case 4:		// Greater \n\
				if (frag_depth <= depth) \n\
					continue; \n\
				break; \n\
			case 5:		// Not equal \n\
				if (frag_depth == depth) \n\
					continue; \n\
				break; \n\
			case 6:		// Greater or equal \n\
				if (frag_depth < depth) \n\
					continue; \n\
				break; \n\
			case 7:		// Always \n\
				break; \n\
			} \n\
		} \n\
		 \n\
		if (getDepthMask(pp)) \n\
		{ \n\
			depth = frag_depth; \n\
			do_depth_test = true; \n\
		} \n\
#endif \n\
		bool area1 = false; \n\
		bool shadowed = false; \n\
		if (isShadowed(pixel)) \n\
		{ \n\
			if (isTwoVolumes(pp)) \n\
				area1 = true; \n\
			else \n\
				shadowed = true; \n\
		} \n\
		vec4 srcColor; \n\
		if (getSrcSelect(pp, area1)) \n\
			srcColor = secondaryBuffer; \n\
		else \n\
		{ \n\
			srcColor = pixel.color; \n\
			if (shadowed) \n\
				srcColor.rgb *= shade_scale_factor; \n\
		} \n\
		vec4 dstColor = getDstSelect(pp, area1) ? secondaryBuffer : finalColor; \n\
		vec4 srcCoef; \n\
		vec4 dstCoef; \n\
		 \n\
		int srcBlend = getSrcBlendFunc(pp, area1); \n\
		switch (srcBlend) \n\
		{ \n\
			case ZERO: \n\
				srcCoef = vec4(0.0); \n\
				break; \n\
			case ONE: \n\
				srcCoef = vec4(1.0); \n\
				break; \n\
			case OTHER_COLOR: \n\
				srcCoef = finalColor; \n\
				break; \n\
			case INVERSE_OTHER_COLOR: \n\
				srcCoef = vec4(1.0) - dstColor; \n\
				break; \n\
			case SRC_ALPHA: \n\
				srcCoef = vec4(srcColor.a); \n\
				break; \n\
			case INVERSE_SRC_ALPHA: \n\
				srcCoef = vec4(1.0 - srcColor.a); \n\
				break; \n\
			case DST_ALPHA: \n\
				srcCoef = vec4(dstColor.a); \n\
				break; \n\
			case INVERSE_DST_ALPHA: \n\
				srcCoef = vec4(1.0 - dstColor.a); \n\
				break; \n\
		} \n\
		int dstBlend = getDstBlendFunc(pp, area1); \n\
		switch (dstBlend) \n\
		{ \n\
			case ZERO: \n\
				dstCoef = vec4(0.0); \n\
				break; \n\
			case ONE: \n\
				dstCoef = vec4(1.0); \n\
				break; \n\
			case OTHER_COLOR: \n\
				dstCoef = srcColor; \n\
				break; \n\
			case INVERSE_OTHER_COLOR: \n\
				dstCoef = vec4(1.0) - srcColor; \n\
				break; \n\
			case SRC_ALPHA: \n\
				dstCoef = vec4(srcColor.a); \n\
				break; \n\
			case INVERSE_SRC_ALPHA: \n\
				dstCoef = vec4(1.0 - srcColor.a); \n\
				break; \n\
			case DST_ALPHA: \n\
				dstCoef = vec4(dstColor.a); \n\
				break; \n\
			case INVERSE_DST_ALPHA: \n\
				dstCoef = vec4(1.0 - dstColor.a); \n\
				break; \n\
		} \n\
		const vec4 result = clamp(dstColor * dstCoef + srcColor * srcCoef, 0.0, 1.0); \n\
		if (getDstSelect(pp, area1)) \n\
			secondaryBuffer = result; \n\
		else \n\
			finalColor = result; \n\
	} \n\
	 \n\
	return finalColor; \n\
	 \n\
} \n\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
	// Compute and output final color for the frame buffer \n\
	// Visualize the number of layers in use \n\
	//FragColor = vec4(float(fillAndSortFragmentArray(coords)) / MAX_PIXELS_PER_FRAGMENT * 4, 0, 0, 1); \n\
	FragColor = resolveAlphaBlend(coords); \n\
} \n\
";

static const char *clear_shader_source = SHADER_HEADER "\
 \n\
void main(void) \n\
{ \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
 \n\
	// Reset pointers \n\
	imageStore(abufferPointerImg, coords, uvec4(EOL)); \n\
 \n\
	// Discard fragment so nothing is written to the framebuffer \n\
	discard; \n\
} \n\
";

static const char *tr_modvol_shader_source = SHADER_HEADER "\
#define MV_MODE %d \n\
#define MAX_PIXELS_PER_FRAGMENT " MAX_PIXELS_PER_FRAGMENT " \n\
 \n\
// Must match ModifierVolumeMode enum values \n\
#define MV_XOR		 0 \n\
#define MV_OR		 1 \n\
#define MV_INCLUSION 2 \n\
#define MV_EXCLUSION 3 \n\
 \n\
void main(void) \n\
{ \n\
#if MV_MODE == MV_XOR || MV_MODE == MV_OR \n\
	setFragDepth(); \n\
#endif \n\
	ivec2 coords = ivec2(gl_FragCoord.xy); \n\
	 \n\
	uint idx = imageLoad(abufferPointerImg, coords).x; \n\
	int list_len = 0; \n\
	while (idx != EOL && list_len < MAX_PIXELS_PER_FRAGMENT) \n\
	{ \n\
		const Pixel pixel = pixels[idx]; \n\
		const PolyParam pp = tr_poly_params[getPolyNumber(pixel)]; \n\
		if (getShadowEnable(pp)) \n\
		{ \n\
#if MV_MODE == MV_XOR \n\
			if (gl_FragDepth >= pixel.depth) \n\
				atomicXor(pixels[idx].seq_num, SHADOW_STENCIL); \n\
#elif MV_MODE == MV_OR \n\
			if (gl_FragDepth >= pixel.depth) \n\
				atomicOr(pixels[idx].seq_num, SHADOW_STENCIL); \n\
#elif MV_MODE == MV_INCLUSION \n\
			uint prev_val = atomicAnd(pixels[idx].seq_num, ~(SHADOW_STENCIL)); \n\
			if ((prev_val & (SHADOW_STENCIL|SHADOW_ACC)) == SHADOW_STENCIL) \n\
				pixels[idx].seq_num = bitfieldInsert(pixel.seq_num, 1u, 31, 1); \n\
#elif MV_MODE == MV_EXCLUSION \n\
			uint prev_val = atomicAnd(pixels[idx].seq_num, ~(SHADOW_STENCIL|SHADOW_ACC)); \n\
			if ((prev_val & (SHADOW_STENCIL|SHADOW_ACC)) == SHADOW_ACC) \n\
				pixels[idx].seq_num = bitfieldInsert(pixel.seq_num, 1u, 31, 1); \n\
#endif \n\
		} \n\
		idx = pixel.next; \n\
		list_len++; \n\
	} \n\
	 \n\
	discard; \n\
} \n\
";

static const char* VertexShaderSource =
"#version 430 \n"
"\
in highp vec3 in_pos; \n\
 \n\
void main() \n\
{ \n\
	gl_Position = vec4(in_pos, 1.0); \n\
}";

void initABuffer()
{
	if (max_image_width > 0 && max_image_height > 0)
	{
		if (pixels_pointers == 0)
			pixels_pointers = glcache.GenTexture();
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, pixels_pointers);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, max_image_width, max_image_height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, 0);
		glBindImageTexture(4, pixels_pointers, 0, false, 0,  GL_READ_WRITE, GL_R32UI);
		glCheck();
	}

	if (pixels_buffer == 0 )
	{
		// get the max buffer size
		GLint64 size;
		glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &size);
		pixel_buffer_size = (GLuint)min((GLint64)pixel_buffer_size, size);

		// Create the buffer
		glGenBuffers(1, &pixels_buffer);
		// Bind it
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, pixels_buffer);
		// Declare storage
		glBufferData(GL_SHADER_STORAGE_BUFFER, pixel_buffer_size, NULL, GL_DYNAMIC_COPY);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pixels_buffer);
		glCheck();
	}

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

	if (g_abuffer_final_shader.program == 0)
	{
		char source[16384];
		sprintf(source, final_shader_source, 1);
		gl4CompilePipelineShader(&g_abuffer_final_shader, false, source, VertexShaderSource);
	}
	if (g_abuffer_final_nosort_shader.program == 0)
	{
		char source[16384];
		sprintf(source, final_shader_source, 0);
		gl4CompilePipelineShader(&g_abuffer_final_nosort_shader, false, source, VertexShaderSource);
	}
	if (g_abuffer_clear_shader.program == 0)
		gl4CompilePipelineShader(&g_abuffer_clear_shader, false, clear_shader_source, VertexShaderSource);
	if (g_abuffer_tr_modvol_shaders[0].program == 0)
	{
		char source[16384];
		for (int mode = 0; mode < ModeCount; mode++)
		{
			sprintf(source, tr_modvol_shader_source, mode);
			gl4CompilePipelineShader(&g_abuffer_tr_modvol_shaders[mode], false, source, VertexShaderSource);
		}
	}

	if (g_quadVertexArray == 0)
		glGenVertexArrays(1, &g_quadVertexArray);
	if (g_quadBuffer == 0)
	{
		glBindVertexArray(g_quadVertexArray);
		glGenBuffers(1, &g_quadBuffer);
		glBindBuffer(GL_ARRAY_BUFFER, g_quadBuffer); glCheck();

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, (void*)0);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); glCheck();
		glBindVertexArray(0);
	}
	glCheck();

	// Clear A-buffer pointers
	glcache.UseProgram(g_abuffer_clear_shader.program);
	gl4ShaderUniforms.Set(&g_abuffer_clear_shader);

	abufferDrawQuad();
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

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
	if (g_quadVertexArray != 0)
	{
		glDeleteVertexArrays(1, &g_quadVertexArray);
		g_quadVertexArray = 0;
	}
	if (g_quadBuffer != 0)
	{
		glDeleteBuffers(1, &g_quadBuffer);
		g_quadBuffer = 0;
	}
	glcache.DeleteProgram(g_abuffer_final_shader.program);
	g_abuffer_final_shader.program = 0;
	glcache.DeleteProgram(g_abuffer_final_nosort_shader.program);
	g_abuffer_final_nosort_shader.program = 0;
	glcache.DeleteProgram(g_abuffer_clear_shader.program);
	g_abuffer_clear_shader.program = 0;
	for (int mode = 0; mode < ModeCount; mode++)
	{
		glcache.DeleteProgram(g_abuffer_tr_modvol_shaders[mode].program);
		g_abuffer_tr_modvol_shaders[mode].program = 0;
	}
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

void abufferDrawQuad()
{
	glBindVertexArray(g_quadVertexArray);

	float vertices[] = {
			-1,  1, 1,
			-1, -1, 1,
			 1,  1, 1,
			 1, -1, 1,
	};
	GLushort indices[] = { 0, 1, 2, 1, 3 };

	glBindBuffer(GL_ARRAY_BUFFER, g_quadBuffer); glCheck();
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STREAM_DRAW); glCheck();

	glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, indices); glCheck();
	glBindVertexArray(0);
	glCheck();
}

void DrawTranslucentModVols(int first, int count)
{
	if (count == 0 || pvrrc.modtrig.used() == 0)
		return;
	glBindVertexArray(gl4.vbo.modvol_vao);

	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE3);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);

	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_STENCIL_TEST);

	glCheck();

	ModifierVolumeParam* params = &pvrrc.global_param_mvo_tr.head()[first];

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

	int mod_base = -1;

	for (u32 cmv = 0; cmv < count; cmv++)
	{
		ModifierVolumeParam& param = params[cmv];

		if (param.count == 0)
			continue;

		u32 mv_mode = param.isp.DepthMode;

		verify(param.first >= 0 && param.first + param.count <= pvrrc.modtrig.used());

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
	glBindVertexArray(gl4.vbo.main_vao);
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

void renderABuffer(bool sortFragments)
{
	// Render to output FBO
	glcache.UseProgram(sortFragments ? g_abuffer_final_shader.program : g_abuffer_final_nosort_shader.program);
	gl4ShaderUniforms.Set(&g_abuffer_final_shader);

	glcache.Disable(GL_DEPTH_TEST);
	glcache.Disable(GL_CULL_FACE);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

	abufferDrawQuad();

	glCheck();

	// Clear A-buffer pointers
	glcache.UseProgram(g_abuffer_clear_shader.program);
	gl4ShaderUniforms.Set(&g_abuffer_clear_shader);

	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	abufferDrawQuad();

	glActiveTexture(GL_TEXTURE0);

	glCheck();
}
