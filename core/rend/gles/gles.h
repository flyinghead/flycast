#pragma once
#include "rend/rend.h"

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#ifndef GL_NV_draw_path
//IMGTEC GLES emulation
#pragma comment(lib,"libEGL.lib")
#pragma comment(lib,"libGLESv2.lib")
#else /* NV gles emulation*/
#pragma comment(lib,"libGLES20.lib")

#endif
#define glCheck() false
#define eglCheck() false

#define VERTEX_POS_ARRAY 0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY 3


//vertex types
extern u32 gcflip;


void DrawStrips();

struct PipelineShader
{
	GLuint program;

	GLuint scale,depth_scale;
	GLuint pp_ClipTest,cp_AlphaTestValue;
	GLuint sp_FOG_COL_RAM,sp_FOG_COL_VERT,sp_FOG_DENSITY,sp_LOG_FOG_COEFS;

	//
	u32 cp_AlphaTest; s32 pp_ClipTestMode;
	u32 pp_Texture, pp_UseAlpha, pp_IgnoreTexA, pp_ShadInstr, pp_Offset, pp_FogCtrl;
};


struct gl_ctx
{
	struct
	{
		EGLNativeWindowType native_wind;
		EGLNativeDisplayType native_disp;
		EGLDisplay display;
		EGLSurface surface;
		EGLContext context;
	} setup;

	struct
	{
		GLuint program;

		GLuint scale,depth_scale;
		GLuint sp_ShaderColor;

	} modvol_shader;

	PipelineShader pogram_table[768*2];
	struct
	{
		GLuint program,scale,depth_scale;
	} OSD_SHADER;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
	} vbo;


	//GLuint matrix;
};

extern gl_ctx gl;

GLuint GetTexture(TSP tsp,TCW tcw);
void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
int GetProgramID(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl);

bool CompilePipelineShader(PipelineShader* s);
#define TEXTURE_LOAD_ERROR 0
GLuint loadPNG(const string& subpath, int &width, int &height);
