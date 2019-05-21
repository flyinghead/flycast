#include <math.h>
#include "gl4.h"
#include "rend/gles/glcache.h"
#include "rend/TexCache.h"
#include "cfg/cfg.h"

#include "oslib/oslib.h"
#include "rend/rend.h"
#include "rend/gui.h"

//Fragment and vertex shaders code

static const char* VertexShaderSource =
"\
#version 140 \n\
#define pp_Gouraud %d \n\
#define ROTATE_90 %d \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp float     extra_depth_scale; \n\
/* Vertex input */ \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in lowp vec4     in_offs; \n\
in mediump vec2  in_uv; \n\
in lowp vec4     in_base1; \n\
in lowp vec4     in_offs1; \n\
in mediump vec2  in_uv1; \n\
/* output */ \n\
INTERPOLATION out lowp vec4 vtx_base; \n\
INTERPOLATION out lowp vec4 vtx_offs; \n\
			  out mediump vec2 vtx_uv; \n\
INTERPOLATION out lowp vec4 vtx_base1; \n\
INTERPOLATION out lowp vec4 vtx_offs1; \n\
			  out mediump vec2 vtx_uv1; \n\
void main() \n\
{ \n\
	vtx_base=in_base; \n\
	vtx_offs=in_offs; \n\
	vtx_uv=in_uv; \n\
	vtx_base1 = in_base1; \n\
	vtx_offs1 = in_offs1; \n\
	vtx_uv1 = in_uv1; \n\
	vec4 vpos=in_pos; \n\
	if (vpos.z < 0.0 || vpos.z > 3.4e37) \n\
	{ \n\
		gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z); \n\
		return; \n\
	} \n\
	\n\
	vpos.w = extra_depth_scale / vpos.z; \n\
	vpos.z = vpos.w; \n\
#if ROTATE_90 == 1 \n\
	vpos.xy = vec2(vpos.y, -vpos.x);  \n\
#endif \n\
	vpos.xy=vpos.xy*scale.xy-scale.zw;  \n\
	vpos.xy*=vpos.w;  \n\
	gl_Position = vpos; \n\
}";

const char* gl4PixelPipelineShader = SHADER_HEADER
"\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
#define pp_TwoVolumes %d \n\
#define pp_DepthFunc %d \n\
#define pp_Gouraud %d \n\
#define pp_BumpMap %d \n\
#define FogClamping %d \n\
#define PASS %d \n\
#define PI 3.1415926 \n\
 \n\
#if PASS <= 1 \n\
out vec4 FragColor; \n\
#endif \n\
 \n\
#if pp_TwoVolumes == 1 \n\
#define IF(x) if (x) \n\
#else \n\
#define IF(x) \n\
#endif \n\
 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
 \n\
/* Shader program params*/ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp float sp_FOG_DENSITY; \n\
uniform highp float shade_scale_factor; \n\
uniform sampler2D tex0, tex1; \n\
layout(binding = 5) uniform sampler2D fog_table; \n\
uniform int pp_Number; \n\
uniform usampler2D shadow_stencil; \n\
uniform sampler2D DepthTex; \n\
uniform lowp float trilinear_alpha; \n\
uniform lowp vec4 fog_clamp_min; \n\
uniform lowp vec4 fog_clamp_max; \n\
 \n\
uniform ivec2 blend_mode[2]; \n\
#if pp_TwoVolumes == 1 \n\
uniform bool use_alpha[2]; \n\
uniform bool ignore_tex_alpha[2]; \n\
uniform int shading_instr[2]; \n\
uniform int fog_control[2]; \n\
#endif \n\
 \n\
uniform highp float extra_depth_scale; \n\
/* Vertex input*/ \n\
INTERPOLATION in lowp vec4 vtx_base; \n\
INTERPOLATION in lowp vec4 vtx_offs; \n\
			  in mediump vec2 vtx_uv; \n\
INTERPOLATION in lowp vec4 vtx_base1; \n\
INTERPOLATION in lowp vec4 vtx_offs1; \n\
			  in mediump vec2 vtx_uv1; \n\
 \n\
lowp float fog_mode2(highp float w) \n\
{ \n\
	highp float z = clamp(w * extra_depth_scale * sp_FOG_DENSITY, 1.0, 255.9999); \n\
	highp float exp = floor(log2(z)); \n\
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0; \n\
	float idx = floor(m) + exp * 16.0 + 0.5; \n\
	vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0)); \n\
	return fog_coef.r; \n\
} \n\
 \n\
highp vec4 fog_clamp(highp vec4 col) \n\
{ \n\
#if FogClamping == 1 \n\
	return clamp(col, fog_clamp_min, fog_clamp_max); \n\
#else \n\
	return col; \n\
#endif \n\
} \n\
 \n\
void main() \n\
{ \n\
	setFragDepth(); \n\
	\n\
	#if PASS == 3 \n\
		// Manual depth testing \n\
		highp float frontDepth = texture(DepthTex, gl_FragCoord.xy / textureSize(DepthTex, 0)).r; \n\
		#if pp_DepthFunc == 0		// Never \n\
			discard; \n\
		#elif pp_DepthFunc == 1		// Greater \n\
			if (gl_FragDepth <= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 2		// Equal \n\
			if (gl_FragDepth != frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 3		// Greater or equal \n\
			if (gl_FragDepth < frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 4		// Less \n\
			if (gl_FragDepth >= frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 5		// Not equal \n\
			if (gl_FragDepth == frontDepth) \n\
				discard; \n\
		#elif pp_DepthFunc == 6		// Less or equal \n\
			if (gl_FragDepth > frontDepth) \n\
				discard; \n\
		#endif \n\
	#endif \n\
	\n\
	// Clip outside the box \n\
	#if pp_ClipTestMode==1 \n\
		if (gl_FragCoord.x < pp_ClipTest.x || gl_FragCoord.x > pp_ClipTest.z \n\
				|| gl_FragCoord.y < pp_ClipTest.y || gl_FragCoord.y > pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	// Clip inside the box \n\
	#if pp_ClipTestMode==-1 \n\
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z \n\
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w) \n\
			discard; \n\
	#endif \n\
	\n\
	highp vec4 color = vtx_base; \n\
	lowp vec4 offset = vtx_offs; \n\
	mediump vec2 uv = vtx_uv; \n\
	bool area1 = false; \n\
	ivec2 cur_blend_mode = blend_mode[0]; \n\
	\n\
	#if pp_TwoVolumes == 1 \n\
		bool cur_use_alpha = use_alpha[0]; \n\
		bool cur_ignore_tex_alpha = ignore_tex_alpha[0]; \n\
		int cur_shading_instr = shading_instr[0]; \n\
		int cur_fog_control = fog_control[0]; \n\
		#if PASS == 1 \n\
			uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
			if (stencil.r == 0x81u) { \n\
				color = vtx_base1; \n\
				offset = vtx_offs1; \n\
				uv = vtx_uv1; \n\
				area1 = true; \n\
				cur_blend_mode = blend_mode[1]; \n\
				cur_use_alpha = use_alpha[1]; \n\
				cur_ignore_tex_alpha = ignore_tex_alpha[1]; \n\
				cur_shading_instr = shading_instr[1]; \n\
				cur_fog_control = fog_control[1]; \n\
			} \n\
		#endif\n\
	#endif\n\
	\n\
	#if pp_UseAlpha==0 || pp_TwoVolumes == 1 \n\
		IF(!cur_use_alpha) \n\
			color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 || pp_TwoVolumes == 1 // LUT Mode 2 \n\
		IF(cur_fog_control == 3) \n\
			color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
		highp vec4 texcol; \n\
		if (area1) \n\
			texcol = texture(tex1, uv); \n\
		else \n\
			texcol = texture(tex0, uv); \n\
		#if pp_BumpMap == 1 \n\
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0; \n\
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0; \n\
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0); \n\
			texcol.rgb = vec3(1.0, 1.0, 1.0);	 \n\
		#else\n\
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1 \n\
				IF(cur_ignore_tex_alpha) \n\
					texcol.a=1.0;	 \n\
			#endif\n\
			\n\
			#if cp_AlphaTest == 1 \n\
				if (cp_AlphaTestValue>texcol.a) discard;\n\
			#endif  \n\
		#endif\n\
		#if pp_ShadInstr==0 || pp_TwoVolumes == 1 // DECAL \n\
		IF(cur_shading_instr == 0) \n\
		{ \n\
			color=texcol; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==1 || pp_TwoVolumes == 1 // MODULATE \n\
		IF(cur_shading_instr == 1) \n\
		{ \n\
			color.rgb*=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==2 || pp_TwoVolumes == 1 // DECAL ALPHA \n\
		IF(cur_shading_instr == 2) \n\
		{ \n\
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a); \n\
		} \n\
		#endif\n\
		#if  pp_ShadInstr==3 || pp_TwoVolumes == 1 // MODULATE ALPHA \n\
		IF(cur_shading_instr == 3) \n\
		{ \n\
			color*=texcol; \n\
		} \n\
		#endif\n\
		\n\
		#if pp_Offset==1 && pp_BumpMap == 0 \n\
		{ \n\
			color.rgb += offset.rgb; \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	#if PASS == 1 && pp_TwoVolumes == 0 \n\
		uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0)); \n\
		if (stencil.r == 0x81u) \n\
			color.rgb *= shade_scale_factor; \n\
	#endif \n\
	 \n\
	color = fog_clamp(color); \n\
	 \n\
	#if pp_FogCtrl==0 || pp_TwoVolumes == 1 // LUT \n\
		IF(cur_fog_control == 0) \n\
		{ \n\
			color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
		} \n\
	#endif\n\
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex \n\
		IF(cur_fog_control == 1) \n\
		{ \n\
			color.rgb=mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a); \n\
		} \n\
	#endif\n\
	 \n\
	color *= trilinear_alpha; \n\
	 \n\
	#if cp_AlphaTest == 1 \n\
		color.a=1.0; \n\
	#endif  \n\
	\n\
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0); \n\
	\n\
	#if PASS == 1  \n\
		FragColor = color; \n\
	#elif PASS > 1 \n\
		// Discard as many pixels as possible \n\
		switch (cur_blend_mode.y) // DST \n\
		{ \n\
		case ONE: \n\
			switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
					discard; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
					if (color == vec4(0.0)) \n\
						discard; \n\
					break; \n\
				case SRC_ALPHA: \n\
					if (color.a == 0.0 || color.rgb == vec3(0.0)) \n\
						discard; \n\
					break; \n\
				case INVERSE_SRC_ALPHA: \n\
					if (color.a == 1.0 || color.rgb == vec3(0.0)) \n\
						discard; \n\
					break; \n\
			} \n\
			break; \n\
		case OTHER_COLOR: \n\
			if (cur_blend_mode.x == ZERO && color == vec4(1.0)) \n\
				discard; \n\
			break; \n\
		case INVERSE_OTHER_COLOR: \n\
			if (cur_blend_mode.x <= SRC_ALPHA && color == vec4(0.0)) \n\
				discard; \n\
			break; \n\
		case SRC_ALPHA: \n\
			if ((cur_blend_mode.x == ZERO || cur_blend_mode.x == INVERSE_SRC_ALPHA) && color.a == 1.0) \n\
				discard; \n\
			break; \n\
		case INVERSE_SRC_ALPHA: \n\
			switch (cur_blend_mode.x) // SRC \n\
			{ \n\
				case ZERO: \n\
				case SRC_ALPHA: \n\
					if (color.a == 0.0) \n\
						discard; \n\
					break; \n\
				case ONE: \n\
				case OTHER_COLOR: \n\
				case INVERSE_OTHER_COLOR: \n\
					if (color == vec4(0.0)) \n\
						discard; \n\
					break; \n\
			} \n\
			break; \n\
		} \n\
		\n\
		ivec2 coords = ivec2(gl_FragCoord.xy); \n\
		uint idx =  getNextPixelIndex(); \n\
		 \n\
		Pixel pixel; \n\
		pixel.color = color; \n\
		pixel.depth = gl_FragDepth; \n\
		pixel.seq_num = uint(pp_Number); \n\
		pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx); \n\
		pixels[idx] = pixel; \n\
		\n\
		discard; \n\
		\n\
	#endif \n\
}";

static const char* ModifierVolumeShader = SHADER_HEADER
" \
/* Vertex input*/ \n\
void main() \n\
{ \n\
	setFragDepth(); \n\
	\n\
}";

gl4_ctx gl4;

struct gl4ShaderUniforms_t gl4ShaderUniforms;

bool gl4CompilePipelineShader(	gl4PipelineShader* s, bool rotate_90, const char *source /* = PixelPipelineShader */)
{
	char vshader[16384];

	sprintf(vshader, VertexShaderSource, s->pp_Gouraud, rotate_90);

	char pshader[16384];

	sprintf(pshader, source,
                s->cp_AlphaTest,s->pp_ClipTestMode,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl, s->pp_TwoVolumes, s->pp_DepthFunc, s->pp_Gouraud, s->pp_BumpMap, s->fog_clamping, s->pass);

	s->program = gl_CompileAndLink(vshader, pshader);

	//setup texture 0 as the input for the shader
	GLint gu = glGetUniformLocation(s->program, "tex0");
	if (s->pp_Texture == 1 && gu != -1)
		glUniform1i(gu, 0);
	// Setup texture 1 as the input for area 1 in two volume mode
	gu = glGetUniformLocation(s->program, "tex1");
	if (s->pp_Texture == 1 && gu != -1)
		glUniform1i(gu, 1);

	//get the uniform locations
	s->scale	            = glGetUniformLocation(s->program, "scale");
	s->extra_depth_scale = glGetUniformLocation(s->program, "extra_depth_scale");

	s->pp_ClipTest      = glGetUniformLocation(s->program, "pp_ClipTest");

	s->sp_FOG_DENSITY   = glGetUniformLocation(s->program, "sp_FOG_DENSITY");

	s->cp_AlphaTestValue= glGetUniformLocation(s->program, "cp_AlphaTestValue");

	//FOG_COL_RAM,FOG_COL_VERT,FOG_DENSITY;
	if (s->pp_FogCtrl==1 && s->pp_Texture==1)
		s->sp_FOG_COL_VERT=glGetUniformLocation(s->program, "sp_FOG_COL_VERT");
	else
		s->sp_FOG_COL_VERT=-1;
	if (s->pp_FogCtrl==0 || s->pp_FogCtrl==3)
	{
		s->sp_FOG_COL_RAM=glGetUniformLocation(s->program, "sp_FOG_COL_RAM");
	}
	else
	{
		s->sp_FOG_COL_RAM=-1;
	}
	s->shade_scale_factor = glGetUniformLocation(s->program, "shade_scale_factor");

	// Use texture 1 for depth texture
	gu = glGetUniformLocation(s->program, "DepthTex");
	if (gu != -1)
		glUniform1i(gu, 2);		// GL_TEXTURE2

	s->trilinear_alpha = glGetUniformLocation(s->program, "trilinear_alpha");
	
	if (s->fog_clamping)
	{
		s->fog_clamp_min = glGetUniformLocation(s->program, "fog_clamp_min");
		s->fog_clamp_max = glGetUniformLocation(s->program, "fog_clamp_max");
	}
	else
	{
		s->fog_clamp_min = -1;
		s->fog_clamp_max = -1;
	}

	// Shadow stencil for OP/PT rendering pass
	gu = glGetUniformLocation(s->program, "shadow_stencil");
	if (gu != -1)
		glUniform1i(gu, 3);		// GL_TEXTURE3

	s->pp_Number = glGetUniformLocation(s->program, "pp_Number");
	s->pp_DepthFunc = glGetUniformLocation(s->program, "pp_DepthFunc");

	s->blend_mode = glGetUniformLocation(s->program, "blend_mode");
	s->use_alpha = glGetUniformLocation(s->program, "use_alpha");
	s->ignore_tex_alpha = glGetUniformLocation(s->program, "ignore_tex_alpha");
	s->shading_instr = glGetUniformLocation(s->program, "shading_instr");
	s->fog_control = glGetUniformLocation(s->program, "fog_control");

	return glIsProgram(s->program)==GL_TRUE;
}

void gl_term();

void gl4_delete_shaders()
{
	for (auto it : gl4.shaders)
	{
		if (it.second.program != 0)
			glcache.DeleteProgram(it.second.program);
	}
	gl4.shaders.clear();
	glcache.DeleteProgram(gl4.modvol_shader.program);
	gl4.modvol_shader.program = 0;
}

static void gles_term(void)
{
	glDeleteBuffers(1, &gl4.vbo.geometry);
	gl4.vbo.geometry = 0;
	glDeleteBuffers(1, &gl4.vbo.modvols);
	glDeleteBuffers(1, &gl4.vbo.idxs);
	glDeleteBuffers(1, &gl4.vbo.idxs2);
	glDeleteBuffers(1, &gl4.vbo.tr_poly_params);
	gl4_delete_shaders();
	glDeleteVertexArrays(1, &gl4.vbo.main_vao);
	glDeleteVertexArrays(1, &gl4.vbo.modvol_vao);

	gl_term();
}

static void create_modvol_shader()
{
	if (gl4.modvol_shader.program != 0)
		return;
	char vshader[16384];
	sprintf(vshader, VertexShaderSource, 1, settings.rend.Rotate90);

	gl4.modvol_shader.program=gl_CompileAndLink(vshader, ModifierVolumeShader);
	gl4.modvol_shader.scale          = glGetUniformLocation(gl4.modvol_shader.program, "scale");
	gl4.modvol_shader.extra_depth_scale = glGetUniformLocation(gl4.modvol_shader.program, "extra_depth_scale");
}

static bool gl_create_resources()
{
	if (gl4.vbo.geometry != 0)
		// Assume the resources have already been created
		return true;

	findGLVersion();

	//create vao
	glGenVertexArrays(1, &gl4.vbo.main_vao);
	glGenVertexArrays(1, &gl4.vbo.modvol_vao);

	//create vbos
	glGenBuffers(1, &gl4.vbo.geometry);
	glGenBuffers(1, &gl4.vbo.modvols);
	glGenBuffers(1, &gl4.vbo.idxs);
	glGenBuffers(1, &gl4.vbo.idxs2);

	gl4SetupMainVBO();
	gl4SetupModvolVBO();

	create_modvol_shader();

	gl_load_osd_resources();

	gui_init();

	// Create the buffer for Translucent poly params
	glGenBuffers(1, &gl4.vbo.tr_poly_params);
	// Bind it
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl4.vbo.tr_poly_params);
	// Declare storage
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gl4.vbo.tr_poly_params);
	glCheck();

	return true;
}

//setup
extern void initABuffer();

static bool gles_init()
{

	if (!gl_init((void*)libPvr_GetRenderTarget(),
		         (void*)libPvr_GetRenderSurface()))
			return false;

	int major = 0;
	int minor = 0;
	glGetIntegerv(GL_MAJOR_VERSION, &major);
	glGetIntegerv(GL_MINOR_VERSION, &minor);
	if (major < 4 || (major == 4 && minor < 3))
	{
		printf("Warning: OpenGL version doesn't support per-pixel sorting.\n");
		return false;
	}
	printf("Per-pixel sorting enabled\n");

	glcache.DisableCache();

	if (!gl_create_resources())
		return false;

//    glEnable(GL_DEBUG_OUTPUT);
//    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
//    glDebugMessageCallback(gl_DebugOutput, NULL);
//    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);


	//clean up the buffer
	glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	gl_swap();

	initABuffer();

	if (settings.rend.TextureUpscale > 1)
	{
		// Trick to preload the tables used by xBRZ
		u32 src[] { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
		u32 dst[16];
		UpscalexBRZ(2, src, dst, 2, 2, false);
	}
	fog_needs_update = true;

	return true;
}

static bool RenderFrame()
{
	static int old_screen_width, old_screen_height, old_screen_scaling;
	if (screen_width != old_screen_width || screen_height != old_screen_height || settings.rend.ScreenScaling != old_screen_scaling) {
		rend_resize(screen_width, screen_height);
		old_screen_width = screen_width;
		old_screen_height = screen_height;
		old_screen_scaling = settings.rend.ScreenScaling;
	}
	DoCleanup();
	create_modvol_shader();

	bool is_rtt=pvrrc.isRTT;

	//if (FrameCount&7) return;

	//these should be adjusted based on the current PVR scaling etc params
	float dc_width=640;
	float dc_height=480;

	if (!is_rtt)
	{
		gcflip=0;
	}
	else
	{
		gcflip=1;

		//For some reason this produces wrong results
		//so for now its hacked based like on the d3d code
		/*
		u32 pvr_stride=(FB_W_LINESTRIDE.stride)*8;
		*/
		dc_width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
		dc_height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
	}

	scale_x = 1;
	scale_y = 1;

	float scissoring_scale_x = 1;

	if (!is_rtt && !pvrrc.isRenderFramebuffer)
	{
		scale_x=fb_scale_x;
		scale_y=fb_scale_y;
		if (SCALER_CTL.interlace == 0 && SCALER_CTL.vscalefactor >= 0x400)
			scale_y *= SCALER_CTL.vscalefactor / 0x400;

		//work out scaling parameters !
		//Pixel doubling is on VO, so it does not affect any pixel operations
		//A second scaling is used here for scissoring
		if (VO_CONTROL.pixel_double)
		{
			scissoring_scale_x = 0.5f;
			scale_x *= 0.5f;
		}

		if (SCALER_CTL.hscale)
		{
			scissoring_scale_x /= 2;
			scale_x*=2;
		}
	}

	dc_width  *= scale_x;
	dc_height *= scale_y;

	/*
		Handle Dc to screen scaling
	*/
	float screen_scaling = settings.rend.ScreenScaling / 100.f;
	float screen_stretching = settings.rend.ScreenStretching / 100.f;

	float dc2s_scale_h;
	float ds2s_offs_x;

	if (is_rtt)
	{
		gl4ShaderUniforms.scale_coefs[0] = 2.0f / dc_width;
		gl4ShaderUniforms.scale_coefs[1] = 2.0f / dc_height;	// FIXME CT2 needs 480 here instead of dc_height=512
		gl4ShaderUniforms.scale_coefs[2] = 1;
		gl4ShaderUniforms.scale_coefs[3] = 1;
	}
	else
	{
		if (settings.rend.Rotate90)
		{
			dc2s_scale_h = screen_height / 640.0;
			ds2s_offs_x =  (screen_width - dc2s_scale_h * 480.0 * screen_stretching) / 2;
			gl4ShaderUniforms.scale_coefs[0] = 2.0f / (screen_width / dc2s_scale_h * scale_x) * screen_stretching;
			gl4ShaderUniforms.scale_coefs[1] = -2.0f / dc_width;
			gl4ShaderUniforms.scale_coefs[2] = 1 - 2 * ds2s_offs_x / screen_width;
			gl4ShaderUniforms.scale_coefs[3] = 1;
		}
		else
		{
			dc2s_scale_h = screen_height / 480.0;
			ds2s_offs_x =  (screen_width - dc2s_scale_h * 640.0 * screen_stretching) / 2;
			//-1 -> too much to left
			gl4ShaderUniforms.scale_coefs[0] = 2.0f / (screen_width / dc2s_scale_h * scale_x) * screen_stretching;
			gl4ShaderUniforms.scale_coefs[1] = -2.0f / dc_height;
			gl4ShaderUniforms.scale_coefs[2] = 1 - 2 * ds2s_offs_x / screen_width;
			gl4ShaderUniforms.scale_coefs[3] = -1;
		}
	}

	gl4ShaderUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

	//printf("scale: %f, %f, %f, %f\n",gl4ShaderUniforms.scale_coefs[0],gl4ShaderUniforms.scale_coefs[1],gl4ShaderUniforms.scale_coefs[2],gl4ShaderUniforms.scale_coefs[3]);

	//VERT and RAM fog color constants
	u8* fog_colvert_bgra=(u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra=(u8*)&FOG_COL_RAM;
	gl4ShaderUniforms.ps_FOG_COL_VERT[0]=fog_colvert_bgra[2]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_VERT[1]=fog_colvert_bgra[1]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_VERT[2]=fog_colvert_bgra[0]/255.0f;

	gl4ShaderUniforms.ps_FOG_COL_RAM[0]=fog_colram_bgra [2]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_RAM[1]=fog_colram_bgra [1]/255.0f;
	gl4ShaderUniforms.ps_FOG_COL_RAM[2]=fog_colram_bgra [0]/255.0f;

	//Fog density constant
	u8* fog_density=(u8*)&FOG_DENSITY;
	float fog_den_mant=fog_density[1]/128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp=(s8)fog_density[0];
	gl4ShaderUniforms.fog_den_float=fog_den_mant*powf(2.0f,fog_den_exp);

	gl4ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;
	
	gl4ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;
	
	if (fog_needs_update && settings.rend.Fog)
	{
		fog_needs_update = false;
		UpdateFogTexture((u8 *)FOG_TABLE, GL_TEXTURE5, GL_RED);
	}

	glcache.UseProgram(gl4.modvol_shader.program);

	glUniform4fv( gl4.modvol_shader.scale, 1, gl4ShaderUniforms.scale_coefs);

	glUniform1f(gl4.modvol_shader.extra_depth_scale, gl4ShaderUniforms.extra_depth_scale);

	gl4ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	GLuint output_fbo;

	//setup render target first
	if (is_rtt)
	{
		GLuint channels,format;
		switch(FB_W_CTRL.fb_packmode)
		{
		case 0: //0x0   0555 KRGB 16 bit  (default)	Bit 15 is the value of fb_kval[7].
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 1: //0x1   565 RGB 16 bit
			channels=GL_RGB;
			format=GL_UNSIGNED_SHORT_5_6_5;
			break;

		case 2: //0x2   4444 ARGB 16 bit
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 3://0x3    1555 ARGB 16 bit    The alpha value is determined by comparison with the value of fb_alpha_threshold.
			channels=GL_RGBA;
			format=GL_UNSIGNED_BYTE;
			break;

		case 4: //0x4   888 RGB 24 bit packed
		case 5: //0x5   0888 KRGB 32 bit    K is the value of fk_kval.
		case 6: //0x6   8888 ARGB 32 bit
			fprintf(stderr, "Unsupported render to texture format: %d\n", FB_W_CTRL.fb_packmode);
			return false;

		case 7: //7     invalid
			die("7 is not valid");
			break;
		}
		//printf("RTT packmode=%d stride=%d - %d,%d -> %d,%d\n", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
		//		FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		output_fbo = gl4BindRTT(FB_W_SOF1 & VRAM_MASK, dc_width, dc_height, channels, format);
	}
	else
	{
		if (settings.rend.ScreenScaling != 100 || gl.swap_buffer_not_preserved)
		{
			output_fbo = init_output_framebuffer(screen_width * screen_scaling + 0.5f, screen_height * screen_scaling + 0.5f);
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER,0);
			glViewport(0, 0, screen_width, screen_height);
			output_fbo = 0;
		}
	}

	bool wide_screen_on = !is_rtt && settings.rend.WideScreen
			&& pvrrc.fb_X_CLIP.min == 0
			&& (pvrrc.fb_X_CLIP.max + 1) / scale_x == 640
			&& pvrrc.fb_Y_CLIP.min == 0
			&& (pvrrc.fb_Y_CLIP.max + 1) / scale_y == 480;

	//Color is cleared by the background plane

	glcache.Disable(GL_SCISSOR_TEST);

	//move vertex to gpu

	if (!pvrrc.isRenderFramebuffer)
	{
		//Main VBO
		glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.geometry); glCheck();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl4.vbo.idxs); glCheck();

		glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW); glCheck();

		glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW);

		//Modvol VBO
		if (pvrrc.modtrig.used())
		{
			glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.modvols); glCheck();
			glBufferData(GL_ARRAY_BUFFER,pvrrc.modtrig.bytes(),pvrrc.modtrig.head(),GL_STREAM_DRAW); glCheck();
		}

		// TR PolyParam data
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl4.vbo.tr_poly_params);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(struct PolyParam) * pvrrc.global_param_tr.used(), pvrrc.global_param_tr.head(), GL_STATIC_DRAW);
		glCheck();

		//not all scaling affects pixel operations, scale to adjust for that
		scale_x *= scissoring_scale_x;

		#if 0
			//handy to debug really stupid render-not-working issues ...
			printf("SS: %dx%d\n", screen_width, screen_height);
			printf("SCI: %d, %f\n", pvrrc.fb_X_CLIP.max, dc2s_scale_h);
			printf("SCI: %f, %f, %f, %f\n", offs_x+pvrrc.fb_X_CLIP.min/scale_x,(pvrrc.fb_Y_CLIP.min/scale_y)*dc2s_scale_h,(pvrrc.fb_X_CLIP.max-pvrrc.fb_X_CLIP.min+1)/scale_x*dc2s_scale_h,(pvrrc.fb_Y_CLIP.max-pvrrc.fb_Y_CLIP.min+1)/scale_y*dc2s_scale_h);
		#endif

		if (!wide_screen_on)
		{
			float width = (pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1) / scale_x;
			float height = (pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1) / scale_y;
			float min_x = pvrrc.fb_X_CLIP.min / scale_x;
			float min_y = pvrrc.fb_Y_CLIP.min / scale_y;
			if (!is_rtt)
			{
				if (SCALER_CTL.interlace && SCALER_CTL.vscalefactor >= 0x400)
				{
					// Clipping is done after scaling/filtering so account for that if enabled
					height *= SCALER_CTL.vscalefactor / 0x400;
					min_y *= SCALER_CTL.vscalefactor / 0x400;
				}
				if (settings.rend.Rotate90)
				{
					float t = width;
					width = height;
					height = t;
					t = min_x;
					min_x = min_y;
					min_y = 640 - t - height;
				}
				// Add x offset for aspect ratio > 4/3
            	min_x = (min_x * dc2s_scale_h * screen_stretching + ds2s_offs_x) * screen_scaling;
				// Invert y coordinates when rendering to screen
				min_y = (screen_height - (min_y + height) * dc2s_scale_h) * screen_scaling;
				width *= dc2s_scale_h * screen_stretching * screen_scaling;
				height *= dc2s_scale_h * screen_scaling;

				if (ds2s_offs_x > 0)
				{
					float scaled_offs_x = ds2s_offs_x * screen_scaling;

					glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
					glcache.Enable(GL_SCISSOR_TEST);
					glScissor(0, 0, scaled_offs_x + 0.5f, screen_height * screen_scaling + 0.5f);
					glClear(GL_COLOR_BUFFER_BIT);
					glScissor(screen_width * screen_scaling - scaled_offs_x + 0.5f, 0, scaled_offs_x + 1.f, screen_height * screen_scaling + 0.5f);
					glClear(GL_COLOR_BUFFER_BIT);
				}
			}
			else if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
			{
				min_x *= settings.rend.RenderToTextureUpscale;
				min_y *= settings.rend.RenderToTextureUpscale;
				width *= settings.rend.RenderToTextureUpscale;
				height *= settings.rend.RenderToTextureUpscale;
			}

			glScissor(min_x + 0.5f, min_y + 0.5f, width + 0.5f, height + 0.5f);
			glcache.Enable(GL_SCISSOR_TEST);
		}

		//restore scale_x
		scale_x /= scissoring_scale_x;
		gl4DrawStrips(output_fbo);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);

		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);

		gl4DrawFramebuffer(dc_width, dc_height);
	}

	eglCheck();

	KillTex=false;

	if (is_rtt)
		ReadRTTBuffer();
	else if (settings.rend.ScreenScaling != 100 || gl.swap_buffer_not_preserved)
		gl4_render_output_framebuffer();

	return !is_rtt;
}

void reshapeABuffer(int w, int h);
void termABuffer();

struct gl4rend : Renderer
{
	bool Init() { return gles_init(); }
	void Resize(int w, int h)
	{
		screen_width=w;
		screen_height=h;
		if (stencilTexId != 0)
		{
			glcache.DeleteTextures(1, &stencilTexId);
			stencilTexId = 0;
		}
		if (depthTexId != 0)
		{
			glcache.DeleteTextures(1, &depthTexId);
			depthTexId = 0;
		}
		if (opaqueTexId != 0)
		{
			glcache.DeleteTextures(1, &opaqueTexId);
			opaqueTexId = 0;
		}
		if (depthSaveTexId != 0)
		{
			glcache.DeleteTextures(1, &depthSaveTexId);
			depthSaveTexId = 0;
		}
		reshapeABuffer(w, h);
	}
	void Term()
	{
		termABuffer();
	   if (stencilTexId != 0)
	   {
		  glcache.DeleteTextures(1, &stencilTexId);
		  stencilTexId = 0;
	   }
	   if (depthTexId != 0)
	   {
		  glcache.DeleteTextures(1, &depthTexId);
		  depthTexId = 0;
	   }
	   if (opaqueTexId != 0)
	   {
		  glcache.DeleteTextures(1, &opaqueTexId);
		  opaqueTexId = 0;
	   }
	   if (depthSaveTexId != 0)
	   {
		  glcache.DeleteTextures(1, &depthSaveTexId);
		  depthSaveTexId = 0;
	   }
	   if (KillTex)
		  killtex();

	   CollectCleanup();

	   gl_free_osd_resources();
	   free_output_framebuffer();
	   gles_term();
	}

	bool Process(TA_context* ctx) { return ProcessFrame(ctx); }
	bool Render() { return RenderFrame(); }
	bool RenderLastFrame() { return gl4_render_output_framebuffer(); }

	void Present() { gl_swap(); }

	void DrawOSD(bool clear_screen)
	{
		glBindVertexArray(gl4.vbo.main_vao);
		glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.geometry); glCheck();

		OSD_DRAW(clear_screen);
	}

	virtual u32 GetTexture(TSP tsp, TCW tcw) {
		return gl_GetTexture(tsp, tcw);
	}
};

Renderer* rend_GL4() { return new gl4rend(); }
