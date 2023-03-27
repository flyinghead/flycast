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
#include "hw/pvr/elan_struct.h"
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
	GLint ndcMat;
	GLint palette_index;

	// Naomi2
	GLint mvMat;
	GLint normalMat;
	GLint projMat;
	GLint glossCoef[2];
	GLint envMapping[2];
	GLint bumpMapping;
	GLint constantColor[2];

	GLint lightCount;
	GLint ambientBase[2];
	GLint ambientOffset[2];
	GLint ambientMaterialBase[2];
	GLint ambientMaterialOffset[2];
	GLint useBaseOver;
	GLint bumpId0;
	GLint bumpId1;
	struct {
		GLint color;
		GLint direction;
		GLint position;
		GLint parallel;
		GLint diffuse[2];
		GLint specular[2];
		GLint routing;
		GLint dmode;
		GLint smode;
		GLint distAttnMode;
		GLint attnDistA;
		GLint attnDistB;
		GLint attnAngleA;
		GLint attnAngleB;
	} lights[elan::MAX_LIGHTS];

	int lastMvMat;
	int lastNormalMat;
	int lastProjMat;
	int lastLightModel;

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
	bool naomi2;
	bool divPosZ;
};

class Gl4MainVertexArray final : public GlVertexArray
{
protected:
	void defineVtxAttribs() override;
};

class Gl4ModvolVertexArray final : public GlVertexArray
{
protected:
	void defineVtxAttribs() override;
};

struct gl4_ctx
{
	struct
	{
		GLuint program;

		GLuint ndcMat;
	} modvol_shader;

	struct
	{
		GLuint program;

		GLuint ndcMat;
		GLint mvMat;
		GLint projMat;
	} n2ModVolShader;

	std::unordered_map<u32, gl4PipelineShader> shaders;

	struct
	{
		std::unique_ptr<GlBuffer> geometry[2];
		std::unique_ptr<GlBuffer> modvols[2];
		std::unique_ptr<GlBuffer> idxs[2];
		Gl4MainVertexArray main_vao[2];
		Gl4ModvolVertexArray modvol_vao[2];
		std::unique_ptr<GlBuffer> tr_poly_params[2];
		int bufferIndex = 0;

		GlBuffer *getVertexBuffer() {
			return geometry[bufferIndex].get();
		}
		GlBuffer *getIndexBuffer() {
			return idxs[bufferIndex].get();
		}
		GlBuffer *getModVolBuffer() {
			return modvols[bufferIndex].get();
		}
		GlBuffer *getPolyParamBuffer() {
			return tr_poly_params[bufferIndex].get();
		}
		Gl4MainVertexArray& getMainVAO() {
			return main_vao[bufferIndex];
		}
		Gl4ModvolVertexArray& getModVolVAO() {
			return modvol_vao[bufferIndex];
		}
		void nextBuffer() {
			bufferIndex = (bufferIndex + 1) % std::size(geometry);
		}
	} vbo;
};

extern gl4_ctx gl4;

extern int max_image_width;
extern int max_image_height;

extern const char *gl4PixelPipelineShader;
bool gl4CompilePipelineShader(gl4PipelineShader* s, const char *pixel_source = nullptr, const char *vertex_source = nullptr);

void initABuffer();
void termABuffer();
void reshapeABuffer(int width, int height);
void renderABuffer();
void DrawTranslucentModVols(int first, int count, bool useOpaqueGeom);
void checkOverflowAndReset();

extern GLuint stencilTexId;
extern GLuint depthTexId;
extern GLuint opaqueTexId;
extern GLuint depthSaveTexId;
extern GLuint geom_fbo;
extern GLuint texSamplers[2];
extern GLuint depth_fbo;

extern const char* ShaderHeader;

class OpenGl4Source : public ShaderSource
{
public:
	OpenGl4Source() : ShaderSource("#version 430") {}
};

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
	glm::mat4 ndcMat;
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
		glUniform1f(s->cp_AlphaTestValue,PT_ALPHA);
		glUniform1f( s->sp_FOG_DENSITY,fog_den_float);
		glUniform3fv( s->sp_FOG_COL_RAM, 1, ps_FOG_COL_RAM);
		glUniform3fv( s->sp_FOG_COL_VERT, 1, ps_FOG_COL_VERT);
		glUniform1f(s->shade_scale_factor, FPU_SHAD_SCALE.scale_factor / 256.f);

		if (s->blend_mode != -1) {
			u32 blend_mode[] = { tsp0.SrcInstr, tsp0.DstInstr, tsp1.SrcInstr, tsp1.DstInstr };
			glUniform2iv(s->blend_mode, 2, (GLint *)blend_mode);
		}

		setUniformArray(s->use_alpha, tsp0.UseAlpha, tsp1.UseAlpha);
		setUniformArray(s->ignore_tex_alpha, tsp0.IgnoreTexA, tsp1.IgnoreTexA);
		setUniformArray(s->shading_instr, tsp0.ShadInstr, tsp1.ShadInstr);
		setUniformArray(s->fog_control, tsp0.FogCtrl, tsp1.FogCtrl);
		glUniform1i(s->pp_Number, poly_number);
		glUniform1f(s->trilinear_alpha, trilinear_alpha);
		glUniform4fv(s->fog_clamp_min, 1, fog_clamp_min);
		glUniform4fv(s->fog_clamp_max, 1, fog_clamp_max);
		glUniformMatrix4fv(s->ndcMat, 1, GL_FALSE, &ndcMat[0][0]);
		glUniform1i(s->palette_index, palette_index);
	}

} gl4ShaderUniforms;

