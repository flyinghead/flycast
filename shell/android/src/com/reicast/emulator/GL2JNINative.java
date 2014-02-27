package com.reicast.emulator;

import java.util.Arrays;
import java.util.HashMap;

import tv.ouya.console.api.OuyaController;
import android.annotation.TargetApi;
import android.app.NativeActivity;
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
import android.view.ViewConfiguration;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.Toast;

import com.reicast.emulator.config.Config;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;
import com.reicast.emulator.emu.OnScreenMenu;
import com.reicast.emulator.emu.OnScreenMenu.FpsPopup;
import com.reicast.emulator.emu.OnScreenMenu.MainPopup;
import com.reicast.emulator.emu.OnScreenMenu.VmuPopup;
import com.reicast.emulator.periph.Gamepad;
import com.reicast.emulator.periph.MOGAInput;
import com.reicast.emulator.periph.SipEmulator;

@TargetApi(Build.VERSION_CODES.HONEYCOMB_MR1)
public class GL2JNINative extends NativeActivity {
	public GL2JNIView mView;
	OnScreenMenu menu;
	public MainPopup popUp;
	VmuPopup vmuPop;
	FpsPopup fpsPop;
	MOGAInput moga = new MOGAInput();
	private SharedPreferences prefs;

	private Config config;
	private Gamepad pad = new Gamepad();

	public static byte[] syms;

	static {
		System.loadLibrary("sexplay");
	}

	public native void registerNative();
	public native void registerXperia(int xperia);

	@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
	@Override
	protected void onCreate(Bundle icicle) {
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		getWindow().takeSurface(null);
		registerNative();

		prefs = PreferenceManager.getDefaultSharedPreferences(this);
		config = new Config(GL2JNINative.this);
		config.getConfigurationPrefs();
		menu = new OnScreenMenu(GL2JNINative.this, prefs);

		pad.isXperiaPlay = pad.IsXperiaPlay();
		pad.isOuyaOrTV = pad.IsOuyaOrTV(GL2JNINative.this);
//		isNvidiaShield = Gamepad.IsNvidiaShield();

		String fileName = null;

		// Call parent onCreate()
		super.onCreate(icicle);
		OuyaController.init(this);
		moga.onCreate(this, pad);

		// Populate device descriptor-to-player-map from preferences
		pad.deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_1", null), 0);
		pad.deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_2", null), 1);
		pad.deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_3", null), 2);
		pad.deviceDescriptor_PlayerNum.put(
				prefs.getString("device_descriptor_player_4", null), 3);
		pad.deviceDescriptor_PlayerNum.remove(null);

		boolean controllerTwoConnected = false;
		boolean controllerThreeConnected = false;
		boolean controllerFourConnected = false;

		for (HashMap.Entry<String, Integer> e : pad.deviceDescriptor_PlayerNum
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
			for (int joy : joys) {
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
				if (pad.isXperiaPlay) {
					if (InputDevice.getDevice(joy).getName()
							.contains("keypad-game-zeus")) {
						pad.keypadZeus.add(joy);
					}
					if (InputDevice.getDevice(joy).getName()
							.contains("synaptics_touchpad")) {
						registerXperia(joy);
						pad.keypadZeus.add(joy);
					}
				}
				Log.d("reidc", "InputDevice Descriptor: " + descriptor);
				pad.deviceId_deviceDescriptor.put(joy, descriptor);
			}

			for (int joy : joys) {
				Integer playerNum = pad.deviceDescriptor_PlayerNum
						.get(pad.deviceId_deviceDescriptor.get(joy));

				if (playerNum != null) {
					String id = pad.portId[playerNum];
					pad.custom[playerNum] = prefs.getBoolean("modified_key_layout" + id, false);
					pad.compat[playerNum] = prefs.getBoolean("controller_compat" + id, false);
					if (InputDevice.getDevice(joy).getName()
							.contains("keypad-zeus")) {
						pad.playerNumX.put(joy, playerNum);
						for (int keys : pad.keypadZeus) {
							pad.playerNumX.put(keys, playerNum);
						}
						if (pad.custom[playerNum]) {
							setCustomMapping(id, playerNum);
						} else {
							pad.map[playerNum] = pad.getXPlayController();
						}
					} else {
						if (!pad.compat[playerNum]) {
							if (pad.custom[playerNum]) {
								setCustomMapping(id, playerNum);
							} else if (InputDevice.getDevice(joy).getName()
									.equals("Sony PLAYSTATION(R)3 Controller")) {
								pad.map[playerNum] = pad.getConsoleController();
							} else if (InputDevice.getDevice(joy).getName()
									.equals("Microsoft X-Box 360 pad")) {
								pad.map[playerNum] = pad.getConsoleController();
							} else if (InputDevice.getDevice(joy).getName()
									.contains("NVIDIA Corporation NVIDIA Controller")) {
								pad.map[playerNum] = pad.getConsoleController();
							} else if (!pad.isActiveMoga[playerNum]) { // Ouya controller
								pad.map[playerNum] = pad.getOUYAController();
							}
						} else{
							getCompatibilityMap(playerNum, id);
						}
						initJoyStickLayout(playerNum);
						pad.playerNumX.put(joy, playerNum);
					}
				} else {
					runCompatibilityMode(joy);
				}
			}
		}

		// When viewing a resource, pass its URI to the native code for opening
		Intent intent = getIntent();
		if (intent.getAction().equals(Intent.ACTION_VIEW))
			fileName = Uri.decode(intent.getData().toString());

		// Create the actual GLES view
		mView = new GL2JNIView(getApplication(), config, fileName, false,
				prefs.getInt("depth_render", 24), 0, false);
		setContentView(mView);
		
		String menu_spec;
		if (pad.isXperiaPlay || pad.isOuyaOrTV) {
			menu_spec = getApplicationContext().getString(R.string.menu_button);
		} else {
			menu_spec = getApplicationContext().getString(R.string.back_button);
		}
		Toast.makeText(
				getApplicationContext(),
				getApplicationContext()
						.getString(R.string.bios_menu, menu_spec),
				Toast.LENGTH_SHORT).show();

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

	private void setCustomMapping(String id, int playerNum) {
		pad.map[playerNum] = pad.setModifiedKeys(id, playerNum, prefs);
	}

	private void initJoyStickLayout(int playerNum) {
		pad.globalLS_X[playerNum] = pad.previousLS_X[playerNum] = 0.0f;
		pad.globalLS_Y[playerNum] = pad.previousLS_Y[playerNum] = 0.0f;
	}
	
	private void runCompatibilityMode(int joy) {
		for (int n = 0; n < 4; n++) {
			if (pad.compat[n]) {
				getCompatibilityMap(n, pad.portId[n]);
				pad.playerNumX.put(joy, n);
				initJoyStickLayout(n);
			} else {
				pad.playerNumX.put(joy, -1);
			}
		}
	}

	private void getCompatibilityMap(int playerNum, String id) {
		pad.name[playerNum] = prefs.getInt("controller" + id, -1);
		if (pad.name[playerNum] != -1) {
			pad.map[playerNum] = pad.setModifiedKeys(id, playerNum, prefs);
		}
	}
	
	public boolean simulatedTouchEvent(int playerNum, float L2, float R2) {
		GL2JNIView.lt[playerNum] = (int) (L2 * 255);
		GL2JNIView.rt[playerNum] = (int) (R2 * 255);
		mView.pushInput();
		return true;
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
			vmuPop.showAtLocation(mView, Gravity.TOP | Gravity.RIGHT, 4, 4);
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

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		// Log.w("INPUT", event.toString() + " " + event.getSource());
		// Get all the axis for the KeyEvent

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD
				&& event.getSource() != Gamepad.Xperia_Touchpad) {

			Integer playerNum = Arrays.asList(pad.name).indexOf(event.getDeviceId());
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD && playerNum == -1) {
				playerNum = pad.deviceDescriptor_PlayerNum
						.get(pad.deviceId_deviceDescriptor.get(event.getDeviceId()));
			} else {
				playerNum = -1;
			}
			if (playerNum == null || playerNum == -1) {
				return false;
			}
			if (!pad.compat[playerNum] && !pad.isActiveMoga[playerNum]) {
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

					pad.previousLS_X[playerNum] = pad.globalLS_X[playerNum];
					pad.previousLS_Y[playerNum] = pad.globalLS_Y[playerNum];
					pad.globalLS_X[playerNum] = LS_X;
					pad.globalLS_Y[playerNum] = LS_Y;

					GL2JNIView.lt[playerNum] = (int) (L2 * 255);
					GL2JNIView.rt[playerNum] = (int) (R2 * 255);

					GL2JNIView.jx[playerNum] = (int) (LS_X * 126);
					GL2JNIView.jy[playerNum] = (int) (LS_Y * 126);

					if (prefs.getBoolean("right_buttons", true)) {
						if (RS_Y > 0.5) {
							handle_key(playerNum, pad.map[playerNum][0]/* A */, true);
							pad.wasKeyStick[playerNum] = true;
						} else if (RS_Y < 0.5) {
							handle_key(playerNum, pad.map[playerNum][1]/* B */, true);
							pad.wasKeyStick[playerNum] = true;
						} else if (pad.wasKeyStick[playerNum]){
							handle_key(playerNum, pad.map[playerNum][0], false);
							handle_key(playerNum, pad.map[playerNum][1], false);
							pad.wasKeyStick[playerNum] = false;
						}
					} else {
						if (RS_Y > 0.5) {
							GL2JNIView.rt[playerNum] = (int) (RS_Y * 255);
						} else if (RS_Y < 0.5) {
							GL2JNIView.lt[playerNum] = (int) (-(RS_Y) * 255);
						}
					}
				}
				mView.pushInput();
			}
			if ((pad.globalLS_X[playerNum] == pad.previousLS_X[playerNum] && pad.globalLS_Y[playerNum] == pad.previousLS_Y[playerNum])
					|| (pad.previousLS_X[playerNum] == 0.0f && pad.previousLS_Y[playerNum] == 0.0f))
				// Only handle Left Stick on an Xbox 360 controller if there was
				// some actual motion on the stick,
				// so otherwise the event can be handled as a DPAD event
				return false;
			else
				return true;
		}
		return false;

	}

	boolean handle_key(Integer playerNum, int kc, boolean down) {
		if (playerNum == null || playerNum == -1)
			return false;
		if (kc == pad.getSelectButtonCode()) {
			return false;
		}
		if (pad.isActiveMoga[playerNum]) {
			return false;
		}

		boolean rav = false;
		for (int i = 0; i < pad.map[playerNum].length; i += 2) {
			if (pad.map[playerNum][i + 0] == kc) {
				if (down)
					GL2JNIView.kcode_raw[playerNum] &= ~pad.map[playerNum][i + 1];
				else
					GL2JNIView.kcode_raw[playerNum] |= pad.map[playerNum][i + 1];
				rav = true;
				break;
			}
		}
		mView.pushInput();
		return rav;
	}

	public boolean onKeyUp(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
			return super.onKeyUp(keyCode, event);
		}
		return true;
	}

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (keyCode == pad.getSelectButtonCode()) {
			return showMenu();
		} 
		if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.GINGERBREAD_MR1
				|| (Build.VERSION.SDK_INT >= Build.VERSION_CODES.ICE_CREAM_SANDWICH 
				&& ViewConfiguration.get(this).hasPermanentMenuKey())) {
			if (keyCode == KeyEvent.KEYCODE_MENU) {
				return showMenu();
			}
		}
		if (keyCode == KeyEvent.KEYCODE_BACK) {
			if (pad.isXperiaPlay) {
				return true;
			} else {
				return showMenu();
			}
		}
		if (keyCode == KeyEvent.KEYCODE_VOLUME_UP || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN) {
			return super.onKeyDown(keyCode, event);
		}
		return true;
	}

	public boolean OnNativeKeyPress(int device, int keyCode, int action, int metaState) {
		Integer playerNum = pad.playerNumX.get(device);
		if (playerNum != null && playerNum != -1 && !pad.isActiveMoga[playerNum]) {
			String id = pad.portId[playerNum];
			if (action == KeyEvent.ACTION_DOWN) {
				if (keyCode == prefs.getInt("l_button" + id, KeyEvent.KEYCODE_BUTTON_L1)) {
					return simulatedTouchEvent(playerNum, 1.0f, 0.0f);
				} else if (keyCode == prefs.getInt("r_button" + id, KeyEvent.KEYCODE_BUTTON_R1)) {
					return simulatedTouchEvent(playerNum, 0.0f, 1.0f);
				} else if (handle_key(playerNum, keyCode, true)) {
					if (playerNum == 0)
						JNIdc.hide_osd();
					return true;
				}
			}
			if (action == KeyEvent.ACTION_UP) {
				if (keyCode == prefs.getInt("l_button" + id,
						KeyEvent.KEYCODE_BUTTON_L1)
						|| keyCode == prefs.getInt("r_button" + id,
								KeyEvent.KEYCODE_BUTTON_R1)) {
					return simulatedTouchEvent(playerNum, 0.0f, 0.0f);
				} else {
					return handle_key(playerNum, keyCode, false);
				}
			}
		}
		return false;
	}

	public boolean OnNativeMotion(int device, int source, int action, int x, int y, boolean newEvent) {
		if (newEvent && source == Gamepad.Xperia_Touchpad) {
			// Source is Xperia Play touchpad
			Integer playerNum = pad.playerNumX.get(device);
			if (playerNum != null && playerNum != -1 && !pad.isActiveMoga[playerNum]) {
				Log.d("reidc", playerNum + " - " + device + ": " + source);
				if (action == MotionEvent.ACTION_UP) {
					x = 0;
					y = 0;
				}
				// Sensitive! Zero out the touchpad release
				if (x > 360 && x < 500) {
					x = 360;
				} else if (x > 500 && x < 640) {
					x = 640;
				}
				if (x >= 640) {
					x =  x - 640;
				}
				y = 366 - y;
				// Right stick is an extension of left stick
				// 360 to 640 is the live gap between sticks
				// The y-axis is inverted from normal layout
				// Imagine it as a small MacBook touch mouse

				GL2JNIView.jx[playerNum] = (int) (x * 126);
				GL2JNIView.jy[playerNum] = (int) (y * 126);
				mView.pushInput();
				return true;
			}
		}
		return false;
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
