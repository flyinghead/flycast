package com.reicast.emulator;

import android.Manifest;
import android.app.Activity;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.annotation.NonNull;
import android.support.v4.app.ActivityCompat;
import android.util.Log;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.widget.PopupWindow;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.OnScreenMenu;
import com.reicast.emulator.emu.OnScreenMenu.FpsPopup;
import com.reicast.emulator.periph.Gamepad;
import com.reicast.emulator.periph.InputDeviceManager;
import com.reicast.emulator.periph.SipEmulator;

import java.util.HashMap;

import tv.ouya.console.api.OuyaController;

public class GL2JNIActivity extends Activity implements ActivityCompat.OnRequestPermissionsResultCallback {
    public GL2JNIView mView;
    OnScreenMenu menu;
    FpsPopup fpsPop;
    private SharedPreferences prefs;

    private Gamepad pad = new Gamepad();

    public static byte[] syms;

    @Override
    protected void onCreate(Bundle icicle) {
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        prefs = PreferenceManager.getDefaultSharedPreferences(this);
        if (prefs.getInt(Config.pref_rendertype, 2) == 2) {
            getWindow().setFlags(
                    WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED,
                    WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED);
        }
        InputDeviceManager.getInstance().startListening(getApplicationContext());

        Emulator app = (Emulator)getApplicationContext();
        app.getConfigurationPrefs(prefs);
        menu = new OnScreenMenu(GL2JNIActivity.this, prefs);

        pad.isOuyaOrTV = pad.IsOuyaOrTV(GL2JNIActivity.this, false);

        /*
         * try { //int rID =
         * getResources().getIdentifier("fortyonepost.com.lfas:raw/syms.map",
         * null, null); //get the file as a stream InputStream is =
         * getResources().openRawResource(R.raw.syms);
         *
         * syms = new byte[(int) is.available()]; is.read(syms); is.close(); }
         * catch (IOException e) { e.getMessage(); e.printStackTrace(); }
         */

        String fileName = null;

        // Call parent onCreate()
        super.onCreate(icicle);
        OuyaController.init(this);

        // Populate device descriptor-to-player-map from preferences
        pad.deviceDescriptor_PlayerNum.put(
                prefs.getString(Gamepad.pref_player1, null), 0);
        pad.deviceDescriptor_PlayerNum.put(
                prefs.getString(Gamepad.pref_player2, null), 1);
        pad.deviceDescriptor_PlayerNum.put(
                prefs.getString(Gamepad.pref_player3, null), 2);
        pad.deviceDescriptor_PlayerNum.put(
                prefs.getString(Gamepad.pref_player4, null), 3);
        pad.deviceDescriptor_PlayerNum.remove(null);

        JNIdc.initControllers(Emulator.maple_devices, Emulator.maple_expansion_devices);

        int joys[] = InputDevice.getDeviceIds();
        for (int joy: joys) {
            String descriptor;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
                descriptor = InputDevice.getDevice(joy).getDescriptor();
            } else {
                descriptor = InputDevice.getDevice(joy).getName();
            }
            Log.d("reicast", "InputDevice ID: " + joy);
            Log.d("reicast",
                    "InputDevice Name: "
                            + InputDevice.getDevice(joy).getName());
            Log.d("reicast", "InputDevice Descriptor: " + descriptor);
            pad.deviceId_deviceDescriptor.put(joy, descriptor);
        }

        boolean detected = false;
        for (int joy : joys) {
            Integer playerNum = pad.deviceDescriptor_PlayerNum
                    .get(pad.deviceId_deviceDescriptor.get(joy));

            if (playerNum != null) {
                detected = true;
                String id = pad.portId[playerNum];
                pad.custom[playerNum] = prefs.getBoolean(Gamepad.pref_js_modified + id, false);
                pad.compat[playerNum] = prefs.getBoolean(Gamepad.pref_js_compat + id, false);
                pad.joystick[playerNum] = prefs.getBoolean(Gamepad.pref_js_merged + id, false);
                if (InputDevice.getDevice(joy).getName()
                        .contains(Gamepad.controllers_gamekey)) {
                    if (pad.custom[playerNum]) {
                        pad.setCustomMapping(id, playerNum, prefs);
                    } else {
                        pad.map[playerNum] = pad.getConsoleController();
                    }
                } else if (!pad.compat[playerNum]) {
                    if (pad.custom[playerNum]) {
                        pad.setCustomMapping(id, playerNum, prefs);
                    } else if (InputDevice.getDevice(joy).getName()
                            .equals(Gamepad.controllers_sony)) {
                        pad.map[playerNum] = pad.getConsoleController();
                    } else if (InputDevice.getDevice(joy).getName()
                            .equals(Gamepad.controllers_xbox)) {
                        pad.map[playerNum] = pad.getConsoleController();
                    } else if (InputDevice.getDevice(joy).getName()
                            .contains(Gamepad.controllers_shield)) {
                        pad.map[playerNum] = pad.getConsoleController();
                    } else if (InputDevice.getDevice(joy).getName()
                            .startsWith(Gamepad.controllers_moga)) {
                        pad.map[playerNum] = pad.getMogaController();
                    } else { // Ouya controller
                        pad.map[playerNum] = pad.getOUYAController();
                    }
                } else {
                    pad.getCompatibilityMap(playerNum, id, prefs);
                }
                pad.initJoyStickLayout(playerNum);
            }
        }
        if (joys.length == 0 || !detected) {
            pad.fullCompatibilityMode(prefs);
        }

        app.loadConfigurationPrefs();

        // When viewing a resource, pass its URI to the native code for opening
        if (getIntent().getAction().equals("com.reicast.EMULATOR"))
            fileName = Uri.decode(getIntent().getData().toString());

        try {
            // Create the actual GLES view
            mView = new GL2JNIView(GL2JNIActivity.this, fileName, false,
                    prefs.getInt(Config.pref_renderdepth, 24), 8, false);
            setContentView(mView);
        } catch (GL2JNIView.EmulatorInitFailed e) {
            finish();
            return;
        }

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

        if (Emulator.showfps) {
            fpsPop = menu.new FpsPopup(this);
            mView.setFpsDisplay(fpsPop);
            mView.post(new Runnable() {
                public void run() {
                    displayFPS();
                }
            });
        }
    }

    public Gamepad getPad() {
        return pad;
    }

    public void displayFPS() {
        fpsPop.showAtLocation(mView, Gravity.TOP | Gravity.LEFT, 20, 20);
        fpsPop.update(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
    }

    public void screenGrab() {
        mView.screenGrab();
    }

    public void displayPopUp(PopupWindow popUp) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            popUp.showAtLocation(mView, Gravity.BOTTOM, 0, 60);
        } else {
            popUp.showAtLocation(mView, Gravity.BOTTOM, 0, 0);
        }
        popUp.update(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT);
    }

    public void displayConfig(PopupWindow popUpConfig) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            popUpConfig.showAtLocation(mView, Gravity.BOTTOM, 0, 60);
        } else {
            popUpConfig.showAtLocation(mView, Gravity.BOTTOM, 0, 0);
        }
        popUpConfig.update(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT);
    }

    public void displayDebug(PopupWindow popUpDebug) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            popUpDebug.showAtLocation(mView, Gravity.BOTTOM, 0, 60);
        } else {
            popUpDebug.showAtLocation(mView, Gravity.BOTTOM, 0, 0);
        }
        popUpDebug.update(LayoutParams.WRAP_CONTENT,
                LayoutParams.WRAP_CONTENT);
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

        if (keyCode == pad.getSelectButtonCode()) {
            return showMenu();
        }
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
        mView.onPause();
        JNIdc.pause();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        InputDeviceManager.getInstance().stopListening();
        if (mView != null) {
            mView.onDestroy();
            // FIXME This should be called to deinit what's been inited...
            JNIdc.destroy();
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mView.onResume();
        JNIdc.resume();
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (permissions.length > 0 && permissions[0] == Manifest.permission.RECORD_AUDIO && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
            SipEmulator sip = new SipEmulator();
            sip.startRecording();
            JNIdc.setupMic(sip);
        }
    }
}
