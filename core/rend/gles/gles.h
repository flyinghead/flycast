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

#ifndef GL_UNSIGNED_INT_8_8_8_8
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#endif

#define glCheck() do { if (unlikely(settings.validate.OpenGlChecks)) { verify(glGetError()==GL_NO_ERROR); } } while(0)
#define eglCheck() false

#define VERTEX_POS_ARRAY 0
#define VERTEX_COL_BASE_ARRAY 1
#define VERTEX_COL_OFFS_ARRAY 2
#define VERTEX_UV_ARRAY 3
// OIT only
#define VERTEX_COL_BASE1_ARRAY 4
#define VERTEX_COL_OFFS1_ARRAY 5
#define VERTEX_UV1_ARRAY 6

#ifndef GL_UNSIGNED_INT_8_8_8_8
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#endif

//vertex types
extern u32 gcflip;
extern float scale_x, scale_y;


void DrawStrips();

struct PipelineShader
{
	GLuint program;

	GLuint scale,depth_scale;
	GLuint extra_depth_scale;
	GLuint pp_ClipTest,cp_AlphaTestValue;
	GLuint sp_FOG_COL_RAM,sp_FOG_COL_VERT,sp_FOG_DENSITY;
	GLuint trilinear_alpha;
	GLuint fog_clamp_min, fog_clamp_max;

	//
	u32 cp_AlphaTest; s32 pp_ClipTestMode;
	u32 pp_Texture, pp_UseAlpha, pp_IgnoreTexA, pp_ShadInstr, pp_Offset, pp_FogCtrl;
	bool pp_Gouraud, pp_BumpMap;
	bool fog_clamping;
	bool trilinear;
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
		GLuint extra_depth_scale;
		GLuint sp_ShaderColor;

	} modvol_shader;

	PipelineShader pogram_table[24576];
	struct
	{
		GLuint program,scale,depth_scale;
		GLuint extra_depth_scale;
	} OSD_SHADER;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
#ifndef GLES
		GLuint vao;
#endif
	} vbo;

	const char *gl_version;
	const char *glsl_version_header;
	int gl_major;
	bool is_gles;
	GLuint fog_image_format;
	GLenum index_type;
	bool swap_buffer_not_preserved;

	size_t get_index_size() { return index_type == GL_UNSIGNED_INT ? sizeof(u32) : sizeof(u16); }
};

extern gl_ctx gl;
extern GLuint fbTextureId;
extern float fb_scale_x, fb_scale_y;

GLuint gl_GetTexture(TSP tsp,TCW tcw);
struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};
enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, ModeCount };

bool gl_init(void* wind, void* disp);
void gl_load_osd_resources();
void gl_free_osd_resources();
void gl_swap();
bool ProcessFrame(TA_context* ctx);
void UpdateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format);
void save_current_frame();
bool render_last_frame();

text_info raw_GetTexture(TSP tsp, TCW tcw);
void killtex();
void CollectCleanup();
void DoCleanup();
void SortPParams(int first, int count);
void SetCull(u32 CullMode);
s32 SetTileClip(u32 val, GLint uniform);
void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc);

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
void ReadRTTBuffer();
void RenderFramebuffer();
void DrawFramebuffer(float w, float h);

void HideOSD();
void OSD_HOOK();
void OSD_DRAW(GLuint shader_program);
int GetProgramID(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear);

GLuint gl_CompileShader(const char* shader, GLuint type);
GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader);
bool CompilePipelineShader(PipelineShader* s);
#define TEXTURE_LOAD_ERROR 0
u8* loadPNGData(const string& subpath, int &width, int &height, bool bottom_to_top = true);
GLuint loadPNG(const string& subpath, int &width, int &height, bool bottom_to_top = true);

extern struct ShaderUniforms_t
{
	float PT_ALPHA;
	float scale_coefs[4];
	float depth_coefs[4];
	float extra_depth_scale;
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
	float trilinear_alpha;
	float fog_clamp_min[4];
	float fog_clamp_max[4];

	void Set(PipelineShader* s)
	{
		if (s->cp_AlphaTestValue!=-1)
			glUniform1f(s->cp_AlphaTestValue,PT_ALPHA);

		if (s->scale!=-1)
			glUniform4fv( s->scale, 1, scale_coefs);

		if (s->depth_scale!=-1)
			glUniform4fv( s->depth_scale, 1, depth_coefs);

		if (s->extra_depth_scale != -1)
			glUniform1f(s->extra_depth_scale, extra_depth_scale);

		if (s->sp_FOG_DENSITY!=-1)
			glUniform1f( s->sp_FOG_DENSITY,fog_den_float);

		if (s->sp_FOG_COL_RAM!=-1)
			glUniform3fv( s->sp_FOG_COL_RAM, 1, ps_FOG_COL_RAM);

		if (s->sp_FOG_COL_VERT!=-1)
			glUniform3fv( s->sp_FOG_COL_VERT, 1, ps_FOG_COL_VERT);

		if (s->trilinear_alpha != -1)
			glUniform1f(s->trilinear_alpha, trilinear_alpha);
		
		if (s->fog_clamp_min != -1)
			glUniform4fv(s->fog_clamp_min, 1, fog_clamp_min);
		if (s->fog_clamp_max != -1)
			glUniform4fv(s->fog_clamp_max, 1, fog_clamp_max);
	}

} ShaderUniforms;

// Render to texture
struct FBT
{
	u32 TexAddr;
	GLuint depthb,stencilb;
	GLuint tex;
	GLuint fbo;
};

extern FBT fb_rtt;

struct PvrTexInfo;
template <class pixel_type> class PixelBuffer;
typedef void TexConvFP(PixelBuffer<u16>* pb,u8* p_in,u32 Width,u32 Height);
typedef void TexConvFP32(PixelBuffer<u32>* pb,u8* p_in,u32 Width,u32 Height);

struct TextureCacheData
{
	TSP tsp;        //dreamcast texture parameters
	TCW tcw;
	
	GLuint texID;   //gl texture
	u16* pData;
	int tex_type;
	
	u32 Lookups;
	
	//decoded texture info
	u32 sa;         //pixel data start address in vram (might be offset for mipmaps/etc)
	u32 sa_tex;		//texture data start address in vram
	u32 w,h;        //width & height of the texture
	u32 size;       //size, in bytes, in vram
	
	PvrTexInfo* tex;
	TexConvFP*  texconv;
	TexConvFP32*  texconv32;
	
	u32 dirty;
	vram_block* lock_block;
	
	u32 Updates;
	
	//used for palette updates
	u32 palette_hash;			// Palette hash at time of last update
	u32  indirect_color_ptr;    //palette color table index for pal. tex
	//VQ quantizers table for VQ tex
	//a texture can't be both VQ and PAL at the same time
	u32 texture_hash;			// xxhash of texture data, used for custom textures
	u8* custom_image_data;		// loaded custom image data
	u32 custom_width;
	u32 custom_height;
	bool custom_load_in_progress;
	
	void PrintTextureName();
	
	bool IsPaletted()
	{
		return tcw.PixelFmt == PixelPal4 || tcw.PixelFmt == PixelPal8;
	}
	
	void Create(bool isGL);
	void ComputeHash();
	void Update();
	void UploadToGPU(GLuint textype, int width, int height, u8 *temp_tex_buffer);
	void CheckCustomTexture();
	//true if : dirty or paletted texture and hashes don't match
	bool NeedsUpdate();
	bool Delete();
};
