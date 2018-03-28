package com.reicast.emulator.config;

import java.io.BufferedReader;
import java.io.InputStream;
import java.io.InputStreamReader;

public class Config {

	public static final String pref_home = "home_directory";
	public static final String pref_games = "game_directory";
	public static final String pref_theme = "button_theme";

	public static final String pref_gamedetails = "game_details";

	public static final String pref_showfps = "show_fps";
	public static final String pref_rendertype = "render_type";
	public static final String pref_renderdepth = "depth_render";
	public static final String pref_forcegpu = "force_gpu";

	public static final String pref_touchvibe = "touch_vibration_enabled";
	public static final String pref_vibrationDuration = "vibration_duration";

	public static int vibrationDuration = 20;

	public static final String pref_mic = "mic_plugged_in";
	public static final String pref_vmu = "vmu_floating";

	public static String git_api = "https://api.github.com/repos/reicast/reicast-emulator/commits";
	public static String git_issues = "https://github.com/reicast/reicast-emulator/issues/";
	public static String log_url = "http://loungekatt.sytes.net:3194/ReicastBot/report/submit.php";
	public static String report_url = "http://loungekatt.sytes.net:3194/ReicastBot/report/logs/";

	public static boolean externalIntent = false;

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
