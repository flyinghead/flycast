package com.reicast.emulator;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.v7.app.AppCompatDelegate;
import android.util.Log;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.AudioBackend;
import com.reicast.emulator.emu.JNIdc;

public class Emulator extends Application {
    private static Context context;

    // see MapleDeviceType in hw/maple/maple_devs.h
    public static final int MDT_SegaController = 0;
    public static final int MDT_SegaVMU = 1;
    public static final int MDT_Microphone = 2;
    public static final int MDT_PurupuruPack = 3;
    public static final int MDT_Keyboard = 4;
    public static final int MDT_Mouse = 5;
    public static final int MDT_LightGun = 6;
    public static final int MDT_NaomiJamma = 7;
    public static final int MDT_None = 8;
    public static final int MDT_Count = 9;

    public static boolean nosound = false;
    public static int vibrationDuration = 20;

    public static int maple_devices[] = {
            MDT_SegaController,
            MDT_None,
            MDT_None,
            MDT_None
    };
    public static int maple_expansion_devices[][] = {
        { MDT_SegaVMU, MDT_None },
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
        { MDT_None, MDT_None },
    };

    /**
     * Load the settings from native code
     *
     */
    public void getConfigurationPrefs() {
        Emulator.nosound = JNIdc.getNosound();
        Emulator.vibrationDuration = JNIdc.getVirtualGamepadVibration();
        JNIdc.getControllers(maple_devices, maple_expansion_devices);
    }

    /**
     * Fetch current configuration settings from the emulator and save them
     *
     */
    public void SaveAndroidSettings(String homeDirectory)
    {
        Log.i("reicast", "SaveAndroidSettings: saving preferences");
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        Emulator.nosound = JNIdc.getNosound();
        Emulator.vibrationDuration = JNIdc.getVirtualGamepadVibration();
        JNIdc.getControllers(maple_devices, maple_expansion_devices);

        prefs.edit()
                .putString(Config.pref_home, homeDirectory).apply();
        AudioBackend.getInstance().enableSound(!Emulator.nosound);
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

    static {
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
    }
}
