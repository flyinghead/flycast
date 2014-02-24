package com.reicast.emulator.config;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

import com.android.util.DreamTime;
import com.reicast.emulator.emu.JNIdc;

public class Config {

	public static boolean dynarecopt = true;
	public static boolean idleskip = true;
	public static boolean unstableopt = false;
	public static int cable = 3;
	public static int dcregion = 3;
	public static int broadcast = 4;
	public static boolean limitfps = true;
	public static boolean nobatch = false;
	public static boolean mipmaps = true;
	public static boolean widescreen = false;
	public static boolean subdivide = false;
	public static int frameskip = 0;
	public static boolean pvrrender = false;
	public static String cheatdisk = "null";

	private SharedPreferences mPrefs;

	public Config(Context mContext) {
		mPrefs = PreferenceManager.getDefaultSharedPreferences(mContext);
	}

	/**
	 * Load the user configuration from preferences
	 * 
	 * @param sharedpreferences
	 *            The preference instance to load values from
	 */
	public void getConfigurationPrefs() {
		Config.dynarecopt = mPrefs.getBoolean("dynarec_opt", dynarecopt);
		Config.unstableopt = mPrefs.getBoolean("unstable_opt", unstableopt);
		Config.cable = mPrefs.getInt("dc_cable", cable);
		Config.dcregion = mPrefs.getInt("dc_region", dcregion);
		Config.broadcast = mPrefs.getInt("dc_broadcast", broadcast);
		Config.limitfps = mPrefs.getBoolean("limit_fps", limitfps);
		Config.mipmaps = mPrefs.getBoolean("use_mipmaps", mipmaps);
		Config.widescreen = mPrefs.getBoolean("stretch_view", widescreen);
		Config.frameskip = mPrefs.getInt("frame_skip", frameskip);
		Config.pvrrender = mPrefs.getBoolean("pvr_render", pvrrender);
		Config.cheatdisk = mPrefs.getString("cheat_disk", cheatdisk);
	}

	/**
	 * Write configuration settings to the emulator
	 * 
	 */
	public void loadConfigurationPrefs() {
		JNIdc.dynarec(Config.dynarecopt ? 1 : 0);
		JNIdc.idleskip(Config.idleskip ? 1 : 0);
		JNIdc.unstable(Config.unstableopt ? 1 : 0);
		JNIdc.cable(Config.cable);
		JNIdc.region(Config.dcregion);
		JNIdc.broadcast(Config.broadcast);
		JNIdc.limitfps(Config.limitfps ? 1 : 0);
		JNIdc.nobatch(Config.nobatch ? 1 : 0);
		JNIdc.mipmaps(Config.mipmaps ? 1 : 0);
		JNIdc.widescreen(Config.widescreen ? 1 : 0);
		JNIdc.subdivide(Config.subdivide ? 1 : 0);
		JNIdc.frameskip(Config.frameskip);
		JNIdc.pvrrender(Config.pvrrender ? 1 : 0);
		JNIdc.cheatdisk(Config.cheatdisk);
		JNIdc.dreamtime(DreamTime.getDreamtime());
	}

}
