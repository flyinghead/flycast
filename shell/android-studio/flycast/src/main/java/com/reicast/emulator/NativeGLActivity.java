package com.reicast.emulator;

import android.os.Bundle;
import android.util.Log;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;

import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.NativeGLView;
import com.reicast.emulator.periph.VJoy;

public final class NativeGLActivity extends BaseGLActivity {

    private static ViewGroup mLayout;   // used for text input
    private NativeGLView mView;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        Log.i("flycast", "NativeGLActivity.onCreate");
        requestWindowFeature(Window.FEATURE_NO_TITLE);

        super.onCreate(savedInstanceState);

        // Create the actual GL view
        mView = new NativeGLView(this);
        mLayout = new RelativeLayout(this);
        mLayout.addView(mView);

        setContentView(mLayout);
        Log.i("flycast", "NativeGLActivity.onCreate done");
    }

    @Override
    protected void doPause() {
        mView.pause();
    }

    @Override
    protected void doResume() {
        mView.resume();
    }

    @Override
    public boolean isSurfaceReady() {
        return mView.isSurfaceReady();
    }

    // Called from native code
    private void VJoyStartEditing() {
        vjoy_d_cached = VJoy.readCustomVjoyValues(getApplicationContext());
        JNIdc.show_osd();
        mView.setEditVjoyMode(true);
    }
    // Called from native code
    private void VJoyResetEditing() {
        VJoy.resetCustomVjoyValues(getApplicationContext());
        mView.readCustomVjoyValues();
        mView.resetEditMode();
        handler.post(new Runnable() {
            @Override
            public void run() {
                mView.requestLayout();
            }
        });
    }
    // Called from native code
    private void VJoyStopEditing(final boolean canceled) {
        handler.post(new Runnable() {
            @Override
            public void run() {
                if (canceled)
                    mView.restoreCustomVjoyValues(vjoy_d_cached);
                mView.setEditVjoyMode(false);
            }
        });
    }
}
