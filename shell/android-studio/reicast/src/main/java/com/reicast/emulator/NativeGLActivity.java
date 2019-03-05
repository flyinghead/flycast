package com.reicast.emulator;

import android.Manifest;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.constraint.ConstraintLayout;
import android.support.design.widget.Snackbar;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.Window;
import android.widget.FrameLayout;
import android.widget.TextView;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.debug.GenerateLogs;
import com.reicast.emulator.emu.AudioBackend;
import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.periph.InputDeviceManager;
import com.reicast.emulator.periph.SipEmulator;
import com.reicast.emulator.periph.VJoy;

import java.util.ArrayList;
import java.util.List;

import tv.ouya.console.api.OuyaController;

public final class NativeGLActivity extends BaseNativeGLActivity implements ActivityCompat.OnRequestPermissionsResultCallback {
    public static byte[] syms;
    private float[][] vjoy_d_cached;    // Used for VJoy editing
    private AudioBackend audioBackend;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        InputDeviceManager.getInstance().startListening(getApplicationContext());

        Emulator app = (Emulator)getApplicationContext();
        app.getConfigurationPrefs();

        OuyaController.init(this);

        super.onCreate(savedInstanceState);

        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        String home_directory = prefs.getString(Config.pref_home, "");
        String result = JNIdc.initEnvironment((Emulator)getApplicationContext(), home_directory);
        if (result != null)
            showToastMessage("Initialization failed: " + result, Snackbar.LENGTH_LONG);

        String android_home_directory = Environment.getExternalStorageDirectory().getAbsolutePath();
        List<String> pathList = new ArrayList<>();
        pathList.add(android_home_directory);
        pathList.addAll(FileBrowser.getExternalMounts());
        Log.i("reicast", "External storage dirs: " + pathList);
        JNIdc.setExternalStorageDirectories(pathList.toArray());

        register(this);

        audioBackend = new AudioBackend();

        // FIXME Maple microphone can be plugged at any time with in-game gui
        // so this perm may be required at any time as well
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
    protected void onPause() {
        super.onPause();
        mView.pause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        mView.resume();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        InputDeviceManager.getInstance().stopListening();
        register(null);
        audioBackend.release();
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

    private void showToastMessage(String message, int duration) {
        View view = findViewById(android.R.id.content);
        Snackbar snackbar = Snackbar.make(view, message, duration);
        View snackbarLayout = snackbar.getView();
        TextView textView = (TextView) snackbarLayout.findViewById(
                android.support.design.R.id.snackbar_text);
        textView.setGravity(Gravity.CENTER_VERTICAL);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1)
            textView.setTextAlignment(View.TEXT_ALIGNMENT_GRAVITY);
        textView.setCompoundDrawablesWithIntrinsicBounds(R.drawable.ic_notification, 0, 0, 0);
        textView.setCompoundDrawablePadding(getResources()
                .getDimensionPixelOffset(R.dimen.snackbar_icon_padding));
        snackbar.show();
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
        mView.vjoy_d_custom = VJoy
                .readCustomVjoyValues(getApplicationContext());
        mView.resetEditMode();
        mView.requestLayout();
    }
    // Called from native code
    private void VJoyStopEditing(boolean canceled) {
        if (canceled)
            mView.restoreCustomVjoyValues(vjoy_d_cached);
        mView.setEditVjoyMode(false);
    }
    // Called from native code
    private void generateErrorLog() {
        new GenerateLogs(this).execute(getFilesDir().getAbsolutePath());
    }

    private static native void register(NativeGLActivity activity);
}
