package com.reicast.emulator.emu;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Handler;
import android.os.SystemClock;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.NativeGLActivity;
import com.reicast.emulator.config.Config;

public class NativeGLView extends SurfaceView implements SurfaceHolder.Callback {
    private boolean surfaceReady = false;
    private boolean paused = false;
    VirtualJoystickDelegate vjoyDelegate;

    public void restoreCustomVjoyValues(float[][] vjoy_d_cached) {
        vjoyDelegate.restoreCustomVjoyValues(vjoy_d_cached);
    }

    @TargetApi(Build.VERSION_CODES.HONEYCOMB)
    public NativeGLView(Context context) {
        this(context, null);
    }

    public NativeGLView(final Context context, AttributeSet attrs) {
        super(context, attrs);
        getHolder().addCallback(this);
        setKeepScreenOn(true);
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            setOnSystemUiVisibilityChangeListener (new OnSystemUiVisibilityChangeListener() {
                public void onSystemUiVisibilityChange(int visibility) {
                    if ((visibility & SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                        NativeGLView.this.setSystemUiVisibility(
                                SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                                        | SYSTEM_UI_FLAG_FULLSCREEN
                                        | SYSTEM_UI_FLAG_HIDE_NAVIGATION);
                        requestLayout();
                    }
                }
            });
        }
        vjoyDelegate = new VirtualJoystickDelegate(this);

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);

        DisplayMetrics dm = context.getResources().getDisplayMetrics();
        JNIdc.screenDpi((int)Math.max(dm.xdpi, dm.ydpi));

        this.setLayerType(prefs.getInt(Config.pref_rendertype, LAYER_TYPE_HARDWARE), null);

        if (NativeGLActivity.syms != null)
            JNIdc.data(1, NativeGLActivity.syms);
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

    @Override
    public boolean onTouchEvent(final MotionEvent event)
    {
        return vjoyDelegate.onTouchEvent(event, getWidth(), getHeight());
    }

    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {

    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int w, int h) {
        //Log.i("reicast", "NativeGLView.surfaceChanged: " + w + "x" + h);
        surfaceReady = true;
        JNIdc.rendinitNative(surfaceHolder.getSurface());
        Emulator.getCurrentActivity().handleStateChange(false);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
        //Log.i("reicast", "NativeGLView.surfaceDestroyed");
        surfaceReady = false;
        JNIdc.rendinitNative(null);
        Emulator.getCurrentActivity().handleStateChange(true);
    }

    public boolean isSurfaceReady() {
        return surfaceReady;
    }

    public void pause() {
        paused = true;
        JNIdc.pause();
        //Log.i("reicast", "NativeGLView.pause");
    }

    public void resume() {
        if (paused) {
            //Log.i("reicast", "NativeGLView.resume");
            paused = false;
            setFocusable(true);
            setFocusableInTouchMode(true);
            requestFocus();
            JNIdc.resume();
        }
    }

    @TargetApi(19)
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus && Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
            requestLayout();
        }
    }

    public void readCustomVjoyValues() {
        vjoyDelegate.readCustomVjoyValues();
    }

    public void setEditVjoyMode(boolean editVjoyMode) {
        vjoyDelegate.setEditVjoyMode(editVjoyMode);
    }
}
