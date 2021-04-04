package com.reicast.emulator.emu;


import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.os.Build;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.View;

import com.android.util.FileUtils;
import com.reicast.emulator.GL2JNIActivity;
import com.reicast.emulator.config.Config;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;


/**
 * A simple GLSurfaceView sub-class that demonstrate how to perform
 * OpenGL ES 2.0 rendering into a GL Surface. Note the following important
 * details:
 *
 * - The class must use a custom context factory to enable 2.0 rendering.
 *   See ContextFactory class definition below.
 *
 * - The class must use a custom EGLConfigChooser to be able to select
 *   an EGLConfig that supports 2.0. This is done by providing a config
 *   specification to eglChooseConfig() that has the attribute
 *   EGL10.ELG_RENDERABLE_TYPE containing the EGL_OPENGL_ES2_BIT flag
 *   set. See ConfigChooser class definition below.
 *
 * - The class must select the surface's format, then choose an EGLConfig
 *   that matches it exactly (with regards to red/green/blue/alpha channels
 *   bit depths). Failure to do so would result in an EGL_BAD_MATCH error.
 */

public class GL2JNIView extends GLSurfaceView
{
    public static final boolean DEBUG = false;

    public static final int LAYER_TYPE_SOFTWARE = 1;
    public static final int LAYER_TYPE_HARDWARE = 2;
    private Handler handler = new Handler();

    VirtualJoystickDelegate vjoyDelegate;

    Renderer rend;

    Context context;

    public void restoreCustomVjoyValues(float[][] vjoy_d_cached) {
        vjoyDelegate.restoreCustomVjoyValues(vjoy_d_cached);
    }

    public GL2JNIView(Context context) {
        super(context);
    }

    public GL2JNIView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public GL2JNIView(final Context context, boolean translucent,
                      int depth, int stencil) {
        super(context);
        this.context = context;
        setKeepScreenOn(true);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            setOnSystemUiVisibilityChangeListener (new OnSystemUiVisibilityChangeListener() {
                public void onSystemUiVisibilityChange(int visibility) {
                    if ((visibility & SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                        GL2JNIView.this.setSystemUiVisibility(
                                SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                                        | SYSTEM_UI_FLAG_FULLSCREEN
                                        | SYSTEM_UI_FLAG_HIDE_NAVIGATION);
                        requestLayout();
                    }
                }
            });
        }
        vjoyDelegate = new VirtualJoystickDelegate(this);

        setPreserveEGLContextOnPause(true);

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);

        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        JNIdc.screenDpi((int)Math.max(dm.xdpi, dm.ydpi));

        this.setLayerType(prefs.getInt(Config.pref_rendertype, LAYER_TYPE_HARDWARE), null);

        if (GL2JNIActivity.syms != null)
            JNIdc.data(1, GL2JNIActivity.syms);

        // By default, GLSurfaceView() creates a RGB_565 opaque surface.
        // If we want a translucent one, we should change the surface's
        // format here, using PixelFormat.TRANSLUCENT for GL Surfaces
        // is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
        if(translucent)
        	this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        else
        	this.getHolder().setFormat(PixelFormat.RGBX_8888);

        // Setup the context factory for 2.0 rendering.
        // See ContextFactory class definition below
        setEGLContextFactory(new GLCFactory.ContextFactory());

        // We need to choose an EGLConfig that matches the format of
        // our surface exactly. This is going to be done in our
        // custom config chooser. See ConfigChooser class definition
        // below.
        setEGLConfigChooser(new GLCFactory.ConfigChooser(
                8, 8, 8, translucent ? 8 : 0, depth, stencil));

        // Set the renderer responsible for frame rendering
        setRenderer(rend = new Renderer(this));
    }

    public GLSurfaceView.Renderer getRenderer()
    {
        return rend;
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom)
    {
        super.onLayout(changed, left, top, right, bottom);
        vjoyDelegate.layout(getWidth(), getHeight());
    }

    public void resetEditMode() {
        vjoyDelegate.resetEditMode();
    }

    @Override public boolean onTouchEvent(final MotionEvent event)
    {
        return vjoyDelegate.onTouchEvent(event, getWidth(), getHeight());
    }

    public void setEditVjoyMode(boolean editVjoyMode) {
        vjoyDelegate.setEditVjoyMode(editVjoyMode);
    }

    public void readCustomVjoyValues() {
        vjoyDelegate.readCustomVjoyValues();
    }
    private static class Renderer implements GLSurfaceView.Renderer
    {

        private GL2JNIView mView;

        Renderer (GL2JNIView mView) {
            this.mView = mView;
        }

        public void onDrawFrame(GL10 gl)
        {
            if (JNIdc.rendframeJava()) {
            }
            if(mView.takeScreenshot){
                mView.takeScreenshot = false;
                FileUtils.saveScreenshot(mView.getContext(), mView.getWidth(), mView.getHeight(), gl);
            }
        }

        public void onSurfaceChanged(GL10 gl,int width,int height)
        {
            gl.glViewport(0, 0, width, height);
            JNIdc.rendinitJava(width, height);
        }

        public void onSurfaceCreated(GL10 gl,EGLConfig config)
        {
            onSurfaceChanged(gl, 800, 480);
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        JNIdc.rendtermJava();
    }

    @TargetApi(19)
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            GL2JNIView.this.setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            requestLayout();
        }
    }

    private boolean takeScreenshot = false;
    public void screenGrab() {
        takeScreenshot = true;
    }
}
