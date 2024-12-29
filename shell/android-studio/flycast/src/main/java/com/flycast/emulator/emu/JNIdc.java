package com.flycast.emulator.emu;

import android.view.Surface;

import com.flycast.emulator.Emulator;
import com.flycast.emulator.periph.SipEmulator;

public final class JNIdc
{
	static { System.loadLibrary("flycast"); }

	public static native String initEnvironment(Emulator emulator, String filesDirectory, String homeDirectory, String locale);
	public static native void setExternalStorageDirectories(Object[] pathList);
	public static native void setGameUri(String fileName);
	public static native void pause();
	public static native void resume();
	public static native void stop();
	public static native void disableOmpAffinity();

	public static native void rendinitNative(Surface surface, int w, int h);

	public static native void setupMic(SipEmulator sip);

	public static native void screenCharacteristics(float screenDpi, float refreshRate);
	public static native void guiOpenSettings();
	public static native boolean guiIsOpen();
	public static native boolean guiIsContentBrowser();
	public static native void guiSetInsets(int left, int right, int top, int bottom);

}
