package com.reicast.emulator;

import android.content.Context;
import android.content.SharedPreferences;
import android.preference.PreferenceManager;

public class VJoy {
	
	public static final int key_CONT_B          = 0x0002;
	  public static final int key_CONT_A          = 0x0004;
	  public static final int key_CONT_START      = 0x0008;
	  public static final int key_CONT_DPAD_UP    = 0x0010;
	  public static final int key_CONT_DPAD_DOWN  = 0x0020;
	  public static final int key_CONT_DPAD_LEFT  = 0x0040;
	  public static final int key_CONT_DPAD_RIGHT = 0x0080;
	  public static final int key_CONT_Y          = 0x0200;
	  public static final int key_CONT_X          = 0x0400;
	  
	  public static final int LAYER_TYPE_SOFTWARE = 1;
	  public static final int LAYER_TYPE_HARDWARE = 2;
	
	public static float[][] getVjoy_d(float[][] vjoy_d_custom) {
	       return new float[][]
	         { 
	           new float[] { 20+0*vjoy_d_custom[0][2]+vjoy_d_custom[0][0],     288+64*vjoy_d_custom[0][2]+vjoy_d_custom[0][1],   64*vjoy_d_custom[0][2],64*vjoy_d_custom[0][2], key_CONT_DPAD_LEFT},
	           new float[] { 20+64*vjoy_d_custom[0][2]+vjoy_d_custom[0][0],    288+0*vjoy_d_custom[0][2]+vjoy_d_custom[0][1],    64*vjoy_d_custom[0][2],64*vjoy_d_custom[0][2], key_CONT_DPAD_UP},
	           new float[] { 20+128*vjoy_d_custom[0][2]+vjoy_d_custom[0][0],   288+64*vjoy_d_custom[0][2]+vjoy_d_custom[0][1],   64*vjoy_d_custom[0][2],64*vjoy_d_custom[0][2], key_CONT_DPAD_RIGHT},
	           new float[] { 20+64*vjoy_d_custom[0][2]+vjoy_d_custom[0][0],    288+128*vjoy_d_custom[0][2]+vjoy_d_custom[0][1],  64*vjoy_d_custom[0][2],64*vjoy_d_custom[0][2], key_CONT_DPAD_DOWN},

	           new float[] { 448+0*vjoy_d_custom[1][2]+vjoy_d_custom[1][0],    288+64*vjoy_d_custom[1][2]+vjoy_d_custom[1][1],  64*vjoy_d_custom[1][2],64*vjoy_d_custom[1][2], key_CONT_X},
	           new float[] { 448+64*vjoy_d_custom[1][2]+vjoy_d_custom[1][0],   288+0*vjoy_d_custom[1][2]+vjoy_d_custom[1][1],   64*vjoy_d_custom[1][2],64*vjoy_d_custom[1][2], key_CONT_Y},
	           new float[] { 448+128*vjoy_d_custom[1][2]+vjoy_d_custom[1][0],  288+64*vjoy_d_custom[1][2]+vjoy_d_custom[1][1],  64*vjoy_d_custom[1][2],64*vjoy_d_custom[1][2], key_CONT_B},
	           new float[] { 448+64*vjoy_d_custom[1][2]+vjoy_d_custom[1][0],   288+128*vjoy_d_custom[1][2]+vjoy_d_custom[1][1], 64*vjoy_d_custom[1][2],64*vjoy_d_custom[1][2], key_CONT_A},

	           new float[] { 320-32+vjoy_d_custom[2][0],   288+128+vjoy_d_custom[2][1],  64*vjoy_d_custom[2][2],64*vjoy_d_custom[2][2], key_CONT_START},
	    
	           new float[] { 440+vjoy_d_custom[3][0], 200+vjoy_d_custom[3][1],  90*vjoy_d_custom[3][2],64*vjoy_d_custom[3][2], -1},
	           new float[] { 542+vjoy_d_custom[4][0], 200+vjoy_d_custom[4][1],  90*vjoy_d_custom[4][2],64*vjoy_d_custom[4][2], -2},
	    
	           new float[] { 16+vjoy_d_custom[5][0],   24+32+vjoy_d_custom[5][1],  128*vjoy_d_custom[5][2],128*vjoy_d_custom[5][2], -3},
	           new float[] { 96+vjoy_d_custom[5][0], 320+vjoy_d_custom[5][1],  32*vjoy_d_custom[5][2],32*vjoy_d_custom[5][2], -4},
	         };
	  }

	  public static void writeCustomVjoyValues(float[][] vjoy_d_custom, Context context) {
	       SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);

	       prefs.edit().putFloat("touch_x_shift_dpad", vjoy_d_custom[0][0]).commit();
	       prefs.edit().putFloat("touch_y_shift_dpad", vjoy_d_custom[0][1]).commit();
	       prefs.edit().putFloat("touch_scale_dpad", vjoy_d_custom[0][2]).commit();

	       prefs.edit().putFloat("touch_x_shift_buttons", vjoy_d_custom[1][0]).commit();
	       prefs.edit().putFloat("touch_y_shift_buttons", vjoy_d_custom[1][1]).commit();
	       prefs.edit().putFloat("touch_scale_buttons", vjoy_d_custom[1][2]).commit();

	       prefs.edit().putFloat("touch_x_shift_start", vjoy_d_custom[2][0]).commit();
	       prefs.edit().putFloat("touch_y_shift_start", vjoy_d_custom[2][1]).commit();
	       prefs.edit().putFloat("touch_scale_start", vjoy_d_custom[2][2]).commit();

	       prefs.edit().putFloat("touch_x_shift_left_trigger", vjoy_d_custom[3][0]).commit();
	       prefs.edit().putFloat("touch_y_shift_left_trigger", vjoy_d_custom[3][1]).commit();
	       prefs.edit().putFloat("touch_scale_left_trigger", vjoy_d_custom[3][2]).commit();

	       prefs.edit().putFloat("touch_x_shift_right_trigger", vjoy_d_custom[4][0]).commit();
	       prefs.edit().putFloat("touch_y_shift_right_trigger", vjoy_d_custom[4][1]).commit();
	       prefs.edit().putFloat("touch_scale_right_trigger", vjoy_d_custom[4][2]).commit();

	       prefs.edit().putFloat("touch_x_shift_analog", vjoy_d_custom[5][0]).commit();
	       prefs.edit().putFloat("touch_y_shift_analog", vjoy_d_custom[5][1]).commit();
	       prefs.edit().putFloat("touch_scale_analog", vjoy_d_custom[5][2]).commit();
	  }
}
