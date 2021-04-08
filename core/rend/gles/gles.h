#pragma once
#include "hw/pvr/ta_structs.h"
#include "hw/pvr/ta_ctx.h"
#include "rend/TexCache.h"
#include "wsi/gl_context.h"
#include "glcache.h"

#include <unordered_map>
#include <glm/glm.hpp>

#define glCheck() do { if (unlikely(config::OpenGlChecks)) { verify(glGetError()==GL_NO_ERROR); } } while(0)

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

extern glm::mat4 ViewportMatrix;

void DrawStrips();

struct PipelineShader
{
	GLuint program;

	GLint depth_scale;
	GLint pp_ClipTest;
	GLint cp_AlphaTestValue;
	GLint sp_FOG_COL_RAM;
	GLint sp_FOG_COL_VERT;
	GLint sp_FOG_DENSITY;
	GLint trilinear_alpha;
	GLint fog_clamp_min, fog_clamp_max;
	GLint normal_matrix;
	GLint palette_index;

	//
	bool cp_AlphaTest;
	bool pp_InsideClipping;
	bool pp_Texture;
	bool pp_UseAlpha;
	bool pp_IgnoreTexA;
	u32 pp_ShadInstr;
	bool pp_Offset;
	u32 pp_FogCtrl;
	bool pp_Gouraud;
	bool pp_BumpMap;
	bool fog_clamping;
	bool trilinear;
	bool palette;
};


struct gl_ctx
{
	struct
	{
		GLuint program;

		GLint depth_scale;
		GLint sp_ShaderColor;
		GLint normal_matrix;

	} modvol_shader;

	std::unordered_map<u32, PipelineShader> shaders;

	struct
	{
		GLuint program;
		GLint scale;
		GLuint vao;
		GLuint geometry;
		GLuint osd_tex;
	} OSD_SHADER;

	struct
	{
		GLuint geometry,modvols,idxs,idxs2;
		GLuint mainVAO;
		GLuint modvolVAO;
	} vbo;

	struct
	{
		u32 texAddress = ~0;
		GLuint depthb;
		GLuint tex;
		GLuint fbo;
		GLuint pbo;
		u32 pboSize;
		bool directXfer;
		u32 width;
		u32 height;
		u32 fb_w_ctrl;
		u32 linestride;
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
	int gl_minor;
	bool is_gles;
	GLuint single_channel_format;
	GLenum index_type;
	bool GL_OES_packed_depth_stencil_supported;
	bool GL_OES_depth24_supported;
	bool highp_float_supported;

	size_t get_index_size() { return index_type == GL_UNSIGNED_INT ? sizeof(u32) : sizeof(u16); }
};

extern gl_ctx gl;
extern GLuint fbTextureId;

u64 gl_GetTexture(TSP tsp,TCW tcw);
struct text_info {
	u16* pdata;
	u32 width;
	u32 height;
	u32 textype; // 0 565, 1 1555, 2 4444
};
enum ModifierVolumeMode { Xor, Or, Inclusion, Exclusion, ModeCount };

void gl_load_osd_resources();
void gl_free_osd_resources();
bool ProcessFrame(TA_context* ctx);
void UpdateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format);
void UpdatePaletteTexture(GLenum texture_slot);
void findGLVersion();
void GetFramebufferScaling(float& scale_x, float& scale_y, float& scissoring_scale_x, float& scissoring_scale_y);
void GetFramebufferSize(float& dc_width, float& dc_height);
void SetupMatrices(float dc_width, float dc_height,
				   float scale_x, float scale_y, float scissoring_scale_x, float scissoring_scale_y,
				   float &ds2s_offs_x, glm::mat4& normal_mat, glm::mat4& scissor_mat);

text_info raw_GetTexture(TSP tsp, TCW tcw);
void SetCull(u32 CullMode);
s32 SetTileClip(u32 val, GLint uniform);
void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc);

GLuint BindRTT(bool withDepthBuffer = true);
void ReadRTTBuffer();
void RenderFramebuffer();
void DrawFramebuffer();
GLuint init_output_framebuffer(int width, int height);
bool render_output_framebuffer();
void free_output_framebuffer();

void OSD_DRAW(bool clear_screen);
PipelineShader *GetProgram(bool cp_AlphaTest, bool pp_InsideClipping,
		bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr, bool pp_Offset,
		u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear,
		bool palette);

GLuint gl_CompileShader(const char* shader, GLuint type);
GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader);
bool CompilePipelineShader(PipelineShader* s);

extern struct ShaderUniforms_t
{
	float PT_ALPHA;
	float depth_coefs[4];
	float fog_den_float;
	float ps_FOG_COL_RAM[3];
	float ps_FOG_COL_VERT[3];
	float trilinear_alpha;
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

	void Set(const PipelineShader* s)
	{
		if (s->cp_AlphaTestValue!=-1)
			glUniform1f(s->cp_AlphaTestValue,PT_ALPHA);

		if (s->depth_scale!=-1)
			glUniform4fv( s->depth_scale, 1, depth_coefs);

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

		if (s->normal_matrix != -1)
			glUniformMatrix4fv(s->normal_matrix, 1, GL_FALSE, &normal_mat[0][0]);

		if (s->palette_index != -1)
			glUniform1i(s->palette_index, palette_index);
	}

} ShaderUniforms;

class TextureCacheData final : public BaseTextureCacheData
{
public:
	GLuint texID;   //gl texture
	std::string GetId() override { return std::to_string(texID); }
	void UploadToGPU(int width, int height, u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded = false) override;
	bool Delete() override;
};

class GlTextureCache final : public BaseTextureCache<TextureCacheData>
{
public:
	void Cleanup()
	{
		if (!texturesToDelete.empty())
		{
			glcache.DeleteTextures((GLsizei)texturesToDelete.size(), &texturesToDelete[0]);
			texturesToDelete.clear();
		}
		CollectCleanup();
	}
	void DeleteLater(GLuint texId) { texturesToDelete.push_back(texId); }

private:
	std::vector<GLuint> texturesToDelete;
};
extern GlTextureCache TexCache;

extern const u32 Zfunction[8];
extern const u32 SrcBlendGL[], DstBlendGL[];

struct OpenGLRenderer : Renderer
{
	bool Init() override;
	void Resize(int w, int h) override { width = w; height = h; }
	void Term() override;

	bool Process(TA_context* ctx) override { return ProcessFrame(ctx); }

	bool Render() override;

	bool RenderLastFrame() override;

	void DrawOSD(bool clear_screen) override { OSD_DRAW(clear_screen); }

	u64 GetTexture(TSP tsp, TCW tcw) override
	{
		return gl_GetTexture(tsp, tcw);
	}

	bool Present() override
	{
		if (!frameRendered)
			return false;
		frameRendered = false;
		return true;
	}

	bool frameRendered = false;
	int width;
	int height;
};

void initQuad();
void termQuad();
void drawQuad(GLuint texId, bool rotate = false, bool swapY = false);

#define SHADER_COMPAT "						\n\
#define TARGET_GL %s						\n\
											\n\
#define GLES2 0 							\n\
#define GLES3 1 							\n\
#define GL2 2								\n\
#define GL3 3								\n\
											\n\
#if TARGET_GL == GL2 						\n\
#define highp								\n\
#define lowp								\n\
#define mediump								\n\
#endif										\n\
#if TARGET_GL == GLES3						\n\
out highp vec4 FragColor;					\n\
#define gl_FragColor FragColor				\n\
#define FOG_CHANNEL a						\n\
#elif TARGET_GL == GL3						\n\
out highp vec4 FragColor;					\n\
#define gl_FragColor FragColor				\n\
#define FOG_CHANNEL r						\n\
#else										\n\
#define texture texture2D					\n\
#define FOG_CHANNEL a						\n\
#endif										\n\
											\n\
"

#define VTX_SHADER_COMPAT SHADER_COMPAT \
"#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in attribute						\n\
#define out varying							\n\
#endif										\n\
"

#define PIX_SHADER_COMPAT SHADER_COMPAT \
"#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in varying							\n\
#endif										\n\
"
