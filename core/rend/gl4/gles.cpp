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
#include "rend/osd.h"
#include "glsl.h"
#include "gl4naomi2.h"
#include "rend/gles/naomi2.h"

#ifdef LIBRETRO
#include "rend/gles/postprocess.h"
#endif

#include <memory>

//Fragment and vertex shaders code

const char* ShaderHeader = R"(
layout(r32ui, binding = 4) uniform coherent restrict uimage2D abufferPointerImg;

layout(binding = 0, offset = 0) uniform atomic_uint buffer_index;
)"
OIT_POLY_PARAM
R"(
layout (binding = 0, std430) coherent restrict buffer PixelBuffer {
	Pixel pixels[];
};

uint getNextPixelIndex()
{
	uint index = atomicCounterIncrement(buffer_index);
	if (index >= pixels.length())
		// Buffer overflow
		discard;

	return index;
}

layout (binding = 1, std430) readonly buffer TrPolyParamBuffer {
	PolyParam tr_poly_params[];
};
)";

static const char* VertexShaderSource = R"(
#if pp_Gouraud == 0
#define INTERPOLATION flat
#else
#define INTERPOLATION
#endif

// Uniforms 
uniform mat4 ndcMat;
uniform int pp_Number;

// Input
in vec4 in_pos;
in vec4 in_base;
in vec4 in_offs;
in vec2 in_uv;
#if pp_TwoVolumes == 1
in vec4 in_base1;
in vec4 in_offs1;
in vec2 in_uv1;
#endif

// Output
INTERPOLATION out vec4 vtx_base;
INTERPOLATION out vec4 vtx_offs;
out vec3 vtx_uv;
#if pp_TwoVolumes == 1
INTERPOLATION out vec4 vtx_base1;
INTERPOLATION out vec4 vtx_offs1;
out vec2 vtx_uv1;
#endif
flat out uint vtx_index;

void main()
{
	vec4 vpos = ndcMat * in_pos;
	#if DIV_POS_Z == 1
		vpos /= vpos.z;
		vpos.z = vpos.w;
	#endif
	vtx_base = in_base;
	vtx_offs = in_offs;
	vtx_uv = vec3(in_uv, vpos.z);
	#if pp_TwoVolumes == 1
		vtx_base1 = in_base1;
		vtx_offs1 = in_offs1;
		vtx_uv1 = in_uv1;
	#endif
	vtx_index = uint(pp_Number) + uint(gl_VertexID);
	#if pp_Gouraud == 1 && DIV_POS_Z != 1
		vtx_base *= vpos.z;
		vtx_offs *= vpos.z;
		#if pp_TwoVolumes == 1
			vtx_base1 *= vpos.z;
			vtx_offs1 *= vpos.z;
		#endif
	#endif
	
	#if DIV_POS_Z != 1
		vtx_uv.xy *= vpos.z;
		#if pp_TwoVolumes == 1
			vtx_uv1 *= vpos.z;
		#endif
		vpos.w = 1.0;
		vpos.z = 0.0;
	#endif
	gl_Position = vpos;
}
)";

const char* gl4PixelPipelineShader = R"(
#define PI 3.1415926

#define PASS_DEPTH 0
#define PASS_COLOR 1
#define PASS_OIT 2

#if PASS == PASS_DEPTH || PASS == PASS_COLOR || NOUVEAU == 1
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
#define INTERPOLATION
#endif

// Uniforms
uniform float cp_AlphaTestValue;
uniform vec4 pp_ClipTest;
uniform vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT;
uniform float sp_FOG_DENSITY;
uniform float shade_scale_factor;
uniform sampler2D tex0, tex1;
layout(binding = 5) uniform sampler2D fog_table;
uniform usampler2D shadow_stencil;
uniform sampler2D DepthTex;
uniform float trilinear_alpha;
uniform vec4 fog_clamp_min;
uniform vec4 fog_clamp_max;
#if pp_Palette == 1
uniform sampler2D palette;
uniform int palette_index;
#endif

uniform ivec2 blend_mode[2];
#if pp_TwoVolumes == 1
uniform bool use_alpha[2];
uniform bool ignore_tex_alpha[2];
uniform int shading_instr[2];
uniform int fog_control[2];
#endif

// Input
INTERPOLATION in vec4 vtx_base;
INTERPOLATION in vec4 vtx_offs;
in vec3 vtx_uv;
#if pp_TwoVolumes == 1
INTERPOLATION in vec4 vtx_base1;
INTERPOLATION in vec4 vtx_offs1;
in vec2 vtx_uv1;
#endif
flat in uint vtx_index;

float fog_mode2(float w)
{
	float z = clamp(
#if DIV_POS_Z == 1
					sp_FOG_DENSITY / w
#else
					sp_FOG_DENSITY * w
#endif
									  , 1.0, 255.9999);
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

vec4 palettePixel(sampler2D tex, vec3 coords)
{
#if DIV_POS_Z == 1
	float colIdx = texture(tex, coords.xy).r;
#else
	float colIdx = textureProj(tex, coords).r;
#endif
	int color_idx = int(floor(colIdx * 255.0 + 0.5)) + palette_index;
	ivec2 c = ivec2(color_idx % 32, color_idx / 32);
	return texelFetch(palette, c, 0);
}

#endif

void main()
{
	setFragDepth(vtx_uv.z);
	
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
				area1 = true;
				cur_blend_mode = blend_mode[1];
				cur_use_alpha = use_alpha[1];
				cur_ignore_tex_alpha = ignore_tex_alpha[1];
				cur_shading_instr = shading_instr[1];
				cur_fog_control = fog_control[1];
			}
		#endif
	#endif
	#if pp_Gouraud == 1 && DIV_POS_Z != 1
		color /= vtx_uv.z;
		offset /= vtx_uv.z;
	#endif
	
	#if pp_UseAlpha==0 || pp_TwoVolumes == 1
		IF(!cur_use_alpha)
			color.a=1.0;
	#endif
	#if pp_FogCtrl==3 || pp_TwoVolumes == 1 // LUT Mode 2
		IF(cur_fog_control == 3)
			color = vec4(sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z));
	#endif
	#if pp_Texture==1
	{
		vec4 texcol;
		#if pp_Palette == 0
			#if DIV_POS_Z == 1
				#if pp_TwoVolumes == 1
					if (area1)
						texcol = texture(tex1, vtx_uv1);
					else
				#endif
						texcol = texture(tex0, vtx_uv.xy);
			#else
				#if pp_TwoVolumes == 1
					if (area1)
						texcol = textureProj(tex1, vec3(vtx_uv1.xy, vtx_uv.z));
					else
				#endif
						texcol = textureProj(tex0, vtx_uv);
			#endif
		#else
			#if pp_TwoVolumes == 1
				if (area1)
					texcol = palettePixel(tex1, vec3(vtx_uv1.xy, vtx_uv.z));
				else
			#endif
					texcol = palettePixel(tex0, vtx_uv);
		#endif

		#if pp_BumpMap == 1
			float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0 * PI * offset.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1 || pp_TwoVolumes == 1
				IF(cur_ignore_tex_alpha)
					texcol.a=1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (cp_AlphaTestValue > texcol.a)
					discard;
				texcol.a = 1.0;
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
			color.rgb = mix(color.rgb, sp_FOG_COL_RAM.rgb, fog_mode2(vtx_uv.z)); 
		}
	#endif
	#if pp_Offset==1 && pp_BumpMap == 0 && (pp_FogCtrl == 1 || pp_TwoVolumes == 1)  // Per vertex
		IF(cur_fog_control == 1)
		{
			color.rgb=mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a);
		}
	#endif
	
	color *= trilinear_alpha;
	
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0);
	
	#if PASS == PASS_COLOR 
		FragColor = color;
	#elif PASS == PASS_OIT
		ivec2 coords = ivec2(gl_FragCoord.xy);
		uint idx =  getNextPixelIndex();
		
		Pixel pixel;
		pixel.color = packColors(clamp(color, vec4(0.0), vec4(1.0)));
		pixel.depth = gl_FragDepth;
		pixel.seq_num = vtx_index;
		pixel.next = imageAtomicExchange(abufferPointerImg, coords, idx);
		pixels[idx] = pixel;
		
#if NOUVEAU == 0
		discard;
#else
		// nouveau may be optimizing a bit too aggressively here
		FragColor = vec4(0.0);
#endif
		
	#endif
}
)";

static const char* ModifierVolumeShader = R"(
in vec3 vtx_uv;

void main()
{
	setFragDepth(vtx_uv.z);
}
)";

class Vertex4Source : public OpenGl4Source
{
public:
	Vertex4Source(bool gouraud, bool divPosZ, bool twoVolumes) : OpenGl4Source()
	{
		addConstant("pp_TwoVolumes", twoVolumes);
		addConstant("pp_Gouraud", gouraud);
		addConstant("DIV_POS_Z", divPosZ);

		addSource(VertexShaderSource);
	}
};

class Fragment4ShaderSource : public OpenGl4Source
{
public:
	Fragment4ShaderSource(const gl4PipelineShader* s) : OpenGl4Source()
	{
		addConstant("cp_AlphaTest", s->cp_AlphaTest);
		addConstant("pp_ClipInside", s->pp_InsideClipping);
		addConstant("pp_UseAlpha", s->pp_UseAlpha);
		addConstant("pp_Texture", s->pp_Texture);
		addConstant("pp_IgnoreTexA", s->pp_IgnoreTexA);
		addConstant("pp_ShadInstr", s->pp_ShadInstr);
		addConstant("pp_Offset", s->pp_Offset);
		addConstant("pp_FogCtrl", s->pp_FogCtrl);
		addConstant("pp_TwoVolumes", s->pp_TwoVolumes);
		addConstant("pp_Gouraud", s->pp_Gouraud);
		addConstant("pp_BumpMap", s->pp_BumpMap);
		addConstant("FogClamping", s->fog_clamping);
		addConstant("pp_Palette", s->palette);
		addConstant("NOUVEAU", gl.mesa_nouveau);
		addConstant("PASS", (int)s->pass);
		addConstant("DIV_POS_Z", s->divPosZ);

		addSource(ShaderHeader);
		addSource(gl4PixelPipelineShader);
	}
};

gl4_ctx gl4;

struct gl4ShaderUniforms_t gl4ShaderUniforms;
int max_image_width;
int max_image_height;

bool gl4CompilePipelineShader(gl4PipelineShader* s, const char *fragment_source /* = nullptr */,
		const char *vertex_source /* = nullptr */)
{
	std::string vertexSource;
	if (s->naomi2)
		vertexSource = N2Vertex4Source(s).generate();
	else
		vertexSource = Vertex4Source(s->pp_Gouraud, s->divPosZ, s->pp_TwoVolumes).generate();
	Fragment4ShaderSource fragmentSource(s);

	s->program = gl_CompileAndLink(vertex_source != nullptr ? vertex_source : vertexSource.c_str(),
			fragment_source != nullptr ? fragment_source : fragmentSource.generate().c_str());

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
	s->ndcMat = glGetUniformLocation(s->program, "ndcMat");

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

	if (s->naomi2)
		initN2Uniforms(s);

	return glIsProgram(s->program)==GL_TRUE;
}

static void gl4_delete_shaders()
{
	for (auto it : gl4.shaders)
	{
		if (it.second.program != 0)
			glcache.DeleteProgram(it.second.program);
	}
	gl4.shaders.clear();
	glcache.DeleteProgram(gl4.modvol_shader.program);
	gl4.modvol_shader.program = 0;
	glcache.DeleteProgram(gl4.n2ModVolShader.program);
	gl4.n2ModVolShader.program = 0;
}

static void gl4_term()
{
	for (auto& buffer : gl4.vbo.geometry)
		buffer.reset();
	for (auto& buffer : gl4.vbo.modvols)
		buffer.reset();
	for (auto& buffer : gl4.vbo.idxs)
		buffer.reset();
	for (auto& buffer : gl4.vbo.tr_poly_params)
		buffer.reset();
	gl4_delete_shaders();
	for (auto& vao : gl4.vbo.main_vao)
		vao.term();
	for (auto& vao : gl4.vbo.modvol_vao)
		vao.term();
}

static void create_modvol_shader()
{
	if (gl4.modvol_shader.program != 0)
		return;
	Vertex4Source vertexShader(false, config::NativeDepthInterpolation, false);
	OpenGl4Source fragmentShader;
	fragmentShader.addConstant("DIV_POS_Z", config::NativeDepthInterpolation)
		.addSource(ShaderHeader)
		.addSource(ModifierVolumeShader);

	gl4.modvol_shader.program = gl_CompileAndLink(vertexShader.generate().c_str(), fragmentShader.generate().c_str());
	gl4.modvol_shader.ndcMat = glGetUniformLocation(gl4.modvol_shader.program, "ndcMat");

	N2Vertex4Source n2VertexShader;
	fragmentShader.setConstant("DIV_POS_Z", false);
	gl4.n2ModVolShader.program = gl_CompileAndLink(n2VertexShader.generate().c_str(), fragmentShader.generate().c_str());
	gl4.n2ModVolShader.ndcMat = glGetUniformLocation(gl4.n2ModVolShader.program, "ndcMat");
	gl4.n2ModVolShader.mvMat = glGetUniformLocation(gl4.n2ModVolShader.program, "mvMat");
	gl4.n2ModVolShader.projMat = glGetUniformLocation(gl4.n2ModVolShader.program, "projMat");
}

static void gl_create_resources()
{
	if (gl4.vbo.geometry[0] != nullptr)
		// Assume the resources have already been created
		return;

	//create vbos
	for (u32 i = 0; i < std::size(gl4.vbo.geometry); i++)
	{
		gl4.vbo.geometry[i] = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER);
		gl4.vbo.modvols[i] = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER);
		gl4.vbo.idxs[i] = std::make_unique<GlBuffer>(GL_ELEMENT_ARRAY_BUFFER);
		// Create the buffer for Translucent poly params
		gl4.vbo.tr_poly_params[i] = std::make_unique<GlBuffer>(GL_SHADER_STORAGE_BUFFER);
		gl4.vbo.bufferIndex = i;
		gl4SetupMainVBO();
		gl4SetupModvolVBO();
	}
	GlVertexArray::unbind();

	initQuad();
	glCheck();
}

struct OpenGL4Renderer : OpenGLRenderer
{
	bool Init() override;

	void Term() override
	{
		termABuffer();
		glcache.DeleteTextures(1, &stencilTexId);
		stencilTexId = 0;
		glcache.DeleteTextures(1, &depthTexId);
		depthTexId = 0;
		glcache.DeleteTextures(1, &opaqueTexId);
		opaqueTexId = 0;
		glcache.DeleteTextures(1, &depthSaveTexId);
		depthSaveTexId = 0;
		glDeleteFramebuffers(1, &geom_fbo);
		geom_fbo = 0;
		glDeleteSamplers(2, texSamplers);
		texSamplers[0] = texSamplers[1] = 0;
		glDeleteFramebuffers(1, &depth_fbo);
		depth_fbo = 0;

		TexCache.Clear();
		termGLCommon();
		gl4_term();
	}

	bool Render() override
	{
		saveCurrentFramebuffer();
		renderFrame(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
		if (pvrrc.isRTT) {
			restoreCurrentFramebuffer();
			return false;
		}

		if (!config::EmulateFramebuffer)
		{
			DrawOSD(false);
			gl.ofbo2.ready = false;
			frameRendered = true;
		}
		restoreCurrentFramebuffer();

		return true;
	}

	GLenum getFogTextureSlot() const override {
		return GL_TEXTURE5;
	}
	GLenum getPaletteTextureSlot() const override {
		return GL_TEXTURE6;
	}

	bool renderFrame(int width, int height);

#ifdef LIBRETRO
	void DrawOSD(bool clearScreen) override
	{
		void DrawVmuTexture(u8 vmu_screen_number, int width, int height);
		void DrawGunCrosshair(u8 port, int width, int height);

		if (settings.platform.isConsole())
		{
			for (int vmu_screen_number = 0 ; vmu_screen_number < 4 ; vmu_screen_number++)
				if (vmu_lcd_status[vmu_screen_number * 2])
					DrawVmuTexture(vmu_screen_number, width, height);
		}

		for (int lightgun_port = 0 ; lightgun_port < 4 ; lightgun_port++)
			DrawGunCrosshair(lightgun_port, width, height);
	}
#endif
};

//setup
void gl_DebugOutput(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
		const GLchar *message, const void *userParam);

bool OpenGL4Renderer::Init()
{
	findGLVersion();
	if (gl.gl_major < 4 || (gl.gl_major == 4 && gl.gl_minor < 3))
	{
		WARN_LOG(RENDERER, "Warning: OpenGL version doesn't support per-pixel sorting.");
		return false;
	}
	INFO_LOG(RENDERER, "Per-pixel sorting enabled");

	glcache.DisableCache();

    //glEnable(GL_DEBUG_OUTPUT);
    //glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    //glDebugMessageCallback(gl_DebugOutput, NULL);
    //glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

	gl_create_resources();

	initABuffer();

	if (config::TextureUpscale > 1)
	{
		// Trick to preload the tables used by xBRZ
		u32 src[] { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
		u32 dst[16];
		UpscalexBRZ(2, src, dst, 2, 2, false);
	}
	fog_needs_update = true;
	forcePaletteUpdate();
	TextureCacheData::SetDirectXColorOrder(false);

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
	}
}

bool OpenGL4Renderer::renderFrame(int width, int height)
{
	const bool is_rtt = pvrrc.isRTT;

	TransformMatrix<COORD_OPENGL> matrices(pvrrc, is_rtt ? pvrrc.getFramebufferWidth() : width,
			is_rtt ? pvrrc.getFramebufferHeight() : height);
	gl4ShaderUniforms.ndcMat = matrices.GetNormalMatrix();
	const glm::mat4& scissor_mat = matrices.GetScissorMatrix();
	ViewportMatrix = matrices.GetViewportMatrix();

	if (!is_rtt && !config::EmulateFramebuffer)
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
		float scaling = config::RenderToTextureBuffer ? 1.f : config::RenderResolution / 480.f;
		rendering_width = matrices.GetDreamcastViewport().x * scaling;
		rendering_height = matrices.GetDreamcastViewport().y * scaling;
	}
	else
	{
		rendering_width = width;
		rendering_height = height;
		getVideoShift(gl.ofbo.shiftX, gl.ofbo.shiftY);
	}
	resize(rendering_width, rendering_height);
	
	//VERT and RAM fog color constants
	FOG_COL_VERT.getRGBColor(gl4ShaderUniforms.ps_FOG_COL_VERT);
	FOG_COL_RAM.getRGBColor(gl4ShaderUniforms.ps_FOG_COL_RAM);

	//Fog density constant
	gl4ShaderUniforms.fog_den_float = FOG_DENSITY.get() * config::ExtraDepthScale;

	pvrrc.fog_clamp_min.getRGBAColor(gl4ShaderUniforms.fog_clamp_min);
	pvrrc.fog_clamp_max.getRGBAColor(gl4ShaderUniforms.fog_clamp_max);
	
	if (config::ModifierVolumes)
	{
		create_modvol_shader();
		glcache.UseProgram(gl4.modvol_shader.program);
		glUniformMatrix4fv(gl4.modvol_shader.ndcMat, 1, GL_FALSE, &gl4ShaderUniforms.ndcMat[0][0]);

		glcache.UseProgram(gl4.n2ModVolShader.program);
		glUniformMatrix4fv(gl4.n2ModVolShader.ndcMat, 1, GL_FALSE, &gl4ShaderUniforms.ndcMat[0][0]);
	}
	for (auto& it : gl4.shaders)
		resetN2UniformCache(&it.second);
	gl4ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);

	GLuint output_fbo;
	//setup render target first
	if (is_rtt)
		output_fbo = BindRTT(false);
	else
	{
		this->width = width;
		this->height = height;
#ifdef LIBRETRO
		if (config::EmulateFramebuffer)
			output_fbo = init_output_framebuffer(width, height);
		else if (config::PowerVR2Filter || gl.ofbo.shiftX != 0 || gl.ofbo.shiftY != 0)
			output_fbo = postProcessor.getFramebuffer(width, height);
		else
			output_fbo = glsm_get_current_framebuffer();
		glViewport(0, 0, width, height);
#else
		output_fbo = init_output_framebuffer(rendering_width, rendering_height);
#endif
	}
	if (output_fbo == 0)
		return false;

	gl4.vbo.nextBuffer();
	glcache.Disable(GL_SCISSOR_TEST);
	if (!is_rtt)
		glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);

	if (!is_rtt && (FB_R_CTRL.fb_enable == 0 || VO_CONTROL.blank_video == 1))
	{
		// Video output disabled
		glBindFramebuffer(GL_FRAMEBUFFER, output_fbo);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else
	{
		//Main VBO
		//move vertex to gpu
		gl4.vbo.getVertexBuffer()->update(pvrrc.verts.data(), pvrrc.verts.size() * sizeof(decltype(*pvrrc.verts.data())));
		gl4.vbo.getIndexBuffer()->update(pvrrc.idx.data(), pvrrc.idx.size() * sizeof(decltype(*pvrrc.idx.data())));

		//Modvol VBO
		if (!pvrrc.modtrig.empty())
			gl4.vbo.getModVolBuffer()->update(pvrrc.modtrig.data(), pvrrc.modtrig.size() * sizeof(decltype(*pvrrc.modtrig.data())));

		// TR PolyParam data
		if (!pvrrc.global_param_tr.empty())
		{
			std::vector<u32> trPolyParams(pvrrc.global_param_tr.size() * 2);
			int i = 0;
			for (const PolyParam& pp : pvrrc.global_param_tr)
			{
				trPolyParams[i++] = (pp.tsp.full & 0xffff00c0) | ((pp.isp.full >> 16) & 0xe400) | ((pp.pcw.full >> 7) & 1);
				trPolyParams[i++] = pp.tsp1.full;
			}
			gl4.vbo.getPolyParamBuffer()->update(trPolyParams.data(), trPolyParams.size() * sizeof(u32));
			// Declare storage
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gl4.vbo.getPolyParamBuffer()->getName());
		}
		glCheck();

		if (is_rtt || !config::Widescreen || matrices.IsClipped() || config::Rotate90 || config::EmulateFramebuffer)
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
				if (config::RenderResolution > 480 && !config::RenderToTextureBuffer)
				{
					float scale = config::RenderResolution / 480.f;
					min_x *= scale;
					min_y *= scale;
					fWidth *= scale;
					fHeight *= scale;
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
#ifdef LIBRETRO
		if ((config::PowerVR2Filter || gl.ofbo.shiftX != 0 || gl.ofbo.shiftY != 0) && !is_rtt && !config::EmulateFramebuffer)
			postProcessor.render(glsm_get_current_framebuffer());
#endif
	}

	if (is_rtt)
		ReadRTTBuffer();
	else if (config::EmulateFramebuffer)
		writeFramebufferToVRAM();
	else
	{
		gl.ofbo.aspectRatio = getOutputFramebufferAspectRatio();
#ifndef LIBRETRO
		renderLastFrame();
#endif
	}
	GlVertexArray::unbind();

	return !is_rtt;
}

Renderer* rend_GL4()
{
	return new OpenGL4Renderer();
}
