#pragma once
#include "hw/pvr/ta_structs.h"
#include "hw/pvr/ta_ctx.h"
#include "rend/TexCache.h"
#include "wsi/gl_context.h"

#include <unordered_map>
#include <glm/glm.hpp>

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

extern glm::mat4 ViewportMatrix;

void DrawStrips();

struct PipelineShader
{
	GLuint program;

	GLuint depth_scale;
	GLuint pp_ClipTest,cp_AlphaTestValue;
	GLuint sp_FOG_COL_RAM,sp_FOG_COL_VERT,sp_FOG_DENSITY;
	GLuint trilinear_alpha;
	GLuint fog_clamp_min, fog_clamp_max;
	GLuint normal_matrix;

	//
	u32 cp_AlphaTest; s32 pp_ClipTestMode;
	u32 pp_Texture, pp_UseAlpha, pp_IgnoreTexA, pp_ShadInstr, pp_Offset, pp_FogCtrl;
	bool pp_Gouraud, pp_BumpMap;
	bool fog_clamping;
	bool trilinear;
};


struct gl_ctx
{
	struct
	{
		GLuint program;

		GLuint depth_scale;
		GLuint sp_ShaderColor;
		GLuint normal_matrix;

	} modvol_shader;

	std::unordered_map<u32, PipelineShader> shaders;

	struct
	{
		GLuint program;
		GLuint scale;
		GLuint vao;
		GLuint geometry;
		GLuint osd_tex;
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
	int gl_minor;
	bool is_gles;
	GLuint fog_image_format;
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
void findGLVersion();
void GetFramebufferScaling(float& scale_x, float& scale_y, float& scissoring_scale_x, float& scissoring_scale_y);
void GetFramebufferSize(float& dc_width, float& dc_height);
void SetupMatrices(float dc_width, float dc_height,
				   float scale_x, float scale_y, float scissoring_scale_x, float scissoring_scale_y,
				   float &ds2s_offs_x, glm::mat4& normal_mat, glm::mat4& scissor_mat);

text_info raw_GetTexture(TSP tsp, TCW tcw);
void DoCleanup();
void SetCull(u32 CullMode);
s32 SetTileClip(u32 val, GLint uniform);
void SetMVS_Mode(ModifierVolumeMode mv_mode, ISP_Modvol ispc);

void BindRTT(u32 addy, u32 fbw, u32 fbh, u32 channels, u32 fmt);
void ReadRTTBuffer();
void RenderFramebuffer();
void DrawFramebuffer();
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
	}

} ShaderUniforms;

struct TextureCacheData : BaseTextureCacheData
{
	GLuint texID;   //gl texture
	virtual std::string GetId() override { return std::to_string(texID); }
	virtual void UploadToGPU(int width, int height, u8 *temp_tex_buffer, bool mipmapped, bool mipmapsIncluded = false) override;
	virtual bool Delete() override;
};

class TextureCache : public BaseTextureCache<TextureCacheData>
{
};
extern TextureCache TexCache;

extern const u32 Zfunction[8];
extern const u32 SrcBlendGL[], DstBlendGL[];
