package com.reicast.emulator.periph;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

import tv.ouya.console.api.OuyaController;
import tv.ouya.console.api.OuyaFacade;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.SparseArray;
import android.view.KeyEvent;

public class Gamepad {

	public static final String pref_player1 = "device_descriptor_player_1";
	public static final String pref_player2 = "device_descriptor_player_2";
	public static final String pref_player3 = "device_descriptor_player_3";
	public static final String pref_player4 = "device_descriptor_player_4";
	public static final String pref_pad = "controller";

	public static final String pref_js_modified = "modified_key_layout";
	public static final String pref_js_compat = "controller_compat";
	public static final String pref_js_separate = "separate_joystick";
	public static final String pref_js_rbuttons = "right_buttons";

	public static final String pref_button_a = "a_button";
	public static final String pref_button_b = "b_button";
	public static final String pref_button_x = "x_button";
	public static final String pref_button_y = "y_button";

	public static final String pref_button_l = "l_button";
	public static final String pref_button_r = "r_button";

	public static final String pref_dpad_up = "dpad_up";
	public static final String pref_dpad_down = "dpad_down";
	public static final String pref_dpad_left = "dpad_left";
	public static final String pref_dpad_right = "dpad_right";

	public static final String pref_button_start = "start_button";
	public static final String pref_button_select = "select_button";

	public static final String controllers_sony = "Sony PLAYSTATION(R)3 Controller";
	public static final String controllers_xbox = "Microsoft X-Box 360 pad";
	public static final String controllers_shield = "NVIDIA Corporation NVIDIA Controller";
	public static final String controllers_play = "keypad-zeus";
	public static final String controllers_play_gp = "keypad-game-zeus";

	public String[] portId = { "_A", "_B", "_C", "_D" };
	public boolean[] compat = { false, false, false, false };
	public boolean[] custom = { false, false, false, false };
	public boolean[] joystick = { false, false, false, false };
	public int[] name = { -1, -1, -1, -1 };
	public float[] globalLS_X = new float[4], globalLS_Y = new float[4],
			previousLS_X = new float[4], previousLS_Y = new float[4];
	public boolean[] wasKeyStick = { false, false, false, false };
	public int map[][] = new int[4][];

	public SparseArray<String> deviceId_deviceDescriptor = new SparseArray<String>();
	public HashMap<String, Integer> deviceDescriptor_PlayerNum = new HashMap<String, Integer>();

	public boolean isActiveMoga[] = { false, false, false, false };
	public boolean isMogaPro[] = { false, false, false, false };

	public SparseArray<Integer> playerNumX = new SparseArray<Integer>();
	public List<Integer> keypadZeus = new ArrayList<Integer>();

	public boolean isXperiaPlay;
	public boolean isOuyaOrTV;
//	public boolean isNvidiaShield;

	public static final int Xperia_Touchpad = 1048584;
	
	public static final int key_CONT_B          = 0x0002;
	public static final int key_CONT_A          = 0x0004;
	public static final int key_CONT_START      = 0x0008;
	public static final int key_CONT_DPAD_UP    = 0x0010;
	public static final int key_CONT_DPAD_DOWN  = 0x0020;
	public static final int key_CONT_DPAD_LEFT  = 0x0040;
	public static final int key_CONT_DPAD_RIGHT = 0x0080;
	public static final int key_CONT_Y          = 0x0200;
	public static final int key_CONT_X          = 0x0400;

	public int[] getConsoleController() {
		return new int[] {
				OuyaController.BUTTON_O,          key_CONT_A,
				OuyaController.BUTTON_A,          key_CONT_B,
				OuyaController.BUTTON_U,          key_CONT_X,
				OuyaController.BUTTON_Y,          key_CONT_Y,

				OuyaController.BUTTON_DPAD_UP,    key_CONT_DPAD_UP,
				OuyaController.BUTTON_DPAD_DOWN,  key_CONT_DPAD_DOWN,
				OuyaController.BUTTON_DPAD_LEFT,  key_CONT_DPAD_LEFT,
				OuyaController.BUTTON_DPAD_RIGHT, key_CONT_DPAD_RIGHT,

				getStartButtonCode(),             key_CONT_START,
				getSelectButtonCode(),            getSelectButtonCode()
				// Redundant, but verifies it is mapped properly
		};
	}

	public int[] getXPlayController() {
		return new int[] { 
				KeyEvent.KEYCODE_DPAD_CENTER,    key_CONT_A,
				KeyEvent.KEYCODE_BACK,           key_CONT_B,
				OuyaController.BUTTON_U,         key_CONT_X,
				OuyaController.BUTTON_Y,         key_CONT_Y,

				OuyaController.BUTTON_DPAD_UP,    key_CONT_DPAD_UP,
				OuyaController.BUTTON_DPAD_DOWN,  key_CONT_DPAD_DOWN,
				OuyaController.BUTTON_DPAD_LEFT,  key_CONT_DPAD_LEFT,
				OuyaController.BUTTON_DPAD_RIGHT, key_CONT_DPAD_RIGHT,

				getStartButtonCode(),             key_CONT_START,
				getSelectButtonCode(),            getSelectButtonCode()
				// Redundant, but verifies it is mapped properly
		};
	}

	public int[] getOUYAController() {
		return new int[] {
				OuyaController.BUTTON_O,          key_CONT_A,
				OuyaController.BUTTON_A,          key_CONT_B,
				OuyaController.BUTTON_U,          key_CONT_X,
				OuyaController.BUTTON_Y,          key_CONT_Y,

				OuyaController.BUTTON_DPAD_UP,    key_CONT_DPAD_UP,
				OuyaController.BUTTON_DPAD_DOWN,  key_CONT_DPAD_DOWN,
				OuyaController.BUTTON_DPAD_LEFT,  key_CONT_DPAD_LEFT,
				OuyaController.BUTTON_DPAD_RIGHT, key_CONT_DPAD_RIGHT,

				getStartButtonCode(),             key_CONT_START,
				OuyaController.BUTTON_R3,         key_CONT_START
		};
	}

	public int[] getMogaController() {
		return new int[] {
				KeyEvent.KEYCODE_BUTTON_A,   key_CONT_A,
				KeyEvent.KEYCODE_BUTTON_B,   key_CONT_B,
				KeyEvent.KEYCODE_BUTTON_X,   key_CONT_X,
				KeyEvent.KEYCODE_BUTTON_Y,   key_CONT_Y,

				KeyEvent.KEYCODE_DPAD_UP,    key_CONT_DPAD_UP,
				KeyEvent.KEYCODE_DPAD_DOWN,  key_CONT_DPAD_DOWN,
				KeyEvent.KEYCODE_DPAD_LEFT,  key_CONT_DPAD_LEFT,
				KeyEvent.KEYCODE_DPAD_RIGHT, key_CONT_DPAD_RIGHT,

				getStartButtonCode(),        key_CONT_START,
				getSelectButtonCode(),       getSelectButtonCode()
			};
	}

	public int[] setModifiedKeys(String id, int playerNum, SharedPreferences mPrefs) {
		return new int[] { 
				mPrefs.getInt(pref_button_a + id, OuyaController.BUTTON_O),             key_CONT_A, 
				mPrefs.getInt(pref_button_b + id, OuyaController.BUTTON_A),             key_CONT_B,
				mPrefs.getInt(pref_button_x + id, OuyaController.BUTTON_U),             key_CONT_X,
				mPrefs.getInt(pref_button_y + id, OuyaController.BUTTON_Y),             key_CONT_Y,

				mPrefs.getInt(pref_dpad_up + id, OuyaController.BUTTON_DPAD_UP),        key_CONT_DPAD_UP,
				mPrefs.getInt(pref_dpad_down + id, OuyaController.BUTTON_DPAD_DOWN),    key_CONT_DPAD_DOWN,
				mPrefs.getInt(pref_dpad_left + id, OuyaController.BUTTON_DPAD_LEFT),    key_CONT_DPAD_LEFT,
				mPrefs.getInt(pref_dpad_right + id, OuyaController.BUTTON_DPAD_RIGHT),  key_CONT_DPAD_RIGHT,

				mPrefs.getInt(pref_button_start + id, getStartButtonCode()),            key_CONT_START,
				mPrefs.getInt(pref_button_select + id, getSelectButtonCode()),          getSelectButtonCode()
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

	public boolean IsOuyaOrTV(Context context) {
		PackageManager pMan = context.getPackageManager();
		if (pMan.hasSystemFeature(PackageManager.FEATURE_TELEVISION)) {
			return true;
		} else if (OuyaFacade.getInstance().isRunningOnOUYAHardware()) {
			return true;
		}
		return false;
	}
	
	public int getStartButtonCode() {
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {
			return 108;
		} else {
			return KeyEvent.KEYCODE_BUTTON_START;
		}
	}
	
	public int getSelectButtonCode() {
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {
			return 109;
		} else {
			return KeyEvent.KEYCODE_BUTTON_SELECT;
		}
	}

	public boolean IsNvidiaShield() {
		return android.os.Build.MODEL.equals("SHIELD")
				|| android.os.Build.DEVICE.equals("roth")
				|| android.os.Build.PRODUCT.equals("thor");
	}
}
