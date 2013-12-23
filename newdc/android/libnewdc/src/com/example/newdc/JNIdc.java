package com.example.newdc;

import android.media.AudioTrack;

public class JNIdc
{
  static { System.loadLibrary("dc"); }
	
  public static native void config(String dirName);
  public static native void init(String fileName);
  public static native void run(Object track);
  public static native void stop();
  
  public static native int send(int cmd, int opt);
  public static native int data(int cmd, byte[] data);
  
  public static native void rendinit(int w,int y);
  public static native void rendframe();
  
  public static native void kcode(int kcode,int lt, int rt, int jx, int jy);
  
  public static native void vjoy(int id,float x, float y, float w, float h);
  //public static native int play(short result[],int size);
}