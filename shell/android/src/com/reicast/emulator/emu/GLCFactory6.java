package com.reicast.emulator.emu;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;

import android.opengl.EGL14;
import android.opengl.EGLExt;
import android.opengl.GLSurfaceView;
import android.util.Log;

public class GLCFactory6 {

	private static void LOGI(String S) { Log.i("GL2JNIView-v6",S); }
	private static void LOGW(String S) { Log.w("GL2JNIView-v6",S); }
	private static void LOGE(String S) { Log.e("GL2JNIView-v6",S); }

	public static class ContextFactory implements GLSurfaceView.EGLContextFactory
	{
		private static final int EGL_CONTEXT_CLIENT_VERSION = 0x3098;

		public EGLContext createContext(EGL10 egl,EGLDisplay display,EGLConfig eglConfig)
		{
			int[] attrList = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE };

			LOGI("Creating OpenGL ES X context");

			checkEglError("Before eglCreateContext",egl);
			EGLContext context = egl.eglCreateContext(display,eglConfig,EGL10.EGL_NO_CONTEXT,attrList);
			checkEglError("After eglCreateContext",egl);
			return(context);
		}

		public void destroyContext(EGL10 egl,EGLDisplay display,EGLContext context)
		{
			LOGI("Destroying OpenGL ES X context");
			egl.eglDestroyContext(display,context);
		}
	}

	private static void checkEglError(String prompt,EGL10 egl)
	{
		int error;

		while((error=egl.eglGetError()) != EGL14.EGL_SUCCESS)
			LOGE(String.format("%s: EGL error: 0x%x",prompt,error));
	}

	public static class ConfigChooser implements GLSurfaceView.EGLConfigChooser
	{
		// Subclasses can adjust these values:
		protected int mRedSize;
		protected int mGreenSize;
		protected int mBlueSize;
		protected int mAlphaSize;
		protected int mDepthSize;
		protected int mStencilSize;
		private int[] mValue = new int[1];

		public ConfigChooser(int r,int g,int b,int a,int depth,int stencil)
		{
			mRedSize     = r;
			mGreenSize   = g;
			mBlueSize    = b;
			mAlphaSize   = a;
			mDepthSize   = depth;
			mStencilSize = stencil;
		}

		public EGLConfig chooseConfig(EGL10 egl, EGLDisplay display) {
			mValue = new int[1];

			int glAPIToTry = EGLExt.EGL_OPENGL_ES3_BIT_KHR;
			int[] configSpec = null;

			do {
				EGL14.eglBindAPI(glAPIToTry);

				int renderableType;
				if (glAPIToTry == EGLExt.EGL_OPENGL_ES3_BIT_KHR) {
					renderableType = EGLExt.EGL_OPENGL_ES3_BIT_KHR;
					// If this API does not work, try ES2 next.
					glAPIToTry = EGL14.EGL_OPENGL_ES2_BIT;
				} else {
					renderableType = EGL14.EGL_OPENGL_ES2_BIT;
					// If this API does not work, try ES next.
					glAPIToTry = EGL14.EGL_OPENGL_ES_API;
				}

				configSpec = new int[] { 
						EGL14.EGL_RED_SIZE, 4,
						EGL14.EGL_GREEN_SIZE, 4, 
						EGL14.EGL_BLUE_SIZE, 4,
						EGL14.EGL_RENDERABLE_TYPE, renderableType,
						EGL14.EGL_DEPTH_SIZE, 16,
						EGL14.EGL_NONE
				};

				if (!egl.eglChooseConfig(display, configSpec, null, 0, mValue)) {
					configSpec[9] = 16;
					if (!egl.eglChooseConfig(display, configSpec, null, 0, mValue)) {
						throw new IllegalArgumentException("Could not get context count");
					}
				}

			} while (glAPIToTry != EGL14.EGL_OPENGL_ES_API && mValue[0]<=0);

			if (mValue[0]<=0) {
				throw new IllegalArgumentException("No configs match configSpec");
			}

			// Get all matching configurations.
			EGLConfig[] configs = new EGLConfig[mValue[0]];
			if (GL2JNIView.DEBUG)
				LOGW(String.format("%d configurations", configs.length));
			if (!egl.eglChooseConfig(display, configSpec, configs, mValue[0], mValue)) {
				throw new IllegalArgumentException("Could not get config data");
			}

			for (int i = 0; i < configs.length; ++i) {
				EGLConfig config = configs[i];
				int d = findConfigAttrib(egl, display, config,
						EGL14.EGL_DEPTH_SIZE, 0);
				int s = findConfigAttrib(egl, display, config,
						EGL14.EGL_STENCIL_SIZE, 0);

				// We need at least mDepthSize and mStencilSize bits
				if (d >= mDepthSize || s >= mStencilSize) {
					// We want an *exact* match for red/green/blue/alpha
					int r = findConfigAttrib(egl, display, config,
							EGL14.EGL_RED_SIZE, 0);
					int g = findConfigAttrib(egl, display, config,
							EGL14.EGL_GREEN_SIZE, 0);
					int b = findConfigAttrib(egl, display, config,
							EGL14.EGL_BLUE_SIZE, 0);
					int a = findConfigAttrib(egl, display, config,
							EGL14.EGL_ALPHA_SIZE, 0);

					if (r == mRedSize && g == mGreenSize && b == mBlueSize
							&& a == mAlphaSize)
						if (GL2JNIView.DEBUG) {
							LOGW(String.format("Configuration %d:", i));
							printConfig(egl, display, configs[i]);
						}
					return config;
				}
			}

			throw new IllegalArgumentException("Could not find suitable EGL config");
		}

		private int findConfigAttrib(EGL10 egl,EGLDisplay display,EGLConfig config,int attribute,int defaultValue)
		{
			return(egl.eglGetConfigAttrib(display,config,attribute,mValue)? mValue[0] : defaultValue);
		}

		private void printConfig(EGL10 egl,EGLDisplay display,EGLConfig config)
		{
			final int[] attributes =
				{
					EGL14.EGL_BUFFER_SIZE,
					EGL14.EGL_ALPHA_SIZE,
					EGL14.EGL_BLUE_SIZE,
					EGL14.EGL_GREEN_SIZE,
					EGL14.EGL_RED_SIZE,
					EGL14.EGL_DEPTH_SIZE,
					EGL14.EGL_STENCIL_SIZE,
					EGL14.EGL_CONFIG_CAVEAT,
					EGL14.EGL_CONFIG_ID,
					EGL14.EGL_LEVEL,
					EGL14.EGL_MAX_PBUFFER_HEIGHT,
					EGL14.EGL_MAX_PBUFFER_PIXELS,
					EGL14.EGL_MAX_PBUFFER_WIDTH,
					EGL14.EGL_NATIVE_RENDERABLE,
					EGL14.EGL_NATIVE_VISUAL_ID,
					EGL14.EGL_NATIVE_VISUAL_TYPE,
					0x3030, // EGL14.EGL_PRESERVED_RESOURCES,
					EGL14.EGL_SAMPLES,
					EGL14.EGL_SAMPLE_BUFFERS,
					EGL14.EGL_SURFACE_TYPE,
					EGL14.EGL_TRANSPARENT_TYPE,
					EGL14.EGL_TRANSPARENT_RED_VALUE,
					EGL14.EGL_TRANSPARENT_GREEN_VALUE,
					EGL14.EGL_TRANSPARENT_BLUE_VALUE,
					0x3039, // EGL14.EGL_BIND_TO_TEXTURE_RGB,
					0x303A, // EGL14.EGL_BIND_TO_TEXTURE_RGBA,
					0x303B, // EGL14.EGL_MIN_SWAP_INTERVAL,
					0x303C, // EGL14.EGL_MAX_SWAP_INTERVAL,
					EGL14.EGL_LUMINANCE_SIZE,
					EGL14.EGL_ALPHA_MASK_SIZE,
					EGL14.EGL_COLOR_BUFFER_TYPE,
					EGL14.EGL_RENDERABLE_TYPE,
					0x3042 // EGL14.EGL_CONFORMANT
				};

			final String[] names =
				{
					"EGL_BUFFER_SIZE",
					"EGL_ALPHA_SIZE",
					"EGL_BLUE_SIZE",
					"EGL_GREEN_SIZE",
					"EGL_RED_SIZE",
					"EGL_DEPTH_SIZE",
					"EGL_STENCIL_SIZE",
					"EGL_CONFIG_CAVEAT",
					"EGL_CONFIG_ID",
					"EGL_LEVEL",
					"EGL_MAX_PBUFFER_HEIGHT",
					"EGL_MAX_PBUFFER_PIXELS",
					"EGL_MAX_PBUFFER_WIDTH",
					"EGL_NATIVE_RENDERABLE",
					"EGL_NATIVE_VISUAL_ID",
					"EGL_NATIVE_VISUAL_TYPE",
					"EGL_PRESERVED_RESOURCES",
					"EGL_SAMPLES",
					"EGL_SAMPLE_BUFFERS",
					"EGL_SURFACE_TYPE",
					"EGL_TRANSPARENT_TYPE",
					"EGL_TRANSPARENT_RED_VALUE",
					"EGL_TRANSPARENT_GREEN_VALUE",
					"EGL_TRANSPARENT_BLUE_VALUE",
					"EGL_BIND_TO_TEXTURE_RGB",
					"EGL_BIND_TO_TEXTURE_RGBA",
					"EGL_MIN_SWAP_INTERVAL",
					"EGL_MAX_SWAP_INTERVAL",
					"EGL_LUMINANCE_SIZE",
					"EGL_ALPHA_MASK_SIZE",
					"EGL_COLOR_BUFFER_TYPE",
					"EGL_RENDERABLE_TYPE",
					"EGL_CONFORMANT"
				};

			int[] value = new int[1];

			for(int i=0 ; i<attributes.length ; i++)
				if(egl.eglGetConfigAttrib(display,config,attributes[i],value))
					LOGI(String.format("  %s: %d\n",names[i],value[0]));
				else
					while(egl.eglGetError()!=EGL14.EGL_SUCCESS);
		}
	}
}
