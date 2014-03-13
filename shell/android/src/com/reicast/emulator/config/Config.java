package com.reicast.emulator.config;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.android.util.DreamTime;
import com.reicast.emulator.R;
import com.reicast.emulator.emu.JNIdc;

public class Config {

	public static final String pref_home = "home_directory";
	public static final String pref_games = "game_directory";

	public static final String pref_nativeact = "enable_native";
	public static final String pref_dynarecopt = "dynarec_opt";
	public static final String pref_unstable = "unstable_opt";
	public static final String pref_cable = "dc_cable";
	public static final String pref_dcregion = "dc_region";
	public static final String pref_broadcast = "dc_broadcast";
	public static final String pref_limitfps = "limit_fps";
	public static final String pref_nosound = "sound_disabled";
	public static final String pref_mipmaps = "use_mipmaps";
	public static final String pref_widescreen = "stretch_view";
	public static final String pref_frameskip = "frame_skip";
	public static final String pref_pvrrender = "pvr_render";
	public static final String pref_cheatdisk = "cheat_disk";

	public static final String pref_showfps = "show_fps";
	public static final String pref_forcegpu = "force_gpu";
	public static final String pref_rendertype = "render_type";
	public static final String pref_renderdepth = "depth_render";

	public static final String pref_touchvibe = "touch_vibration_enabled";
	public static final String pref_mic = "mic_plugged_in";
	public static final String pref_vmu = "vmu_floating";

	public static boolean dynarecopt = true;
	public static boolean idleskip = true;
	public static boolean unstableopt = false;
	public static int cable = 3;
	public static int dcregion = 3;
	public static int broadcast = 4;
	public static boolean limitfps = true;
	public static boolean nobatch = false;
	public static boolean nosound = false;
	public static boolean mipmaps = true;
	public static boolean widescreen = false;
	public static boolean subdivide = false;
	public static int frameskip = 0;
	public static boolean pvrrender = false;
	public static String cheatdisk = "null";
	public static boolean nativeact = false;

	private SharedPreferences mPrefs;

	public Config(Context mContext) {
		mPrefs = PreferenceManager.getDefaultSharedPreferences(mContext);
	}

	/**
	 * Load the user configuration from preferences
	 * 
	 */
	public void getConfigurationPrefs() {
		Config.dynarecopt = mPrefs.getBoolean(pref_dynarecopt, dynarecopt);
		Config.unstableopt = mPrefs.getBoolean(pref_unstable, unstableopt);
		Config.cable = mPrefs.getInt(pref_cable, cable);
		Config.dcregion = mPrefs.getInt(pref_dcregion, dcregion);
		Config.broadcast = mPrefs.getInt(pref_broadcast, broadcast);
		Config.limitfps = mPrefs.getBoolean(pref_limitfps, limitfps);
		Config.nosound = mPrefs.getBoolean(pref_nosound, nosound);
		Config.mipmaps = mPrefs.getBoolean(pref_mipmaps, mipmaps);
		Config.widescreen = mPrefs.getBoolean(pref_widescreen, widescreen);
		Config.frameskip = mPrefs.getInt(pref_frameskip, frameskip);
		Config.pvrrender = mPrefs.getBoolean(pref_pvrrender, pvrrender);
		Config.cheatdisk = mPrefs.getString(pref_cheatdisk, cheatdisk);
		Config.nativeact = mPrefs.getBoolean(pref_nativeact, nativeact);
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
		JNIdc.nosound(Config.nosound ? 1 : 0);
		JNIdc.mipmaps(Config.mipmaps ? 1 : 0);
		JNIdc.widescreen(Config.widescreen ? 1 : 0);
		JNIdc.subdivide(Config.subdivide ? 1 : 0);
		JNIdc.frameskip(Config.frameskip);
		JNIdc.pvrrender(Config.pvrrender ? 1 : 0);
		JNIdc.cheatdisk(Config.cheatdisk);
		JNIdc.dreamtime(DreamTime.getDreamtime());
	}

	/**
	 * Read the output of a shell command
	 * 
	 * @param command
	 *            The shell command being issued to the terminal
	 */
	public static String readOutput(String command) {
		try {
			Process p = Runtime.getRuntime().exec(command);
			InputStream is = null;
			if (p.waitFor() == 0) {
				is = p.getInputStream();
			} else {
				is = p.getErrorStream();
			}
			BufferedReader br = new BufferedReader(new InputStreamReader(is),
					2048);
			String line = br.readLine();
			br.close();
			return line;
		} catch (Exception ex) {
			return "ERROR: " + ex.getMessage();
		}
	}

}
