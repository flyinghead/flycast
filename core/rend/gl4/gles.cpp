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
#include "gl4.h"
#include "rend/gles/glcache.h"
#include "rend/transform_matrix.h"

//Fragment and vertex shaders code

static const char* VertexShaderSource = R"(#version 140
#define pp_Gouraud %d

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

/* Vertex constants*/ 
uniform vec4 scale;
uniform mat4 normal_matrix;
/* Vertex input */
in vec4 in_pos;
in vec4 in_base;
in vec4 in_offs;
in vec2 in_uv;
in vec4 in_base1;
in vec4 in_offs1;
in vec2 in_uv1;
/* output */
INTERPOLATION out vec4 vtx_base;
INTERPOLATION out vec4 vtx_offs;
			  out vec2 vtx_uv;
INTERPOLATION out vec4 vtx_base1;
INTERPOLATION out vec4 vtx_offs1;
			  out vec2 vtx_uv1;
void main()
{
	vtx_base = in_base;
	vtx_offs = in_offs;
	vtx_uv = in_uv;
	vtx_base1 = in_base1;
	vtx_offs1 = in_offs1;
	vtx_uv1 = in_uv1;
	vec4 vpos = normal_matrix * in_pos;
	
	vpos.w = 1.0 / vpos.z;
	vpos.z = vpos.w;
	vpos.xy *= vpos.w; 
	gl_Position = vpos;
}
)";

const char* gl4PixelPipelineShader = SHADER_HEADER
R"(
#define cp_AlphaTest %d
#define pp_ClipInside %d
#define pp_UseAlpha %d
#define pp_Texture %d
#define pp_IgnoreTexA %d
#define pp_ShadInstr %d
#define pp_Offset %d
#define pp_FogCtrl %d
#define pp_TwoVolumes %d
#define pp_Gouraud %d
#define pp_BumpMap %d
#define FogClamping %d
#define pp_Palette %d
#define PASS %d
#define PI 3.1415926

#define PASS_DEPTH 0
#define PASS_COLOR 1
#define PASS_OIT 2

#if PASS == PASS_DEPTH || PASS == PASS_COLOR
out vec4 FragColor;
#endif

#if pp_TwoVolumes == 1
#define IF(x) if (x)
#else
#define IF(x)
#endif

#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION smooth
#endif

/* Shader program params*/
uniform float cp_AlphaTestValue;
uniform vec4 pp_ClipTest;
uniform vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT;
uniform float sp_FOG_DENSITY;
uniform float shade_scale_factor;
uniform sampler2D tex0, tex1;
layout(binding = 5) uniform sampler2D fog_table;
uniform int pp_Number;
uniform usampler2D shadow_stencil;
uniform sampler2D DepthTex;
uniform float trilinear_alpha;
uniform vec4 fog_clamp_min;
uniform vec4 fog_clamp_max;
uniform sampler2D palette;
uniform int palette_index;

uniform ivec2 blend_mode[2];
#if pp_TwoVolumes == 1
uniform bool use_alpha[2];
uniform bool ignore_tex_alpha[2];
uniform int shading_instr[2];
uniform int fog_control[2];
#endif

/* Vertex input*/
INTERPOLATION in vec4 vtx_base;
INTERPOLATION in vec4 vtx_offs;
			  in vec2 vtx_uv;
INTERPOLATION in vec4 vtx_base1;
INTERPOLATION in vec4 vtx_offs1;
			  in vec2 vtx_uv1;

float fog_mode2(float w)
{
	float z = clamp(w * sp_FOG_DENSITY, 1.0, 255.9999);
	float exp = floor(log2(z));
	float m = z * 16.0 / pow(2.0, exp) - 16.0;
	float idx = floor(m) + exp * 16.0 + 0.5;
	vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
	return fog_coef.r;
}

vec4 fog_clamp(vec4 col)
{
#if FogClamping == 1
	return clamp(col, fog_clamp_min, fog_clamp_max);
#else
	return col;
#endif
}

#if pp_Palette == 1

vec4 palettePixel(sampler2D tex, vec2 coords)
{
	int color_idx = int(floor(texture(tex, coords).r * 255.0 + 0.5)) + palette_index;
	vec2 c = vec2(float(color_idx % 32) / 31.0, float(color_idx / 32) / 31.0);
	return texture(palette, c);
}

#endif

void main()
{
	setFragDepth();
	
	#if PASS == PASS_OIT
		// Manual depth testing
		float frontDepth = texture(DepthTex, gl_FragCoord.xy / textureSize(DepthTex, 0)).r;
		if (gl_FragDepth < frontDepth)
			discard;
	#endif
	
	// Clip inside the box
	#if pp_ClipInside == 1
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w)
			discard;
	#endif
	
	vec4 color = vtx_base;
	vec4 offset = vtx_offs;
	vec2 uv = vtx_uv;
	bool area1 = false;
	ivec2 cur_blend_mode = blend_mode[0];
	
	#if pp_TwoVolumes == 1
		bool cur_use_alpha = use_alpha[0];
		bool cur_ignore_tex_alpha = ignore_tex_alpha[0];
		int cur_shading_instr = shading_instr[0];
		int cur_fog_control = fog_control[0];
		#if PASS == PASS_COLOR
			uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0));
			if (stencil.r == 0x81u) {
				color = vtx_base1;
				offset = vtx_offs1;
				uv = vtx_uv1;
				area1 = true;
				cur_blend_mode = blend_mode[1];
				cur_use_alpha = use_alpha[1];
				cur_ignore_tex_alpha = ignore_tex_alpha[1];
				cur_shading_instr = shading_instr[1];
				cur_fog_control = fog_control[1];
			}
		#endif
	#endif
	
	#if pp_UseAlpha==0 || pp_TwoVolumes == 1
		IF(!cur_use_alpha)
			color.a=1.0;
	#endif
	#if pp_FogCtrl==3 || pp_TwoVolumes == 1 // LUT Mode 2
		IF(cur_fog_control == 3)
			color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));
	#endif
	#if pp_Texture==1
	{
		vec4 texcol;
		#if pp_Palette == 0
			if (area1)
				texcol = texture(tex1, uv);
			else
				texcol = texture(tex0, uv);
		#else
			if (area1)
				texcol = palettePixel(tex1, uv);
			else
				texcol = palettePixel(tex0, uv);
		#endif

		#if pp_BumpMap == 1
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1
				IF(cur_ignore_tex_alpha)
					texcol.a=1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (cp_AlphaTestValue>texcol.a) discard;
			#endif 
		#endif
		#if pp_ShadInstr==0 || pp_TwoVolumes == 1 // DECAL
		IF(cur_shading_instr == 0)
		{
			color=texcol;
		}
		#endif
		#if pp_ShadInstr==1 || pp_TwoVolumes == 1 // MODULATE
		IF(cur_shading_instr == 1)
		{
			color.rgb*=texcol.rgb;
			color.a=texcol.a;
		}
		#endif
		#if pp_ShadInstr==2 || pp_TwoVolumes == 1 // DECAL ALPHA
		IF(cur_shading_instr == 2)
		{
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
		}
		#endif
		#if  pp_ShadInstr==3 || pp_TwoVolumes == 1 // MODULATE ALPHA
		IF(cur_shading_instr == 3)
		{
			color*=texcol;
		}
		#endif
		
		#if pp_Offset==1 && pp_BumpMap == 0
		{
			color.rgb += offset.rgb;
		}
		#endif
	}
	#endif
	#if PASS == PASS_COLOR && pp_TwoVolumes == 0
		uvec4 stencil = texture(shadow_stencil, gl_FragCoord.xy / textureSize(shadow_stencil, 0));
		if (stencil.r == 0x81u)
			color.rgb *= shade_scale_factor;
	#endif
	
	color = fog_clamp(color);
	
	#if pp_FogCtrl==0 || pp_TwoVolumes == 1 // LUT
		IF(cur_fog_control == 0)
		{
			color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); 
		}
	#endif
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex
		IF(cur_fog_control == 1)
		{
			color.rgb=mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a);
		}
	#endif
	
	color *= trilinear_alpha;
	
	#if cp_AlphaTest == 1
		color.a=1.0;
	#endif 
	
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0);
	
	#if PASS == PASS_COLOR 
		FragColor = color;
	#elif PASS == PASS_OIT
		// Discard as many pixels as possible
		switch (cur_blend_mode.y) // DST
		{
		case ONE:
			switch (cur_blend_mode.x) // SRC
			{
				case ZERO:
					discard;
				case ONE:
				case OTHER_COLOR:
				case INVERSE_OTHER_COLOR:
					if (color == vec4(0.0))
						discard;
					break;
				case SRC_ALPHA:
					if (color.a == 0.0 || color.rgb == vec3(0.0))
						discard;
					break;
				case INVERSE_SRC_ALPHA:
					if (color.a == 1.0 || color.rgb == vec3(0.0))
						discard;
					break;
			}
			break;
		case OTHER_COLOR:
			if (cur_blend_mode.x == ZERO && color == vec4(1.0))
				discard;
			break;
		case INVERSE_OTHER_COLOR:
			if (cur_blend_mode.x <= SRC_ALPHA && color == vec4(0.0))
				discard;
			break;
		case SRC_ALPHA:
			if ((cur_blend_mode.x == ZERO || cur_blend_mode.x == INVERSE_SRC_ALPHA) && color.a == 1.0)
				discard;
			break;
		case INVERSE_SRC_ALPHA:
			switch (cur_blend_mode.x) // SRC
			{
				case ZERO:
				case SRC_ALPHA:
					if (color.a == 0.0)
						discard;
					break;
				case ONE:
				case OTHER_COLOR:
				case INVERSE_OTHER_COLOR:
					if (color == vec4(0.0))
						discard;
					break;
			}
			break;
		}
		
		ivec2 coords = ivec2(gl_FragCoord.xy);
		uint idx =  getNextPixelIndex();
		
		Pixel pixel;
		pixel.color = color;
		pixel.depth = gl_FragDepth;
		pixel.seq_num = uint(pp_Number);
		pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx);
		pixels[idx] = pixel;
		
		discard;
		
	#endif
}
)";

static const char* ModifierVolumeShader = SHADER_HEADER
R"(
void main()
{
	setFragDepth();
}
)";

gl4_ctx gl4;

struct gl4ShaderUniforms_t gl4ShaderUniforms;
int max_image_width;
int max_image_height;

bool gl4CompilePipelineShader(	gl4PipelineShader* s, const char *pixel_source /* = PixelPipelineShader */, const char *vertex_source /* = NULL */)
{
	char vshader[16384];

	sprintf(vshader, vertex_source == NULL ? VertexShaderSource : vertex_source, s->pp_Gouraud);

	char pshader[16384];

	sprintf(pshader, pixel_source,
                s->cp_AlphaTest, s->pp_InsideClipping, s->pp_UseAlpha,
                s->pp_Texture, s->pp_IgnoreTexA, s->pp_ShadInstr, s->pp_Offset, s->pp_FogCtrl,
				s->pp_TwoVolumes, s->pp_Gouraud, s->pp_BumpMap, s->fog_clamping, s->palette,
				(int)s->pass);

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
	s->normal_matrix = glGetUniformLocation(s->program, "normal_matrix");

	// Shadow stencil for OP/PT rendering pass
	gu = glGetUniformLocation(s->program, "shadow_stencil");
	if (gu != -1)
		glUniform1i(gu, 3);		// GL_TEXTURE3

	s->pp_Number = glGetUniformLocation(s->program, "pp_Number");

	s->blend_mode = glGetUniformLocation(s->program, "blend_mode");
	s->use_alpha = glGetUniformLocation(s->program, "use_alpha");
	s->ignore_tex_alpha = glGetUniformLocation(s->program, "ignore_tex_alpha");
	s->shading_instr = glGetUniformLocation(s->program, "shading_instr");
	s->fog_control = glGetUniformLocation(s->program, "fog_control");

	gu = glGetUniformLocation(s->program, "palette");
	if (gu != -1)
		glUniform1i(gu, 6);		// GL_TEXTURE6
	s->palette_index = glGetUniformLocation(s->program, "palette_index");

	return glIsProgram(s->program)==GL_TRUE;
}

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

static void gl4_term()
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
}

static void create_modvol_shader()
{
	if (gl4.modvol_shader.program != 0)
		return;
	char vshader[16384];
	sprintf(vshader, VertexShaderSource, 1);

	gl4.modvol_shader.program=gl_CompileAndLink(vshader, ModifierVolumeShader);
	gl4.modvol_shader.normal_matrix  = glGetUniformLocation(gl4.modvol_shader.program, "normal_matrix");
}

static bool gl_create_resources()
{
	if (gl4.vbo.geometry != 0)
		// Assume the resources have already been created
		return true;

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

	// Create the buffer for Translucent poly params
	glGenBuffers(1, &gl4.vbo.tr_poly_params);
	// Bind it
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl4.vbo.tr_poly_params);
	// Declare storage
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gl4.vbo.tr_poly_params);
	initQuad();
	glCheck();

	return true;
}

//setup
extern void initABuffer();
void reshapeABuffer(int width, int height);
extern void gl4CreateTextures(int width, int height);

static bool gl4_init()
{
	findGLVersion();
	if (gl.gl_major < 4 || (gl.gl_major == 4 && gl.gl_minor < 3))
	{
		WARN_LOG(RENDERER, "Warning: OpenGL version doesn't support per-pixel sorting.");
		return false;
	}
	INFO_LOG(RENDERER, "Per-pixel sorting enabled");

	glcache.DisableCache();

	if (!gl_create_resources())
		return false;

//    glEnable(GL_DEBUG_OUTPUT);
//    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
//    glDebugMessageCallback(gl_DebugOutput, NULL);
//    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

	initABuffer();

	if (config::TextureUpscale > 1)
	{
		// Trick to preload the tables used by xBRZ
		u32 src[] { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
		u32 dst[16];
		UpscalexBRZ(2, src, dst, 2, 2, false);
	}
	fog_needs_update = true;

	return true;
}

static void resize(int w, int h)
{
	if (w > max_image_width || h > max_image_height || stencilTexId == 0)
	{
		if (w > max_image_width)
			max_image_width = w;
		if (h > max_image_height)
			max_image_height = h;

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
		gl4CreateTextures(max_image_width, max_image_height);
		reshapeABuffer(max_image_width, max_image_height);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}
}

static bool RenderFrame(int width, int height)
{
	create_modvol_shader();

	const bool is_rtt = pvrrc.isRTT;

	TransformMatrix<true> matrices(pvrrc, width, height);
	gl4ShaderUniforms.normal_mat = matrices.GetNormalMatrix();
	const glm::mat4& scissor_mat = matrices.GetScissorMatrix();
	ViewportMatrix = matrices.GetViewportMatrix();

	if (!is_rtt)
		gcflip = 0;
	else
		gcflip = 1;
	
	/*
		Handle Dc to screen scaling
	*/
	int rendering_width;
	int rendering_height;
	if (is_rtt)
	{
		int scaling = config::RenderToTextureBuffer ? 1 : config::RenderToTextureUpscale;
		rendering_width = matrices.GetDreamcastViewport().x * scaling;
		rendering_height = matrices.GetDreamcastViewport().y * scaling;
	}
	else
	{
		rendering_width = width;
		rendering_height = height;
	}
	resize(rendering_width, rendering_height);
	
	//DEBUG_LOG(RENDERER, "scale: %f, %f, %f, %f", gl4ShaderUniforms.scale_coefs[0], gl4ShaderUniforms.scale_coefs[1], gl4ShaderUniforms.scale_coefs[2], gl4ShaderUniforms.scale_coefs[3]);

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
	gl4ShaderUniforms.fog_den_float = fog_den_mant * powf(2.0f,fog_den_exp) * config::ExtraDepthScale;

	gl4ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;
	
	gl4ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	gl4ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;
	
	if (fog_needs_update && config::Fog)
	{
		fog_needs_update = false;
		UpdateFogTexture((u8 *)FOG_TABLE, GL_TEXTURE5, GL_RED);
	}
	if (palette_updated)
	{
		UpdatePaletteTexture(GL_TEXTURE6);
		palette_updated = false;
	}

	glcache.UseProgram(gl4.modvol_shader.program);

	glUniformMatrix4fv(gl4.modvol_shader.normal_matrix, 1, GL_FALSE, &gl4ShaderUniforms.normal_mat[0][0]);

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
			WARN_LOG(RENDERER, "Unsupported render to texture format: %d", FB_W_CTRL.fb_packmode);
			return false;

		case 7: //7     invalid
			die("7 is not valid");
			return false;
		}
		DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d,%d -> %d,%d", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
				FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		output_fbo = gl4BindRTT(FB_W_SOF1 & VRAM_MASK, (u32)lroundf(matrices.GetDreamcastViewport().x),
				(u32)lroundf(matrices.GetDreamcastViewport().y), channels, format);
	}
	else
	{
		output_fbo = init_output_framebuffer(rendering_width, rendering_height);
	}

	glcache.Disable(GL_SCISSOR_TEST);

	if (!pvrrc.isRenderFramebuffer)
	{
		//Main VBO
		glBindBuffer(GL_ARRAY_BUFFER, gl4.vbo.geometry); glCheck();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl4.vbo.idxs); glCheck();

		//move vertex to gpu
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

		if (is_rtt || !config::Widescreen || matrices.IsClipped() || config::Rotate90)
		{
			float fWidth;
			float fHeight;
			float min_x;
			float min_y;
			if (!is_rtt)
			{
				glm::vec4 clip_min(pvrrc.fb_X_CLIP.min, pvrrc.fb_Y_CLIP.min, 0, 1);
				glm::vec4 clip_dim(pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1,
								   pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1, 0, 0);
				clip_min = scissor_mat * clip_min;
				clip_dim = scissor_mat * clip_dim;
				
				min_x = clip_min[0];
				min_y = clip_min[1];
				fWidth = clip_dim[0];
				fHeight = clip_dim[1];
				if (fWidth < 0)
				{
					min_x += fWidth;
					fWidth = -fWidth;
				}
				if (fHeight < 0)
				{
					min_y += fHeight;
					fHeight = -fHeight;
				}
				if (matrices.GetSidebarWidth() > 0)
				{
					float scaled_offs_x = matrices.GetSidebarWidth();

					glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
					glcache.Enable(GL_SCISSOR_TEST);
					glcache.Scissor(0, 0, (GLsizei)lroundf(scaled_offs_x), rendering_height);
					glClear(GL_COLOR_BUFFER_BIT);
					glcache.Scissor((GLint)lroundf(rendering_width - scaled_offs_x), 0, (GLsizei)lroundf(scaled_offs_x) + 1, rendering_height);
					glClear(GL_COLOR_BUFFER_BIT);
				}
			}
			else
			{
				fWidth = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
				fHeight = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
				min_x = pvrrc.fb_X_CLIP.min;
				min_y = pvrrc.fb_Y_CLIP.min;
				if (config::RenderToTextureUpscale > 1 && !config::RenderToTextureBuffer)
				{
					min_x *= config::RenderToTextureUpscale;
					min_y *= config::RenderToTextureUpscale;
					fWidth *= config::RenderToTextureUpscale;
					fHeight *= config::RenderToTextureUpscale;
				}
			}
			gl4ShaderUniforms.base_clipping.enabled = true;
			gl4ShaderUniforms.base_clipping.x = (int)lroundf(min_x);
			gl4ShaderUniforms.base_clipping.y = (int)lroundf(min_y);
			gl4ShaderUniforms.base_clipping.width = (int)lroundf(fWidth);
			gl4ShaderUniforms.base_clipping.height = (int)lroundf(fHeight);
			glcache.Scissor(gl4ShaderUniforms.base_clipping.x, gl4ShaderUniforms.base_clipping.y,
					gl4ShaderUniforms.base_clipping.width, gl4ShaderUniforms.base_clipping.height);
			glcache.Enable(GL_SCISSOR_TEST);
		}
		else
		{
			gl4ShaderUniforms.base_clipping.enabled = false;
		}

		gl4DrawStrips(output_fbo, rendering_width, rendering_height);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);

		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);

		DrawFramebuffer();
	}

	if (is_rtt)
		ReadRTTBuffer();
	else
		render_output_framebuffer();

	return !is_rtt;
}

void termABuffer();

struct OpenGL4Renderer : OpenGLRenderer
{
	bool Init() override
	{
		return gl4_init();
	}

	void Resize(int w, int h) override
	{
		width = w;
		height = h;
		resize(w, h);
	}

	void Term() override
	{
		termQuad();
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
		if (geom_fbo != 0)
		{
			glDeleteFramebuffers(1, &geom_fbo);
			geom_fbo = 0;
		}
		if (texSamplers[0] != 0)
		{
			glDeleteSamplers(2, texSamplers);
			texSamplers[0] = texSamplers[1] = 0;
		}
		if (depth_fbo != 0)
		{
			glDeleteFramebuffers(1, &depth_fbo);
			depth_fbo = 0;
		}
		TexCache.Clear();

		gl_free_osd_resources();
		free_output_framebuffer();
		gl4_term();
	}

	bool Render() override
	{
		RenderFrame(width, height);
		if (pvrrc.isRTT)
			return false;

		DrawOSD(false);
		frameRendered = true;

		return true;
	}

	bool RenderLastFrame() override
	{
		return render_output_framebuffer();
	}
};

Renderer* rend_GL4()
{
	return new OpenGL4Renderer();
}
