package com.reicast.emulator;

import android.os.Bundle;
import android.preference.PreferenceManager;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;
import android.widget.RelativeLayout;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.periph.VJoy;

public class GL2JNIActivity extends BaseGLActivity {
    private static ViewGroup mLayout;   // used for text input

    @Override
    protected void onCreate(Bundle icicle) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
        if (prefs.getInt(Config.pref_rendertype, 2) == 2) {
            getWindow().setFlags(
                    WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
                    WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);
        }

        // Call parent onCreate()
        super.onCreate(icicle);

        // Create the actual GLES view
        mView = new GL2JNIView(GL2JNIActivity.this, false,
                    prefs.getInt(Config.pref_renderdepth, 24), 8);
        mLayout = new RelativeLayout(this);
        mLayout.addView(mView);

        setContentView(mLayout);
    }

    public void screenGrab() {
        ((GL2JNIView)mView).screenGrab();
    }

    @Override
    protected void doPause() {
        ((GL2JNIView)mView).onPause();
        JNIdc.pause();
    }

    @Override
    protected boolean isSurfaceReady() {
        return true;    // FIXME
    }

    @Override
    protected void doResume() {
        ((GL2JNIView)mView).onResume();
        JNIdc.resume();
    }

    // Called from native code
    private void VJoyStartEditing() {
        vjoy_d_cached = VJoy.readCustomVjoyValues(getApplicationContext());
        JNIdc.show_osd();
        ((GL2JNIView)mView).setEditVjoyMode(true);
    }
    // Called from native code
    private void VJoyResetEditing() {
        VJoy.resetCustomVjoyValues(getApplicationContext());
        ((GL2JNIView)mView).readCustomVjoyValues();
        ((GL2JNIView)mView).resetEditMode();
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mView.requestLayout();
            }
        });
    }
    // Called from native code
    private void VJoyStopEditing(final boolean canceled) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (canceled)
                    ((GL2JNIView)mView).restoreCustomVjoyValues(vjoy_d_cached);
                ((GL2JNIView)mView).setEditVjoyMode(false);
            }
        });
    }
}
