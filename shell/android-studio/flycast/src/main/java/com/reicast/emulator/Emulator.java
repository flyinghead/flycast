package com.reicast.emulator;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.net.wifi.WifiManager;
import android.preference.PreferenceManager;
import android.util.Log;

import androidx.appcompat.app.AppCompatDelegate;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.JNIdc;

public class Emulator extends Application {
    private static Context context;
    private static BaseGLActivity currentActivity;
    private WifiManager wifiManager = null;
    private WifiManager.MulticastLock multicastLock = null;

    // see MapleDeviceType in hw/maple/maple_devs.h
    public static final int MDT_Microphone = 2;
    public static final int MDT_None = 8;

    public static int vibrationDuration = 20;

    public static int maple_devices[] = {
            MDT_None,
            MDT_None,
            MDT_None,
            MDT_None
    };
    public static int maple_expansion_devices[][] = {
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
    };

    /**
     * Load the settings from native code
     *
     */
    public void getConfigurationPrefs() {
        Emulator.vibrationDuration = JNIdc.getVirtualGamepadVibration();
        JNIdc.getControllers(maple_devices, maple_expansion_devices);
    }

    /**
     * Fetch current configuration settings from the emulator and save them
     * Called from JNI code
     */
    public void SaveAndroidSettings(String homeDirectory)
    {
        Log.i("flycast", "SaveAndroidSettings: saving preferences");
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        Emulator.vibrationDuration = JNIdc.getVirtualGamepadVibration();
        JNIdc.getControllers(maple_devices, maple_expansion_devices);

        prefs.edit()
                .putString(Config.pref_home, homeDirectory).apply();

        if (micPluggedIn() && currentActivity instanceof BaseGLActivity) {
            ((BaseGLActivity)currentActivity).requestRecordAudioPermission();
        }
    }

    public static boolean micPluggedIn() {
        JNIdc.getControllers(maple_devices, maple_expansion_devices);
        for (int i = 0; i < maple_expansion_devices.length; i++)
            if (maple_expansion_devices[i][0] == MDT_Microphone
                    || maple_expansion_devices[i][1] == MDT_Microphone)
                return true;
        return false;
    }

    @Override
    public void onCreate() {
        super.onCreate();
        Emulator.context = getApplicationContext();
    }

    public static Context getAppContext() {
        return Emulator.context;
    }

    public static BaseGLActivity getCurrentActivity() {
        return Emulator.currentActivity;
    }

    public static void setCurrentActivity(BaseGLActivity activity) {
        Emulator.currentActivity = activity;
    }

    static {
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
    }

    public void enableNetworkBroadcast(boolean enable) {
        if (enable) {
            if (wifiManager == null)
                wifiManager = (WifiManager)Emulator.context.getSystemService(Context.WIFI_SERVICE);
            if (multicastLock == null)
                multicastLock = wifiManager.createMulticastLock("Flycast");
            if (multicastLock != null && !multicastLock.isHeld())
                multicastLock.acquire();
        }
        else
        {
            if (multicastLock != null && multicastLock.isHeld())
                multicastLock.release();
        }
    }
}
