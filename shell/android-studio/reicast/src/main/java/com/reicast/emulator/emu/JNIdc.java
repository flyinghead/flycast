package com.reicast.emulator.emu;

import android.view.Surface;

import com.reicast.emulator.Emulator;
import com.reicast.emulator.periph.SipEmulator;

public final class JNIdc
{
	static { System.loadLibrary("dc"); }

	public static native void initEnvironment(Emulator emulator);
	public static native void config(String dirName);
	public static native String init(String fileName);
	public static native void query(EmuThread thread);
	public static native void run(EmuThread thread);
	public static native void pause();
	public static native void resume();
	public static native void stop();
	public static native void destroy();

	public static native int send(int cmd, int opt);
	public static native int data(int cmd, byte[] data);

	public static native void rendinitNative(Surface surface, int w, int h);
	public static native boolean rendframeNative();
	public static native void rendinitJava(int w, int h);
	public static native boolean rendframeJava();
	public static native void rendtermJava();

	public static native void vjoy(int id,float x, float y, float w, float h);

	public static native void getControllers(int[] controllers, int[][] peripherals);
	public static native void initControllers(int[] controllers, int[][] peripherals);

	public static native void setupMic(SipEmulator sip);
	public static native void diskSwap(String disk);
	public static native void vmuSwap();
	public static native void setupVmu(Object sip);
	public static native boolean getDynarec();
	public static native void setDynarec(boolean dynarec);
	public static native boolean getIdleskip();
	public static native void setIdleskip(boolean idleskip);
	public static native boolean getUnstable();
	public static native void setUnstable(boolean unstable);
	public static native boolean getSafemode();
	public static native void setSafemode(boolean safemode);
	public static native int getCable();
	public static native void setCable(int cable);
	public static native int getRegion();
	public static native void setRegion(int region);
	public static native int getBroadcast();
	public static native void setBroadcast(int broadcast);
	public static native int getLanguage();
	public static native void setLanguage(int language);
	public static native boolean getLimitfps();
	public static native void setLimitfps(boolean limiter);
	public static native boolean getNobatch();
	public static native void setNobatch(boolean nobatch);
	public static native boolean getNosound();
	public static native void setNosound(boolean noaudio);
	public static native boolean getMipmaps();
	public static native void setMipmaps(boolean mipmaps);
	public static native boolean getWidescreen();
	public static native void setWidescreen(boolean stretch);
	public static native int getFrameskip();
	public static native void setFrameskip(int frames);
	public static native int getPvrrender();
	public static native void setPvrrender(int render);
	public static native boolean getSyncedrender();
	public static native void setSyncedrender(boolean sync);
	public static native boolean getModvols();
	public static native void setModvols(boolean volumes);
	public static native boolean getClipping();
	public static native void setClipping(boolean clipping);
	public static native void bootdisk(String disk);
	public static native boolean getUsereios();
	public static native void setUsereios(boolean reios);
	public static native boolean getCustomtextures();
	public static native void setCustomtextures(boolean customtex);
    public static native boolean getShowfps();
    public static native void setShowfps(boolean showfps);
	public static native boolean getRenderToTextureBuffer();
	public static native void setRenderToTextureBuffer(boolean render);
	public static native int getRenderToTextureUpscale();
	public static native void setRenderToTextureUpscale(int upscale);
	public static native int getTextureUpscale();
	public static native void setTextureUpscale(int upscale);
	public static native int getMaxFilteredTextureSize();
	public static native void setMaxFilteredTextureSize(int maxSize);
	public static native int getMaxThreads();
	public static native void setMaxThreads(int maxThreads);

	public static native void screenDpi(int screenDpi);
	public static native void guiOpenSettings();
	public static native boolean guiIsOpen();

	public static void show_osd() {
		JNIdc.vjoy(13, 1,0,0,0);
	}

	public static void hide_osd() {
		JNIdc.vjoy(13, 0,0,0,0);
	}	// FIXME use HideOSD()
}
