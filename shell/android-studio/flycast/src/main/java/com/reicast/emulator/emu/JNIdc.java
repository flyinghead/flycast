package com.reicast.emulator.emu;

import android.view.Surface;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.periph.SipEmulator;

public final class JNIdc
{
	static { System.loadLibrary("flycast"); }

	public static native String initEnvironment(Emulator emulator, String filesDirectory, String homeDirectory, String locale);
	public static native void setExternalStorageDirectories(Object[] pathList);
	public static native void setGameUri(String fileName);
	public static native void pause();
	public static native void resume();
	public static native void stop();

	public static native void rendinitNative(Surface surface, int w, int h);

	public static native void vjoy(int id, float x, float y, float w, float h);

	public static native void getControllers(int[] controllers, int[][] peripherals);

	public static native void setupMic(SipEmulator sip);
	public static native int getVirtualGamepadVibration();

	public static native void screenDpi(int screenDpi);
	public static native void guiOpenSettings();
	public static native boolean guiIsOpen();
	public static native boolean guiIsContentBrowser();
	public static native void guiSetInsets(int left, int right, int top, int bottom);

	public static void show_osd() {
		JNIdc.vjoy(14, 1, 0, 0, 0);
	}
	public static native void hideOsd();

	public static native void setButtons(byte[] data);
}
