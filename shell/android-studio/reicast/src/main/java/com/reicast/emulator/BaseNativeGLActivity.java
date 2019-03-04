package com.reicast.emulator;

import android.app.Activity;
import android.net.Uri;
import android.os.Bundle;
import android.support.annotation.Nullable;
import android.util.Log;
import android.view.SurfaceHolder;

import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.NativeGLView;

public class BaseNativeGLActivity extends Activity {
    protected NativeGLView mView;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Create the actual GL view
//        mView = new NativeGLView(this);
//        setContentView(mView);
        setContentView(R.layout.nativegl_content);
        mView = (NativeGLView)findViewById(R.id.glView);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopEmulator();
    }

    protected void stopEmulator() {
        JNIdc.stop();
    }
}
