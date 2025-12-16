#include "glcache.h"
#include "gles.h"
#include "quad.h"
#include "hw/pvr/ta.h"
#ifndef LIBRETRO
#include "ui/gui.h"
#else
#include "rend/gles/postprocess.h"
#include "vmu_xhair.h"
#endif
#include "rend/transform_matrix.h"
#include "wsi/gl_context.h"
#include "emulator.h"
#include "naomi2.h"
#include "oslib/i18n.h"

#ifdef TEST_AUTOMATION
#include "cfg/cfg.h"
#endif

#include <cmath>
#include <memory>

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
#define FOG_CHANNEL r						
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

const char* GouraudSource = R"(
#if (TARGET_GL == GL3 || TARGET_GL == GLES3) && pp_Gouraud == 0
	#define INTERPOLATION flat
#else
	#define INTERPOLATION
#endif
)";

static const char* VertexShaderSource = R"(
/* Vertex constants*/ 
uniform highp vec4 depth_scale;
uniform highp mat4 ndcMat;
uniform highp float sp_FOG_DENSITY;

/* Vertex input */
in highp vec4 in_pos;
in lowp vec4 in_base;
in lowp vec4 in_offs;
in highp vec2 in_uv;
/* output */
INTERPOLATION out highp vec4 vtx_base;
INTERPOLATION out highp vec4 vtx_offs;
out highp vec3 vtx_uv;

void main()
{
	highp vec4 vpos = ndcMat * in_pos;
	vtx_base = in_base;
	vtx_offs = in_offs;
#if TARGET_GL == GLES2
	vtx_uv = vec3(in_uv, vpos.z * sp_FOG_DENSITY);
	vpos.w = 1.0 / vpos.z;
	vpos.z = depth_scale.x + depth_scale.y * vpos.w;
	vpos.xy *= vpos.w;
#else
	#if DIV_POS_Z == 1
		vpos /= vpos.z;
		vpos.z = vpos.w;
	#endif
	#if pp_Gouraud == 1 && DIV_POS_Z != 1
		vtx_base *= vpos.z;
		vtx_offs *= vpos.z;
	#endif
	vtx_uv = vec3(in_uv, vpos.z);
	#if DIV_POS_Z != 1
		vtx_uv.xy *= vpos.z;
		vpos.w = 1.0;
		vpos.z = 0.0;
	#endif
#endif
	gl_Position = vpos;
}
)";

const char* PixelPipelineShader = R"(
#define PI 3.1415926

/* Shader program params*/
/* gles has no alpha test stage, so its emulated on the shader */
uniform highp float cp_AlphaTestValue;
uniform lowp vec4 pp_ClipTest;
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT;
uniform highp float sp_FOG_DENSITY;
uniform sampler2D tex,fog_table;
uniform lowp float trilinear_alpha;
uniform lowp vec4 fog_clamp_min;
uniform lowp vec4 fog_clamp_max;
#if pp_Palette != 0
uniform sampler2D palette;
uniform mediump int palette_index;
#if TARGET_GL == GLES2 || TARGET_GL == GL2
uniform lowp vec2 texSize;
#endif
#endif
#if DITHERING == 1
uniform lowp vec4 ditherDivisor;
#endif

/* Vertex input*/
INTERPOLATION in highp vec4 vtx_base;
INTERPOLATION in highp vec4 vtx_offs;
in highp vec3 vtx_uv;

lowp float fog_mode2(highp float w)
{
	highp float z = clamp(
#if TARGET_GL == GLES2
						  vtx_uv.z
#elif DIV_POS_Z == 1
						  sp_FOG_DENSITY / w
#else
						  sp_FOG_DENSITY * w
#endif
											, 1.0, 255.9999);
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

#if pp_Palette != 0

lowp vec4 getPaletteEntry(highp float colorIndex)
{
	highp int color_idx = int(floor(colorIndex * 255.0 + 0.5)) + palette_index;
#if TARGET_GL == GLES2 || TARGET_GL == GL2
    highp vec2 c = vec2((mod(float(color_idx), 32.0) * 2.0 + 1.0) / 64.0, (float(color_idx / 32) * 2.0 + 1.0) / 64.0);
	return texture(palette, c);
#else
    highp ivec2 c = ivec2(color_idx % 32, color_idx / 32);
	return texelFetch(palette, c, 0);
#endif
}

#endif

#if pp_Palette == 1		// Nearest filtering

lowp vec4 palettePixel(highp vec3 coords)
{
#if TARGET_GL == GLES2 || TARGET_GL == GL2 || DIV_POS_Z == 1
	return getPaletteEntry(texture(tex, coords.xy).FOG_CHANNEL);
#else
	return getPaletteEntry(textureProj(tex, coords).FOG_CHANNEL);
#endif

}

#elif pp_Palette == 2		// Bi-linear filtering

lowp vec4 palettePixelBilinear(highp vec3 coords)
{
#if TARGET_GL != GLES2 && TARGET_GL != GL2 && DIV_POS_Z != 1
	coords.xy /= coords.z;
#endif
#if TARGET_GL != GLES2 && TARGET_GL != GL2
	lowp vec2 texSize = vec2(textureSize(tex, 0));
#endif
	highp vec2 pixCoord = coords.xy * texSize - 0.5;		// coordinates of top left pixel
	highp vec2 originPixCoord = floor(pixCoord);

	highp vec2 sampleUV = (originPixCoord + 0.5) / texSize;	// UV coordinates of center of top left pixel

    // Sample from all surrounding texels
    lowp vec4 c00 = getPaletteEntry(texture(tex, sampleUV).FOG_CHANNEL);
#if TARGET_GL != GLES2 && TARGET_GL != GL2
    lowp vec4 c01 = getPaletteEntry(textureOffset(tex, sampleUV, ivec2(0, 1)).FOG_CHANNEL);
    lowp vec4 c11 = getPaletteEntry(textureOffset(tex, sampleUV, ivec2(1, 1)).FOG_CHANNEL);
    lowp vec4 c10 = getPaletteEntry(textureOffset(tex, sampleUV, ivec2(1, 0)).FOG_CHANNEL);
#else
	sampleUV = (originPixCoord + vec2(0.5, 1.5)) / texSize;
    lowp vec4 c01 = getPaletteEntry(texture(tex, sampleUV).FOG_CHANNEL);
	sampleUV = (originPixCoord + vec2(1.5, 1.5)) / texSize;
    lowp vec4 c11 = getPaletteEntry(texture(tex, sampleUV).FOG_CHANNEL);
	sampleUV = (originPixCoord + vec2(1.5, 0.5)) / texSize;
    lowp vec4 c10 = getPaletteEntry(texture(tex, sampleUV).FOG_CHANNEL);
#endif

	highp vec2 weight = pixCoord - originPixCoord;

    // Bi-linear mixing
    lowp vec4 temp0 = mix(c00, c10, weight.x);
    lowp vec4 temp1 = mix(c01, c11, weight.x);
    return mix(temp0, temp1, weight.y);
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
	#if pp_Gouraud == 1 && TARGET_GL != GLES2 && DIV_POS_Z != 1
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
		  #if TARGET_GL == GLES2 || TARGET_GL == GL2 || DIV_POS_Z == 1
			lowp vec4 texcol = texture(tex, vtx_uv.xy);
		  #else
			lowp vec4 texcol = textureProj(tex, vtx_uv);
		  #endif
		#elif pp_Palette == 1
			lowp vec4 texcol = palettePixel(vtx_uv);
		#else
			lowp vec4 texcol = palettePixelBilinear(vtx_uv);
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
	
	#if cp_AlphaTest == 1
		color.a = floor(color.a * 255.0 + 0.5) / 255.0;
		if (cp_AlphaTestValue > color.a)
			discard;
		color.a = 1.0;
	#endif 

	//color.rgb = vec3(vtx_uv.z * sp_FOG_DENSITY / 128.0);
#if TARGET_GL != GLES2
#if DIV_POS_Z == 1
	highp float w = 100000.0 / vtx_uv.z;
#else
	highp float w = 100000.0 * vtx_uv.z;
#endif
	gl_FragDepth = log2(1.0 + max(w, -0.999999)) / 34.0;

#if DITHERING == 1
	mediump float ditherTable[16] = float[](
		5., 13.,  7., 15.,
		9.,  1., 11.,  3.,
		6., 14.,  4., 12.,
		10., 2.,  8.,  0.
	);
	mediump float r = ditherTable[int(mod(gl_FragCoord.y, 4.)) * 4 + int(mod(gl_FragCoord.x, 4.))];
	mediump vec4 dv = vec4(r, r, r, 1.) / ditherDivisor;
	color = clamp(floor(color * 255. + dv) / 255., 0., 1.);
#endif
#endif
	gl_FragColor = color;
}
)";

static const char* ModifierVolumeShader = R"(
uniform lowp float sp_ShaderColor;

/* Vertex input*/
in highp vec3 vtx_uv;

void main()
{
#if TARGET_GL != GLES2
#if DIV_POS_Z == 1
	highp float w = 100000.0 / vtx_uv.z;
#else
	highp float w = 100000.0 * vtx_uv.z;
#endif
	gl_FragDepth = log2(1.0 + max(w, -0.999999)) / 34.0;
#endif
	gl_FragColor=vec4(0.0, 0.0, 0.0, sp_ShaderColor);
}
)";

void os_VideoRoutingTermGL();

GLCache glcache;
gl_ctx gl;

GLuint fogTextureId;
GLuint paletteTextureId;

glm::mat4 ViewportMatrix;

#ifdef TEST_AUTOMATION
void do_swap_automation()
{
	static FILE* video_file = fopen(config::loadStr("record", "rawvid").c_str(), "wb");
	extern bool do_screenshot;

	GlFramebuffer *framebuffer = gl.ofbo2.ready ? gl.ofbo2.framebuffer.get() : gl.ofbo.framebuffer.get();
	if (framebuffer == nullptr)
		return;
	int bytesz = framebuffer->getWidth() * framebuffer->getHeight() * 3;
	if (video_file)
	{
		u8* img = new u8[bytesz];
		
		framebuffer->bind(GL_READ_FRAMEBUFFER);
		glReadPixels(0, 0, framebuffer->getWidth(), framebuffer->getHeight(), GL_RGB, GL_UNSIGNED_BYTE, img);
		fwrite(img, 1, bytesz, video_file);
		delete[] img;
		fflush(video_file);
	}

	if (do_screenshot)
	{
		u8* img = new u8[bytesz];
		
		framebuffer->bind(GL_READ_FRAMEBUFFER);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, framebuffer->getWidth(), framebuffer->getHeight(), GL_RGB, GL_UNSIGNED_BYTE, img);
		dump_screenshot(img, framebuffer->getWidth(), framebuffer->getHeight());
		delete[] img;
		dc_exit();
		void sdl_window_destroy();
		sdl_window_destroy(); // avoid crash
		flycast_term();
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
	glcache.DeleteProgram(gl.n2ModVolShader.program);
	gl.n2ModVolShader.program = 0;
}

void termGLCommon()
{
#ifdef VIDEO_ROUTING
	os_VideoRoutingTermGL();
#endif
	gl.quad.reset();

	// palette, fog
	glcache.DeleteTextures(1, &fogTextureId);
	fogTextureId = 0;
	glcache.DeleteTextures(1, &paletteTextureId);
	paletteTextureId = 0;
	// RTT
	gl.rtt.framebuffer.reset();

	gl.ofbo.framebuffer.reset();
	glcache.DeleteTextures(1, &gl.dcfb.tex);
	gl.dcfb.tex = 0;
	gl.ofbo2.framebuffer.reset();
	gl.fbscaling.framebuffer.reset();
	gl.videorouting.framebuffer.reset();
	termVmuLightgun();
#ifdef LIBRETRO
	postProcessor.term();
#endif
}

static void gles_term()
{
	gl.vbo.mainVAO.term();
	gl.vbo.modvolVAO.term();
	gl.vbo.geometry.reset();
	gl.vbo.modvols.reset();
	gl.vbo.idxs.reset();
	termGLCommon();

	gl_delete_shaders();
}

bool testBlitFramebuffer();

void findGLVersion()
{
	gl.index_type = GL_UNSIGNED_INT;
	gl.gl_major = theGLContext.getMajorVersion();
	gl.gl_minor = theGLContext.getMinorVersion();
	gl.is_gles = theGLContext.isGLES();
	if (gl.is_gles)
	{
		gl.border_clamp_supported = false;
		if (gl.gl_major >= 3)
		{
			gl.gl_version = "GLES3";
			gl.glsl_version_header = "#version 300 es";
			if (gl.gl_major > 3 || gl.gl_minor >= 2)
		    	gl.border_clamp_supported = true;
			gl.prim_restart_supported = false;
			gl.prim_restart_fixed_supported = true;
			gl.single_channel_format = GL_RED;
		}
		else
		{
			gl.gl_version = "GLES2";
			gl.glsl_version_header = "";
			gl.index_type = GL_UNSIGNED_SHORT;
			gl.prim_restart_supported = false;
			gl.prim_restart_fixed_supported = false;
			gl.single_channel_format = GL_ALPHA;
		}
		const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
		if (strstr(extensions, "GL_OES_packed_depth_stencil") != NULL)
			gl.GL_OES_packed_depth_stencil_supported = true;
		if (strstr(extensions, "GL_OES_depth24") != NULL)
			gl.GL_OES_depth24_supported = true;
		if (!gl.GL_OES_packed_depth_stencil_supported && gl.gl_major < 3)
			INFO_LOG(RENDERER, "Packed depth/stencil not supported: no modifier volumes when rendering to a texture");
		GLint ranges[2];
		GLint precision;
		glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, ranges, &precision);
		gl.highp_float_supported = (ranges[0] != 0 || ranges[1] != 0) && precision != 0;
		if (!gl.border_clamp_supported)
			gl.border_clamp_supported = strstr(extensions, "GL_EXT_texture_border_clamp") != nullptr;
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
			gl.prim_restart_supported = gl.gl_major > 3 || gl.gl_minor >= 1; // 3.1 min
			gl.prim_restart_fixed_supported = gl.gl_major > 4
					|| (gl.gl_major == 4 && gl.gl_minor >= 3);				// 4.3 min
		}
		else
		{
			gl.gl_version = "GL2";
			gl.glsl_version_header = "#version 120";
			gl.single_channel_format = GL_ALPHA;
			gl.prim_restart_supported = false;
			gl.prim_restart_fixed_supported = false;
		}
    	gl.highp_float_supported = true;
    	gl.border_clamp_supported = true;
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
	const char *vendor = (const char *)glGetString(GL_VENDOR);
	const char *renderer = (const char *)glGetString(GL_RENDERER);
	gl.mesa_nouveau = !stricmp(vendor, "nouveau")
			|| (!stricmp(vendor, "Mesa") && !strncmp(renderer, "NV", 2));
	gl.mali = !stricmp(vendor, "arm");
	NOTICE_LOG(RENDERER, "OpenGL%s version %d.%d", gl.is_gles ? " ES" : "", gl.gl_major, gl.gl_minor);
	NOTICE_LOG(RENDERER, "Vendor '%s' Renderer '%s' Version '%s'", vendor, renderer, glGetString(GL_VERSION));
	while (glGetError() != GL_NO_ERROR)
		;
	gl.bogusBlitFramebuffer = true;	// not supported in GL/GLES 2
#ifndef GLES2
	if (gl.gl_major >= 3)
	{
		gl.bogusBlitFramebuffer = !testBlitFramebuffer();
		if (gl.bogusBlitFramebuffer)
			WARN_LOG(RENDERER, "glBlitFramebuffer is bogus. Using quad drawer instead");
		else
			NOTICE_LOG(RENDERER, "glBlitFramebuffer test successful");
	}
#endif
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

GLuint gl_CompileAndLink(const char *vertexShader, const char *fragmentShader)
{
	//create shaders
	GLuint vs = gl_CompileShader(vertexShader, GL_VERTEX_SHADER);
	GLuint ps = gl_CompileShader(fragmentShader, GL_FRAGMENT_SHADER);

	GLuint program = glCreateProgram();
	glAttachShader(program, vs);
	glAttachShader(program, ps);

	//bind vertex attribute to vbo inputs
	glBindAttribLocation(program, VERTEX_POS_ARRAY,      "in_pos");
	glBindAttribLocation(program, VERTEX_COL_BASE_ARRAY, "in_base");
	glBindAttribLocation(program, VERTEX_COL_OFFS_ARRAY, "in_offs");
	glBindAttribLocation(program, VERTEX_UV_ARRAY,       "in_uv");
	// Per-pixel attributes
	glBindAttribLocation(program, VERTEX_COL_BASE1_ARRAY, "in_base1");
	glBindAttribLocation(program, VERTEX_COL_OFFS1_ARRAY, "in_offs1");
	glBindAttribLocation(program, VERTEX_UV1_ARRAY,       "in_uv1");
	// Naomi 2
	glBindAttribLocation(program, VERTEX_NORM_ARRAY,     "in_normal");

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
		INFO_LOG(RENDERER, "// VERTEX SHADER\n%s\n// END", vertexShader);
		INFO_LOG(RENDERER, "// FRAGMENT SHADER\n%s\n// END", fragmentShader);
		die("shader compile fail\n");
	}
	glDetachShader(program, vs);
	glDetachShader(program, ps);
	glDeleteShader(vs);
	glDeleteShader(ps);

	glcache.UseProgram(program);

	return program;
}

PipelineShader *GetProgram(bool cp_AlphaTest, bool pp_InsideClipping,
		bool pp_Texture, bool pp_UseAlpha, bool pp_IgnoreTexA, u32 pp_ShadInstr, bool pp_Offset,
		u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear,
		int palette, bool naomi2, bool dithering)
{
	u32 rv=0;

	rv |= pp_InsideClipping;
	rv <<= 1; rv |= cp_AlphaTest;
	rv <<= 1; rv |= pp_Texture;
	rv <<= 1; rv |= pp_UseAlpha;
	rv <<= 1; rv |= pp_IgnoreTexA;
	rv <<= 2; rv |= pp_ShadInstr;
	rv <<= 1; rv |= pp_Offset;
	rv <<= 2; rv |= pp_FogCtrl;
	rv <<= 1; rv |= pp_Gouraud;
	rv <<= 1; rv |= pp_BumpMap;
	rv <<= 1; rv |= fog_clamping;
	rv <<= 1; rv |= trilinear;
	rv <<= 2; rv |= palette;
	rv <<= 1; rv |= naomi2;
	rv <<= 1, rv |= !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
	rv <<= 1; rv |= dithering;

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
		shader->naomi2 = naomi2;
		shader->divPosZ = !settings.platform.isNaomi2() && config::NativeDepthInterpolation;
		shader->dithering = dithering;
		CompilePipelineShader(shader);
	}

	return shader;
}

class VertexSource : public OpenGlSource
{
public:
	VertexSource(bool gouraud, bool divPosZ) : OpenGlSource() {
		addConstant("pp_Gouraud", gouraud);
		addConstant("DIV_POS_Z", divPosZ);

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
		addConstant("DIV_POS_Z", s->divPosZ);
		addConstant("DITHERING", s->dithering);

		addSource(PixelCompatShader);
		addSource(GouraudSource);
		addSource(PixelPipelineShader);
	}
};

bool CompilePipelineShader(PipelineShader* s)
{
	std::string vertexShader;
	if (s->naomi2)
		vertexShader = N2VertexSource(s->pp_Gouraud, false, s->pp_Texture).generate();
	else
		vertexShader = VertexSource(s->pp_Gouraud, s->divPosZ).generate();
	FragmentShaderSource fragmentSource(s);

	s->program = gl_CompileAndLink(vertexShader.c_str(), fragmentSource.generate().c_str());

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
	s->ndcMat = glGetUniformLocation(s->program, "ndcMat");
	s->ditherDivisor = glGetUniformLocation(s->program, "ditherDivisor");
	s->texSize = glGetUniformLocation(s->program, "texSize");

	if (s->naomi2)
		initN2Uniforms(s);

	ShaderUniforms.Set(s);

	return true;
}

static void create_modvol_shader()
{
	if (gl.modvol_shader.program != 0)
		return;
	VertexSource vertexShader(false, config::NativeDepthInterpolation);

	OpenGlSource fragmentShader;
	fragmentShader.addConstant("pp_Gouraud", 0)
			.addConstant("DIV_POS_Z", config::NativeDepthInterpolation)
			.addSource(PixelCompatShader)
			.addSource(GouraudSource)
			.addSource(ModifierVolumeShader);

	gl.modvol_shader.program = gl_CompileAndLink(vertexShader.generate().c_str(), fragmentShader.generate().c_str());
	gl.modvol_shader.ndcMat = glGetUniformLocation(gl.modvol_shader.program, "ndcMat");
	gl.modvol_shader.sp_ShaderColor = glGetUniformLocation(gl.modvol_shader.program, "sp_ShaderColor");
	gl.modvol_shader.depth_scale = glGetUniformLocation(gl.modvol_shader.program, "depth_scale");

	if (gl.gl_major >= 3)
	{
		N2VertexSource n2vertexShader(false, true, false);
		fragmentShader.setConstant("DIV_POS_Z", false);
		gl.n2ModVolShader.program = gl_CompileAndLink(n2vertexShader.generate().c_str(), fragmentShader.generate().c_str());
		gl.n2ModVolShader.ndcMat = glGetUniformLocation(gl.n2ModVolShader.program, "ndcMat");
		gl.n2ModVolShader.sp_ShaderColor = glGetUniformLocation(gl.n2ModVolShader.program, "sp_ShaderColor");
		gl.n2ModVolShader.depth_scale = glGetUniformLocation(gl.n2ModVolShader.program, "depth_scale");
		gl.n2ModVolShader.mvMat = glGetUniformLocation(gl.n2ModVolShader.program, "mvMat");
		gl.n2ModVolShader.projMat = glGetUniformLocation(gl.n2ModVolShader.program, "projMat");
	}
}

static void gl_create_resources()
{
	if (gl.vbo.geometry != nullptr)
		// Assume the resources have already been created
		return;

	findGLVersion();

#ifndef LIBRETRO
	if (gl.gl_major >= 3)
		// will be used later. Better fail fast
		verify(glGenVertexArrays != nullptr);
#endif

	//create vbos
	gl.vbo.geometry = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER);
	gl.vbo.modvols = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER);
	gl.vbo.idxs = std::make_unique<GlBuffer>(GL_ELEMENT_ARRAY_BUFFER);

	gl.quad = std::make_unique<GlQuadDrawer>();
}

GLuint gl_CompileShader(const char* shader,GLuint type);

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

bool OpenGLRenderer::Init()
{
	glcache.EnableCache();

	gl_create_resources();

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
	updateFogTable = true;
	TextureCacheData::SetDirectXColorOrder(false);
	TextureCacheData::setUploadToGPUFlavor();

	return true;
}

static void updateFogTexture(u8 *fog_table, GLenum texture_slot, GLint fog_image_format)
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
	else {
		glcache.BindTexture(GL_TEXTURE_2D, fogTextureId);
	}

	u8 temp_tex_buffer[256];
	MakeFogTexture(temp_tex_buffer);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	GLint internalformat;
	if (gl.is_gles && fog_image_format == GL_RED)
		internalformat = GL_R8;
	else
		internalformat = fog_image_format;
	glTexImage2D(GL_TEXTURE_2D, 0, internalformat, 128, 2, 0, fog_image_format, GL_UNSIGNED_BYTE, temp_tex_buffer);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}

static void updatePaletteTexture(GLenum texture_slot)
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
	else {
		glcache.BindTexture(GL_TEXTURE_2D, paletteTextureId);
	}

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, palette32_ram);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}

void OpenGLRenderer::drawOSD()
{
	drawVmusAndCrosshairs(width, height);
#ifndef LIBRETRO
	gui_display_osd();
#endif
}

void OpenGLRenderer::Process(TA_context* ctx)
{
	if (gl.gl_major < 3 && settings.platform.isNaomi2())
		throw FlycastException(i18n::Ts("OpenGL ES 3.0+ required for Naomi 2"));

	if (resetTextureCache) {
		TexCache.Clear();
		resetTextureCache = false;
	}
	TexCache.Cleanup();

	if (updateFogTable && config::Fog) {
		updateFogTable = false;
		updateFogTexture((u8 *)FOG_TABLE, getFogTextureSlot(), gl.single_channel_format);
	}
	if (updatePalette) {
		updatePaletteTexture(getPaletteTextureSlot());
		updatePalette = false;
	}
	ta_parse(ctx, gl.prim_restart_fixed_supported || gl.prim_restart_supported);
}

static void upload_vertex_indices()
{
	if (gl.index_type == GL_UNSIGNED_SHORT)
	{
		static std::vector<u16> short_idx;
		short_idx.clear();
		short_idx.reserve(pvrrc.idx.size());
		for (u32 i : pvrrc.idx)
			short_idx.push_back(i);
		gl.vbo.idxs->update(short_idx.data(), short_idx.size() * sizeof(u16));
	}
	else
		gl.vbo.idxs->update(pvrrc.idx.data(), pvrrc.idx.size() * sizeof(decltype(*pvrrc.idx.data())));
	glCheck();
}

bool OpenGLRenderer::renderFrame(int width, int height)
{
	if (!config::EmulateFramebuffer)
		initVideoRoutingFrameBuffer();
	
	bool is_rtt = pvrrc.isRTT;

	float vtx_max_fZ = pvrrc.fZ_max;

	//sanitise the values, now with NaN detection (for omap)
	//0x49800000 is 1024*1024. Using integer math to avoid issues w/ infs and nans
	if ((s32&)vtx_max_fZ < 0 || (u32&)vtx_max_fZ > 0x49800000)
		vtx_max_fZ = 10 * 1024;

	//add some extra range to avoid clipping border cases
	vtx_max_fZ *= 1.001f;

	TransformMatrix<COORD_OPENGL> matrices(pvrrc, is_rtt ? pvrrc.getFramebufferWidth() : width,
			is_rtt ? pvrrc.getFramebufferHeight() : height);
	ShaderUniforms.ndcMat = matrices.GetNormalMatrix();
	const glm::mat4& scissor_mat = matrices.GetScissorMatrix();
	ViewportMatrix = matrices.GetViewportMatrix();

	ShaderUniforms.depth_coefs[0] = 2.f / vtx_max_fZ;
	ShaderUniforms.depth_coefs[1] = -1.f;
	ShaderUniforms.depth_coefs[2] = 0;
	ShaderUniforms.depth_coefs[3] = 0;

	//VERT and RAM fog color constants
	FOG_COL_VERT.getRGBColor(ShaderUniforms.ps_FOG_COL_VERT);
	FOG_COL_RAM.getRGBColor(ShaderUniforms.ps_FOG_COL_RAM);

	//Fog density constant
	ShaderUniforms.fog_den_float = FOG_DENSITY.get() * config::ExtraDepthScale;

	pvrrc.fog_clamp_min.getRGBAColor(ShaderUniforms.fog_clamp_min);
	pvrrc.fog_clamp_max.getRGBAColor(ShaderUniforms.fog_clamp_max);
	
	if (config::ModifierVolumes)
	{
		create_modvol_shader();
		glcache.UseProgram(gl.modvol_shader.program);
		if (gl.modvol_shader.depth_scale != -1)
			glUniform4fv(gl.modvol_shader.depth_scale, 1, ShaderUniforms.depth_coefs);
		glUniformMatrix4fv(gl.modvol_shader.ndcMat, 1, GL_FALSE, &ShaderUniforms.ndcMat[0][0]);
		glUniform1f(gl.modvol_shader.sp_ShaderColor, 1 - FPU_SHAD_SCALE.scale_factor / 256.f);

		glcache.UseProgram(gl.n2ModVolShader.program);
		if (gl.n2ModVolShader.depth_scale != -1)
			glUniform4fv(gl.n2ModVolShader.depth_scale, 1, ShaderUniforms.depth_coefs);
		glUniformMatrix4fv(gl.n2ModVolShader.ndcMat, 1, GL_FALSE, &ShaderUniforms.ndcMat[0][0]);
		glUniform1f(gl.n2ModVolShader.sp_ShaderColor, 1 - FPU_SHAD_SCALE.scale_factor / 256.f);
	}

	ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	if (config::EmulateFramebuffer && pvrrc.fb_W_CTRL.fb_dither && pvrrc.fb_W_CTRL.fb_packmode <= 3)
	{
		ShaderUniforms.dithering = true;
		switch (pvrrc.fb_W_CTRL.fb_packmode)
		{
		case 0: // 0555 KRGB 16 bit
		case 3: // 1555 ARGB 16 bit
			ShaderUniforms.ditherDivisor[0] = ShaderUniforms.ditherDivisor[1] = ShaderUniforms.ditherDivisor[2] = 2.f;
			break;
		case 1: // 565 RGB 16 bit
			ShaderUniforms.ditherDivisor[0] = ShaderUniforms.ditherDivisor[2] = 2.f;
			ShaderUniforms.ditherDivisor[1] = 4.f;
			break;
		case 2: // 4444 ARGB 16 bit
			ShaderUniforms.ditherDivisor[0] = ShaderUniforms.ditherDivisor[1] = ShaderUniforms.ditherDivisor[2] = 1.f;
			break;
		default:
			break;
		}
		ShaderUniforms.ditherDivisor[3] = 1.f;
	}
	else
	{
		ShaderUniforms.dithering = false;
	}

	for (auto& it : gl.shaders)
	{
		glcache.UseProgram(it.second.program);
		ShaderUniforms.Set(&it.second);
		resetN2UniformCache(&it.second);
	}
#ifndef GLES2
	if (gl.prim_restart_fixed_supported)
		glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
#ifndef GLES
	else if (gl.prim_restart_supported) {
		glEnable(GL_PRIMITIVE_RESTART);
		glPrimitiveRestartIndex(-1);
	}
#endif
#endif
	//setup render target first
	if (is_rtt)
	{
		if (BindRTT() == 0)
			return false;
	}
	else
	{
		this->width = width;
		this->height = height;
		getVideoShift(gl.ofbo.shiftX, gl.ofbo.shiftY);
#ifdef LIBRETRO
		if (config::EmulateFramebuffer)
		{
			if (init_output_framebuffer(width, height) == 0)
				return false;
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, postProcessor.getFramebuffer(width, height));
			glViewport(0, 0, width, height);
		}
#else
		if (init_output_framebuffer(width, height) == 0)
			return false;
#endif
	}

	bool wide_screen_on = !is_rtt && config::Widescreen && !matrices.IsClipped()
			&& !config::Rotate90 && !config::EmulateFramebuffer;

	//Color is cleared by the background plane

	glcache.Disable(GL_SCISSOR_TEST);

	glcache.DepthMask(GL_TRUE);
	glClearDepthf(0.0);
	glStencilMask(0xFF); glCheck();
    glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();
	if (!is_rtt)
		glcache.ClearColor(VO_BORDER_COL.red(), VO_BORDER_COL.green(), VO_BORDER_COL.blue(), 1.f);
	else
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);

	if (is_rtt || pvrrc.clearFramebuffer)
		glClear(GL_COLOR_BUFFER_BIT);
	//move vertex to gpu
	//Main VBO
	gl.vbo.geometry->update(&pvrrc.verts[0], pvrrc.verts.size() * sizeof(decltype(pvrrc.verts[0])));

	upload_vertex_indices();

	//Modvol VBO
	if (!pvrrc.modtrig.empty())
		gl.vbo.modvols->update(&pvrrc.modtrig[0], pvrrc.modtrig.size() * sizeof(decltype(pvrrc.modtrig[0])));

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
			min_x = (float)pvrrc.getFramebufferMinX();
			min_y = (float)pvrrc.getFramebufferMinY();
			fWidth = (float)pvrrc.getFramebufferWidth() - min_x;
			fHeight = (float)pvrrc.getFramebufferHeight() - min_y;
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
	if (!is_rtt && !config::EmulateFramebuffer)
		postProcessor.render(glsm_get_current_framebuffer());
#endif

	if (is_rtt)
		ReadRTTBuffer();
	else if (config::EmulateFramebuffer)
		writeFramebufferToVRAM();
	else {
		gl.ofbo.aspectRatio = getOutputFramebufferAspectRatio();
#ifndef LIBRETRO
		gl.ofbo2.ready = false;
		renderLastFrame();
#endif
	}
	GlVertexArray::unbind();

	return !is_rtt;
}

void OpenGLRenderer::initVideoRoutingFrameBuffer()
{
#ifdef VIDEO_ROUTING
	if (config::VideoRouting)
	{
		int targetWidth = (config::VideoRoutingScale ? config::VideoRoutingVRes * settings.display.width / settings.display.height : settings.display.width);
		int targetHeight = (config::VideoRoutingScale ? config::VideoRoutingVRes : settings.display.height);
		if (gl.videorouting.framebuffer != nullptr
			&& (gl.videorouting.framebuffer->getWidth() != targetWidth || gl.videorouting.framebuffer->getHeight() != targetHeight))
			gl.videorouting.framebuffer.reset();
		if (gl.videorouting.framebuffer == nullptr)
			gl.videorouting.framebuffer = std::make_unique<GlFramebuffer>(targetWidth, targetHeight, true, true);
	}
#endif
}

void OpenGLRenderer::Term()
{
	TexCache.Clear();
	gles_term();
}

bool OpenGLRenderer::Render()
{
	saveCurrentFramebuffer();
	renderFrame(pvrrc.framebufferWidth, pvrrc.framebufferHeight);
	if (pvrrc.isRTT) {
		restoreCurrentFramebuffer();
		return false;
	}

	if (!config::EmulateFramebuffer)
	{
		frameRendered = true;
		clearLastFrame = false;
		drawOSD();
		renderVideoRouting();
	}
	restoreCurrentFramebuffer();

	return true;
}

void OpenGLRenderer::renderVideoRouting()
{
#ifdef VIDEO_ROUTING
	if (config::VideoRouting)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, gl.ofbo.origFbo);
		gl.videorouting.framebuffer->bind(GL_DRAW_FRAMEBUFFER);
		glcache.Disable(GL_SCISSOR_TEST);
		int targetWidth = (config::VideoRoutingScale ? config::VideoRoutingVRes * settings.display.width / settings.display.height : settings.display.width);
		int targetHeight = (config::VideoRoutingScale ? config::VideoRoutingVRes : settings.display.height);
		glBlitFramebuffer(0, 0, settings.display.width, settings.display.height,
						  0, 0, targetWidth, targetHeight,
						  GL_COLOR_BUFFER_BIT, GL_LINEAR);
		extern void os_VideoRoutingPublishFrameTexture(GLuint texID, GLuint texTarget, float w, float h);
		os_VideoRoutingPublishFrameTexture(gl.videorouting.framebuffer->getTexture(), GL_TEXTURE_2D, targetWidth, targetHeight);
	}
	else
	{
		os_VideoRoutingTermGL();
	}
#endif
}

Renderer* rend_GLES2()
{
	return new OpenGLRenderer();
}
