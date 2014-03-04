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

	public static final String pref_native = "enable_native";
	public static final String pref_dynarec = "dynarec_opt";
	public static final String pref_unstable = "unstable_opt";

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
		Config.dynarecopt = mPrefs.getBoolean(pref_dynarec, dynarecopt);
		Config.unstableopt = mPrefs.getBoolean(pref_unstable, unstableopt);
		Config.cable = mPrefs.getInt("dc_cable", cable);
		Config.dcregion = mPrefs.getInt("dc_region", dcregion);
		Config.broadcast = mPrefs.getInt("dc_broadcast", broadcast);
		Config.limitfps = mPrefs.getBoolean("limit_fps", limitfps);
		Config.nosound = mPrefs.getBoolean("sound_disabled", nosound);
		Config.mipmaps = mPrefs.getBoolean("use_mipmaps", mipmaps);
		Config.widescreen = mPrefs.getBoolean("stretch_view", widescreen);
		Config.frameskip = mPrefs.getInt("frame_skip", frameskip);
		Config.pvrrender = mPrefs.getBoolean("pvr_render", pvrrender);
		Config.cheatdisk = mPrefs.getString("cheat_disk", cheatdisk);
		Config.nativeact = mPrefs.getBoolean(pref_native, nativeact);
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

	public static void customNotify(Activity activity, int icon, int message) {
		LayoutInflater inflater = activity.getLayoutInflater();
		View layout = inflater.inflate(R.layout.toast_layout,
				(ViewGroup) activity.findViewById(R.id.toast_layout_root));

		ImageView image = (ImageView) layout.findViewById(R.id.image);
		if (icon != -1) {
			image.setImageResource(icon);
		} else {
			image.setImageResource(R.drawable.ic_launcher);
		}
		
		TextView text = (TextView) layout.findViewById(R.id.text);
		text.setText(activity.getString(message));

		DisplayMetrics metrics = new DisplayMetrics();
		activity.getWindowManager().getDefaultDisplay().getMetrics(metrics);
		final float scale = activity.getResources().getDisplayMetrics().density;
		int toastPixels = (int) ((metrics.widthPixels * scale + 0.5f) / 14);

		Toast toast = new Toast(activity);
		toast.setGravity(Gravity.BOTTOM, 0, toastPixels);
		toast.setDuration(Toast.LENGTH_SHORT);
		toast.setView(layout);
		toast.show();
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
