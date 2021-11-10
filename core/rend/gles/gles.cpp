#include "glcache.h"
#include "gles.h"
#include "cfg/cfg.h"
#include "hw/pvr/ta.h"
#ifndef LIBRETRO
#include "rend/gui.h"
#else
#include "vmu_xhair.h"
#endif
#include "rend/osd.h"
#include "rend/TexCache.h"
#include "rend/transform_matrix.h"
#include "wsi/gl_context.h"
#include "emulator.h"

#include <cmath>

#ifdef GLES
#ifndef GL_RED
#define GL_RED                            0x1903
#endif
#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION                  0x821B
#endif
#ifndef GL_MINOR_VERSION
#define GL_MINOR_VERSION                  0x821C
#endif
#endif

//Fragment and vertex shaders code

const char* ShaderCompatSource = R"(
#define GLES2 0 							
#define GLES3 1 							
#define GL2 2								
#define GL3 3								
											
#if TARGET_GL == GL2 						
#define highp								
#define lowp								
#define mediump								
#endif										
#if TARGET_GL == GLES3						
out highp vec4 FragColor;					
#define gl_FragColor FragColor				
#define FOG_CHANNEL a						
#elif TARGET_GL == GL3						
out highp vec4 FragColor;					
#define gl_FragColor FragColor				
#define FOG_CHANNEL r						
#else										
#define texture texture2D					
#define FOG_CHANNEL a						
#endif										
)";

const char *VertexCompatShader = R"(
#if TARGET_GL == GLES2 || TARGET_GL == GL2
#define in attribute
#define out varying
#endif
)";

const char *PixelCompatShader = R"(
#if TARGET_GL == GLES2 || TARGET_GL == GL2
#define in varying
#endif
)";

static const char* GouraudSource = R"(
#if TARGET_GL == GL3 || defined(GL_NV_shader_noperspective_interpolation)
	#define NOPERSPECTIVE noperspective
	#if pp_Gouraud == 0
		#define INTERPOLATION flat
	#else
		#define INTERPOLATION noperspective
	#endif
#elif TARGET_GL == GLES3
	#define NOPERSPECTIVE
	#if pp_Gouraud == 0
		#define INTERPOLATION flat
	#else
		#define INTERPOLATION
	#endif
#else
	#define NOPERSPECTIVE
	#define INTERPOLATION
#endif
)";

static const char* VertexShaderSource = R"(
/* Vertex constants*/ 
uniform highp vec4 depth_scale;
uniform highp mat4 normal_matrix;
uniform highp float sp_FOG_DENSITY;

/* Vertex input */
in highp vec4 in_pos;
in lowp vec4 in_base;
in lowp vec4 in_offs;
in highp vec2 in_uv;
/* output */
INTERPOLATION out highp vec4 vtx_base;
INTERPOLATION out highp vec4 vtx_offs;
NOPERSPECTIVE out highp vec3 vtx_uv;

void main()
{
	highp vec4 vpos = normal_matrix * in_pos;
	vtx_base = in_base;
	vtx_offs = in_offs;
#if TARGET_GL == GLES2
	vtx_uv = vec3(in_uv, vpos.z * sp_FOG_DENSITY);
	vpos.w = 1.0 / vpos.z;
	vpos.z = depth_scale.x + depth_scale.y * vpos.w;
	vpos.xy *= vpos.w;
#else
#if pp_Gouraud == 1
	vtx_base *= vpos.z;
	vtx_offs *= vpos.z;
#endif
	vtx_uv = vec3(in_uv * vpos.z, vpos.z);
	vpos.w = 1.0;
	vpos.z = 0.0;
#endif
	gl_Position = vpos;
}
)";

const char* PixelPipelineShader = R"(
#define PI 3.1415926

/* Shader program params*/
/* gles has no alpha test stage, so its emulated on the shader */
uniform lowp float cp_AlphaTestValue;
uniform lowp vec4 pp_ClipTest;
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT;
uniform highp float sp_FOG_DENSITY;
uniform sampler2D tex,fog_table;
uniform lowp float trilinear_alpha;
uniform lowp vec4 fog_clamp_min;
uniform lowp vec4 fog_clamp_max;
#if pp_Palette == 1
uniform sampler2D palette;
uniform mediump int palette_index;
#endif

/* Vertex input*/
INTERPOLATION in highp vec4 vtx_base;
INTERPOLATION in highp vec4 vtx_offs;
NOPERSPECTIVE in highp vec3 vtx_uv;

lowp float fog_mode2(highp float w)
{
#if TARGET_GL == GLES2
	highp float z = clamp(vtx_uv.z, 1.0, 255.9999);
#else
	highp float z = clamp(w * sp_FOG_DENSITY, 1.0, 255.9999);
#endif
	mediump float exp = floor(log2(z));
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0;
	mediump float idx = floor(m) + exp * 16.0 + 0.5;
	highp vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0));
	return fog_coef.FOG_CHANNEL;
}

highp vec4 fog_clamp(lowp vec4 col)
{
#if FogClamping == 1
	return clamp(col, fog_clamp_min, fog_clamp_max);
#else
	return col;
#endif
}

#if pp_Palette == 1

lowp vec4 palettePixel(highp vec3 coords)
{
#if TARGET_GL == GLES2 || TARGET_GL == GL2
	highp int color_idx = int(floor(texture(tex, coords.xy).FOG_CHANNEL * 255.0 + 0.5)) + palette_index;
    highp vec2 c = vec2((mod(float(color_idx), 32.0) * 2.0 + 1.0) / 64.0, (float(color_idx / 32) * 2.0 + 1.0) / 64.0);
	return texture(palette, c);
#else
	highp int color_idx = int(floor(textureProj(tex, coords).FOG_CHANNEL * 255.0 + 0.5)) + palette_index;
    highp ivec2 c = ivec2(color_idx % 32, color_idx / 32);
	return texelFetch(palette, c, 0);
#endif
}

#endif

#if TARGET_GL == GLES2
#define depth gl_FragCoord.w
#else
#define depth vtx_uv.z
#endif

void main()
{
	// Clip inside the box
	#if pp_ClipInside == 1
		if (gl_FragCoord.x >= pp_ClipTest.x && gl_FragCoord.x <= pp_ClipTest.z
				&& gl_FragCoord.y >= pp_ClipTest.y && gl_FragCoord.y <= pp_ClipTest.w)
			discard;
	#endif
	
	highp vec4 color = vtx_base;
	highp vec4 offset = vtx_offs;
	#if pp_Gouraud == 1 && TARGET_GL != GLES2
		color /= vtx_uv.z;
		offset /= vtx_uv.z;
	#endif
	#if pp_UseAlpha==0
		color.a=1.0;
	#endif
	#if pp_FogCtrl==3
		color = vec4(sp_FOG_COL_RAM.rgb, fog_mode2(depth));
	#endif
	#if pp_Texture==1
	{
		#if pp_Palette == 0
		  #if TARGET_GL == GLES2 || TARGET_GL == GL2
			lowp vec4 texcol = texture(tex, vtx_uv.xy);
		  #else
			lowp vec4 texcol = textureProj(tex, vtx_uv);
		  #endif
		#else
			lowp vec4 texcol = palettePixel(vtx_uv);
		#endif
		
		#if pp_BumpMap == 1
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0;
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0;
			texcol.a = clamp(offset.a + offset.r * sin(s) + offset.g * cos(s) * cos(r - 2.0 * PI * offset.b), 0.0, 1.0);
			texcol.rgb = vec3(1.0, 1.0, 1.0);	
		#else
			#if pp_IgnoreTexA==1
				texcol.a=1.0;	
			#endif
			
			#if cp_AlphaTest == 1
				if (cp_AlphaTestValue > texcol.a)
					discard;
				texcol.a = 1.0;
			#endif 
		#endif
		#if pp_ShadInstr==0
		{
			color=texcol;
		}
		#endif
		#if pp_ShadInstr==1
		{
			color.rgb*=texcol.rgb;
			color.a=texcol.a;
		}
		#endif
		#if pp_ShadInstr==2
		{
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a);
		}
		#endif
		#if  pp_ShadInstr==3
		{
			color*=texcol;
		}
		#endif
		
		#if pp_Offset==1 && pp_BumpMap == 0
			color.rgb += offset.rgb;
		#endif
	}
	#endif
	
	color = fog_clamp(color);
	
	#if pp_FogCtrl == 0
		color.rgb = mix(color.rgb, sp_FOG_COL_RAM.rgb, fog_mode2(depth)); 
	#endif
	#if pp_FogCtrl == 1 && pp_Offset==1 && pp_BumpMap == 0
		color.rgb = mix(color.rgb, sp_FOG_COL_VERT.rgb, offset.a);
	#endif
	
	#if pp_TriLinear == 1
	color *= trilinear_alpha;
	#endif
	
	//color.rgb = vec3(vtx_uv.z * sp_FOG_DENSITY / 128.0);
#if TARGET_GL != GLES2
	highp float w = vtx_uv.z * 100000.0;
	gl_FragDepth = log2(1.0 + w) / 34.0;
#endif
	gl_FragColor = color;
}
)";

static const char* ModifierVolumeShader = R"(
uniform lowp float sp_ShaderColor;

/* Vertex input*/
NOPERSPECTIVE in highp vec3 vtx_uv;

void main()
{
#if TARGET_GL != GLES2
	highp float w = vtx_uv.z * 100000.0;
	gl_FragDepth = log2(1.0 + w) / 34.0;
#endif
	gl_FragColor=vec4(0.0, 0.0, 0.0, sp_ShaderColor);
}
)";

const char* OSD_VertexShader = R"(
uniform highp vec4      scale;

in highp vec4    in_pos;
in lowp vec4     in_base;
in mediump vec2  in_uv;

out lowp vec4 vtx_base;
out mediump vec2 vtx_uv;

void main()
{
	vtx_base = in_base;
	vtx_uv = in_uv;
	highp vec4 vpos = in_pos;
	
	vpos.w = 1.0;
	vpos.z = vpos.w;
	vpos.xy = vpos.xy * scale.xy - scale.zw; 
	gl_Position = vpos;
}
)";

const char* OSD_Shader = R"(
in lowp vec4 vtx_base;
in mediump vec2 vtx_uv;

uniform sampler2D tex;
void main()
{
	gl_FragColor = vtx_base * texture(tex, vtx_uv);
}
)";

GLCache glcache;
gl_ctx gl;

GLuint fogTextureId;
GLuint paletteTextureId;

glm::mat4 ViewportMatrix;

#ifdef TEST_AUTOMATION
void do_swap_automation()
{
	static FILE* video_file = fopen(cfgLoadStr("record", "rawvid","").c_str(), "wb");
	extern bool do_screenshot;

	if (video_file)
	{
		int bytesz = gl.ofbo.width * gl.ofbo.height * 3;
		u8* img = new u8[bytesz];
		
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.ofbo.fbo);
		glReadPixels(0, 0, gl.ofbo.width, gl.ofbo.height, GL_RGB, GL_UNSIGNED_BYTE, img);
		fwrite(img, 1, bytesz, video_file);
		delete[] img;
		fflush(video_file);
	}

	if (do_screenshot)
	{
		int bytesz = gl.ofbo.width * gl.ofbo.height * 3;
		u8* img = new u8[bytesz];
		
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.ofbo.fbo);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, gl.ofbo.width, gl.ofbo.height, GL_RGB, GL_UNSIGNED_BYTE, img);
		dump_screenshot(img, gl.ofbo.width, gl.ofbo.height);
		delete[] img;
		dc_exit();
		theGLContext.term();
		exit(0);
	}
}

#endif

static void gl_delete_shaders()
{
	for (const auto& it : gl.shaders)
	{
		if (it.second.program != 0)
			glcache.DeleteProgram(it.second.program);
	}
	gl.shaders.clear();
	glcache.DeleteProgram(gl.modvol_shader.program);
	gl.modvol_shader.program = 0;
}

void termGLCommon()
{
	termQuad();
	postProcessor.term();

	// palette, fog
	glcache.DeleteTextures(1, &fogTextureId);
	fogTextureId = 0;
	glcache.DeleteTextures(1, &paletteTextureId);
	paletteTextureId = 0;
	// RTT
	glDeleteBuffers(1, &gl.rtt.pbo);
	gl.rtt.pbo = 0;
	gl.rtt.pboSize = 0;
	glDeleteFramebuffers(1, &gl.rtt.fbo);
	gl.rtt.fbo = 0;
	glcache.DeleteTextures(1, &gl.rtt.tex);
	gl.rtt.tex = 0;
	glDeleteRenderbuffers(1, &gl.rtt.depthb);
	gl.rtt.depthb = 0;
	gl.rtt.texAddress = ~0;

	gl_free_osd_resources();
	free_output_framebuffer();
	glcache.DeleteTextures(1, &fbTextureId);
	fbTextureId = 0;
#ifdef LIBRETRO
	termVmuLightgun();
#endif
}

static void gles_term()
{
#ifndef GLES2
	glDeleteVertexArrays(1, &gl.vbo.mainVAO);
	gl.vbo.mainVAO = 0;
	glDeleteVertexArrays(1, &gl.vbo.modvolVAO);
	gl.vbo.modvolVAO = 0;
#endif
	glDeleteBuffers(1, &gl.vbo.geometry);
	gl.vbo.geometry = 0;
	glDeleteBuffers(1, &gl.vbo.modvols);
	glDeleteBuffers(1, &gl.vbo.idxs);
	glDeleteBuffers(1, &gl.vbo.idxs2);
	termGLCommon();

	gl_delete_shaders();
}

void findGLVersion()
{
	gl.index_type = GL_UNSIGNED_INT;
	gl.gl_major = theGLContext.getMajorVersion();
	gl.gl_minor = theGLContext.getMinorVersion();
	gl.is_gles = theGLContext.isGLES();
	if (gl.is_gles)
	{
		if (gl.gl_major >= 3)
		{
			gl.gl_version = "GLES3";
			gl.glsl_version_header = "#version 300 es";
		}
		else
		{
			gl.gl_version = "GLES2";
			gl.glsl_version_header = "";
			gl.index_type = GL_UNSIGNED_SHORT;
		}
		gl.single_channel_format = GL_ALPHA;
		const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
		if (strstr(extensions, "GL_OES_packed_depth_stencil") != NULL)
			gl.GL_OES_packed_depth_stencil_supported = true;
		if (strstr(extensions, "GL_OES_depth24") != NULL)
			gl.GL_OES_depth24_supported = true;
		if (!gl.GL_OES_packed_depth_stencil_supported)
			INFO_LOG(RENDERER, "Packed depth/stencil not supported: no modifier volumes when rendering to a texture");
		GLint ranges[2];
		GLint precision;
		glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, ranges, &precision);
		gl.highp_float_supported = (ranges[0] != 0 || ranges[1] != 0) && precision != 0;
	}
	else
	{
    	if (gl.gl_major >= 3)
    	{
			gl.gl_version = "GL3";
#if defined(__APPLE__)
			gl.glsl_version_header = "#version 150";
#else
			gl.glsl_version_header = "#version 130";
#endif
			gl.single_channel_format = GL_RED;
		}
		else
		{
			gl.gl_version = "GL2";
			gl.glsl_version_header = "#version 120";
			gl.single_channel_format = GL_ALPHA;
		}
    	gl.highp_float_supported = true;
	}
	gl.max_anisotropy = 1.f;
#if !defined(GLES2)
	if (gl.gl_major >= 3)
	{
		bool anisotropicExtension = false;
		const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
		// glGetString(GL_EXTENSIONS) is deprecated and might return NULL in core contexts.
		// In that case, use glGetStringi instead
		if (extensions == nullptr)
		{
			GLint n = 0;
			glGetIntegerv(GL_NUM_EXTENSIONS, &n);
			for (GLint i = 0; i < n; i++)
			{
				const char* extension = (const char *)glGetStringi(GL_EXTENSIONS, i);
				if (!strcmp(extension, "GL_EXT_texture_filter_anisotropic"))
				{
					anisotropicExtension = true;
					break;
				}
			}
		}
		else if (strstr(extensions, "GL_EXT_texture_filter_anisotropic") != nullptr)
			anisotropicExtension = true;
		if (anisotropicExtension)
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &gl.max_anisotropy);
	}
#endif
	gl.mesa_nouveau = strstr((const char *)glGetString(GL_VERSION), "Mesa") != nullptr && !strcmp((const char *)glGetString(GL_VENDOR), "nouveau");
	NOTICE_LOG(RENDERER, "Open GL%s version %d.%d", gl.is_gles ? "ES" : "", gl.gl_major, gl.gl_minor);
	while (glGetError() != GL_NO_ERROR)
		;
}

struct ShaderUniforms_t ShaderUniforms;

GLuint gl_CompileShader(const char* shader,GLuint type)
{
	GLint result;
	GLint compile_log_len;
	GLuint rv=glCreateShader(type);
	glShaderSource(rv, 1,&shader, NULL);
	glCompileShader(rv);

	//lets see if it compiled ...
	glGetShaderiv(rv, GL_COMPILE_STATUS, &result);
	glGetShaderiv(rv, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
		char* compile_log=(char*)malloc(compile_log_len);
		*compile_log=0;

		glGetShaderInfoLog(rv, compile_log_len, &compile_log_len, compile_log);
		WARN_LOG(RENDERER, "Shader: %s \n%s", result ? "compiled!" : "failed to compile", compile_log);

		free(compile_log);
	}

	return rv;
}

GLuint gl_CompileAndLink(const char* VertexShader, const char* FragmentShader)
{
	//create shaders
	GLuint vs=gl_CompileShader(VertexShader ,GL_VERTEX_SHADER);
	GLuint ps=gl_CompileShader(FragmentShader ,GL_FRAGMENT_SHADER);

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, ps);

	//bind vertex attribute to vbo inputs
	glBindAttribLocation(program, VERTEX_POS_ARRAY,      "in_pos");
	glBindAttribLocation(program, VERTEX_COL_BASE_ARRAY, "in_base");
	glBindAttribLocation(program, VERTEX_COL_OFFS_ARRAY, "in_offs");
	glBindAttribLocation(program, VERTEX_UV_ARRAY,       "in_uv");
	glBindAttribLocation(program, VERTEX_COL_BASE1_ARRAY, "in_base1");
	glBindAttribLocation(program, VERTEX_COL_OFFS1_ARRAY, "in_offs1");
	glBindAttribLocation(program, VERTEX_UV1_ARRAY,       "in_uv1");

#ifndef GLES
	if (!gl.is_gles && gl.gl_major >= 3)
		glBindFragDataLocation(program, 0, "FragColor");
#endif

	glLinkProgram(program);

	GLint result;
	glGetProgramiv(program, GL_LINK_STATUS, &result);


	GLint compile_log_len;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &compile_log_len);

	if (!result && compile_log_len>0)
	{
		compile_log_len+= 1024;
		char* compile_log=(char*)malloc(compile_log_len);
		*compile_log=0;

		glGetProgramInfoLog(program, compile_log_len, &compile_log_len, compile_log);
		WARN_LOG(RENDERER, "Shader linking: %s \n (%d bytes), - %s -", result ? "linked" : "failed to link", compile_log_len, compile_log);

		free(compile_log);

		// Dump the shaders source for troubleshooting
		INFO_LOG(RENDERER, "// VERTEX SHADER\n%s\n// END", VertexShader);
		INFO_LOG(RENDERER, "// FRAGMENT SHADER\n%s\n// END", FragmentShader);
		die("shader compile fail\n");
	}

	glDeleteShader(vs);
	glDeleteShader(ps);

	glcache.UseProgram(program);

	verify(glIsProgram(program));

	return program;
}

PipelineShader *GetProgram(bool cp_AlphaTest, bool pp_InsideClipping,
		bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr, bool pp_Offset,
		u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear,
		bool palette)
{
	u32 rv=0;

	rv |= pp_InsideClipping;
	rv<<=1; rv|=cp_AlphaTest;
	rv<<=1; rv|=pp_Texture;
	rv<<=1; rv|=pp_UseAlpha;
	rv<<=1; rv|=pp_IgnoreTexA;
	rv<<=2; rv|=pp_ShadInstr;
	rv<<=1; rv|=pp_Offset;
	rv<<=2; rv|=pp_FogCtrl;
	rv<<=1; rv|=pp_Gouraud;
	rv<<=1; rv|=pp_BumpMap;
	rv<<=1; rv|=fog_clamping;
	rv<<=1; rv|=trilinear;
	rv<<=1; rv|=palette;

	PipelineShader *shader = &gl.shaders[rv];
	if (shader->program == 0)
	{
		shader->cp_AlphaTest = cp_AlphaTest;
		shader->pp_InsideClipping = pp_InsideClipping;
		shader->pp_Texture = pp_Texture;
		shader->pp_UseAlpha = pp_UseAlpha;
		shader->pp_IgnoreTexA = pp_IgnoreTexA;
		shader->pp_ShadInstr = pp_ShadInstr;
		shader->pp_Offset = pp_Offset;
		shader->pp_FogCtrl = pp_FogCtrl;
		shader->pp_Gouraud = pp_Gouraud;
		shader->pp_BumpMap = pp_BumpMap;
		shader->fog_clamping = fog_clamping;
		shader->trilinear = trilinear;
		shader->palette = palette;
		CompilePipelineShader(shader);
	}

	return shader;
}

class VertexSource : public OpenGlSource
{
public:
	VertexSource(bool gouraud) : OpenGlSource() {
		addConstant("pp_Gouraud", gouraud);

		addSource(VertexCompatShader);
		addSource(GouraudSource);
		addSource(VertexShaderSource);
	}
};

class FragmentShaderSource : public OpenGlSource
{
public:
	FragmentShaderSource(const PipelineShader* s) : OpenGlSource()
	{
		addConstant("cp_AlphaTest", s->cp_AlphaTest);
		addConstant("pp_ClipInside", s->pp_InsideClipping);
		addConstant("pp_UseAlpha", s->pp_UseAlpha);
		addConstant("pp_Texture", s->pp_Texture);
		addConstant("pp_IgnoreTexA", s->pp_IgnoreTexA);
		addConstant("pp_ShadInstr", s->pp_ShadInstr);
		addConstant("pp_Offset", s->pp_Offset);
		addConstant("pp_FogCtrl", s->pp_FogCtrl);
		addConstant("pp_Gouraud", s->pp_Gouraud);
		addConstant("pp_BumpMap", s->pp_BumpMap);
		addConstant("FogClamping", s->fog_clamping);
		addConstant("pp_TriLinear", s->trilinear);
		addConstant("pp_Palette", s->palette);

		addSource(PixelCompatShader);
		addSource(GouraudSource);
		addSource(PixelPipelineShader);
	}
};

bool CompilePipelineShader(PipelineShader* s)
{
	VertexSource vertexSource(s->pp_Gouraud);
	FragmentShaderSource fragmentSource(s);

	s->program = gl_CompileAndLink(vertexSource.generate().c_str(), fragmentSource.generate().c_str());

	//setup texture 0 as the input for the shader
	GLint gu = glGetUniformLocation(s->program, "tex");
	if (s->pp_Texture==1)
		glUniform1i(gu,0);

	//get the uniform locations
	s->depth_scale      = glGetUniformLocation(s->program, "depth_scale");

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
	// Setup texture 1 as the fog table
	gu = glGetUniformLocation(s->program, "fog_table");
	if (gu != -1)
		glUniform1i(gu, 1);
	// And texture 2 as palette
	gu = glGetUniformLocation(s->program, "palette");
	if (gu != -1)
		glUniform1i(gu, 2);
	s->palette_index = glGetUniformLocation(s->program, "palette_index");

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

	ShaderUniforms.Set(s);

	return glIsProgram(s->program)==GL_TRUE;
}

static void SetupOSDVBO()
{
#ifndef GLES2
	if (gl.OSD_SHADER.vao != 0)
	{
		glBindVertexArray(gl.OSD_SHADER.vao);
		glBindBuffer(GL_ARRAY_BUFFER, gl.OSD_SHADER.geometry);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		return;
	}
	if (gl.gl_major >= 3)
	{
		glGenVertexArrays(1, &gl.OSD_SHADER.vao);
		glBindVertexArray(gl.OSD_SHADER.vao);
	}
#endif
	if (gl.OSD_SHADER.geometry == 0)
		glGenBuffers(1, &gl.OSD_SHADER.geometry);
	glBindBuffer(GL_ARRAY_BUFFER, gl.OSD_SHADER.geometry);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	//setup vertex buffers attrib pointers
	glEnableVertexAttribArray(VERTEX_POS_ARRAY);
	glVertexAttribPointer(VERTEX_POS_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(OSDVertex), (void*)offsetof(OSDVertex, x));

	glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
	glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(OSDVertex), (void*)offsetof(OSDVertex, r));

	glEnableVertexAttribArray(VERTEX_UV_ARRAY);
	glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(OSDVertex), (void*)offsetof(OSDVertex, u));

	glDisableVertexAttribArray(VERTEX_COL_OFFS_ARRAY);
	glCheck();
}

void gl_load_osd_resources()
{
	OpenGlSource vertexSource;
	vertexSource.addSource(VertexCompatShader)
			.addSource(OSD_VertexShader);
	OpenGlSource fragmentSource;
	fragmentSource.addSource(PixelCompatShader)
			.addSource(OSD_Shader);

	gl.OSD_SHADER.program = gl_CompileAndLink(vertexSource.generate().c_str(), fragmentSource.generate().c_str());
	gl.OSD_SHADER.scale = glGetUniformLocation(gl.OSD_SHADER.program, "scale");
	glUniform1i(glGetUniformLocation(gl.OSD_SHADER.program, "tex"), 0);		//bind osd texture to slot 0

#ifdef __ANDROID__
	if (gl.OSD_SHADER.osd_tex == 0)
	{
		int width, height;
		u8 *image_data = loadOSDButtons(width, height);
		//Now generate the OpenGL texture object
		gl.OSD_SHADER.osd_tex = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, gl.OSD_SHADER.osd_tex);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, (GLvoid *)image_data);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

		delete[] image_data;
	}
#endif
	SetupOSDVBO();
}

void gl_free_osd_resources()
{
	if (gl.OSD_SHADER.program != 0)
	{
		glcache.DeleteProgram(gl.OSD_SHADER.program);
		gl.OSD_SHADER.program = 0;
	}

    if (gl.OSD_SHADER.osd_tex != 0) {
        glcache.DeleteTextures(1, &gl.OSD_SHADER.osd_tex);
        gl.OSD_SHADER.osd_tex = 0;
    }
	glDeleteBuffers(1, &gl.OSD_SHADER.geometry);
	gl.OSD_SHADER.geometry = 0;
#ifndef GLES2
	glDeleteVertexArrays(1, &gl.OSD_SHADER.vao);
	gl.OSD_SHADER.vao = 0;
#endif
}

static void create_modvol_shader()
{
	if (gl.modvol_shader.program != 0)
		return;
	VertexSource vertexShader(false);

	OpenGlSource fragmentShader;
	fragmentShader.addConstant("pp_Gouraud", 0)
			.addSource(PixelCompatShader)
			.addSource(GouraudSource)
			.addSource(ModifierVolumeShader);

	gl.modvol_shader.program = gl_CompileAndLink(vertexShader.generate().c_str(), fragmentShader.generate().c_str());
	gl.modvol_shader.normal_matrix  = glGetUniformLocation(gl.modvol_shader.program, "normal_matrix");
	gl.modvol_shader.sp_ShaderColor = glGetUniformLocation(gl.modvol_shader.program, "sp_ShaderColor");
	gl.modvol_shader.depth_scale    = glGetUniformLocation(gl.modvol_shader.program, "depth_scale");
}

bool gl_create_resources()
{
	if (gl.vbo.geometry != 0)
		// Assume the resources have already been created
		return true;

	findGLVersion();

	if (gl.gl_major >= 3)
		// will be used later. Better fail fast
		verify(glGenVertexArrays != nullptr);

	//create vbos
	glGenBuffers(1, &gl.vbo.geometry);
	glGenBuffers(1, &gl.vbo.modvols);
	glGenBuffers(1, &gl.vbo.idxs);
	glGenBuffers(1, &gl.vbo.idxs2);

	create_modvol_shader();
	initQuad();

	return true;
}

GLuint gl_CompileShader(const char* shader,GLuint type);

bool gl_create_resources();

//setup

#ifndef __APPLE__
void gl_DebugOutput(GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        const GLchar *message,
        const void *userParam)
{
	if (id == 131185)
		return;
	switch (severity)
	{
	default:
	case GL_DEBUG_SEVERITY_NOTIFICATION:
	case GL_DEBUG_SEVERITY_LOW:
		DEBUG_LOG(RENDERER, "opengl:[%d] %s", id, message);
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		INFO_LOG(RENDERER, "opengl:[%d] %s", id, message);
		break;
	case GL_DEBUG_SEVERITY_HIGH:
		WARN_LOG(RENDERER, "opengl:[%d] %s", id, message);
		break;
	}
}
#endif

bool gles_init()
{
	glcache.EnableCache();

	if (!gl_create_resources())
		return false;

#if 0
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#ifdef GLES
	glDebugMessageCallback((RGLGENGLDEBUGPROC)gl_DebugOutput, NULL);
#else
    glDebugMessageCallback(gl_DebugOutput, NULL);
#endif
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);
#endif

#if defined(GL_GENERATE_MIPMAP_HINT) && !defined(__SWITCH__)
	if (gl.is_gles)
		glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
#endif
	glCheck();

	if (config::TextureUpscale > 1)
	{
		// Trick to preload the tables used by xBRZ
		u32 src[] { 0x11111111, 0x22222222, 0x33333333, 0x44444444 };
		u32 dst[16];
		UpscalexBRZ(2, src, dst, 2, 2, false);
	}
	fog_needs_update = true;
	palette_updated = true;
	TextureCacheData::SetDirectXColorOrder(false);

	return true;
}


void UpdateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format)
{
	glActiveTexture(texture_slot);
	if (fogTextureId == 0)
	{
		fogTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);

	u8 temp_tex_buffer[256];
	MakeFogTexture(temp_tex_buffer);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, fog_image_format, 128, 2, 0, fog_image_format, GL_UNSIGNED_BYTE, temp_tex_buffer);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}

void UpdatePaletteTexture(GLenum texture_slot)
{
	glActiveTexture(texture_slot);
	if (paletteTextureId == 0)
	{
		paletteTextureId = glcache.GenTexture();
		glcache.BindTexture(GL_TEXTURE_2D, paletteTextureId);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
		glcache.BindTexture(GL_TEXTURE_2D, paletteTextureId);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, palette32_ram);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}

void OSD_DRAW(bool clear_screen)
{
#ifdef LIBRETRO
	void DrawVmuTexture(u8 vmu_screen_number);
	void DrawGunCrosshair(u8 port);

	if (settings.platform.system == DC_PLATFORM_DREAMCAST)
	{
		for (int vmu_screen_number = 0 ; vmu_screen_number < 4 ; vmu_screen_number++)
			if (vmu_lcd_status[vmu_screen_number * 2])
				DrawVmuTexture(vmu_screen_number);
	}

	for (int lightgun_port = 0 ; lightgun_port < 4 ; lightgun_port++)
		DrawGunCrosshair(lightgun_port);

#else
	gui_display_osd();
#ifdef __ANDROID__
	if (gl.OSD_SHADER.osd_tex == 0)
		gl_load_osd_resources();
	if (gl.OSD_SHADER.osd_tex != 0)
	{
		glcache.Disable(GL_SCISSOR_TEST);
		glViewport(0, 0, settings.display.width, settings.display.height);

		if (clear_screen)
		{
			glcache.ClearColor(0.7f, 0.7f, 0.7f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
			render_output_framebuffer();
			glViewport(0, 0, settings.display.width, settings.display.height);
		}

#ifndef GLES2
		if (gl.gl_major >= 3)
			glBindVertexArray(gl.OSD_SHADER.vao);
		else
#endif
			SetupOSDVBO();

		glBindBuffer(GL_ARRAY_BUFFER, gl.OSD_SHADER.geometry);

		verify(glIsProgram(gl.OSD_SHADER.program));
		glcache.UseProgram(gl.OSD_SHADER.program);

		float scale_h = settings.display.height / 480.f;
		float offs_x = (settings.display.width - scale_h * 640.f) / 2.f;
		float scale[4];
		scale[0] = 2.f / (settings.display.width / scale_h);
		scale[1]= -2.f / 480.f;
		scale[2]= 1.f - 2.f * offs_x / settings.display.width;
		scale[3]= -1.f;
		glUniform4fv(gl.OSD_SHADER.scale, 1, scale);

		glActiveTexture(GL_TEXTURE0);
		glcache.BindTexture(GL_TEXTURE_2D, gl.OSD_SHADER.osd_tex);

		glBindFramebuffer(GL_FRAMEBUFFER, gl.ofbo.origFbo);

		const std::vector<OSDVertex>& osdVertices = GetOSDVertices();
		glBufferData(GL_ARRAY_BUFFER, osdVertices.size() * sizeof(OSDVertex), osdVertices.data(), GL_STREAM_DRAW); glCheck();

		glcache.Enable(GL_BLEND);
		glcache.Disable(GL_DEPTH_TEST);
		glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glcache.DepthMask(false);
		glcache.DepthFunc(GL_ALWAYS);

		glcache.Disable(GL_CULL_FACE);
		int dfa = osdVertices.size() / 4;

		for (int i = 0; i < dfa; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);

		glCheck();
	}
#endif
#endif
#ifndef GLES2
	if (gl.gl_major >= 3)
		glBindVertexArray(0);
#endif
}

bool OpenGLRenderer::Process(TA_context* ctx)
{
	if (KillTex)
		TexCache.Clear();
	TexCache.Cleanup();

	if (ctx->rend.isRenderFramebuffer)
	{
		RenderFramebuffer();
	}
	else
	{
		if (fog_needs_update && config::Fog)
		{
			fog_needs_update = false;
			UpdateFogTexture((u8 *)FOG_TABLE, getFogTextureSlot(), gl.single_channel_format);
		}
		if (palette_updated)
		{
			UpdatePaletteTexture(getPaletteTextureSlot());
			palette_updated = false;
		}

		if (!ta_parse_vdrc(ctx))
			return false;
	}

	return true;
}

static void upload_vertex_indices()
{
	if (gl.index_type == GL_UNSIGNED_SHORT)
	{
		static bool overrun;
		static List<u16> short_idx;
		if (short_idx.daty != NULL)
			short_idx.Free();
		short_idx.Init(pvrrc.idx.used(), &overrun, NULL);
		for (u32 *p = pvrrc.idx.head(); p < pvrrc.idx.LastPtr(0); p++)
			*(short_idx.Append()) = *p;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, short_idx.bytes(), short_idx.head(), GL_STREAM_DRAW);
	}
	else
		glBufferData(GL_ELEMENT_ARRAY_BUFFER,pvrrc.idx.bytes(),pvrrc.idx.head(),GL_STREAM_DRAW);
	glCheck();
}

bool RenderFrame(int width, int height)
{
	bool is_rtt = pvrrc.isRTT;

	float vtx_min_fZ = 0.f;	//pvrrc.fZ_min;
	float vtx_max_fZ = pvrrc.fZ_max;

	//sanitise the values, now with NaN detection (for omap)
	//0x49800000 is 1024*1024. Using integer math to avoid issues w/ infs and nans
	if ((s32&)vtx_max_fZ < 0 || (u32&)vtx_max_fZ > 0x49800000)
		vtx_max_fZ = 10 * 1024;

	//add some extra range to avoid clipping border cases
	vtx_min_fZ *= 0.98f;
	vtx_max_fZ *= 1.001f;

	TransformMatrix<COORD_OPENGL> matrices(pvrrc, width, height);
	ShaderUniforms.normal_mat = matrices.GetNormalMatrix();
	const glm::mat4& scissor_mat = matrices.GetScissorMatrix();
	ViewportMatrix = matrices.GetViewportMatrix();

	if (!is_rtt)
		gcflip = 0;
	else
		gcflip = 1;

	ShaderUniforms.depth_coefs[0] = 2 / (vtx_max_fZ - vtx_min_fZ);
	ShaderUniforms.depth_coefs[1] = -vtx_min_fZ - 1;
	ShaderUniforms.depth_coefs[2] = 0;
	ShaderUniforms.depth_coefs[3] = 0;

	//VERT and RAM fog color constants
	u8* fog_colvert_bgra = (u8*)&FOG_COL_VERT;
	u8* fog_colram_bgra = (u8*)&FOG_COL_RAM;
	ShaderUniforms.ps_FOG_COL_VERT[0] = fog_colvert_bgra[2] / 255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[1] = fog_colvert_bgra[1] / 255.0f;
	ShaderUniforms.ps_FOG_COL_VERT[2] = fog_colvert_bgra[0] / 255.0f;

	ShaderUniforms.ps_FOG_COL_RAM[0] = fog_colram_bgra[2] / 255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[1] = fog_colram_bgra[1] / 255.0f;
	ShaderUniforms.ps_FOG_COL_RAM[2] = fog_colram_bgra[0] / 255.0f;

	//Fog density constant
	u8* fog_density = (u8*)&FOG_DENSITY;
	float fog_den_mant = fog_density[1] / 128.0f;  //bit 7 -> x. bit, so [6:0] -> fraction -> /128
	s32 fog_den_exp = (s8)fog_density[0];
	ShaderUniforms.fog_den_float = fog_den_mant * powf(2.0f, fog_den_exp) * config::ExtraDepthScale;

	ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;
	
	ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;
	
	glcache.UseProgram(gl.modvol_shader.program);

	if (gl.modvol_shader.depth_scale != -1)
		glUniform4fv(gl.modvol_shader.depth_scale, 1, ShaderUniforms.depth_coefs);
	glUniformMatrix4fv(gl.modvol_shader.normal_matrix, 1, GL_FALSE, &ShaderUniforms.normal_mat[0][0]);

	ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	for (const auto& it : gl.shaders)
	{
		glcache.UseProgram(it.second.program);
		ShaderUniforms.Set(&it.second);
	}

	//setup render target first
#ifdef LIBRETRO
	gl.ofbo.origFbo = glsm_get_current_framebuffer();
#else
	gl.ofbo.origFbo = 0;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING, (GLint *)&gl.ofbo.origFbo);
#endif
	if (is_rtt)
	{
		if (BindRTT() == 0)
			return false;
	}
	else
	{
#ifdef LIBRETRO
		gl.ofbo.width = width;
		gl.ofbo.height = height;
		if (config::PowerVR2Filter && !pvrrc.isRenderFramebuffer)
			glBindFramebuffer(GL_FRAMEBUFFER, postProcessor.getFramebuffer(width, height));
		else
			glBindFramebuffer(GL_FRAMEBUFFER, glsm_get_current_framebuffer());
		glViewport(0, 0, width, height);
#else
		if (init_output_framebuffer(width, height) == 0)
			return false;
#endif
	}

	bool wide_screen_on = !is_rtt && config::Widescreen && !matrices.IsClipped() && !config::Rotate90;

	//Color is cleared by the background plane

	glcache.Disable(GL_SCISSOR_TEST);

	glcache.DepthMask(GL_TRUE);
	glClearDepthf(0.0);
	glStencilMask(0xFF); glCheck();
    glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();
	if (!is_rtt)
		glcache.ClearColor(VO_BORDER_COL.Red / 255.f, VO_BORDER_COL.Green / 255.f, VO_BORDER_COL.Blue / 255.f, 1.f);

	if (!is_rtt && (FB_R_CTRL.fb_enable == 0 || VO_CONTROL.blank_video == 1))
	{
		// Video output disabled
		glClear(GL_COLOR_BUFFER_BIT);
	}
	else if (!pvrrc.isRenderFramebuffer)
	{
		//move vertex to gpu
		//Main VBO
		glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl.vbo.idxs); glCheck();

		glBufferData(GL_ARRAY_BUFFER,pvrrc.verts.bytes(),pvrrc.verts.head(),GL_STREAM_DRAW); glCheck();

		upload_vertex_indices();

		//Modvol VBO
		if (pvrrc.modtrig.used())
		{
			glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.modvols); glCheck();
			glBufferData(GL_ARRAY_BUFFER,pvrrc.modtrig.bytes(),pvrrc.modtrig.head(),GL_STREAM_DRAW); glCheck();
		}

		if (!wide_screen_on)
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
					glcache.Scissor(0, 0, (GLsizei)lroundf(scaled_offs_x), (GLsizei)height);
					glClear(GL_COLOR_BUFFER_BIT);
					glcache.Scissor(width - scaled_offs_x, 0, (GLsizei)lroundf(scaled_offs_x + 1.f), (GLsizei)height);
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
			ShaderUniforms.base_clipping.enabled = true;
			ShaderUniforms.base_clipping.x = (int)lroundf(min_x);
			ShaderUniforms.base_clipping.y = (int)lroundf(min_y);
			ShaderUniforms.base_clipping.width = (int)lroundf(fWidth);
			ShaderUniforms.base_clipping.height = (int)lroundf(fHeight);
			glcache.Scissor(ShaderUniforms.base_clipping.x, ShaderUniforms.base_clipping.y, ShaderUniforms.base_clipping.width, ShaderUniforms.base_clipping.height);
			glcache.Enable(GL_SCISSOR_TEST);
		}
		else
		{
			ShaderUniforms.base_clipping.enabled = false;
		}

		DrawStrips();
#ifdef LIBRETRO
		if (config::PowerVR2Filter && !is_rtt)
			postProcessor.render(glsm_get_current_framebuffer());
#endif
	}
	else
	{
		glClear(GL_COLOR_BUFFER_BIT);
		DrawFramebuffer();
	}

	if (is_rtt)
		ReadRTTBuffer();
#ifndef LIBRETRO
	else
		render_output_framebuffer();
#endif
#ifndef GLES2
	if (gl.gl_major >= 3)
		glBindVertexArray(0);
#endif

	return !is_rtt;
}

bool OpenGLRenderer::Init()
{
	return gles_init();
}

void OpenGLRenderer::Term()
{
	TexCache.Clear();
	gles_term();
}

bool OpenGLRenderer::Render()
{
	RenderFrame(width, height);
	if (pvrrc.isRTT)
		return false;

	DrawOSD(false);
	frameRendered = true;

	return true;
}

bool OpenGLRenderer::RenderLastFrame()
{
	return render_output_framebuffer();
}

Renderer* rend_GLES2()
{
	return new OpenGLRenderer();
}
