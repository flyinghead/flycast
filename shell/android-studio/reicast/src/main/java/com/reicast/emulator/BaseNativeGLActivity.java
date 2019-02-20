package com.reicast.emulator;

import android.app.Activity;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.util.Log;
import android.view.SurfaceHolder;

import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.NativeGLView;

public class BaseNativeGLActivity extends Activity implements SurfaceHolder.Callback {
    protected boolean editVjoyMode;
    protected NativeGLView mView;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String fileName = null;
        if (getIntent().getAction().equals("com.reicast.EMULATOR"))
            fileName = Uri.decode(getIntent().getData().toString());

        // Create the actual GL view
        try {
            mView = new NativeGLView(this, fileName, editVjoyMode);
            setContentView(mView);
            mView.getHolder().addCallback(this);
        } catch (NativeGLView.EmulatorInitFailed e) {
            finish();
        }
    }

    @Override
    protected void onPause() {
        super.onPause();
        JNIdc.pause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        JNIdc.resume();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopEmulator();
    }

    protected void stopEmulator() {
        if (mView != null) {
            mView.stop();
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {

    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        //Log.i("BaseNativeGLActivity", "surfaceChanged: " + w + "x" + h);
        JNIdc.rendinitNative(holder.getSurface(), w, h);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        //Log.i("BaseNativeGLActivity", "surfaceDestroyed");
        JNIdc.rendinitNative(null, 0, 0);
    }
}
