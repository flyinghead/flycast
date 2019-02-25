package com.reicast.emulator;

import android.Manifest;
import android.app.Activity;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.ViewConfiguration;
import android.view.Window;

import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.NativeGLView;
import com.reicast.emulator.periph.InputDeviceManager;
import com.reicast.emulator.periph.SipEmulator;

import tv.ouya.console.api.OuyaController;

public class NativeGLActivity extends BaseNativeGLActivity implements ActivityCompat.OnRequestPermissionsResultCallback {
    public static byte[] syms;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        InputDeviceManager.getInstance().startListening(getApplicationContext());

        Emulator app = (Emulator)getApplicationContext();
        app.getConfigurationPrefs(prefs);

        OuyaController.init(this);
        JNIdc.initControllers(Emulator.maple_devices, Emulator.maple_expansion_devices);

        app.loadConfigurationPrefs();

        super.onCreate(savedInstanceState);

        //setup mic
        if (Emulator.micPluggedIn()) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                ActivityCompat.requestPermissions(this,
                        new String[]{
                                Manifest.permission.RECORD_AUDIO
                        },
                        0);
            }
            else
            {
                onRequestPermissionsResult(0, new String[] { Manifest.permission.RECORD_AUDIO }, new int[] { PackageManager.PERMISSION_GRANTED });
            }
        }
    }

    private boolean showMenu() {
        JNIdc.guiOpenSettings();
        return true;
    }

    private boolean processJoystickInput(MotionEvent event, int axis) {
        float v = event.getAxisValue(axis);
        return InputDeviceManager.getInstance().joystickAxisEvent(event.getDeviceId(), axis, (int)Math.round(v * 32767.f));
    }
    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) == InputDevice.SOURCE_CLASS_JOYSTICK && event.getAction() == MotionEvent.ACTION_MOVE) {
            boolean rc = processJoystickInput(event, MotionEvent.AXIS_X);
            rc |= processJoystickInput(event, MotionEvent.AXIS_Y);
            rc |= processJoystickInput(event, MotionEvent.AXIS_LTRIGGER);
            rc |= processJoystickInput(event, MotionEvent.AXIS_RTRIGGER);
            rc |= processJoystickInput(event, MotionEvent.AXIS_RX);
            rc |= processJoystickInput(event, MotionEvent.AXIS_RY);
            if (rc)
                return true;
        }
        else if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) == InputDevice.SOURCE_CLASS_POINTER)
        {
            if (mView != null) {
                float scl = mView.getHeight() / 480.0f;
                float tx = (mView.getWidth() - 640.0f * scl) / 2;
                int xpos = Math.round((event.getX() - tx) / scl);
                int ypos = Math.round(event.getY() / scl);
                InputDeviceManager.getInstance().mouseEvent(xpos, ypos, event.getButtonState());
            }

        }
        return super.onGenericMotionEvent(event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (InputDeviceManager.getInstance().joystickButtonEvent(event.getDeviceId(), keyCode, false))
            return true;
        return super.onKeyUp(keyCode, event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            // FIXME
            showMenu();
        }
        if (InputDeviceManager.getInstance().joystickButtonEvent(event.getDeviceId(), keyCode, true))
            return true;

        if (ViewConfiguration.get(this).hasPermanentMenuKey()) {
            if (keyCode == KeyEvent.KEYCODE_MENU) {
                return showMenu();
            }
        }
        return super.onKeyDown(keyCode, event);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        InputDeviceManager.getInstance().stopListening();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (permissions.length > 0 && Manifest.permission.RECORD_AUDIO .equals(permissions[0]) && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            SipEmulator sip = new SipEmulator();
            sip.startRecording();
            JNIdc.setupMic(sip);
        }
    }
}
