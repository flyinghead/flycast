package com.reicast.emulator.emu;

import android.content.Context;

public class OnScreenMenu {

	public static int getPixelsFromDp(float dps, Context context) {
		return (int) (dps * context.getResources().getDisplayMetrics().density + 0.5f);
	}
}
