package com.reicast.emulator.periph;

import tv.ouya.console.api.OuyaController;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.preference.PreferenceManager;
import android.view.KeyEvent;

public class Gamepad {
	
	public final int key_CONT_B 			= 0x0002;
	public final int key_CONT_A 			= 0x0004;
	public final int key_CONT_START 		= 0x0008;
	public final int key_CONT_DPAD_UP 		= 0x0010;
	public final int key_CONT_DPAD_DOWN 	= 0x0020;
	public final int key_CONT_DPAD_LEFT 	= 0x0040;
	public final int key_CONT_DPAD_RIGHT 	= 0x0080;
	public final int key_CONT_Y 			= 0x0200;
	public final int key_CONT_X 			= 0x0400;

	private SharedPreferences mPrefs;
	private Context mContext;

	public Gamepad(Context mContext) {
		this.mContext = mContext;
		mPrefs = PreferenceManager.getDefaultSharedPreferences(mContext);
	}

	public int[] getConsoleController() {
		return new int[] {
				OuyaController.BUTTON_O, 			key_CONT_A,
				OuyaController.BUTTON_A, 			key_CONT_B,
				OuyaController.BUTTON_U, 			key_CONT_X,
				OuyaController.BUTTON_Y, 			key_CONT_Y,

				OuyaController.BUTTON_DPAD_UP, 		key_CONT_DPAD_UP,
				OuyaController.BUTTON_DPAD_DOWN, 	key_CONT_DPAD_DOWN,
				OuyaController.BUTTON_DPAD_LEFT, 	key_CONT_DPAD_LEFT,
				OuyaController.BUTTON_DPAD_RIGHT, 	key_CONT_DPAD_RIGHT,

				KeyEvent.KEYCODE_BUTTON_START, 		key_CONT_START
		};
	}

	public int[] getXPlayController() {
		return new int[] { 
				KeyEvent.KEYCODE_DPAD_CENTER, 		key_CONT_A,
				KeyEvent.KEYCODE_BACK, 				key_CONT_B,
				OuyaController.BUTTON_U, 			key_CONT_X,
				OuyaController.BUTTON_Y, 			key_CONT_Y,

				OuyaController.BUTTON_DPAD_UP, 		key_CONT_DPAD_UP,
				OuyaController.BUTTON_DPAD_DOWN, 	key_CONT_DPAD_DOWN,
				OuyaController.BUTTON_DPAD_LEFT, 	key_CONT_DPAD_LEFT,
				OuyaController.BUTTON_DPAD_RIGHT, 	key_CONT_DPAD_RIGHT,

				KeyEvent.KEYCODE_BUTTON_START, 		key_CONT_START
		};
	}

	public int[] getOUYAController() {
		return new int[] {
				OuyaController.BUTTON_O, 			key_CONT_A,
				OuyaController.BUTTON_A, 			key_CONT_B,
				OuyaController.BUTTON_U, 			key_CONT_X,
				OuyaController.BUTTON_Y, 			key_CONT_Y,

				OuyaController.BUTTON_DPAD_UP, 		key_CONT_DPAD_UP,
				OuyaController.BUTTON_DPAD_DOWN, 	key_CONT_DPAD_DOWN,
				OuyaController.BUTTON_DPAD_LEFT, 	key_CONT_DPAD_LEFT,
				OuyaController.BUTTON_DPAD_RIGHT, 	key_CONT_DPAD_RIGHT,

				OuyaController.BUTTON_MENU, 		key_CONT_START,
				KeyEvent.KEYCODE_BUTTON_START, 		key_CONT_START
		};
	}

	public int[] setModifiedKeys(String id, int playerNum) {
		return new int[] { 
				mPrefs.getInt("a_button" + id, OuyaController.BUTTON_O), 			key_CONT_A, 
				mPrefs.getInt("b_button" + id, OuyaController.BUTTON_A), 			key_CONT_B,
				mPrefs.getInt("x_button" + id, OuyaController.BUTTON_U), 			key_CONT_X,
				mPrefs.getInt("y_button" + id, OuyaController.BUTTON_Y), 			key_CONT_Y,

				mPrefs.getInt("dpad_up" + id, OuyaController.BUTTON_DPAD_UP), 		key_CONT_DPAD_UP,
				mPrefs.getInt("dpad_down" + id, OuyaController.BUTTON_DPAD_DOWN), 	key_CONT_DPAD_DOWN,
				mPrefs.getInt("dpad_left" + id, OuyaController.BUTTON_DPAD_LEFT), 	key_CONT_DPAD_LEFT,
				mPrefs.getInt("dpad_right" + id, OuyaController.BUTTON_DPAD_RIGHT), key_CONT_DPAD_RIGHT,

				mPrefs.getInt("start_button" + id, KeyEvent.KEYCODE_BUTTON_START), 	key_CONT_START,
		};
	}

	public boolean IsXperiaPlay() {
		return android.os.Build.MODEL.equals("R800a")
				|| android.os.Build.MODEL.equals("R800i")
				|| android.os.Build.MODEL.equals("R800x")
				|| android.os.Build.MODEL.equals("R800at")
				|| android.os.Build.MODEL.equals("SO-01D")
				|| android.os.Build.MODEL.equals("zeus");
	}

	public boolean IsOuyaOrTV() {
		PackageManager pMan = mContext.getPackageManager();
		if (pMan.hasSystemFeature(PackageManager.FEATURE_TELEVISION)) {
			return true;
		}
		return false;
	}
}
