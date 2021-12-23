package com.reicast.emulator.emu;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.WindowInsets;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.periph.InputDeviceManager;

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
        Log.i("flycast", "Display density: " + dm.xdpi + " x " + dm.ydpi + " dpi");
        JNIdc.screenDpi((int)Math.max(dm.xdpi, dm.ydpi));

        this.setLayerType(LAYER_TYPE_HARDWARE, null);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom)
    {
        super.onLayout(changed, left, top, right, bottom);
        vjoyDelegate.layout(getWidth(), getHeight());
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            // Get the display cutouts if any
            WindowInsets insets = getRootWindowInsets();
            if (insets != null) {
                DisplayCutout cutout = insets.getDisplayCutout();
                if (cutout != null)
                    JNIdc.guiSetInsets(cutout.getSafeInsetLeft(), cutout.getSafeInsetRight(),
                            cutout.getSafeInsetTop(), cutout.getSafeInsetBottom());
            }
        }
    }

    public void resetEditMode() {
        vjoyDelegate.resetEditMode();
    }

    @Override
    public boolean onTouchEvent(final MotionEvent event)
    {
        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            // Mouse motion events are reported by onTouchEvent when mouse button is down. Go figure...
            InputDeviceManager.getInstance().mouseEvent(Math.round(event.getX()), Math.round(event.getY()), event.getButtonState());
            return true;
        }
        else
            return vjoyDelegate.onTouchEvent(event, getWidth(), getHeight());
    }

    @Override
    public void surfaceCreated(SurfaceHolder surfaceHolder) {

    }

    @Override
    public void surfaceChanged(SurfaceHolder surfaceHolder, int format, int w, int h) {
        Log.i("flycast", "NativeGLView.surfaceChanged: " + w + "x" + h);
        surfaceReady = true;
        JNIdc.rendinitNative(surfaceHolder.getSurface(), w, h);
        Emulator.getCurrentActivity().handleStateChange(false);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder surfaceHolder) {
        Log.i("flycast", "NativeGLView.surfaceDestroyed");
        surfaceReady = false;
        JNIdc.rendinitNative(null, 0, 0);
        Emulator.getCurrentActivity().handleStateChange(true);
    }

    public boolean isSurfaceReady() {
        return surfaceReady;
    }

    public void pause() {
        paused = true;
        JNIdc.pause();
        Log.i("flycast", "NativeGLView.pause");
    }

    public void resume() {
        if (paused) {
            Log.i("flycast", "NativeGLView.resume");
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
