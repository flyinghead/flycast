package com.reicast.emulator.emu;

import java.util.Arrays;
import java.util.HashMap;

import tv.ouya.console.api.OuyaController;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.Toast;

import com.reicast.emulator.R;
import com.reicast.emulator.config.ConfigureFragment;
import com.reicast.emulator.emu.OnScreenMenu.FpsPopup;
import com.reicast.emulator.emu.OnScreenMenu.MainPopup;
import com.reicast.emulator.emu.OnScreenMenu.VmuPopup;
import com.reicast.emulator.periph.Gamepad;
import com.reicast.emulator.periph.MOGAInput;
import com.reicast.emulator.periph.SipEmulator;

@TargetApi(Build.VERSION_CODES.HONEYCOMB_MR1)
public class GL2JNIActivity extends Activity {
	public GL2JNIView mView;
	OnScreenMenu menu;
	MainPopup popUp;
	VmuPopup vmuPop;
	FpsPopup fpsPop;
	MOGAInput moga = new MOGAInput();
	private SharedPreferences prefs;
	public String[] portId = { "_A", "_B", "_C", "_D" };
	public boolean[] compat = { false, false, false, false };
	public boolean[] custom = { false, false, false, false };
	public boolean[] jsDpad = { false, false, false, false };
	public int[] name = { -1, -1, -1, -1 };
	
	private Gamepad gamepad;

	public boolean isXperiaPlay;
	public boolean isOuyaOrTV;

	float[] globalLS_X = new float[4], globalLS_Y = new float[4],
			previousLS_X = new float[4], previousLS_Y = new float[4];

	public static HashMap<Integer, String> deviceId_deviceDescriptor = new HashMap<Integer, String>();
	public static HashMap<String, Integer> deviceDescriptor_PlayerNum = new HashMap<String, Integer>();

	int map[][];

	static byte[] syms;

	@Override
	protected void onCreate(Bundle icicle) {
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		moga.onCreate(this);
		
		prefs = PreferenceManager.getDefaultSharedPreferences(this);

		menu = new OnScreenMenu(GL2JNIActivity.this, prefs);
		gamepad = new Gamepad(GL2JNIActivity.this);

		isXperiaPlay = gamepad.IsXperiaPlay();
		isOuyaOrTV = gamepad.IsOuyaOrTV();

		/*
		 * try { //int rID =
		 * getResources().getIdentifier("fortyonepost.com.lfas:raw/syms.map",
		 * null, null); //get the file as a stream InputStream is =
		 * getResources().openRawResource(R.raw.syms);
		 * 
		 * syms = new byte[(int) is.available()]; is.read(syms); is.close(); }
		 * catch (IOException e) { e.getMessage(); e.printStackTrace(); }
		 */
		

		String fileName = null;

		// Call parent onCreate()
		super.onCreate(icicle);
		OuyaController.init(this);

		map = new int[4][];

		// Populate device descriptor-to-player-map from preferences
		deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_1", null), 0);
		deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_2", null), 1);
		deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_3", null), 2);
		deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_4", null), 3);
		deviceDescriptor_PlayerNum.remove(null);

		boolean controllerTwoConnected = false;
		boolean controllerThreeConnected = false;
		boolean controllerFourConnected = false;

		for (HashMap.Entry<String, Integer> e : deviceDescriptor_PlayerNum
				.entrySet()) {
			String descriptor = e.getKey();
			Integer playerNum = e.getValue();

			switch (playerNum) {
			case 1:
				if (descriptor != null)
					controllerTwoConnected = true;
				break;
			case 2:
				if (descriptor != null)
					controllerThreeConnected = true;
				break;
			case 3:
				if (descriptor != null)
					controllerFourConnected = true;
				break;
			}
		}

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {

			JNIdc.initControllers(new boolean[] { controllerTwoConnected,
					controllerThreeConnected, controllerFourConnected });
			int joys[] = InputDevice.getDeviceIds();
			for (int joy: joys) {
				String descriptor = null;
				if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
					descriptor = InputDevice.getDevice(joy).getDescriptor();
				} else {
					descriptor = InputDevice.getDevice(joy).getName();
				}
				Log.d("reidc", "InputDevice ID: " + joy);
				Log.d("reidc",
						"InputDevice Name: "
								+ InputDevice.getDevice(joy).getName());
				Log.d("reidc", "InputDevice Descriptor: " + descriptor);
				deviceId_deviceDescriptor.put(joy, descriptor);
			}

			for (int joy :joys) {
				Integer playerNum = deviceDescriptor_PlayerNum
						.get(deviceId_deviceDescriptor.get(joy));

				if (playerNum != null) {
					String id = portId[playerNum];
					custom[playerNum] = prefs.getBoolean("modified_key_layout" + id, false);
					compat[playerNum] = prefs.getBoolean("controller_compat" + id, false);
					jsDpad[playerNum] = prefs.getBoolean("dpad_js_layout" + id, false);
					if (!compat[playerNum]) {
						if (custom[playerNum]) {
							map[playerNum] = gamepad.setModifiedKeys(id, playerNum);

							if (jsDpad[playerNum]) {
								initJoyStickLayout(playerNum);
							}
						} else if (InputDevice.getDevice(joy).getName()
								.equals("Sony PLAYSTATION(R)3 Controller")) {
							map[playerNum] = gamepad.getConsoleController();
						} else if (InputDevice.getDevice(joy).getName()
								.equals("Microsoft X-Box 360 pad")) {
							map[playerNum] = gamepad.getConsoleController();

							jsDpad[playerNum] = true;

							initJoyStickLayout(playerNum);
						} else if (InputDevice.getDevice(joy).getName()
								.contains("NVIDIA Corporation NVIDIA Controller")) {
							map[playerNum] = gamepad.getConsoleController();
							jsDpad[playerNum] = true;

							initJoyStickLayout(playerNum);
						} else if (InputDevice.getDevice(joy).getName()
								.contains("keypad-zeus")) {
							map[playerNum] = gamepad.getXPlayController();

							initJoyStickLayout(playerNum);
						} else if (!moga.isActive[playerNum]) { // Ouya controller
							map[playerNum] = gamepad.getOUYAController();
						}
					} else {
						getCompatibilityMap(playerNum, id);
					}
				}
			}
			if (joys.length == 0) {
				runCompatibilityMode();
			}
		} else {
			runCompatibilityMode();
		}

		// When viewing a resource, pass its URI to the native code for opening
		Intent intent = getIntent();
		if (intent.getAction().equals(Intent.ACTION_VIEW))
			fileName = Uri.decode(intent.getData().toString());

		// Create the actual GLES view
		mView = new GL2JNIView(getApplication(), fileName, false, prefs.getInt("depth_render", 24), 0, false);
		setContentView(mView);

		//setup mic
		boolean micPluggedIn = prefs.getBoolean("mic_plugged_in", false);
		if(micPluggedIn){
			SipEmulator sip = new SipEmulator();
			sip.startRecording();
			JNIdc.setupMic(sip);
		}
		
		popUp = menu.new MainPopup(this);
		vmuPop = menu.new VmuPopup(this);
		if(prefs.getBoolean("vmu_floating", false)){
			//kind of a hack - if the user last had the vmu on screen
			//inverse it and then "toggle"
			prefs.edit().putBoolean("vmu_floating", false).commit();
			//can only display a popup after onCreate
			mView.post(new Runnable() {
				public void run() {
					toggleVmu();
				}
			});
		}
		JNIdc.setupVmu(menu.getVmu());
		if (prefs.getBoolean("show_fps", false)) {
			fpsPop = menu.new FpsPopup(this);
			mView.setFpsDisplay(fpsPop);
			mView.post(new Runnable() {
				public void run() {
					displayFPS();
				}
			});
		}
	}

	private void initJoyStickLayout(int playerNum) {
		globalLS_X[playerNum] = previousLS_X[playerNum] = 0.0f;
		globalLS_Y[playerNum] = previousLS_Y[playerNum] = 0.0f;
	}
	
	private void runCompatibilityMode() {
		for (int n = 0; n < 4; n++) {
			if (compat[n]) {
				getCompatibilityMap(n, portId[n]);
			}
		}
	}

	private void getCompatibilityMap(int playerNum, String id) {
		name[playerNum] = prefs.getInt("controller" + id, -1);
		if (name[playerNum] != -1) {
			map[playerNum] = gamepad.setModifiedKeys(id, playerNum);
		}
		if (jsDpad[playerNum]) {
			initJoyStickLayout(playerNum);
		}
	}

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		// Log.w("INPUT", event.toString() + " " + event.getSource());
		// Get all the axis for the KeyEvent

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {

			Integer playerNum = Arrays.asList(name).indexOf(event.getDeviceId());
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD && playerNum == -1) {
				playerNum = deviceDescriptor_PlayerNum
					.get(deviceId_deviceDescriptor.get(event.getDeviceId()));
			} else {
				playerNum = -1;
			}

			if (playerNum == null || playerNum == -1)
				return false;

			if (!moga.isActive[playerNum] || compat[playerNum]) {
				// TODO: Moga should handle this locally

				// Joystick
				if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {

					// do other things with joystick
					float LS_X = event.getAxisValue(OuyaController.AXIS_LS_X);
					float LS_Y = event.getAxisValue(OuyaController.AXIS_LS_Y);
					float RS_X = event.getAxisValue(OuyaController.AXIS_RS_X);
					float RS_Y = event.getAxisValue(OuyaController.AXIS_RS_Y);
					float L2 = event.getAxisValue(OuyaController.AXIS_L2);
					float R2 = event.getAxisValue(OuyaController.AXIS_R2);

					if (jsDpad[playerNum]) {
						previousLS_X[playerNum] = globalLS_X[playerNum];
						previousLS_Y[playerNum] = globalLS_Y[playerNum];
						globalLS_X[playerNum] = LS_X;
						globalLS_Y[playerNum] = LS_Y;
					}

					GL2JNIView.lt[playerNum] = (int) (L2 * 255);
					GL2JNIView.rt[playerNum] = (int) (R2 * 255);

					GL2JNIView.jx[playerNum] = (int) (LS_X * 126);
					GL2JNIView.jy[playerNum] = (int) (LS_Y * 126);
				}

			}
			mView.pushInput();
			if ((jsDpad[playerNum])
					&& ((globalLS_X[playerNum] == previousLS_X[playerNum] && globalLS_Y[playerNum] == previousLS_Y[playerNum]) || (previousLS_X[playerNum] == 0.0f && previousLS_Y[playerNum] == 0.0f)))
				// Only handle Left Stick on an Xbox 360 controller if there was
				// some actual motion on the stick,
				// so otherwise the event can be handled as a DPAD event
				return false;
			else
				return true;

		} else {
			return false;
		}

	}
	
	public boolean simulatedTouchEvent(int playerNum, float L2, float R2) {
		GL2JNIView.lt[playerNum] = (int) (L2 * 255);
		GL2JNIView.rt[playerNum] = (int) (R2 * 255);
		mView.pushInput();
		return true;
	}

	// TODO: Controller mapping in options. Trunk has Ouya layout. This is a DS3
	// layout.
	/*
	 * map[]= new int[] { OuyaController.BUTTON_Y, key_CONT_B,
	 * OuyaController.BUTTON_U, key_CONT_A, OuyaController.BUTTON_O, key_CONT_X,
	 * OuyaController.BUTTON_A, key_CONT_Y,
	 * 
	 * OuyaController.BUTTON_DPAD_UP, key_CONT_DPAD_UP,
	 * OuyaController.BUTTON_DPAD_DOWN, key_CONT_DPAD_DOWN,
	 * OuyaController.BUTTON_DPAD_LEFT, key_CONT_DPAD_LEFT,
	 * OuyaController.BUTTON_DPAD_RIGHT, key_CONT_DPAD_RIGHT,
	 * 
	 * OuyaController.BUTTON_MENU, key_CONT_START, OuyaController.BUTTON_L1,
	 * key_CONT_START
	 * 
	 * };
	 */

	/*
	 * int map[] = new int[] { OuyaController.BUTTON_Y, key_CONT_B,
	 * OuyaController.BUTTON_U, key_CONT_A, OuyaController.BUTTON_O, key_CONT_X,
	 * OuyaController.BUTTON_A, key_CONT_Y,
	 * 
	 * OuyaController.BUTTON_DPAD_UP, key_CONT_DPAD_UP,
	 * OuyaController.BUTTON_DPAD_DOWN, key_CONT_DPAD_DOWN,
	 * OuyaController.BUTTON_DPAD_LEFT, key_CONT_DPAD_LEFT,
	 * OuyaController.BUTTON_DPAD_RIGHT, key_CONT_DPAD_RIGHT,
	 * 
	 * OuyaController.BUTTON_MENU, key_CONT_START, OuyaController.BUTTON_L1,
	 * key_CONT_START
	 * 
	 * };
	 */

	boolean handle_key(Integer playerNum, int kc, boolean down) {
		if (playerNum == null || playerNum == -1)
			return false;
		if (!moga.isActive[playerNum]) {

			boolean rav = false;
			for (int i = 0; i < map[playerNum].length; i += 2) {
				if (map[playerNum][i + 0] == kc) {
					if (down)
						GL2JNIView.kcode_raw[playerNum] &= ~map[playerNum][i + 1];
					else
						GL2JNIView.kcode_raw[playerNum] |= map[playerNum][i + 1];
					rav = true;
					break;
				}
			}
			mView.pushInput();
			return rav;

		} else {
			return true;
		}
	}
	
	public void displayPopUp(PopupWindow popUp) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			popUp.showAtLocation(mView, Gravity.BOTTOM, 0, 60);
		} else {
			popUp.showAtLocation(mView, Gravity.BOTTOM, 0, 0);
		}
		popUp.update(LayoutParams.WRAP_CONTENT,
				LayoutParams.WRAP_CONTENT);
	}
	
	public void displayDebug(PopupWindow popUpDebug) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			popUpDebug.showAtLocation(mView, Gravity.BOTTOM, 0, 60);
		} else {
			popUpDebug.showAtLocation(mView, Gravity.BOTTOM, 0, 0);
		}
		popUpDebug.update(LayoutParams.WRAP_CONTENT,
				LayoutParams.WRAP_CONTENT);
	}

	public void displayFPS() {
		fpsPop.showAtLocation(mView, Gravity.TOP | Gravity.LEFT, 20, 20);
		fpsPop.update(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
	}

	public void toggleVmu() {
		boolean showFloating = !prefs.getBoolean("vmu_floating", false);
		if(showFloating){
			if(popUp.isShowing()){
				popUp.dismiss();
			}
			//remove from popup menu
			LinearLayout parent = (LinearLayout) popUp.getContentView();
			parent.removeView(menu.getVmu());
			//add to floating window
			vmuPop.showVmu();
			vmuPop.showAtLocation(mView, Gravity.TOP | Gravity.RIGHT, 20, 20);
			vmuPop.update(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
		}else{
			vmuPop.dismiss();
			//remove from floating window
			LinearLayout parent = (LinearLayout) vmuPop.getContentView();
			parent.removeView(menu.getVmu());
			//add back to popup menu
			popUp.showVmu();
		}
		prefs.edit().putBoolean("vmu_floating", showFloating).commit();
	}
	
	public void displayConfig(PopupWindow popUpConfig) {
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
			popUpConfig.showAtLocation(mView, Gravity.BOTTOM, 0, 60);
		} else {
			popUpConfig.showAtLocation(mView, Gravity.BOTTOM, 0, 0);
		}
		popUpConfig.update(LayoutParams.WRAP_CONTENT,
				LayoutParams.WRAP_CONTENT);
	}

	public boolean onKeyUp(int keyCode, KeyEvent event) {
		Integer playerNum = Arrays.asList(name).indexOf(event.getDeviceId());
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD && playerNum == -1) {
			playerNum = deviceDescriptor_PlayerNum
				.get(deviceId_deviceDescriptor.get(event.getDeviceId()));
		} else {
			playerNum = -1;
		}

		if (playerNum != null && playerNum != -1) {
			if (compat[playerNum] || custom[playerNum]) {
				String id = portId[playerNum];
				if (keyCode == prefs.getInt("l_button" + id,
						KeyEvent.KEYCODE_BUTTON_L1)
						|| keyCode == prefs.getInt("r_button" + id,
								KeyEvent.KEYCODE_BUTTON_R1)) {
					return simulatedTouchEvent(playerNum, 0.0f, 0.0f);
				}
			}
		}

		return handle_key(playerNum, keyCode, false)
				|| super.onKeyUp(keyCode, event);
	}

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		Integer playerNum = Arrays.asList(name).indexOf(event.getDeviceId());
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD && playerNum == -1) {
			playerNum = deviceDescriptor_PlayerNum
				.get(deviceId_deviceDescriptor.get(event.getDeviceId()));
		} else {
			playerNum = -1;
		}
		
		if (playerNum != null && playerNum != -1) {
			if (compat[playerNum] || custom[playerNum]) {
				String id = portId[playerNum];
				if (keyCode == prefs.getInt("l_button" + id, KeyEvent.KEYCODE_BUTTON_L1)) {
					return simulatedTouchEvent(playerNum, 1.0f, 0.0f);
				}
				if (keyCode == prefs.getInt("r_button" + id, KeyEvent.KEYCODE_BUTTON_R1)) {
					return simulatedTouchEvent(playerNum, 0.0f, 1.0f);
				}
			}
		}

		if (handle_key(playerNum, keyCode, true)) {
			if (playerNum == 0)
				JNIdc.hide_osd();
			return true;
		}

		if (isXperiaPlay) {
			if (keyCode == KeyEvent.KEYCODE_MENU) {
				return showMenu();
			}
			if (keyCode == KeyEvent.KEYCODE_BACK) {
				return false;
			}
		} else {
			if (keyCode == KeyEvent.KEYCODE_BACK || keyCode == OuyaController.BUTTON_MENU) {
				return showMenu();
			}
		}
		return super.onKeyDown(keyCode, event);
	}
	
	private boolean showMenu() {
		if (popUp != null) {
			if (!menu.dismissPopUps()) {
				if (!popUp.isShowing()) {
					displayPopUp(popUp);
				} else {
					popUp.dismiss();
				}
			}
		}
		return true;
	}

	@Override
	protected void onPause() {
		super.onPause();
		mView.onPause();
		moga.onPause();
	}

	@Override
	protected void onDestroy() {
		super.onDestroy();
		moga.onDestroy();
	}

	@Override
	protected void onStop() {
		// TODO Auto-generated method stub
		JNIdc.stop();
		mView.onStop();
		super.onStop();
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
	}

	@Override
	protected void onResume() {
		super.onResume();
		mView.onResume();
		moga.onResume();
	}
}
