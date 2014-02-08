package com.android.util;

import java.util.Calendar;

public class DreamTime {

	private static long dreamRTC = ((20 * 365 + 5) * 86400);

	public static String getDreamtime() {
		Calendar cal = Calendar.getInstance();
		int utcOffset = cal.get(Calendar.ZONE_OFFSET)
				+ cal.get(Calendar.DST_OFFSET);
		return String.valueOf((System.currentTimeMillis() / 1000) + dreamRTC
				+ utcOffset / 1000);
	}
}
