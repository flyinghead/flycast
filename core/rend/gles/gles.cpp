#include <math.h>
#include "glcache.h"
#include "rend/TexCache.h"
#include "cfg/cfg.h"
#include "rend/gui.h"

#ifdef TARGET_PANDORA
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>

#ifndef FBIO_WAITFORVSYNC
	#define FBIO_WAITFORVSYNC _IOW('F', 0x20, __u32)
#endif
int fbdev = -1;
#endif

#ifndef GLES
#if HOST_OS != OS_DARWIN
#undef ARRAY_SIZE	// macros are evil
#include <GL4/gl3w.c>
#pragma comment(lib,"Opengl32.lib")
#endif
#else
#ifndef GL_RED
#define GL_RED                            0x1903
#endif
#ifndef GL_MAJOR_VERSION
#define GL_MAJOR_VERSION                  0x821B
#endif
#endif
#ifdef _ANDROID
#include <android/native_window.h> // requires ndk r5 or newer
#endif
#include "deps/libpng/png.h"

/*
GL|ES 2
Slower, smaller subset of gl2

*Optimisation notes*
Keep stuff in packed ints
Keep data as small as possible
Keep vertex programs as small as possible
The drivers more or less suck. Don't depend on dynamic allocation, or any 'complex' feature
as it is likely to be problematic/slow
Do we really want to enable striping joins?

*Design notes*
Follow same architecture as the d3d renderer for now
Render to texture, keep track of textures in GL memory
Direct flip to screen (no vlbank/fb emulation)
Do we really need a combining shader? it is needlessly expensive for openGL | ES
Render contexts
Free over time? we actually care about ram usage here?
Limit max resource size? for psp 48k verts worked just fine

FB:
Pixel clip, mapping

SPG/VO:
mapping

TA:
Tile clip

*/

#include "oslib/oslib.h"
#include "rend/rend.h"
#include "input/gamepad.h"
#include <glm/gtx/transform.hpp>

static float fb_scale_x, fb_scale_y;

//Fragment and vertex shaders code
const char* VertexShaderSource =
"\
%s \n\
#define TARGET_GL %s \n\
#define pp_Gouraud %d \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in attribute \n\
#define out varying \n\
#endif \n\
 \n\
 \n\
#if TARGET_GL == GL3 || TARGET_GL == GLES3 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
#else \n\
#define INTERPOLATION \n\
#endif \n\
 \n\
/* Vertex constants*/  \n\
uniform highp vec4      scale; \n\
uniform highp vec4      depth_scale; \n\
uniform highp mat4 normal_matrix; \n\
/* Vertex input */ \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in lowp vec4     in_offs; \n\
in mediump vec2  in_uv; \n\
/* output */ \n\
INTERPOLATION out lowp vec4 vtx_base; \n\
INTERPOLATION out lowp vec4 vtx_offs; \n\
              out mediump vec2 vtx_uv; \n\
void main() \n\
{ \n\
	vtx_base = in_base; \n\
	vtx_offs = in_offs; \n\
	vtx_uv = in_uv; \n\
	highp vec4 vpos = in_pos; \n\
	if (vpos.z < 0.0 || vpos.z > 3.4e37) \n\
	{ \n\
		gl_Position = vec4(0.0, 0.0, 1.0, 1.0 / vpos.z); \n\
		return; \n\
	} \n\
	\n\
	vpos = normal_matrix * vpos; \n\
	vpos.w = 1.0 / vpos.z; \n\
#if TARGET_GL != GLES2 \n\
	vpos.z = vpos.w; \n\
#else \n\
	vpos.z = depth_scale.x + depth_scale.y * vpos.w;  \n\
#endif \n\
	vpos.xy *= vpos.w; \n\
	gl_Position = vpos; \n\
}";

const char* PixelPipelineShader =
"\
%s \n\
#define TARGET_GL %s \n\
\n\
#define cp_AlphaTest %d \n\
#define pp_ClipTestMode %d \n\
#define pp_UseAlpha %d \n\
#define pp_Texture %d \n\
#define pp_IgnoreTexA %d \n\
#define pp_ShadInstr %d \n\
#define pp_Offset %d \n\
#define pp_FogCtrl %d \n\
#define pp_Gouraud %d \n\
#define pp_BumpMap %d \n\
#define FogClamping %d \n\
#define pp_TriLinear %d \n\
#define PI 3.1415926 \n\
\n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES3 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#define FOG_CHANNEL a \n\
#elif TARGET_GL == GL3 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#define FOG_CHANNEL r \n\
#else \n\
#define in varying \n\
#define texture texture2D \n\
#define FOG_CHANNEL a \n\
#endif \n\
 \n\
 \n\
#if TARGET_GL == GL3 || TARGET_GL == GLES3 \n\
#if pp_Gouraud == 0 \n\
#define INTERPOLATION flat \n\
#else \n\
#define INTERPOLATION smooth \n\
#endif \n\
#else \n\
#define INTERPOLATION \n\
#endif \n\
 \n\
/* Shader program params*/ \n\
/* gles has no alpha test stage, so its emulated on the shader */ \n\
uniform lowp float cp_AlphaTestValue; \n\
uniform lowp vec4 pp_ClipTest; \n\
uniform lowp vec3 sp_FOG_COL_RAM,sp_FOG_COL_VERT; \n\
uniform highp float sp_FOG_DENSITY; \n\
uniform sampler2D tex,fog_table; \n\
uniform lowp float trilinear_alpha; \n\
uniform lowp vec4 fog_clamp_min; \n\
uniform lowp vec4 fog_clamp_max; \n\
uniform highp float extra_depth_scale; \n\
/* Vertex input*/ \n\
INTERPOLATION in lowp vec4 vtx_base; \n\
INTERPOLATION in lowp vec4 vtx_offs; \n\
			  in mediump vec2 vtx_uv; \n\
 \n\
lowp float fog_mode2(highp float w) \n\
{ \n\
	highp float z = clamp(w * extra_depth_scale * sp_FOG_DENSITY, 1.0, 255.9999); \n\
	highp float exp = floor(log2(z)); \n\
	highp float m = z * 16.0 / pow(2.0, exp) - 16.0; \n\
	lowp float idx = floor(m) + exp * 16.0 + 0.5; \n\
	highp vec4 fog_coef = texture(fog_table, vec2(idx / 128.0, 0.75 - (m - floor(m)) / 2.0)); \n\
	return fog_coef.FOG_CHANNEL; \n\
} \n\
 \n\
highp vec4 fog_clamp(lowp vec4 col) \n\
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
	lowp vec4 color=vtx_base; \n\
	#if pp_UseAlpha==0 \n\
		color.a=1.0; \n\
	#endif\n\
	#if pp_FogCtrl==3 \n\
		color=vec4(sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w)); \n\
	#endif\n\
	#if pp_Texture==1 \n\
	{ \n\
		lowp vec4 texcol=texture(tex, vtx_uv); \n\
		 \n\
		#if pp_BumpMap == 1 \n\
			highp float s = PI / 2.0 * (texcol.a * 15.0 * 16.0 + texcol.r * 15.0) / 255.0; \n\
			highp float r = 2.0 * PI * (texcol.g * 15.0 * 16.0 + texcol.b * 15.0) / 255.0; \n\
			texcol.a = clamp(vtx_offs.a + vtx_offs.r * sin(s) + vtx_offs.g * cos(s) * cos(r - 2.0 * PI * vtx_offs.b), 0.0, 1.0); \n\
			texcol.rgb = vec3(1.0, 1.0, 1.0);	 \n\
		#else\n\
			#if pp_IgnoreTexA==1 \n\
				texcol.a=1.0;	 \n\
			#endif\n\
			\n\
			#if cp_AlphaTest == 1 \n\
				if (cp_AlphaTestValue>texcol.a) discard;\n\
			#endif  \n\
		#endif \n\
		#if pp_ShadInstr==0 \n\
		{ \n\
			color=texcol; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==1 \n\
		{ \n\
			color.rgb*=texcol.rgb; \n\
			color.a=texcol.a; \n\
		} \n\
		#endif\n\
		#if pp_ShadInstr==2 \n\
		{ \n\
			color.rgb=mix(color.rgb,texcol.rgb,texcol.a); \n\
		} \n\
		#endif\n\
		#if  pp_ShadInstr==3 \n\
		{ \n\
			color*=texcol; \n\
		} \n\
		#endif\n\
		\n\
		#if pp_Offset==1 && pp_BumpMap == 0 \n\
		{ \n\
			color.rgb+=vtx_offs.rgb; \n\
		} \n\
		#endif\n\
	} \n\
	#endif\n\
	 \n\
	color = fog_clamp(color); \n\
	 \n\
	#if pp_FogCtrl == 0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_RAM.rgb,fog_mode2(gl_FragCoord.w));  \n\
	} \n\
	#endif\n\
	#if pp_FogCtrl == 1 && pp_Offset==1 && pp_BumpMap == 0 \n\
	{ \n\
		color.rgb=mix(color.rgb,sp_FOG_COL_VERT.rgb,vtx_offs.a); \n\
	} \n\
	#endif\n\
	 \n\
	#if pp_TriLinear == 1 \n\
	color *= trilinear_alpha; \n\
	#endif \n\
	 \n\
	#if cp_AlphaTest == 1 \n\
		color.a=1.0; \n\
	#endif  \n\
	//color.rgb=vec3(gl_FragCoord.w * sp_FOG_DENSITY / 128.0);\n\
#if TARGET_GL != GLES2 \n\
	highp float w = gl_FragCoord.w * 100000.0; \n\
	gl_FragDepth = log2(1.0 + w) / 34.0; \n\
#endif \n\
	gl_FragColor =color; \n\
}";

const char* ModifierVolumeShader =
"\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL != GLES2 && TARGET_GL != GL2 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#endif \n\
 \n\
uniform lowp float sp_ShaderColor; \n\
/* Vertex input*/ \n\
void main() \n\
{ \n\
#if TARGET_GL != GLES2 \n\
	highp float w = gl_FragCoord.w * 100000.0; \n\
	gl_FragDepth = log2(1.0 + w) / 34.0; \n\
#endif \n\
	gl_FragColor=vec4(0.0, 0.0, 0.0, sp_ShaderColor); \n\
}";

const char* OSD_VertexShader =
"\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL == GLES2 || TARGET_GL == GL2 \n\
#define in attribute \n\
#define out varying \n\
#endif \n\
 \n\
uniform highp vec4      scale; \n\
 \n\
in highp vec4    in_pos; \n\
in lowp vec4     in_base; \n\
in mediump vec2  in_uv; \n\
 \n\
out lowp vec4 vtx_base; \n\
out mediump vec2 vtx_uv; \n\
 \n\
void main() \n\
{ \n\
	vtx_base = in_base; \n\
	vtx_uv = in_uv; \n\
	highp vec4 vpos = in_pos; \n\
	\n\
	vpos.w = 1.0; \n\
	vpos.z = vpos.w; \n\
	vpos.xy = vpos.xy * scale.xy - scale.zw;  \n\
	gl_Position = vpos; \n\
}";

const char* OSD_Shader =
"\
%s \n\
#define TARGET_GL %s \n\
 \n\
#define GLES2 0 \n\
#define GLES3 1 \n\
#define GL2 2 \n\
#define GL3 3 \n\
 \n\
#if TARGET_GL == GL2 \n\
#define highp \n\
#define lowp \n\
#define mediump \n\
#endif \n\
#if TARGET_GL != GLES2 && TARGET_GL != GL2 \n\
out highp vec4 FragColor; \n\
#define gl_FragColor FragColor \n\
#else \n\
#define in varying \n\
#define texture texture2D \n\
#endif \n\
 \n\
in lowp vec4 vtx_base; \n\
in mediump vec2 vtx_uv; \n\
/* Vertex input*/ \n\
uniform sampler2D tex; \n\
void main() \n\
{ \n\
	mediump vec2 uv=vtx_uv; \n\
	uv.y=1.0-uv.y; \n\
	gl_FragColor = vtx_base*texture(tex,uv.st); \n\
}";

GLCache glcache;
gl_ctx gl;

int screen_width;
int screen_height;
GLuint fogTextureId;

glm::mat4 ViewportMatrix;

#ifdef TEST_AUTOMATION
void dump_screenshot(u8 *buffer, u32 width, u32 height)
{
	FILE *fp = fopen("screenshot.png", "wb");
	if (fp == NULL)
	{
		ERROR_LOG(RENDERER, "Failed to open screenshot.png for writing\n");
		return;
	}

	png_bytepp rows = (png_bytepp)malloc(height * sizeof(png_bytep));
	for (int y = 0; y < height; y++)
	{
		rows[height - y - 1] = (png_bytep)buffer + y * width * 3;
	}

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct(png_ptr);

	png_init_io(png_ptr, fp);


	// write header
	png_set_IHDR(png_ptr, info_ptr, width, height,
			 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
			 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	png_write_info(png_ptr, info_ptr);


	// write bytes
	png_write_image(png_ptr, rows);

	// end write
	png_write_end(png_ptr, NULL);
	fclose(fp);

	free(rows);

}
#endif

#ifdef USE_EGL

	extern "C" void load_gles_symbols();
	static bool created_context;

	bool egl_makecurrent()
	{
		if (gl.setup.surface == EGL_NO_SURFACE || gl.setup.context == EGL_NO_CONTEXT)
			return false;
		return eglMakeCurrent(gl.setup.display, gl.setup.surface, gl.setup.surface, gl.setup.context);
	}

	// Create a basic GLES context
	bool gl_init(void* wind, void* disp)
	{
		gl.setup.native_wind=(EGLNativeWindowType)wind;
		gl.setup.native_disp=(EGLNativeDisplayType)disp;

		//try to get a display
		gl.setup.display = eglGetDisplay(gl.setup.native_disp);

		//if failed, get the default display (this will not happen in win32)
		if(gl.setup.display == EGL_NO_DISPLAY)
			gl.setup.display = eglGetDisplay((EGLNativeDisplayType) EGL_DEFAULT_DISPLAY);


		// Initialise EGL
		EGLint maj, min;
		if (!eglInitialize(gl.setup.display, &maj, &min))
		{
			ERROR_LOG(RENDERER, "EGL Error: eglInitialize failed");
			return false;
		}

		if (gl.setup.surface == 0)
		{
			EGLint pi32ConfigAttribs[]  = {
					EGL_SURFACE_TYPE, EGL_WINDOW_BIT | EGL_SWAP_BEHAVIOR_PRESERVED_BIT,
					EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
					EGL_DEPTH_SIZE, 24,
					EGL_STENCIL_SIZE, 8,
					EGL_NONE
			};

			int num_config;

			EGLConfig config;
			if (!eglChooseConfig(gl.setup.display, pi32ConfigAttribs, &config, 1, &num_config) || (num_config != 1))
			{
                // Fall back to non preserved swap buffers
                EGLint pi32ConfigFallbackAttribs[] = {
						EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
						EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
						EGL_DEPTH_SIZE, 24,
						EGL_STENCIL_SIZE, 8,
						EGL_NONE
				};
				if (!eglChooseConfig(gl.setup.display, pi32ConfigFallbackAttribs, &config, 1, &num_config) || (num_config != 1))
				{
					ERROR_LOG(RENDERER, "EGL Error: eglChooseConfig failed");
					return false;
				}
			}
#ifdef _ANDROID
			EGLint format;
			if (!eglGetConfigAttrib(gl.setup.display, config, EGL_NATIVE_VISUAL_ID, &format))
			{
				ERROR_LOG(RENDERER, "eglGetConfigAttrib() returned error %x", eglGetError());
				return false;
			}
			ANativeWindow_setBuffersGeometry((ANativeWindow *)wind, 0, 0, format);
#endif
			gl.setup.surface = eglCreateWindowSurface(gl.setup.display, config, (EGLNativeWindowType)wind, NULL);

			if (gl.setup.surface == EGL_NO_SURFACE)
			{
				ERROR_LOG(RENDERER, "EGL Error: eglCreateWindowSurface failed: %x", eglGetError());
				return false;
			}

#ifndef GLES
			bool try_full_gl = true;
			if (!eglBindAPI(EGL_OPENGL_API))
			{
				INFO_LOG(RENDERER, "eglBindAPI(EGL_OPENGL_API) failed: %x", eglGetError());
				try_full_gl = false;
			}
			if (try_full_gl)
			{
				EGLint contextAttrs[] = { EGL_CONTEXT_MAJOR_VERSION_KHR, 3,
										  EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR,
										  EGL_NONE };
				gl.setup.context = eglCreateContext(gl.setup.display, config, NULL, contextAttrs);
				if (gl.setup.context != EGL_NO_CONTEXT)
				{
					egl_makecurrent();
					if (gl3wInit())
						ERROR_LOG(RENDERER, "gl3wInit() failed");
				}
			}
#endif
			if (gl.setup.context == EGL_NO_CONTEXT)
			{
				if (!eglBindAPI(EGL_OPENGL_ES_API))
				{
					ERROR_LOG(RENDERER, "eglBindAPI() failed: %x", eglGetError());
					return false;
				}
				EGLint contextAttrs[] = { EGL_CONTEXT_CLIENT_VERSION, 2 , EGL_NONE };

				gl.setup.context = eglCreateContext(gl.setup.display, config, NULL, contextAttrs);

				if (gl.setup.context == EGL_NO_CONTEXT)
				{
					ERROR_LOG(RENDERER, "eglCreateContext() failed: %x", eglGetError());
					return false;
				}
#ifdef GLES
// EGL only supports runtime loading with android? TDB
				load_gles_symbols();
#else
				egl_makecurrent();
				if (gl3wInit())
					INFO_LOG(RENDERER, "gl3wInit() failed");
#endif
			}
			created_context = true;
		}
		else if (glGetError == NULL)
		{
			// Needed when the context is not created here (Android, iOS)
			load_gles_symbols();
		}

		if (!egl_makecurrent())
		{
			ERROR_LOG(RENDERER, "eglMakeCurrent() failed: %x", eglGetError());
			return false;
		}

		EGLint w,h;
		eglQuerySurface(gl.setup.display, gl.setup.surface, EGL_WIDTH, &w);
		eglQuerySurface(gl.setup.display, gl.setup.surface, EGL_HEIGHT, &h);

		screen_width=w;
		screen_height=h;

		// Required when doing partial redraws
        if (!eglSurfaceAttrib(gl.setup.display, gl.setup.surface, EGL_SWAP_BEHAVIOR, EGL_BUFFER_PRESERVED))
        {
        	INFO_LOG(RENDERER, "Swap buffers are not preserved. Last frame copy enabled");
        	gl.swap_buffer_not_preserved = true;
        }

        INFO_LOG(RENDERER, "EGL config: %p, %p, %p %dx%d", gl.setup.context, gl.setup.display, gl.setup.surface, w, h);
		return true;
	}

	void egl_stealcntx()
	{
		gl.setup.context = eglGetCurrentContext();
		gl.setup.display = eglGetCurrentDisplay();
		gl.setup.surface = eglGetCurrentSurface(EGL_DRAW);
	}

	//swap buffers
	void gl_swap()
	{
#ifdef TARGET_PANDORA0
		if (fbdev >= 0)
		{
			int arg = 0;
			ioctl(fbdev,FBIO_WAITFORVSYNC,&arg);
		}
#endif
		eglSwapBuffers(gl.setup.display, gl.setup.surface);
	}

	void gl_term()
	{
		if (!created_context)
			return;
		created_context = false;
		eglMakeCurrent(gl.setup.display, NULL, NULL, EGL_NO_CONTEXT);
#if HOST_OS == OS_WINDOWS
		ReleaseDC((HWND)gl.setup.native_wind,(HDC)gl.setup.native_disp);
#else
		if (gl.setup.context != NULL)
			eglDestroyContext(gl.setup.display, gl.setup.context);
		if (gl.setup.surface != NULL)
			eglDestroySurface(gl.setup.display, gl.setup.surface);
#ifdef TARGET_PANDORA
		if (gl.setup.display)
			eglTerminate(gl.setup.display);
		if (fbdev>=0)
			close( fbdev );
		fbdev=-1;
#endif
#endif	// !OS_WINDOWS
		gl.setup.context = EGL_NO_CONTEXT;
		gl.setup.surface = EGL_NO_SURFACE;
		gl.setup.display = EGL_NO_DISPLAY;
	}

#elif HOST_OS == OS_WINDOWS && !defined(USE_SDL)
	#define WGL_DRAW_TO_WINDOW_ARB         0x2001
	#define WGL_ACCELERATION_ARB           0x2003
	#define WGL_SWAP_METHOD_ARB            0x2007
	#define WGL_SUPPORT_OPENGL_ARB         0x2010
	#define WGL_DOUBLE_BUFFER_ARB          0x2011
	#define WGL_PIXEL_TYPE_ARB             0x2013
	#define WGL_COLOR_BITS_ARB             0x2014
	#define WGL_DEPTH_BITS_ARB             0x2022
	#define WGL_STENCIL_BITS_ARB           0x2023
	#define WGL_FULL_ACCELERATION_ARB      0x2027
	#define WGL_SWAP_EXCHANGE_ARB          0x2028
	#define WGL_TYPE_RGBA_ARB              0x202B
	#define WGL_CONTEXT_MAJOR_VERSION_ARB  0x2091
	#define WGL_CONTEXT_MINOR_VERSION_ARB  0x2092
	#define WGL_CONTEXT_FLAGS_ARB              0x2094

	#define		WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
	#define 	WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
	#define 	WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
	#define 	WGL_CONTEXT_LAYER_PLANE_ARB   0x2093
	#define 	WGL_CONTEXT_FLAGS_ARB   0x2094
	#define 	WGL_CONTEXT_DEBUG_BIT_ARB   0x0001
	#define 	WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB   0x0002
	#define 	ERROR_INVALID_VERSION_ARB   0x2095
	#define		WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

	typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats,
															int *piFormats, UINT *nNumFormats);
	typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int *attribList);
	typedef BOOL (WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);

	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
	PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
	PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;


	HDC ourWindowHandleToDeviceContext;
	bool gl_init(void* hwnd, void* hdc)
	{
		if (ourWindowHandleToDeviceContext)
			// Already initialized
			return true;

		PIXELFORMATDESCRIPTOR pfd =
		{
				sizeof(PIXELFORMATDESCRIPTOR),
				1,
				PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
				PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
				32,                        //Colordepth of the framebuffer.
				0, 0, 0, 0, 0, 0,
				0,
				0,
				0,
				0, 0, 0, 0,
				24,                        //Number of bits for the depthbuffer
				8,                        //Number of bits for the stencilbuffer
				0,                        //Number of Aux buffers in the framebuffer.
				PFD_MAIN_PLANE,
				0,
				0, 0, 0
		};

		/*HDC*/ ourWindowHandleToDeviceContext = (HDC)hdc;//GetDC((HWND)hwnd);

		int  letWindowsChooseThisPixelFormat;
		letWindowsChooseThisPixelFormat = ChoosePixelFormat(ourWindowHandleToDeviceContext, &pfd);
		SetPixelFormat(ourWindowHandleToDeviceContext,letWindowsChooseThisPixelFormat, &pfd);

		HGLRC ourOpenGLRenderingContext = wglCreateContext(ourWindowHandleToDeviceContext);
		wglMakeCurrent (ourWindowHandleToDeviceContext, ourOpenGLRenderingContext);

		bool rv = true;

		if (rv) {

			wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
			if(!wglChoosePixelFormatARB)
			{
				return false;
			}

			wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			if(!wglCreateContextAttribsARB)
			{
				return false;
			}

			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
			if(!wglSwapIntervalEXT)
			{
				return false;
			}

			int attribs[] =
			{
				WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
				WGL_CONTEXT_MINOR_VERSION_ARB, 3,
				WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
				0
			};

			HGLRC m_hrc = wglCreateContextAttribsARB(ourWindowHandleToDeviceContext,0, attribs);

			if (!m_hrc)
			{
				INFO_LOG(RENDERER, "Open GL 4.3 not supported");
				// Try Gl 3.1
				attribs[1] = 3;
				attribs[3] = 1;
				m_hrc = wglCreateContextAttribsARB(ourWindowHandleToDeviceContext,0, attribs);
			}

			if (m_hrc)
				wglMakeCurrent(ourWindowHandleToDeviceContext,m_hrc);
			else
				rv = false;

			wglDeleteContext(ourOpenGLRenderingContext);
		}

		if (rv) {
			rv = gl3wInit() != -1 && gl3wIsSupported(3, 1);
		}

		RECT r;
		GetClientRect((HWND)hwnd, &r);
		screen_width = r.right - r.left;
		screen_height = r.bottom - r.top;

		return rv;
	}
	#include <wingdi.h>
	void gl_swap()
	{
		wglSwapLayerBuffers(ourWindowHandleToDeviceContext,WGL_SWAP_MAIN_PLANE);
		//SwapBuffers(ourWindowHandleToDeviceContext);
	}

	void gl_term()
	{
	}
#elif defined(SUPPORT_X11) && !defined(USE_SDL)
	//! windows && X11
	//let's assume glx for now

	#include <X11/X.h>
	#include <X11/Xlib.h>
	#include <GL/gl.h>
	#include <GL/glx.h>


	bool gl_init(void* wind, void* disp)
	{
		extern void* x11_glc;

		glXMakeCurrent((Display*)libPvr_GetRenderSurface(),
			(GLXDrawable)libPvr_GetRenderTarget(),
			(GLXContext)x11_glc);

		screen_width = 640;
		screen_height = 480;
		return gl3wInit() != -1 && gl3wIsSupported(3, 1);
	}

	void gl_swap()
	{
#ifdef TEST_AUTOMATION
		static FILE* video_file = fopen(cfgLoadStr("record", "rawvid","").c_str(), "wb");
		extern bool do_screenshot;

		if (video_file)
		{
			int bytesz = screen_width * screen_height * 3;
			u8* img = new u8[bytesz];

			glReadPixels(0, 0, screen_width, screen_height, GL_RGB, GL_UNSIGNED_BYTE, img);
			fwrite(img, 1, bytesz, video_file);
			fflush(video_file);
		}

		if (do_screenshot)
		{
			extern void dc_exit();
			int bytesz = screen_width * screen_height * 3;
			u8* img = new u8[bytesz];

			glReadPixels(0, 0, screen_width, screen_height, GL_RGB, GL_UNSIGNED_BYTE, img);
			dump_screenshot(img, screen_width, screen_height);
			delete[] img;
			dc_exit();
			exit(0);
		}
#endif
		glXSwapBuffers((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget());

		Window win;
		int temp;
		unsigned int tempu, new_w, new_h;
		XGetGeometry((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget(),
					&win, &temp, &temp, &new_w, &new_h,&tempu,&tempu);

		//if resized, clear up the draw buffers, to avoid out-of-draw-area junk data
		if (new_w != screen_width || new_h != screen_height) {
			screen_width = new_w;
			screen_height = new_h;
		}

		#if 0
			//handy to debug really stupid render-not-working issues ...

			glcache.ClearColor( 0, 0.5, 1, 1 );
			glClear( GL_COLOR_BUFFER_BIT );
			glXSwapBuffers((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget());


			glcache.ClearColor ( 1, 0.5, 0, 1 );
			glClear ( GL_COLOR_BUFFER_BIT );
			glXSwapBuffers((Display*)libPvr_GetRenderSurface(), (GLXDrawable)libPvr_GetRenderTarget());
		#endif
	}

	void gl_term()
	{
	}

#else
extern void gl_term();
#endif

static void gl_delete_shaders()
{
	for (auto it : gl.shaders)
	{
		if (it.second.program != 0)
			glcache.DeleteProgram(it.second.program);
	}
	gl.shaders.clear();
	glcache.DeleteProgram(gl.modvol_shader.program);
	gl.modvol_shader.program = 0;
}

static void gles_term()
{
	glDeleteBuffers(1, &gl.vbo.geometry);
	gl.vbo.geometry = 0;
	glDeleteBuffers(1, &gl.vbo.modvols);
	glDeleteBuffers(1, &gl.vbo.idxs);
	glDeleteBuffers(1, &gl.vbo.idxs2);
	glcache.DeleteTextures(1, &fbTextureId);
	fbTextureId = 0;
	glcache.DeleteTextures(1, &fogTextureId);
	fogTextureId = 0;
	gl_free_osd_resources();
	free_output_framebuffer();

	gl_delete_shaders();
	gl_term();
}

void findGLVersion()
{
	gl.index_type = GL_UNSIGNED_INT;

	while (true)
		if (glGetError() == GL_NO_ERROR)
			break;
	glGetIntegerv(GL_MAJOR_VERSION, &gl.gl_major);
	if (glGetError() == GL_INVALID_ENUM)
		gl.gl_major = 2;
	const char *version = (const char *)glGetString(GL_VERSION);
	INFO_LOG(RENDERER, "OpenGL version: %s", version);
	if (!strncmp(version, "OpenGL ES", 9))
	{
		gl.is_gles = true;
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
		gl.fog_image_format = GL_ALPHA;
		const char *extensions = (const char *)glGetString(GL_EXTENSIONS);
		if (strstr(extensions, "GL_OES_packed_depth_stencil") != NULL)
			gl.GL_OES_packed_depth_stencil_supported = true;
		if (strstr(extensions, "GL_OES_depth24") != NULL)
			gl.GL_OES_depth24_supported = true;
		if (!gl.GL_OES_packed_depth_stencil_supported)
			INFO_LOG(RENDERER, "Packed depth/stencil not supported: no modifier volumes when rendering to a texture");
	}
	else
	{
		gl.is_gles = false;
    	if (gl.gl_major >= 3)
    	{
			gl.gl_version = "GL3";
#if HOST_OS == OS_DARWIN
			gl.glsl_version_header = "#version 150";
#else
			gl.glsl_version_header = "#version 130";
#endif
			gl.fog_image_format = GL_RED;
		}
		else
		{
			gl.gl_version = "GL2";
			gl.glsl_version_header = "#version 120";
			gl.fog_image_format = GL_ALPHA;
		}
	}
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

#ifdef glBindFragDataLocation
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

PipelineShader *GetProgram(u32 cp_AlphaTest, u32 pp_ClipTestMode,
							u32 pp_Texture, u32 pp_UseAlpha, u32 pp_IgnoreTexA, u32 pp_ShadInstr, u32 pp_Offset,
							u32 pp_FogCtrl, bool pp_Gouraud, bool pp_BumpMap, bool fog_clamping, bool trilinear)
{
	u32 rv=0;

	rv|=pp_ClipTestMode;
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

	PipelineShader *shader = &gl.shaders[rv];
	if (shader->program == 0)
	{
		shader->cp_AlphaTest = cp_AlphaTest;
		shader->pp_ClipTestMode = pp_ClipTestMode-1;
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
		CompilePipelineShader(shader);
	}

	return shader;
}

bool CompilePipelineShader(	PipelineShader* s)
{
	char vshader[8192];

	sprintf(vshader, VertexShaderSource, gl.glsl_version_header, gl.gl_version, s->pp_Gouraud);

	char pshader[8192];

	sprintf(pshader,PixelPipelineShader, gl.glsl_version_header, gl.gl_version,
                s->cp_AlphaTest,s->pp_ClipTestMode,s->pp_UseAlpha,
                s->pp_Texture,s->pp_IgnoreTexA,s->pp_ShadInstr,s->pp_Offset,s->pp_FogCtrl, s->pp_Gouraud, s->pp_BumpMap,
				s->fog_clamping, s->trilinear);

	s->program=gl_CompileAndLink(vshader, pshader);


	//setup texture 0 as the input for the shader
	GLuint gu=glGetUniformLocation(s->program, "tex");
	if (s->pp_Texture==1)
		glUniform1i(gu,0);

	//get the uniform locations
	s->depth_scale      = glGetUniformLocation(s->program, "depth_scale");
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
	// Setup texture 1 as the fog table
	gu = glGetUniformLocation(s->program, "fog_table");
	if (gu != -1)
		glUniform1i(gu, 1);
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

GLuint osd_tex;

void gl_load_osd_resources()
{
	char vshader[8192];
	char fshader[8192];

	sprintf(vshader, OSD_VertexShader, gl.glsl_version_header, gl.gl_version);
	sprintf(fshader, OSD_Shader, gl.glsl_version_header, gl.gl_version);

	gl.OSD_SHADER.program = gl_CompileAndLink(vshader, fshader);
	gl.OSD_SHADER.scale = glGetUniformLocation(gl.OSD_SHADER.program, "scale");
	glUniform1i(glGetUniformLocation(gl.OSD_SHADER.program, "tex"), 0);		//bind osd texture to slot 0

#ifdef _ANDROID
	int w, h;
	if (osd_tex == 0)
		osd_tex = loadPNG(get_readonly_data_path(DATA_PATH "buttons.png"), w, h);
#endif
}

void gl_free_osd_resources()
{
	glcache.DeleteProgram(gl.OSD_SHADER.program);

    if (osd_tex != 0) {
        glcache.DeleteTextures(1, &osd_tex);
        osd_tex = 0;
    }
}

static void create_modvol_shader()
{
	if (gl.modvol_shader.program != 0)
		return;
	char vshader[8192];
	sprintf(vshader, VertexShaderSource, gl.glsl_version_header, gl.gl_version, 1);
	char fshader[8192];
	sprintf(fshader, ModifierVolumeShader, gl.glsl_version_header, gl.gl_version);

	gl.modvol_shader.program = gl_CompileAndLink(vshader, fshader);
	gl.modvol_shader.normal_matrix  = glGetUniformLocation(gl.modvol_shader.program, "normal_matrix");
	gl.modvol_shader.sp_ShaderColor = glGetUniformLocation(gl.modvol_shader.program, "sp_ShaderColor");
	gl.modvol_shader.depth_scale    = glGetUniformLocation(gl.modvol_shader.program, "depth_scale");
	gl.modvol_shader.extra_depth_scale = glGetUniformLocation(gl.modvol_shader.program, "extra_depth_scale");
}

bool gl_create_resources()
{
	if (gl.vbo.geometry != 0)
		// Assume the resources have already been created
		return true;

	findGLVersion();

	if (gl.gl_major >= 3)
	{
		verify(glGenVertexArrays != NULL);
		//create vao
		//This is really not "proper", vaos are supposed to be defined once
		//i keep updating the same one to make the es2 code work in 3.1 context
#ifndef GLES2
		glGenVertexArrays(1, &gl.vbo.vao);
#endif
	}

	//create vbos
	glGenBuffers(1, &gl.vbo.geometry);
	glGenBuffers(1, &gl.vbo.modvols);
	glGenBuffers(1, &gl.vbo.idxs);
	glGenBuffers(1, &gl.vbo.idxs2);

	create_modvol_shader();

	gl_load_osd_resources();

	gui_init();

	return true;
}

//swap buffers
void gl_swap();

GLuint gl_CompileShader(const char* shader,GLuint type);

bool gl_create_resources();

//setup


bool gles_init()
{
	if (!gl_init((void*)libPvr_GetRenderTarget(),
		         (void*)libPvr_GetRenderSurface()))
			return false;

	glcache.EnableCache();

	if (!gl_create_resources())
		return false;

#ifdef USE_EGL
	#ifdef TARGET_PANDORA
	fbdev=open("/dev/fb0", O_RDONLY);
	#else
	eglSwapInterval(gl.setup.display,1);
	#endif
#endif

	//    glEnable(GL_DEBUG_OUTPUT);
	//    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	//    glDebugMessageCallback(gl_DebugOutput, NULL);
	//    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, NULL, GL_TRUE);

	//clean up the buffer
	glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT);
	gl_swap();

#ifdef GL_GENERATE_MIPMAP_HINT
	if (gl.is_gles)
		glHint(GL_GENERATE_MIPMAP_HINT, GL_FASTEST);
#endif

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
	for (int i = 0; i < 128; i++)
	{
		temp_tex_buffer[i] = fog_table[i * 4];
		temp_tex_buffer[i + 128] = fog_table[i * 4 + 1];
	}
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, fog_image_format, 128, 2, 0, fog_image_format, GL_UNSIGNED_BYTE, temp_tex_buffer);
	glCheck();

	glActiveTexture(GL_TEXTURE0);
}


extern u16 kcode[4];
extern u8 rt[4],lt[4];

#if defined(_ANDROID)
extern float vjoy_pos[14][8];
#else

float vjoy_pos[14][8]=
{
	{24+0,24+64,64,64},     //LEFT
	{24+64,24+0,64,64},     //UP
	{24+128,24+64,64,64},   //RIGHT
	{24+64,24+128,64,64},   //DOWN

	{440+0,280+64,64,64},   //X
	{440+64,280+0,64,64},   //Y
	{440+128,280+64,64,64}, //B
	{440+64,280+128,64,64}, //A

	{320-32,360+32,64,64},  //Start

	{440,200,90,64},        //RT
	{542,200,90,64},        //LT

	{-24,128+224,128,128},  //ANALOG_RING
	{96,320,64,64},         //ANALOG_POINT
	{1}
};
#endif // !_ANDROID

static List<Vertex> osd_vertices;
static bool osd_vertices_overrun;

static const float vjoy_sz[2][14] = {
	{ 64,64,64,64, 64,64,64,64, 64, 90,90, 128, 64 },
	{ 64,64,64,64, 64,64,64,64, 64, 64,64, 128, 64 },
};

void HideOSD()
{
	vjoy_pos[13][0] = 0;
	vjoy_pos[13][1] = 0;
	vjoy_pos[13][2] = 0;
	vjoy_pos[13][3] = 0;
}

static void DrawButton(float* xy, u32 state)
{
	Vertex vtx;

	vtx.z = 1;

	vtx.col[0]=vtx.col[1]=vtx.col[2]=(0x7F-0x40*state/255)*vjoy_pos[13][0];

	vtx.col[3]=0xA0*vjoy_pos[13][4];

	vjoy_pos[13][4]+=(vjoy_pos[13][0]-vjoy_pos[13][4])/2;



	vtx.x = xy[0]; vtx.y = xy[1];
	vtx.u=xy[4]; vtx.v=xy[5];
	*osd_vertices.Append() = vtx;

	vtx.x = xy[0] + xy[2]; vtx.y = xy[1];
	vtx.u=xy[6]; vtx.v=xy[5];
	*osd_vertices.Append() = vtx;

	vtx.x = xy[0]; vtx.y = xy[1] + xy[3];
	vtx.u=xy[4]; vtx.v=xy[7];
	*osd_vertices.Append() = vtx;

	vtx.x = xy[0] + xy[2]; vtx.y = xy[1] + xy[3];
	vtx.u=xy[6]; vtx.v=xy[7];
	*osd_vertices.Append() = vtx;
}

static void DrawButton2(float* xy, bool state) { DrawButton(xy,state?0:255); }

static void osd_gen_vertices()
{
	osd_vertices.Init(ARRAY_SIZE(vjoy_pos) * 4, &osd_vertices_overrun, "OSD vertices");
	DrawButton2(vjoy_pos[0],kcode[0] & DC_DPAD_LEFT);
	DrawButton2(vjoy_pos[1],kcode[0] & DC_DPAD_UP);
	DrawButton2(vjoy_pos[2],kcode[0] & DC_DPAD_RIGHT);
	DrawButton2(vjoy_pos[3],kcode[0] & DC_DPAD_DOWN);

	DrawButton2(vjoy_pos[4],kcode[0] & DC_BTN_X);
	DrawButton2(vjoy_pos[5],kcode[0] & DC_BTN_Y);
	DrawButton2(vjoy_pos[6],kcode[0] & DC_BTN_B);
	DrawButton2(vjoy_pos[7],kcode[0] & DC_BTN_A);

	DrawButton2(vjoy_pos[8],kcode[0] & DC_BTN_START);

	DrawButton(vjoy_pos[9],lt[0]);

	DrawButton(vjoy_pos[10],rt[0]);

	DrawButton2(vjoy_pos[11],1);
	DrawButton2(vjoy_pos[12],0);
}

#define OSD_TEX_W 512
#define OSD_TEX_H 256

void OSD_DRAW(bool clear_screen)
{
#ifdef _ANDROID
	if (osd_tex == 0)
		gl_load_osd_resources();
	if (osd_tex != 0)
	{
		osd_gen_vertices();

		float u=0;
		float v=0;

		for (int i=0;i<13;i++)
		{
			//umin,vmin,umax,vmax
			vjoy_pos[i][4]=(u+1)/OSD_TEX_W;
			vjoy_pos[i][5]=(v+1)/OSD_TEX_H;

			vjoy_pos[i][6]=((u+vjoy_sz[0][i]-1))/OSD_TEX_W;
			vjoy_pos[i][7]=((v+vjoy_sz[1][i]-1))/OSD_TEX_H;

			u+=vjoy_sz[0][i];
			if (u>=OSD_TEX_W)
			{
				u-=OSD_TEX_W;
				v+=vjoy_sz[1][i];
			}
			//v+=vjoy_pos[i][3];
		}

		verify(glIsProgram(gl.OSD_SHADER.program));
		glcache.UseProgram(gl.OSD_SHADER.program);

		float scale_h = screen_height / 480.f;
		float offs_x = (screen_width - scale_h * 640.f) / 2.f;
		float scale[4];
		scale[0] = 2.f / (screen_width / scale_h);
		scale[1]= -2.f / 480.f;
		scale[2]= 1.f - 2.f * offs_x / screen_width;
		scale[3]= -1.f;
		glUniform4fv(gl.OSD_SHADER.scale, 1, scale);

		glActiveTexture(GL_TEXTURE0);
		glcache.BindTexture(GL_TEXTURE_2D, osd_tex);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		glBufferData(GL_ARRAY_BUFFER, osd_vertices.bytes(), osd_vertices.head(), GL_STREAM_DRAW); glCheck();

		glcache.Enable(GL_BLEND);
		glcache.Disable(GL_DEPTH_TEST);
		glcache.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glcache.DepthMask(false);
		glcache.DepthFunc(GL_ALWAYS);

		glcache.Disable(GL_CULL_FACE);
		glcache.Disable(GL_SCISSOR_TEST);
		glViewport(0, 0, screen_width, screen_height);

		if (clear_screen)
		{
			glcache.ClearColor(0.7f, 0.7f, 0.7f, 1.f);
			glClear(GL_COLOR_BUFFER_BIT);
		}
		int dfa = osd_vertices.used() / 4;

		for (int i = 0; i < dfa; i++)
			glDrawArrays(GL_TRIANGLE_STRIP, i * 4, 4);
	}
#endif
	gui_display_osd();
}

bool ProcessFrame(TA_context* ctx)
{
	ctx->rend_inuse.Lock();

	if (KillTex)
		killtex();

	if (ctx->rend.isRenderFramebuffer)
	{
		RenderFramebuffer();
		ctx->rend_inuse.Unlock();
	}
	else
	{
		if (!ta_parse_vdrc(ctx))
			return false;
	}
	CollectCleanup();

	if (ctx->rend.Overrun)
		WARN_LOG(PVR, "ERROR: TA context overrun");

	return !ctx->rend.Overrun;
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

void GetFramebufferScaling(float& scale_x, float& scale_y, float& scissoring_scale_x, float& scissoring_scale_y)
{
	scale_x = 1;
	scale_y = 1;
	scissoring_scale_x = 1;
	scissoring_scale_y = 1;
	
	if (!pvrrc.isRTT && !pvrrc.isRenderFramebuffer)
	{
		scale_x = fb_scale_x;
		scale_y = fb_scale_y;
		if (SCALER_CTL.vscalefactor >= 0x400)
		{
			// Interlace mode A (single framebuffer)
			if (SCALER_CTL.interlace == 0)
				scale_y *= (float)SCALER_CTL.vscalefactor / 0x400;
			else
				// Interlace mode B (alternating framebuffers)
				scissoring_scale_y /= (float)SCALER_CTL.vscalefactor / 0x400;
		}
		
		// VO pixel doubling is done after fb rendering/clipping
		// so it should be used for scissoring as well
		if (VO_CONTROL.pixel_double)
			scale_x *= 0.5f;
		
		// the X Scaler halves the horizontal resolution but
		// before clipping/scissoring
		if (SCALER_CTL.hscale)
		{
			scissoring_scale_x /= 2.f;
			scale_x *= 2.f;
		}
	}
}

void GetFramebufferSize(float& dc_width, float& dc_height)
{
	if (pvrrc.isRTT)
	{
		dc_width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
		dc_height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
	}
	else
	{
		dc_width = 640;
		dc_height = 480;
	}
}

void SetupMatrices(float dc_width, float dc_height,
				   float scale_x, float scale_y, float scissoring_scale_x, float scissoring_scale_y,
				   float &ds2s_offs_x, glm::mat4& normal_mat, glm::mat4& scissor_mat)
{
	float screen_scaling = settings.rend.ScreenScaling / 100.f;
	
	if (pvrrc.isRTT)
	{
		ShaderUniforms.normal_mat = glm::translate(glm::vec3(-1, -1, 0))
			* glm::scale(glm::vec3(2.0f / dc_width, 2.0f / dc_height, 1.f));
		scissor_mat = ShaderUniforms.normal_mat;
	}
	else
	{
		float startx = 0;
		float starty = 0;
		bool vga = FB_R_CTRL.vclk_div == 1;
		switch (SPG_LOAD.hcount)
		{
			case 857: // NTSC, VGA
				startx = VO_STARTX.HStart - (vga ? 0xa8 : 0xa4);
				break;
			case 863: // PAL
				startx = VO_STARTX.HStart - 0xae;
				break;
			default:
				INFO_LOG(PVR, "unknown video mode: hcount %d", SPG_LOAD.hcount);
				break;
		}
		switch (SPG_LOAD.vcount)
		{
			case 524: // NTSC, VGA
				starty = VO_STARTY.VStart_field1 - (vga ? 0x28 : 0x12);
				break;
			case 262: // NTSC 240p
				starty = VO_STARTY.VStart_field1 - 0x11;
				break;
			case 624: // PAL
				starty = VO_STARTY.VStart_field1 - 0x2d;
				break;
			case 312: // PAL 240p
				starty = VO_STARTY.VStart_field1 - 0x2e;
				break;
			default:
				INFO_LOG(PVR, "unknown video mode: vcount %d", SPG_LOAD.vcount);
				break;
		}
		// some heuristic...
		startx *= 0.8;
		starty *= 1.1;
		normal_mat = glm::translate(glm::vec3(startx, starty, 0));
		scissor_mat = normal_mat;

		float screen_stretching = settings.rend.ScreenStretching / 100.f;
		float dc2s_scale_h;

		if (settings.rend.Rotate90)
		{
			dc2s_scale_h = screen_height / 640.0f;
			ds2s_offs_x =  (screen_width - dc2s_scale_h * 480.0f * screen_stretching) / 2;
			float y_coef = 2.0f / (screen_width / dc2s_scale_h * scale_y) * screen_stretching;
			float x_coef = -2.0f / dc_width;
			glm::mat4 trans_rot = glm::rotate((float)M_PI_2, glm::vec3(0, 0, 1))
				* glm::translate(glm::vec3(1, -1 + 2 * ds2s_offs_x / screen_width, 0));
			normal_mat = trans_rot
				* glm::scale(glm::vec3(x_coef, y_coef, 1.f))
				* normal_mat;
			scissor_mat = trans_rot
				* glm::scale(glm::vec3(x_coef / scissoring_scale_x,
								   y_coef/ scissoring_scale_y, 1.f))
				* scissor_mat;	// FIXME scale_x not used, except in dc_width???
		}
		else
		{
			dc2s_scale_h = screen_height / 480.0f;
			ds2s_offs_x =  (screen_width - dc2s_scale_h * 640.0f * screen_stretching) / 2;
			float x_coef = 2.0f / (screen_width / dc2s_scale_h * scale_x) * screen_stretching;
			float y_coef = -2.0f / dc_height;
			normal_mat = glm::translate(glm::vec3(-1 + 2 * ds2s_offs_x / screen_width, 1, 0))
				* glm::scale(glm::vec3(x_coef, y_coef, 1.f))
				* normal_mat;
			scissor_mat = glm::translate(glm::vec3(-1 + 2 * ds2s_offs_x / screen_width, 1, 0))
				* glm::scale(glm::vec3(x_coef / scissoring_scale_x,
								   y_coef / scissoring_scale_y, 1.f))
				* scissor_mat;
		}
	}
	normal_mat = glm::scale(glm::vec3(1, 1, 1 / settings.rend.ExtraDepthScale))
			* normal_mat;

	glm::mat4 vp_trans = glm::translate(glm::vec3(1, 1, 0));
	if (pvrrc.isRTT)
	{
		vp_trans = glm::scale(glm::vec3(dc_width / 2, dc_height / 2, 1.f))
			* vp_trans;
	}
	else
	{
		vp_trans = glm::scale(glm::vec3(screen_width * screen_scaling / 2, screen_height * screen_scaling / 2, 1.f))
			* vp_trans;
	}
	ViewportMatrix = vp_trans * normal_mat;
	scissor_mat = vp_trans * scissor_mat;
}

bool RenderFrame()
{
	DoCleanup();
	create_modvol_shader();

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

	//calculate a projection so that it matches the pvr x,y setup, and
	//a) Z is linearly scaled between 0 ... 1
	//b) W is passed though for proper perspective calculations

	//these should be adjusted based on the current PVR scaling etc params
	float dc_width;
	float dc_height;
	GetFramebufferSize(dc_width, dc_height);

	if (!is_rtt)
		gcflip = 0;
	else
		gcflip = 1;

	float scale_x;
	float scale_y;
	float scissoring_scale_x;
	float scissoring_scale_y;
	GetFramebufferScaling(scale_x, scale_y, scissoring_scale_x, scissoring_scale_y);

	dc_width  *= scale_x;
	dc_height *= scale_y;

	/*
		Handle Dc to screen scaling
	*/
	float screen_scaling = settings.rend.ScreenScaling / 100.f;

	float ds2s_offs_x;

	glm::mat4 scissor_mat;
	SetupMatrices(dc_width, dc_height, scale_x, scale_y, scissoring_scale_x, scissoring_scale_y, ds2s_offs_x, ShaderUniforms.normal_mat, scissor_mat);

	ShaderUniforms.depth_coefs[0] = 2 / (vtx_max_fZ - vtx_min_fZ);
	ShaderUniforms.depth_coefs[1] = -vtx_min_fZ - 1;
	ShaderUniforms.depth_coefs[2] = 0;
	ShaderUniforms.depth_coefs[3] = 0;

	ShaderUniforms.extra_depth_scale = settings.rend.ExtraDepthScale;

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
	ShaderUniforms.fog_den_float = fog_den_mant * powf(2.0f, fog_den_exp);

	ShaderUniforms.fog_clamp_min[0] = ((pvrrc.fog_clamp_min >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[1] = ((pvrrc.fog_clamp_min >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[2] = ((pvrrc.fog_clamp_min >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_min[3] = ((pvrrc.fog_clamp_min >> 24) & 0xFF) / 255.0f;
	
	ShaderUniforms.fog_clamp_max[0] = ((pvrrc.fog_clamp_max >> 16) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[1] = ((pvrrc.fog_clamp_max >> 8) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[2] = ((pvrrc.fog_clamp_max >> 0) & 0xFF) / 255.0f;
	ShaderUniforms.fog_clamp_max[3] = ((pvrrc.fog_clamp_max >> 24) & 0xFF) / 255.0f;
	
	if (fog_needs_update && settings.rend.Fog)
	{
		fog_needs_update = false;
		UpdateFogTexture((u8 *)FOG_TABLE, GL_TEXTURE1, gl.fog_image_format);
	}

	glcache.UseProgram(gl.modvol_shader.program);

	glUniform4fv( gl.modvol_shader.depth_scale, 1, ShaderUniforms.depth_coefs);
	glUniform1f(gl.modvol_shader.extra_depth_scale, ShaderUniforms.extra_depth_scale);
	glUniformMatrix4fv(gl.modvol_shader.normal_matrix, 1, GL_FALSE, &ShaderUniforms.normal_mat[0][0]);

	ShaderUniforms.PT_ALPHA=(PT_ALPHA_REF&0xFF)/255.0f;

	for (auto it : gl.shaders)
	{
		glcache.UseProgram(it.second.program);
		ShaderUniforms.Set(&it.second);
	}

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
			break;
		}
		DEBUG_LOG(RENDERER, "RTT packmode=%d stride=%d - %d,%d -> %d,%d", FB_W_CTRL.fb_packmode, FB_W_LINESTRIDE.stride * 8,
				FB_X_CLIP.min, FB_Y_CLIP.min, FB_X_CLIP.max, FB_Y_CLIP.max);
		BindRTT(FB_W_SOF1 & VRAM_MASK, dc_width, dc_height, channels, format);
	}
	else
	{
		if (settings.rend.ScreenScaling != 100 || gl.swap_buffer_not_preserved)
		{
			init_output_framebuffer(screen_width * screen_scaling + 0.5f, screen_height * screen_scaling + 0.5f);
		}
		else
		{
#if HOST_OS != OS_DARWIN
			//Fix this in a proper way
			glBindFramebuffer(GL_FRAMEBUFFER,0);
#endif
			glViewport(0, 0, screen_width, screen_height);
		}
	}

	bool wide_screen_on = !is_rtt && settings.rend.WideScreen
			&& pvrrc.fb_X_CLIP.min == 0
			&& int((pvrrc.fb_X_CLIP.max + 1) / scale_x + 0.5f) == 640
			&& pvrrc.fb_Y_CLIP.min == 0
			&& int((pvrrc.fb_Y_CLIP.max + 1) / scale_y + 0.5f) == 480;

	//Color is cleared by the background plane

	glcache.Disable(GL_SCISSOR_TEST);

	glcache.DepthMask(GL_TRUE);
	glClearDepthf(0.0);
	glStencilMask(0xFF); glCheck();
    glClearStencil(0);
	glClear(GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); glCheck();

	//move vertex to gpu

	if (!pvrrc.isRenderFramebuffer)
	{
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
			float width;
			float height;
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
				width = clip_dim[0];
				height = clip_dim[1];
				if (width < 0)
				{
					min_x += width;
					width = -width;
				}
				if (height < 0)
				{
					min_y += height;
					height = -height;
				}
				if (ds2s_offs_x > 0)
				{
					float scaled_offs_x = ds2s_offs_x * screen_scaling;

					glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
					glcache.Enable(GL_SCISSOR_TEST);
					glScissor(0, 0, scaled_offs_x + 0.5f, screen_height * screen_scaling + 0.5f);
					glClear(GL_COLOR_BUFFER_BIT);
					glScissor(screen_width * screen_scaling - scaled_offs_x, 0, scaled_offs_x + 1.f, screen_height * screen_scaling + 0.5f);
					glClear(GL_COLOR_BUFFER_BIT);
				}
			}
			else
			{
				width = pvrrc.fb_X_CLIP.max - pvrrc.fb_X_CLIP.min + 1;
				height = pvrrc.fb_Y_CLIP.max - pvrrc.fb_Y_CLIP.min + 1;
				min_x = pvrrc.fb_X_CLIP.min;
				min_y = pvrrc.fb_Y_CLIP.min;
				if (settings.rend.RenderToTextureUpscale > 1 && !settings.rend.RenderToTextureBuffer)
				{
					min_x *= settings.rend.RenderToTextureUpscale;
					min_y *= settings.rend.RenderToTextureUpscale;
					width *= settings.rend.RenderToTextureUpscale;
					height *= settings.rend.RenderToTextureUpscale;
				}
			}
			glScissor(min_x + 0.5f, min_y + 0.5f, width + 0.5f, height + 0.5f);
			glcache.Enable(GL_SCISSOR_TEST);
		}

		DrawStrips();
	}
	else
	{
		glcache.ClearColor(0.f, 0.f, 0.f, 0.f);
		glClear(GL_COLOR_BUFFER_BIT);
		DrawFramebuffer(dc_width, dc_height);
		glBufferData(GL_ARRAY_BUFFER, pvrrc.verts.bytes(), pvrrc.verts.head(), GL_STREAM_DRAW);
		upload_vertex_indices();
	}
	#if HOST_OS==OS_WINDOWS
		//Sleep(40); //to test MT stability
	#endif

	eglCheck();

	if (is_rtt)
		ReadRTTBuffer();
	else if (settings.rend.ScreenScaling != 100 || gl.swap_buffer_not_preserved)
		render_output_framebuffer();

	return !is_rtt;
}

void rend_set_fb_scale(float x, float y)
{
	fb_scale_x = x;
	fb_scale_y = y;
}

struct glesrend : Renderer
{
	bool Init() { return gles_init(); }
	void Resize(int w, int h) { screen_width=w; screen_height=h; }
	void Term()
	{
		killtex();
		gles_term();
	}

	bool Process(TA_context* ctx) { return ProcessFrame(ctx); }
	bool Render() { return RenderFrame(); }
	bool RenderLastFrame() { return gl.swap_buffer_not_preserved ? render_output_framebuffer() : false; }
	void Present() { gl_swap(); glViewport(0, 0, screen_width, screen_height); }

	void DrawOSD(bool clear_screen)
	{
#ifndef GLES2
		if (gl.gl_major >= 3)
			glBindVertexArray(gl.vbo.vao);
#endif
		glBindBuffer(GL_ARRAY_BUFFER, gl.vbo.geometry); glCheck();
		glEnableVertexAttribArray(VERTEX_POS_ARRAY);
		glVertexAttribPointer(VERTEX_POS_ARRAY, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,x));

		glEnableVertexAttribArray(VERTEX_COL_BASE_ARRAY);
		glVertexAttribPointer(VERTEX_COL_BASE_ARRAY, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex,col));

		glEnableVertexAttribArray(VERTEX_UV_ARRAY);
		glVertexAttribPointer(VERTEX_UV_ARRAY, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex,u));

		OSD_DRAW(clear_screen);
	}

	virtual u32 GetTexture(TSP tsp, TCW tcw) {
		return gl_GetTexture(tsp, tcw);
	}
};


FILE* pngfile;

void png_cstd_read(png_structp png_ptr, png_bytep data, png_size_t length)
{
	fread(data,1, length,pngfile);
}

u8* loadPNGData(const string& fname, int &width, int &height)
{
	const char* filename=fname.c_str();
	FILE* file = fopen(filename, "rb");
	pngfile=file;

	if (!file)
	{
		EMUERROR("Error opening %s\n", filename);
		return NULL;
	}

	//header for testing if it is a png
	png_byte header[8];

	//read the header
	fread(header,1,8,file);

	//test if png
	int is_png = !png_sig_cmp(header, 0, 8);
	if (!is_png)
	{
		fclose(file);
		WARN_LOG(RENDERER, "Not a PNG file : %s", filename);
		return NULL;
	}

	//create png struct
	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL,
		NULL, NULL);
	if (!png_ptr)
	{
		fclose(file);
		WARN_LOG(RENDERER, "Unable to create PNG struct : %s", filename);
		return (NULL);
	}

	//create png info struct
	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
		WARN_LOG(RENDERER, "Unable to create PNG info : %s", filename);
		fclose(file);
		return (NULL);
	}

	//create png info struct
	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
		WARN_LOG(RENDERER, "Unable to create PNG end info : %s", filename);
		fclose(file);
		return (NULL);
	}

	//png error stuff, not sure libpng man suggests this.
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		fclose(file);
		WARN_LOG(RENDERER, "Error during setjmp : %s", filename);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return (NULL);
	}

	//init png reading
	//png_init_io(png_ptr, fp);
	png_set_read_fn(png_ptr, NULL, png_cstd_read);

	//let libpng know you already read the first 8 bytes
	png_set_sig_bytes(png_ptr, 8);

	// read all the info up to the image data
	png_read_info(png_ptr, info_ptr);

	//variables to pass to get info
	int bit_depth, color_type;
	png_uint_32 twidth, theight;

	// get info about png
	png_get_IHDR(png_ptr, info_ptr, &twidth, &theight, &bit_depth, &color_type,
		NULL, NULL, NULL);

	//update width and height based on png info
	width = twidth;
	height = theight;

	// Update the png info struct.
	png_read_update_info(png_ptr, info_ptr);

	// Row size in bytes.
	int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	// Allocate the image_data as a big block, to be given to opengl
	png_byte *image_data = new png_byte[rowbytes * height];
	if (!image_data)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		WARN_LOG(RENDERER, "Unable to allocate image_data while loading %s", filename);
		fclose(file);
		return NULL;
	}

	//row_pointers is for pointing to image_data for reading the png with libpng
	png_bytep *row_pointers = new png_bytep[height];
	if (!row_pointers)
	{
		//clean up memory and close stuff
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		delete[] image_data;
		WARN_LOG(RENDERER, "Unable to allocate row_pointer while loading %s", filename);
		fclose(file);
		return NULL;
	}

	// set the individual row_pointers to point at the correct offsets of image_data
	for (int i = 0; i < height; ++i)
		row_pointers[height - 1 - i] = image_data + i * rowbytes;

	//read the png into image_data through row_pointers
	png_read_image(png_ptr, row_pointers);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
	delete[] row_pointers;
	fclose(file);

	return image_data;
}

GLuint loadPNG(const string& fname, int &width, int &height)
{
	png_byte *image_data = loadPNGData(fname, width, height);
	if (image_data == NULL)
		return TEXTURE_LOAD_ERROR;

	//Now generate the OpenGL texture object
	GLuint texture = glcache.GenTexture();
	glcache.BindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
		GL_UNSIGNED_BYTE, (GLvoid*) image_data);
	glcache.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	delete[] image_data;

	return texture;
}


Renderer* rend_GLES2() { return new glesrend(); }
