package com.reicast.emulator;

import android.content.SharedPreferences;
import android.os.Environment;

import com.reicast.emulator.config.Config;

import org.apache.commons.lang3.StringUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.HashSet;

public class FileBrowser {
	android.support.v4.content.FileProvider provider;	// To avoid ClassNotFoundException at runtime

	public static HashSet<String> getExternalMounts() {
		final HashSet<String> out = new HashSet<>();
		String reg = "(?i).*vold.*(vfat|ntfs|exfat|fat32|ext3|ext4|fuse|sdfat).*rw.*";
		StringBuilder s = new StringBuilder();
		try {
			final Process process = new ProcessBuilder().command("mount")
					.redirectErrorStream(true).start();
			process.waitFor();
			InputStream is = process.getInputStream();
			byte[] buffer = new byte[1024];
			while (is.read(buffer) != -1) {
				s.append(new String(buffer));
			}
			is.close();

			String[] lines = s.toString().split("\n");
			for (String line : lines) {
				if (StringUtils.containsIgnoreCase(line, "secure"))
					continue;
				if (StringUtils.containsIgnoreCase(line, "asec"))
					continue;
				if (line.matches(reg)) {
					String[] parts = line.split(" ");
					for (String part : parts) {
						if (part.startsWith("/"))
							if (!StringUtils.containsIgnoreCase(part, "vold")) {
								part = part.replace("/mnt/media_rw", "/storage");
								out.add(part);
							}
					}
				}
			}
		} catch (final Exception e) {
			e.printStackTrace();
		}
		return out;
	}

	public static void installButtons(SharedPreferences prefs) {
		try {
			File buttons = null;
			// TODO button themes
			String theme = prefs.getString(Config.pref_theme, null);
			if (theme != null) {
				buttons = new File(theme);
			}
			String home_directory = prefs.getString(Config.pref_home, Environment.getExternalStorageDirectory().getAbsolutePath());
			File file = new File(home_directory, "data/buttons.png");
			InputStream in = null;
			if (buttons != null && buttons.exists()) {
				in = new FileInputStream(buttons);
			} else if (!file.exists() || file.length() == 0) {
				in = Emulator.getAppContext().getAssets().open("buttons.png");
			}
			if (in != null) {
				OutputStream out = new FileOutputStream(file);

				// Transfer bytes from in to out
				byte[] buf = new byte[4096];
				int len;
				while ((len = in.read(buf)) != -1) {
					out.write(buf, 0, len);
				}
				in.close();
				out.flush();
				out.close();
			}
		} catch (FileNotFoundException fnf) {
			fnf.printStackTrace();
		} catch (IOException ioe) {
			ioe.printStackTrace();
		}
	}
}
