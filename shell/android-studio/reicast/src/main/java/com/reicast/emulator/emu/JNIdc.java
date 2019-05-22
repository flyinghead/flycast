package com.reicast.emulator.emu;

import android.util.Log;
import android.view.Surface;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.periph.SipEmulator;

public final class JNIdc
{
	static { System.loadLibrary("dc"); }

	public static native String initEnvironment(Emulator emulator, String homeDirectory);
	public static native void setExternalStorageDirectories(Object[] pathList);
	public static native void setGameUri(String fileName);
	public static native void pause();
	public static native void resume();
	public static native void stop();
	public static native void destroy();

	public static native int send(int cmd, int opt);
	public static native int data(int cmd, byte[] data);

	public static native void rendinitNative(Surface surface);
	public static native void rendinitJava(int w, int h);
	public static native boolean rendframeJava();
	public static native void rendtermJava();

	public static native void vjoy(int id,float x, float y, float w, float h);

	public static native void getControllers(int[] controllers, int[][] peripherals);

	public static native void setupMic(SipEmulator sip);
	public static native boolean getNosound();
	public static native boolean getWidescreen();
	public static native int getVirtualGamepadVibration();

	public static native void screenDpi(int screenDpi);
	public static native void guiOpenSettings();
	public static native boolean guiIsOpen();
	public static native boolean guiIsContentBrowser();

	public static void show_osd() {
		JNIdc.vjoy(13, 1,0,0,0);
	}
	public static native void hideOsd();
}
