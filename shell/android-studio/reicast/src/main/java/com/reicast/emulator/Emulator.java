package com.reicast.emulator;

import android.app.Activity;
import android.app.Application;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.support.v7.app.AppCompatDelegate;
import android.util.Log;

import com.reicast.emulator.emu.JNIdc;

public class Emulator extends Application {
    public static final String pref_dynarecopt = "dynarec_opt";
    public static final String pref_unstable = "unstable_opt";
    public static final String pref_dynsafemode = "dyn_safemode";
    public static final String pref_idleskip = "idle_skip";
    public static final String pref_cable = "dc_cable";
    public static final String pref_dcregion = "dc_region";
    public static final String pref_broadcast = "dc_broadcast";
    public static final String pref_language = "dc_language";
    public static final String pref_limitfps = "limit_fps";
    public static final String pref_nosound = "sound_disabled";
    public static final String pref_nobatch = "nobatch";
    public static final String pref_interrupt = "delay_interrupt";
    public static final String pref_mipmaps = "use_mipmaps";
    public static final String pref_widescreen = "stretch_view";
    public static final String pref_frameskip = "frame_skip";
    public static final String pref_pvrrender = "pvr_render";
    public static final String pref_syncedrender = "synced_render";
    public static final String pref_modvols = "modifier_volumes";
    public static final String pref_clipping = "clipping";
    public static final String pref_bootdisk = "boot_disk";
    public static final String pref_usereios = "use_reios";
    public static final String pref_customtextures = "custom_textures";
    public static final String pref_showfps = "show_fps";
    public static final String pref_RenderToTextureBuffer = "RenderToTextureBuffer";
    public static final String pref_RenderToTextureUpscale = "RenderToTextureUpscale";
    public static final String pref_TextureUpscale = "TextureUpscale";
    public static final String pref_MaxFilteredTextureSize= "MaxFilteredTextureSize";
    public static final String pref_MaxThreads = "MaxThreads";
    public static final String pref_controller_type = "controller_type";
    public static final String pref_peripheral_type = "peripheral_type";
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

    public static boolean dynarecopt = true;
    public static boolean idleskip = true;
    public static boolean unstableopt = false;
    public static boolean dynsafemode = true;
    public static int cable = 3;
    public static int dcregion = 3;
    public static int broadcast = 4;
    public static int language = 6;
    public static boolean limitfps = true;
    public static boolean nobatch = false;
    public static boolean nosound = false;
    public static boolean interrupt = false;
    public static boolean mipmaps = true;
    public static boolean widescreen = false;
    public static int frameskip = 0;
    public static int pvrrender = 0;
    public static boolean syncedrender = true;
    public static boolean modvols = true;
    public static boolean clipping = true;
    public static String bootdisk = null;
    public static boolean usereios = false;
    public static boolean customtextures = false;
    public static boolean showfps = false;
    public static boolean RenderToTextureBuffer = false;
    public static int RenderToTextureUpscale = 1;
    public static int TextureUpscale = 1;
    public static int MaxFilteredTextureSize = 256;
    public static int MaxThreads = 1;
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
     * Load the user configuration from preferences
     *
     */
    public void getConfigurationPrefs(SharedPreferences mPrefs) {
        Emulator.dynarecopt = mPrefs.getBoolean(pref_dynarecopt, dynarecopt);
        Emulator.unstableopt = mPrefs.getBoolean(pref_unstable, unstableopt);
        Emulator.dynsafemode = mPrefs.getBoolean(pref_dynsafemode, dynsafemode);
        Emulator.idleskip = mPrefs.getBoolean(pref_idleskip, idleskip);
        Emulator.cable = mPrefs.getInt(pref_cable, cable);
        Emulator.dcregion = mPrefs.getInt(pref_dcregion, dcregion);
        Emulator.broadcast = mPrefs.getInt(pref_broadcast, broadcast);
        Emulator.language = mPrefs.getInt(pref_language, language);
        Emulator.limitfps = mPrefs.getBoolean(pref_limitfps, limitfps);
        Emulator.nosound = mPrefs.getBoolean(pref_nosound, nosound);
        Emulator.nobatch = mPrefs.getBoolean(pref_nobatch, nobatch);
        Emulator.mipmaps = mPrefs.getBoolean(pref_mipmaps, mipmaps);
        Emulator.widescreen = mPrefs.getBoolean(pref_widescreen, widescreen);
        Emulator.frameskip = mPrefs.getInt(pref_frameskip, frameskip);
        Emulator.pvrrender = mPrefs.getInt(pref_pvrrender, pvrrender);
        Emulator.syncedrender = mPrefs.getBoolean(pref_syncedrender, syncedrender);
        Emulator.modvols = mPrefs.getBoolean(pref_modvols, modvols);
        Emulator.clipping = mPrefs.getBoolean(pref_clipping, clipping);
        Emulator.bootdisk = mPrefs.getString(pref_bootdisk, bootdisk);
        Emulator.usereios = mPrefs.getBoolean(pref_usereios, usereios);
        Emulator.customtextures = mPrefs.getBoolean(pref_customtextures, customtextures);
        Emulator.showfps = mPrefs.getBoolean(pref_showfps, showfps);
        Emulator.RenderToTextureBuffer = mPrefs.getBoolean(pref_RenderToTextureBuffer, RenderToTextureBuffer);
        Emulator.RenderToTextureUpscale = mPrefs.getInt(pref_RenderToTextureUpscale, RenderToTextureUpscale);
        Emulator.TextureUpscale = mPrefs.getInt(pref_TextureUpscale, TextureUpscale);
        Emulator.MaxFilteredTextureSize = mPrefs.getInt(pref_MaxFilteredTextureSize, MaxFilteredTextureSize);
        Emulator.MaxThreads = mPrefs.getInt(pref_MaxThreads, MaxThreads);
        for (int i = 0; i < maple_devices.length; i++) {
            maple_devices[i] = mPrefs.getInt(pref_controller_type + i, Emulator.maple_devices[i]);
            maple_expansion_devices[i][0] = mPrefs.getInt(pref_peripheral_type + i + "0", Emulator.maple_expansion_devices[i][0]);
            maple_expansion_devices[i][1] = mPrefs.getInt(pref_peripheral_type + i + "1", Emulator.maple_expansion_devices[i][1]);
        }
    }

    /**
     * Write configuration settings to the emulator
     *
     */
    public void loadConfigurationPrefs() {
        JNIdc.setDynarec(Emulator.dynarecopt);
        JNIdc.setIdleskip(Emulator.idleskip);
        JNIdc.setUnstable(Emulator.unstableopt);
        JNIdc.setSafemode(Emulator.dynsafemode);
        JNIdc.setCable(Emulator.cable);
        JNIdc.setRegion(Emulator.dcregion);
        JNIdc.setBroadcast(Emulator.broadcast);
        JNIdc.setLanguage(Emulator.language);
        JNIdc.setLimitfps(Emulator.limitfps);
        JNIdc.setNobatch(Emulator.nobatch);
        JNIdc.setNosound(Emulator.nosound);
        JNIdc.setMipmaps(Emulator.mipmaps);
        JNIdc.setWidescreen(Emulator.widescreen);
        JNIdc.setFrameskip(Emulator.frameskip);
        JNIdc.setPvrrender(Emulator.pvrrender);
        JNIdc.setSyncedrender(Emulator.syncedrender);
        JNIdc.setModvols(Emulator.modvols);
        JNIdc.setClipping(Emulator.clipping);
        JNIdc.setUsereios(Emulator.usereios);
        JNIdc.bootdisk(Emulator.bootdisk);
        JNIdc.setCustomtextures(Emulator.customtextures);
        JNIdc.setShowfps(Emulator.showfps);
        JNIdc.setRenderToTextureBuffer(Emulator.RenderToTextureBuffer);
        JNIdc.setRenderToTextureUpscale(Emulator.RenderToTextureUpscale);
        JNIdc.setTextureUpscale(Emulator.TextureUpscale);
        JNIdc.setMaxFilteredTextureSize(Emulator.MaxFilteredTextureSize);
        JNIdc.setMaxThreads(Emulator.MaxThreads);
        JNIdc.initControllers(maple_devices, maple_expansion_devices);
    }

    public void loadGameConfiguration(String gameId) {
        SharedPreferences mPrefs = getSharedPreferences(gameId, Activity.MODE_PRIVATE);
        JNIdc.setDynarec(mPrefs.getBoolean(pref_dynarecopt, dynarecopt));
        JNIdc.setUnstable(mPrefs.getBoolean(pref_unstable, unstableopt));
        JNIdc.setSafemode(mPrefs.getBoolean(pref_dynsafemode, dynsafemode));
        JNIdc.setFrameskip(mPrefs.getInt(pref_frameskip, frameskip));
        JNIdc.setPvrrender(mPrefs.getInt(pref_pvrrender, pvrrender));
        JNIdc.setSyncedrender(mPrefs.getBoolean(pref_syncedrender, syncedrender));
        JNIdc.setModvols(mPrefs.getBoolean(pref_modvols, modvols));
        JNIdc.setClipping(mPrefs.getBoolean(pref_clipping, clipping));
        JNIdc.bootdisk(mPrefs.getString(pref_bootdisk, bootdisk));
        JNIdc.setCustomtextures(mPrefs.getBoolean(pref_customtextures, customtextures));
    }

    /**
     * Fetch current configuration settings from the emulator and save them
     *
     */
    public void SaveSettings()
    {
        Log.i("Emulator", "SaveSettings: saving preferences");
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);

        Emulator.dynarecopt = JNIdc.getDynarec();
        Emulator.idleskip = JNIdc.getIdleskip();
        Emulator.unstableopt = JNIdc.getUnstable();
        Emulator.dynsafemode = JNIdc.getSafemode();
        Emulator.cable = JNIdc.getCable();
        Emulator.dcregion = JNIdc.getRegion();
        Emulator.broadcast = JNIdc.getBroadcast();
        Emulator.language = JNIdc.getLanguage();
        Emulator.limitfps = JNIdc.getLimitfps();
        Emulator.nobatch = JNIdc.getNobatch();
        Emulator.nosound = JNIdc.getNosound();
        Emulator.mipmaps = JNIdc.getMipmaps();
        Emulator.widescreen = JNIdc.getWidescreen();
        Emulator.frameskip = JNIdc.getFrameskip();
        Emulator.pvrrender = JNIdc.getPvrrender();
        Emulator.syncedrender = JNIdc.getSyncedrender();
        Emulator.modvols = JNIdc.getModvols();
        Emulator.clipping = JNIdc.getClipping();
        Emulator.usereios = JNIdc.getUsereios();
        //Emulator.bootdisk = JNIdc.getBootdisk();
        Emulator.customtextures = JNIdc.getCustomtextures();
        Emulator.showfps = JNIdc.getShowfps();
        Emulator.RenderToTextureBuffer = JNIdc.getRenderToTextureBuffer();
        Emulator.RenderToTextureUpscale = JNIdc.getRenderToTextureUpscale();
        Emulator.TextureUpscale = JNIdc.getTextureUpscale();
        Emulator.MaxFilteredTextureSize = JNIdc.getMaxFilteredTextureSize();
        Emulator.MaxThreads = JNIdc.getMaxThreads();
        JNIdc.getControllers(maple_devices, maple_expansion_devices);

        SharedPreferences.Editor editor = prefs.edit()
                .putBoolean(Emulator.pref_dynarecopt, Emulator.dynarecopt)
                .putBoolean(Emulator.pref_idleskip, Emulator.idleskip)
                .putBoolean(Emulator.pref_unstable, Emulator.unstableopt)
                .putBoolean(Emulator.pref_dynsafemode, Emulator.dynsafemode)
                .putInt(Emulator.pref_cable, Emulator.cable)
                .putInt(Emulator.pref_dcregion, Emulator.dcregion)
                .putInt(Emulator.pref_broadcast, Emulator.broadcast)
                .putInt(Emulator.pref_language, Emulator.language)
                .putBoolean(Emulator.pref_limitfps, Emulator.limitfps)
                .putBoolean(Emulator.pref_nobatch, Emulator.nobatch)
                .putBoolean(Emulator.pref_nosound, Emulator.nosound)
                .putBoolean(Emulator.pref_mipmaps, Emulator.mipmaps)
                .putBoolean(Emulator.pref_widescreen, Emulator.widescreen)
                .putInt(Emulator.pref_frameskip, Emulator.frameskip)
                .putInt(Emulator.pref_pvrrender, Emulator.pvrrender)
                .putBoolean(Emulator.pref_syncedrender, Emulator.syncedrender)
                .putBoolean(Emulator.pref_modvols, Emulator.modvols)
                .putBoolean(Emulator.pref_clipping, Emulator.clipping)
                .putBoolean(Emulator.pref_usereios, Emulator.usereios)
                .putBoolean(Emulator.pref_customtextures, Emulator.customtextures)
                .putBoolean(Emulator.pref_showfps, Emulator.showfps)
                .putBoolean(Emulator.pref_RenderToTextureBuffer, Emulator.RenderToTextureBuffer)
                .putInt(Emulator.pref_RenderToTextureUpscale, Emulator.RenderToTextureUpscale)
                .putInt(Emulator.pref_TextureUpscale, Emulator.TextureUpscale)
                .putInt(Emulator.pref_MaxFilteredTextureSize, Emulator.MaxFilteredTextureSize)
                .putInt(Emulator.pref_MaxThreads, Emulator.MaxThreads);
        for (int i = 0; i < maple_devices.length; i++) {
            editor.putInt(pref_controller_type + i, Emulator.maple_devices[i]);
            editor.putInt(pref_peripheral_type + i + "0", Emulator.maple_expansion_devices[i][0]);
            editor.putInt(pref_peripheral_type + i + "1", Emulator.maple_expansion_devices[i][1]);
        }
        editor.apply();
    }

    public static boolean micPluggedIn() {
        for (int i = 0; i < maple_expansion_devices.length; i++)
            if (maple_expansion_devices[i][0] == MDT_Microphone
                    || maple_expansion_devices[i][1] == MDT_Microphone)
                return true;
        return false;
    }

    static {
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
    }
}
