package com.reicast.emulator;

import android.os.Bundle;
import android.support.annotation.Nullable;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.RelativeLayout;

import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.NativeGLView;
import com.reicast.emulator.periph.VJoy;

public final class NativeGLActivity extends BaseGLActivity {

    private static ViewGroup mLayout;   // used for text input

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(savedInstanceState);

        // Create the actual GL view
        mView = new NativeGLView(this);
        mLayout = new RelativeLayout(this);
        mLayout.addView(mView);

        setContentView(mLayout);
    }

    @Override
    protected void doPause() {
        ((NativeGLView)mView).pause();
    }

    @Override
    protected void doResume() {
        ((NativeGLView)mView).resume();
    }

    @Override
    public boolean isSurfaceReady() {
        return ((NativeGLView)mView).isSurfaceReady();
    }

    // Called from native code
    private void VJoyStartEditing() {
        vjoy_d_cached = VJoy.readCustomVjoyValues(getApplicationContext());
        JNIdc.show_osd();
        ((NativeGLView)mView).setEditVjoyMode(true);
    }
    // Called from native code
    private void VJoyResetEditing() {
        VJoy.resetCustomVjoyValues(getApplicationContext());
        ((NativeGLView)mView).readCustomVjoyValues();
        ((NativeGLView)mView).resetEditMode();
        mView.requestLayout();
    }
    // Called from native code
    private void VJoyStopEditing(boolean canceled) {
        if (canceled)
            ((NativeGLView)mView).restoreCustomVjoyValues(vjoy_d_cached);
        ((NativeGLView)mView).setEditVjoyMode(false);
    }
}
