#pragma once
#include "rend/rend.h"


#ifdef GLES
#if defined(TARGET_IPHONE) //apple-specific ogles2 headers
//#include <APPLE/egl.h>
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#else
#if !defined(TARGET_NACL32)
#include <EGL/egl.h>
#endif
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#ifndef GL_NV_draw_path
//IMGTEC GLES emulation
#pragma comment(lib,"libEGL.lib")
#pragma comment(lib,"libGLESv2.lib")
#else /* NV gles emulation*/
#pragma comment(lib,"libGLES20.lib")
#endif

#else
#if HOST_OS == OS_DARWIN
    #include <OpenGL/gl3.h>
#else
	#include <GL3/gl3w.h>
#endif
#endif


#define glCheck() do { if (unlikely(settings.validate.OpenGlChecks)) { verify(glGetError()==GL_NO_ERROR); } } while(0)
#define eglCheck() false

#define VERTEX_POS_ARRAY 0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY 3

struct float2
{
	float x;
	float y;
};

struct float3
{
	float x;
	float y;
	float z;
};

struct GLFramebufferData {
	GLuint framebuffer;
	GLuint framebufferRenderbuffer;
	GLuint framebufferTexture;
	GLuint positionsBuffer;
	GLuint texcoordsBuffer;
	GLuint indexBuffer;
};

//vertex types
extern u32 gcflip;

extern u32 rttDepthCounter;
extern GLFramebufferData fullscreenQuad;

enum rttSelectedOption
{
	Disabled = 0,
	Zeros,
	Ones,
	ShadowCircle,
	Full
};

void DrawStrips();
void DrawFullscreenQuad(float, float, float, float);

bool isExtensionSupported(const char *);
void rttCheckIfUpdated();

struct PipelineShader
{
	GLuint program;

	GLuint scale,depth_scale;
	GLuint pp_ClipTest,cp_AlphaTestValue;
	GLuint sp_FOG_COL_RAM,sp_FOG_COL_VERT,sp_FOG_DENSITY;

	//
	u32 cp_AlphaTest; s32 pp_ClipTestMode;
	u32 pp_Texture, pp_UseAlpha, pp_IgnoreTexA, pp_ShadInstr, pp_Offset, pp_FogCtrl;
};


struct gl_ctx
{
#if defined(GLES) && HOST_OS != OS_DARWIN && !defined(TARGET_NACL32)
	struct
	{
		EGLNativeWindowType native_wind;
		EGLNativeDisplayType native_disp;
		EGLDisplay display;
		EGLSurface surface;
		EGLContext context;
	} setup;
#endif

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
		const char *adrenoRenderer = "Adreno";
	} workarounds;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
#ifndef GLES
		GLuint vao;
#endif
	} vbo;

	GLuint fullscreenQuadShader;

	const char *renderer;
	const char *gl_version;
	const char *glsl_version_header;
	int gl_major;
	bool is_gles;
	GLuint fog_image_format;

	//GLuint matrix;
};

extern gl_ctx gl;

GLuint gl_GetTexture(TSP tsp,TCW tcw);
struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};

text_info raw_GetTexture(TSP tsp, TCW tcw);
void CollectCleanup();
void DoCleanup();
void SortPParams();

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
void ReadRTT();
int GetProgramID(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl);
void InitShadowCircle();

bool CompilePipelineShader(PipelineShader* s);
#define TEXTURE_LOAD_ERROR 0
GLuint loadPNG(const string& subpath, int &width, int &height);
