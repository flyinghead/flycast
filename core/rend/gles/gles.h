#pragma once
#include <unordered_map>
#include <atomic>
#include "rend/rend.h"

#if (defined(GLES) && !defined(TARGET_NACL32) && HOST_OS != OS_DARWIN && !defined(USE_SDL)) || defined(_ANDROID)
#define USE_EGL
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#ifdef GLES
#if defined(TARGET_IPHONE) //apple-specific ogles2 headers
//#include <APPLE/egl.h>
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#endif
#include <GLES32/gl32.h>
#include <GLES32/gl2ext.h>
#ifndef GLES2
#include "gl32funcs.h"
#endif

#ifndef GL_NV_draw_path
//IMGTEC GLES emulation
#pragma comment(lib,"libEGL.lib")
#pragma comment(lib,"libGLESv2.lib")
#else /* NV gles emulation*/
#pragma comment(lib,"libGLES20.lib")
#endif

#elif HOST_OS == OS_DARWIN
    #include <OpenGL/gl3.h>
#else
	#include <GL4/gl3w.h>
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
#ifdef USE_EGL
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

	std::unordered_map<u32, PipelineShader> shaders;
	bool rotate90;

	struct
	{
		GLuint program;
		GLuint scale;
	} OSD_SHADER;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
		GLuint vao;
	} vbo;

	struct
	{
		u32 TexAddr;
		GLuint depthb;
		GLuint tex;
		GLuint fbo;
	} rtt;

	struct
	{
		GLuint depthb;
		GLuint colorb;
		GLuint tex;
		GLuint fbo;
		int width;
		int height;
	} ofbo;

	const char *gl_version;
	const char *glsl_version_header;
	int gl_major;
	bool is_gles;
	GLuint fog_image_format;
	GLenum index_type;
	bool swap_buffer_not_preserved;
	bool GL_OES_packed_depth_stencil_supported;
	bool GL_OES_depth24_supported;

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
void findGLVersion();

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
GLuint init_output_framebuffer(int width, int height);
bool render_output_framebuffer();
void free_output_framebuffer();

void HideOSD();
void OSD_DRAW(bool clear_screen);
PipelineShader *GetProgram(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear);

GLuint gl_CompileShader(const char* shader, GLuint type);
GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader);
bool CompilePipelineShader(PipelineShader* s);
#define TEXTURE_LOAD_ERROR 0
u8* loadPNGData(const string& subpath, int &width, int &height);
GLuint loadPNG(const string& subpath, int &width, int &height);

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

		if (s->fog_clamp_min != -1)
			glUniform4fv(s->fog_clamp_min, 1, fog_clamp_min);
		if (s->fog_clamp_max != -1)
			glUniform4fv(s->fog_clamp_max, 1, fog_clamp_max);
	}

} ShaderUniforms;

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
	u32 old_texture_hash;		// legacy hash
	u8* volatile custom_image_data;		// loaded custom image data
	volatile u32 custom_width;
	volatile u32 custom_height;
	std::atomic_int custom_load_in_progress;
	
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
