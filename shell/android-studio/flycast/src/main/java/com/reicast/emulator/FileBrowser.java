package com.reicast.emulator;

import org.apache.commons.lang3.StringUtils;

import java.io.InputStream;
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
}
