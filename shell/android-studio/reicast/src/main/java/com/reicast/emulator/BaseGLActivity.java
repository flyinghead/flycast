package com.reicast.emulator;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.debug.GenerateLogs;
import com.reicast.emulator.emu.AudioBackend;
import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.periph.InputDeviceManager;
import com.reicast.emulator.periph.SipEmulator;

import java.util.ArrayList;
import java.util.List;

import tv.ouya.console.api.OuyaController;

public abstract class BaseGLActivity extends Activity implements ActivityCompat.OnRequestPermissionsResultCallback {
    private static final int STORAGE_PERM_REQUEST = 1001;
    private static final int AUDIO_PERM_REQUEST = 1002;

    protected View mView;
    protected SharedPreferences prefs;
    protected float[][] vjoy_d_cached;    // Used for VJoy editing
    private AudioBackend audioBackend;
    private Handler handler = new Handler();
    public static byte[] syms;
    private boolean audioPermissionRequested = false;
    private boolean paused = true;
    private boolean resumedCalled = false;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (!getFilesDir().exists()) {
            getFilesDir().mkdir();
        }
        prefs = PreferenceManager.getDefaultSharedPreferences(this);

        Emulator app = (Emulator)getApplicationContext();
        app.getConfigurationPrefs();
        Emulator.setCurrentActivity(this);

        OuyaController.init(this);

        String home_directory = prefs.getString(Config.pref_home, "");
        String result = JNIdc.initEnvironment((Emulator)getApplicationContext(), home_directory);
        if (result != null) {
            AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(this);
            dlgAlert.setMessage("Initialization failed. Please try again and/or reinstall.\n\n"
                    + "Error: " + result);
            dlgAlert.setTitle("Reicast Error");
            dlgAlert.setPositiveButton("Exit",
                    new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog,int id) {
                            BaseGLActivity.this.finish();
                        }
                    });
            dlgAlert.setIcon(android.R.drawable.ic_dialog_alert);
            dlgAlert.setCancelable(false);
            dlgAlert.create().show();

            return;
        }

        setStorageDirectories();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            ActivityCompat.requestPermissions(this,
                    new String[]{
                            Manifest.permission.READ_EXTERNAL_STORAGE,
                            Manifest.permission.WRITE_EXTERNAL_STORAGE
                    },
                    STORAGE_PERM_REQUEST);
        }

        InputDeviceManager.getInstance().startListening(getApplicationContext());
        register(this);

        audioBackend = new AudioBackend();

        // When viewing a resource, pass its URI to the native code for opening
        Intent intent = getIntent();
        if (intent.getAction() != null) {
            if (intent.getAction().equals(Intent.ACTION_VIEW)) {
                Uri gameUri = Uri.parse(intent.getData().toString());
                // Flush the intent to prevent multiple calls
                getIntent().setData(null);
                setIntent(null);
                if (gameUri != null && Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                    gameUri = Uri.parse(gameUri.toString().replace("content://"
                            + gameUri.getAuthority() + "/external_files", "/storage"));
                }
                if (gameUri != null)
                    JNIdc.setGameUri(gameUri.toString());
            }
        }
    }

    private void setStorageDirectories()
    {
        String android_home_directory = Environment.getExternalStorageDirectory().getAbsolutePath();
        List<String> pathList = new ArrayList<>();
        pathList.add(android_home_directory);
        pathList.addAll(FileBrowser.getExternalMounts());
        Log.i("reicast", "External storage dirs: " + pathList);
        JNIdc.setExternalStorageDirectories(pathList.toArray());
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        paused = true;
        InputDeviceManager.getInstance().stopListening();
        register(null);
        if (audioBackend != null)
            audioBackend.release();
        Emulator.setCurrentActivity(null);
        stopEmulator();
    }

    @Override
    protected void onPause() {
        super.onPause();
        resumedCalled = false;
        handleStateChange(true);
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumedCalled = true;
        handleStateChange(false);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            handleStateChange(false);
        } else {
            handleStateChange(true);
        }
    }

    protected abstract void doPause();
    protected abstract void doResume();
    protected abstract boolean isSurfaceReady();

    public void handleStateChange(boolean paused)
    {
        if (paused == this.paused)
            return;
        if (!paused && (!resumedCalled || !isSurfaceReady()))
            return;
        this.paused = paused;
        if (paused) {
            doPause();
        }
        else {
            doResume();
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
        if (event.getRepeatCount() == 0) {
            if (keyCode == KeyEvent.KEYCODE_BACK) {
                if (!JNIdc.guiIsOpen()) {
                    showMenu();
                    return true;
                }
                else if (JNIdc.guiIsContentBrowser()) {
                    finish();
                    return true;
                }
            }
            if (InputDeviceManager.getInstance().joystickButtonEvent(event.getDeviceId(), keyCode, true))
                return true;

            if (ViewConfiguration.get(this).hasPermanentMenuKey()) {
                if (keyCode == KeyEvent.KEYCODE_MENU) {
                    return showMenu();
                }
            }
        }
        return super.onKeyDown(keyCode, event);
    }

    protected void stopEmulator() {
        JNIdc.stop();
    }

    void requestRecordAudioPermission() {
        if (audioPermissionRequested)
            return;
        audioPermissionRequested = true;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            handler.post(new Runnable() {
                @Override
                public void run() {
                    ActivityCompat.requestPermissions(BaseGLActivity.this,
                            new String[]{
                                    Manifest.permission.RECORD_AUDIO
                            },
                            AUDIO_PERM_REQUEST);

                }
            });
        }
        else
        {
            onRequestPermissionsResult(AUDIO_PERM_REQUEST, new String[] { Manifest.permission.RECORD_AUDIO },
                    new int[] { PackageManager.PERMISSION_GRANTED });
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == AUDIO_PERM_REQUEST && permissions.length > 0
                && Manifest.permission.RECORD_AUDIO .equals(permissions[0]) && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            SipEmulator sip = new SipEmulator();
            sip.startRecording();
            JNIdc.setupMic(sip);
        }
        else if (requestCode == STORAGE_PERM_REQUEST) {
            setStorageDirectories();
            //setup mic
            if (Emulator.micPluggedIn())
                requestRecordAudioPermission();
        }

    }

    // Called from native code
    protected void generateErrorLog() {
        try {
            new GenerateLogs(this).execute(getFilesDir().getAbsolutePath());
		} catch (RuntimeException e) {
            AlertDialog.Builder dlgAlert  = new AlertDialog.Builder(this);
            dlgAlert.setMessage("An error occurred retrieving the log file:\n\n"
                    + e.getMessage());
            dlgAlert.setTitle("Reicast Error");
            dlgAlert.setPositiveButton("Ok",
                    new DialogInterface.OnClickListener() {
                        @Override
                        public void onClick(DialogInterface dialog,int id) {
                            dialog.cancel();
                        }
                    });
            dlgAlert.setIcon(android.R.drawable.ic_dialog_info);
            dlgAlert.setCancelable(false);
            dlgAlert.create().show();
		}
    }

    private static native void register(BaseGLActivity activity);
}
