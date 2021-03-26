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
#pragma once
#include "rend/gles/gles.h"
#include <unordered_map>

void gl4DrawStrips(GLuint output_fbo, int width, int height);

enum class Pass { Depth, Color, OIT };

struct gl4PipelineShader
{
	GLuint program;

	GLint pp_ClipTest;
	GLint cp_AlphaTestValue;
	GLint sp_FOG_COL_RAM;
	GLint sp_FOG_COL_VERT;
	GLint sp_FOG_DENSITY;
	GLint shade_scale_factor;
	GLint pp_Number;
	GLint blend_mode;
	GLint use_alpha;
	GLint ignore_tex_alpha;
	GLint shading_instr;
	GLint fog_control;
	GLint trilinear_alpha;
	GLint fog_clamp_min, fog_clamp_max;
	GLint normal_matrix;
	GLint palette_index;

	bool cp_AlphaTest;
	bool pp_InsideClipping;
	bool pp_Texture;
	bool pp_UseAlpha;
	bool pp_IgnoreTexA;
	u32 pp_ShadInstr;
	bool pp_Offset;
	u32 pp_FogCtrl;
	Pass pass;
	bool pp_TwoVolumes;
	bool pp_Gouraud;
	bool pp_BumpMap;
	bool fog_clamping;
	bool palette;
};


struct gl4_ctx
{
	struct
	{
		GLuint program;

		GLuint normal_matrix;
	} modvol_shader;

	std::unordered_map<u32, gl4PipelineShader> shaders;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
		GLuint main_vao;
		GLuint modvol_vao;
		GLuint tr_poly_params;
	} vbo;
};

extern gl4_ctx gl4;

extern int max_image_width;
extern int max_image_height;

GLuint gl4BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);

extern const char *gl4PixelPipelineShader;
bool gl4CompilePipelineShader(gl4PipelineShader* s, const char *pixel_source = gl4PixelPipelineShader, const char *vertex_source = NULL);

void initABuffer();
void termABuffer();
void reshapeABuffer(int width, int height);
void renderABuffer();
void DrawTranslucentModVols(int first, int count);
void checkOverflowAndReset();

extern GLuint stencilTexId;
extern GLuint depthTexId;
extern GLuint opaqueTexId;
extern GLuint depthSaveTexId;
extern GLuint geom_fbo;
extern GLuint texSamplers[2];
extern GLuint depth_fbo;

#define SHADER_HEADER "#version 430 \n\
\n\
layout(r32ui, binding = 4) uniform coherent restrict uimage2D abufferPointerImg; \n\
struct Pixel { \n\
	vec4 color; \n\
	float depth; \n\
	uint seq_num; \n\
	uint next; \n\
}; \n\
#define EOL 0xFFFFFFFFu \n\
layout (binding = 0, std430) coherent restrict buffer PixelBuffer { \n\
	Pixel pixels[]; \n\
}; \n\
layout(binding = 0, offset = 0) uniform atomic_uint buffer_index; \n\
\n\
#define ZERO				0 \n\
#define ONE					1 \n\
#define OTHER_COLOR			2 \n\
#define INVERSE_OTHER_COLOR	3 \n\
#define SRC_ALPHA			4 \n\
#define INVERSE_SRC_ALPHA	5 \n\
#define DST_ALPHA			6 \n\
#define INVERSE_DST_ALPHA	7 \n\
 \n\
uint getNextPixelIndex() \n\
{ \n\
	uint index = atomicCounterIncrement(buffer_index); \n\
	if (index >= pixels.length()) \n\
		// Buffer overflow \n\
		discard; \n\
	 \n\
	return index; \n\
} \n\
\n\
void setFragDepth(void) \n\
{ \n\
	float w = 100000.0 * gl_FragCoord.w; \n\
	gl_FragDepth = log2(1.0 + w) / 34.0; \n\
} \n\
struct PolyParam { \n\
	int first; \n\
	int count; \n\
	int texid_low; \n\
	int texid_high; \n\
	int tsp; \n\
	int tcw; \n\
	int pcw; \n\
	int isp; \n\
	float zvZ; \n\
	int tileclip; \n\
	int tsp1; \n\
	int tcw1; \n\
	int texid1_low; \n\
	int texid1_high; \n\
}; \n\
layout (binding = 1, std430) readonly buffer TrPolyParamBuffer { \n\
	PolyParam tr_poly_params[]; \n\
}; \n\
 \n\
#define GET_TSP_FOR_AREA int tsp; if (area1) tsp = pp.tsp1; else tsp = pp.tsp; \n\
 \n\
int getSrcBlendFunc(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 29) & 7; \n\
} \n\
\n\
int getDstBlendFunc(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 26) & 7; \n\
} \n\
\n\
bool getSrcSelect(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 25) & 1) != 0; \n\
} \n\
\n\
bool getDstSelect(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 24) & 1) != 0; \n\
} \n\
\n\
int getFogControl(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 22) & 3; \n\
} \n\
\n\
bool getUseAlpha(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 20) & 1) != 0; \n\
} \n\
\n\
bool getIgnoreTexAlpha(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return ((tsp >> 19) & 1) != 0; \n\
} \n\
\n\
int getShadingInstruction(const PolyParam pp, bool area1) \n\
{ \n\
	GET_TSP_FOR_AREA \n\
	return (tsp >> 6) & 3; \n\
} \n\
\n\
int getDepthFunc(const PolyParam pp) \n\
{ \n\
	return (pp.isp >> 29) & 7; \n\
} \n\
\n\
bool getDepthMask(const PolyParam pp) \n\
{ \n\
	return ((pp.isp >> 26) & 1) != 1; \n\
} \n\
\n\
bool getShadowEnable(const PolyParam pp) \n\
{ \n\
	return ((pp.pcw >> 7) & 1) != 0; \n\
} \n\
\n\
uint getPolyNumber(const Pixel pixel) \n\
{ \n\
	return pixel.seq_num & 0x3FFFFFFFu; \n\
} \n\
\n\
#define SHADOW_STENCIL 0x40000000u \n\
#define SHADOW_ACC	   0x80000000u \n\
\n\
bool isShadowed(const Pixel pixel) \n\
{ \n\
	return (pixel.seq_num & SHADOW_ACC) == SHADOW_ACC; \n\
} \n\
\n\
bool isTwoVolumes(const PolyParam pp) \n\
{ \n\
	return pp.tsp1 != -1 || pp.tcw1 != -1; \n\
} \n\
 \n\
"

void gl4SetupMainVBO();
void gl4SetupModvolVBO();
void gl4CreateTextures(int width, int height);

extern struct gl4ShaderUniforms_t
{
	float PT_ALPHA;
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
	int poly_number;
	float trilinear_alpha;
	TSP tsp0;
	TSP tsp1;
	TCW tcw0;
	TCW tcw1;
	float fog_clamp_min[4];
	float fog_clamp_max[4];
	glm::mat4 normal_mat;
	struct {
		bool enabled;
		int x;
		int y;
		int width;
		int height;
	} base_clipping;
	int palette_index;

	void setUniformArray(GLint location, int v0, int v1)
	{
		int array[] = { v0, v1 };
		glUniform1iv(location, 2, array);
	}

	void Set(gl4PipelineShader* s)
	{
		if (s->cp_AlphaTestValue!=-1)
			glUniform1f(s->cp_AlphaTestValue,PT_ALPHA);

		if (s->sp_FOG_DENSITY!=-1)
			glUniform1f( s->sp_FOG_DENSITY,fog_den_float);

		if (s->sp_FOG_COL_RAM!=-1)
			glUniform3fv( s->sp_FOG_COL_RAM, 1, ps_FOG_COL_RAM);

		if (s->sp_FOG_COL_VERT!=-1)
			glUniform3fv( s->sp_FOG_COL_VERT, 1, ps_FOG_COL_VERT);

		if (s->shade_scale_factor != -1)
			glUniform1f(s->shade_scale_factor, FPU_SHAD_SCALE.scale_factor / 256.f);

		if (s->blend_mode != -1) {
			u32 blend_mode[] = { tsp0.SrcInstr, tsp0.DstInstr, tsp1.SrcInstr, tsp1.DstInstr };
			glUniform2iv(s->blend_mode, 2, (GLint *)blend_mode);
		}

		if (s->use_alpha != -1)
			setUniformArray(s->use_alpha, tsp0.UseAlpha, tsp1.UseAlpha);

		if (s->ignore_tex_alpha != -1)
			setUniformArray(s->ignore_tex_alpha, tsp0.IgnoreTexA, tsp1.IgnoreTexA);

		if (s->shading_instr != -1)
			setUniformArray(s->shading_instr, tsp0.ShadInstr, tsp1.ShadInstr);

		if (s->fog_control != -1)
			setUniformArray(s->fog_control, tsp0.FogCtrl, tsp1.FogCtrl);

		if (s->pp_Number != -1)
			glUniform1i(s->pp_Number, poly_number);

		if (s->trilinear_alpha != -1)
			glUniform1f(s->trilinear_alpha, trilinear_alpha);
		
		if (s->fog_clamp_min != -1)
			glUniform4fv(s->fog_clamp_min, 1, fog_clamp_min);
		if (s->fog_clamp_max != -1)
			glUniform4fv(s->fog_clamp_max, 1, fog_clamp_max);

		if (s->normal_matrix != -1)
			glUniformMatrix4fv(s->normal_matrix, 1, GL_FALSE, &normal_mat[0][0]);

		if (s->palette_index != -1)
			glUniform1i(s->palette_index, palette_index);
	}

} gl4ShaderUniforms;

