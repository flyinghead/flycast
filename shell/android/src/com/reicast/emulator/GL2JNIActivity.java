package com.reicast.emulator;

import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;

import tv.ouya.console.api.OuyaController;

import com.reicast.emulator.GL2JNIView.EmuThread;

import android.content.SharedPreferences;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
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
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.widget.ImageButton;
import android.widget.ImageView.ScaleType;
import android.widget.LinearLayout;
import android.widget.PopupWindow;
import android.widget.Toast;

@TargetApi(Build.VERSION_CODES.HONEYCOMB_MR1)
public class GL2JNIActivity extends Activity {
	GL2JNIView mView;
	PopupWindow popUp;
	LayoutParams params;
	MOGAInput moga = new MOGAInput();
	static boolean[] xbox = { false, false, false, false }, nVidia = { false, false, false, false };
	float[] globalLS_X = new float[4], globalLS_Y = new float[4], previousLS_X = new float[4], previousLS_Y = new float[4];

	public static HashMap<Integer, String> deviceId_deviceDescriptor = new HashMap<Integer, String>();
	public static HashMap<String, Integer> deviceDescriptor_PlayerNum = new HashMap<String, Integer>();

	int map[][];

	public static int getPixelsFromDp(float dps, Context context) {
		return (int) (dps * context.getResources().getDisplayMetrics().density + 0.5f);
	}

	View addbut(int x, OnClickListener ocl) {
		ImageButton but = new ImageButton(this);

		but.setImageResource(x);
		but.setScaleType(ScaleType.FIT_CENTER);
		but.setOnClickListener(ocl);

		return but;
	}

	static byte[] syms;

	void createPopup() {
		popUp = new PopupWindow(this);
		// LinearLayout layout = new LinearLayout(this);

		// tv = new TextView(this);
		int p = getPixelsFromDp(60, this);
		params = new LayoutParams(p, p);

		// layout.setOrientation(LinearLayout.VERTICAL);
		// tv.setText("Hi this is a sample text for popup window");
		// layout.addView(tv, params);

		LinearLayout hlay = new LinearLayout(this);

		hlay.setOrientation(LinearLayout.HORIZONTAL);

		hlay.addView(addbut(R.drawable.close, new OnClickListener() {
			public void onClick(View v) {
				Intent inte = new Intent(GL2JNIActivity.this, MainActivity.class);
				startActivity(inte);
				GL2JNIActivity.this.finish();
			}
		}), params);

		hlay.addView(addbut(R.drawable.config, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(0, 0);
				popUp.dismiss();
			}
		}), params);

		hlay.addView(addbut(R.drawable.profiler, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(1, 3000);
				popUp.dismiss();
			}
		}), params);

		hlay.addView(addbut(R.drawable.profiler, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(1, 0);
				popUp.dismiss();
			}
		}), params);

		hlay.addView(addbut(R.drawable.disk_unknown, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(0, 1);
				popUp.dismiss();
			}
		}), params);

		hlay.addView(addbut(R.drawable.profiler, new OnClickListener() {
			public void onClick(View v) {
				JNIdc.send(0, 2);
				popUp.dismiss();
			}
		}), params);

		// layout.addView(hlay,params);
		popUp.setContentView(hlay);
	}

	@Override
	protected void onCreate(Bundle icicle) {
		requestWindowFeature(Window.FEATURE_NO_TITLE);
		moga.onCreate(this);
		
		createPopup();
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
		SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
		deviceDescriptor_PlayerNum.put(prefs.getString("device_descriptor_player_1", null), 0);
		deviceDescriptor_PlayerNum.put(prefs.getString("device_descriptor_player_2", null), 1);
		deviceDescriptor_PlayerNum.put(prefs.getString("device_descriptor_player_3", null), 2);
		deviceDescriptor_PlayerNum.put(prefs.getString("device_descriptor_player_4", null), 3);

		boolean controllerTwoConnected = false;
		boolean controllerThreeConnected = false;
		boolean controllerFourConnected = false;

		for (HashMap.Entry<String, Integer> e : deviceDescriptor_PlayerNum.entrySet()) {
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

		JNIdc.initControllers(new boolean[] {controllerTwoConnected, controllerThreeConnected, controllerFourConnected});

		int joys[] = InputDevice.getDeviceIds();
		for (int i = 0; i < joys.length; i++) {
			String descriptor = null;
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
				descriptor = InputDevice.getDevice(joys[i]).getDescriptor();
			} else {
				descriptor = InputDevice.getDevice(joys[i]).getName();
			}
			Log.d("reidc", "InputDevice ID: " + joys[i]);
			Log.d("reidc", "InputDevice Name: "
					+ InputDevice.getDevice(joys[i]).getName());
			Log.d("reidc", "InputDevice Descriptor: " + descriptor);
			deviceId_deviceDescriptor.put(joys[i], descriptor);
		}

		for (int i = 0; i < joys.length; i++) {
			Integer playerNum = deviceDescriptor_PlayerNum.get(deviceId_deviceDescriptor.get(joys[i]));

			if (playerNum != null) {

			if (InputDevice.getDevice(joys[i]).getName()
					.equals("Sony PLAYSTATION(R)3 Controller")) {
				map[playerNum] = new int[] {
						OuyaController.BUTTON_Y, key_CONT_Y,
						OuyaController.BUTTON_U, key_CONT_X,
						OuyaController.BUTTON_O, key_CONT_A,
						OuyaController.BUTTON_A, key_CONT_B,

							OuyaController.BUTTON_DPAD_UP, key_CONT_DPAD_UP,
							OuyaController.BUTTON_DPAD_DOWN,
							key_CONT_DPAD_DOWN,
							OuyaController.BUTTON_DPAD_LEFT,
							key_CONT_DPAD_LEFT,
							OuyaController.BUTTON_DPAD_RIGHT,
							key_CONT_DPAD_RIGHT,

						OuyaController.BUTTON_MENU, key_CONT_START,
						OuyaController.BUTTON_R1, key_CONT_START

					};
				} else if (InputDevice.getDevice(joys[i]).getName()
					.equals("Microsoft X-Box 360 pad")) {
					map[playerNum] = new int[] {
							OuyaController.BUTTON_O, key_CONT_A,
							OuyaController.BUTTON_A, key_CONT_B,
							OuyaController.BUTTON_Y, key_CONT_Y,
							OuyaController.BUTTON_U, key_CONT_X,

							OuyaController.BUTTON_DPAD_UP, key_CONT_DPAD_UP,
							OuyaController.BUTTON_DPAD_DOWN,
							key_CONT_DPAD_DOWN,
							OuyaController.BUTTON_DPAD_LEFT,
							key_CONT_DPAD_LEFT,
							OuyaController.BUTTON_DPAD_RIGHT,
							key_CONT_DPAD_RIGHT,

							OuyaController.BUTTON_MENU, key_CONT_START,
							OuyaController.BUTTON_R1, key_CONT_START };

					xbox[playerNum] = true;

					globalLS_X[playerNum] = previousLS_X[playerNum] = 0.0f;
					globalLS_Y[playerNum] = previousLS_Y[playerNum] = 0.0f;
				} else if (InputDevice.getDevice(joys[i]).getName()
						.contains("NVIDIA Corporation NVIDIA Controller")) {
					map[playerNum] = new int[] {
							OuyaController.BUTTON_O, key_CONT_A,
							OuyaController.BUTTON_A, key_CONT_B,
							OuyaController.BUTTON_Y, key_CONT_Y,
							OuyaController.BUTTON_U, key_CONT_X,

							OuyaController.BUTTON_DPAD_UP, key_CONT_DPAD_UP,
							OuyaController.BUTTON_DPAD_DOWN,
							key_CONT_DPAD_DOWN,
							OuyaController.BUTTON_DPAD_LEFT,
							key_CONT_DPAD_LEFT,
							OuyaController.BUTTON_DPAD_RIGHT,
							key_CONT_DPAD_RIGHT,

							OuyaController.BUTTON_MENU, key_CONT_START,
							OuyaController.BUTTON_R1, key_CONT_START };
					nVidia[playerNum] = true;

					globalLS_X[playerNum] = previousLS_X[playerNum] = 0.0f;
					globalLS_Y[playerNum] = previousLS_Y[playerNum] = 0.0f;
				} else if (!moga.isActive) { // Ouya controller
					map[playerNum] = new int[] {
							OuyaController.BUTTON_O, key_CONT_A,
							OuyaController.BUTTON_A, key_CONT_B,
							OuyaController.BUTTON_Y, key_CONT_Y,
							OuyaController.BUTTON_U, key_CONT_X,

							OuyaController.BUTTON_DPAD_UP, key_CONT_DPAD_UP,
							OuyaController.BUTTON_DPAD_DOWN,
							key_CONT_DPAD_DOWN,
							OuyaController.BUTTON_DPAD_LEFT,
							key_CONT_DPAD_LEFT,
							OuyaController.BUTTON_DPAD_RIGHT,
							key_CONT_DPAD_RIGHT,

							OuyaController.BUTTON_MENU, key_CONT_START,
							OuyaController.BUTTON_R1, key_CONT_START };
				}
			}
		
		}

		}

		// When viewing a resource, pass its URI to the native code for opening
		Intent intent = getIntent();
		if (intent.getAction().equals(Intent.ACTION_VIEW))
			fileName = Uri.decode(intent.getData().toString());

		// Create the actual GLES view
		mView = new GL2JNIView(getApplication(), fileName, false, 24, 0, false);
		setContentView(mView);

		Toast.makeText(getApplicationContext(),
				"Press the back button for a menu", Toast.LENGTH_SHORT).show();
	}

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		// Log.w("INPUT", event.toString() + " " + event.getSource());
		// Get all the axis for the KeyEvent
		
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {

		Integer playerNum = deviceDescriptor_PlayerNum.get(deviceId_deviceDescriptor.get(event.getDeviceId()));

		if (playerNum == null)
			return false;

		if (!moga.isActive) {

		// Joystick
		if ((event.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {

			// do other things with joystick
			float LS_X = event.getAxisValue(OuyaController.AXIS_LS_X);
			float LS_Y = event.getAxisValue(OuyaController.AXIS_LS_Y);
			float RS_X = event.getAxisValue(OuyaController.AXIS_RS_X);
			float RS_Y = event.getAxisValue(OuyaController.AXIS_RS_Y);
			float L2 = event.getAxisValue(OuyaController.AXIS_L2);
			float R2 = event.getAxisValue(OuyaController.AXIS_R2);

			if (xbox[playerNum] || nVidia[playerNum]) {
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
		
		if ((xbox[playerNum] || nVidia[playerNum]) && ((globalLS_X[playerNum] == previousLS_X[playerNum] && globalLS_Y[playerNum] == previousLS_Y[playerNum])
		 || (previousLS_X[playerNum] == 0.0f && previousLS_Y[playerNum] == 0.0f)))
			// Only handle Left Stick on an Xbox 360 controller if there was some actual motion on the stick,
			// so otherwise the event can be handled as a DPAD event
			return false;
		else
			return true;

		} else {
			return false;
		}

	}

	private static final int key_CONT_B 			= 0x0002;
	private static final int key_CONT_A 			= 0x0004;
	private static final int key_CONT_START 		= 0x0008;
	private static final int key_CONT_DPAD_UP 		= 0x0010;
	private static final int key_CONT_DPAD_DOWN 	= 0x0020;
	private static final int key_CONT_DPAD_LEFT 	= 0x0040;
	private static final int key_CONT_DPAD_RIGHT 	= 0x0080;
	private static final int key_CONT_Y 			= 0x0200;
	private static final int key_CONT_X 			= 0x0400;

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
	    if (playerNum == null)
		return false;

	    if (!moga.isActive) {

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

		return rav;

		} else {
		    return true;
		}
	}

	public boolean onKeyUp(int keyCode, KeyEvent event) {
		Integer playerNum = deviceDescriptor_PlayerNum.get(deviceId_deviceDescriptor.get(event.getDeviceId()));

		return handle_key(playerNum, keyCode, false) || super.onKeyUp(keyCode, event);
	}

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		Integer playerNum = deviceDescriptor_PlayerNum.get(deviceId_deviceDescriptor.get(event.getDeviceId()));

		if (handle_key(playerNum, keyCode, true)) {
			if(playerNum == 0)
				JNIdc.hide_osd();
			return true;
		}

		if (keyCode == KeyEvent.KEYCODE_MENU
				|| keyCode == KeyEvent.KEYCODE_BACK) {
			if (!popUp.isShowing()) {
				popUp.showAtLocation(mView, Gravity.BOTTOM, 0, 0);
				popUp.update(LayoutParams.WRAP_CONTENT,
						LayoutParams.WRAP_CONTENT);

			} else {
				popUp.dismiss();
			}

			return true;
		} else
			return super.onKeyDown(keyCode, event);
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
		if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
			// do your task
		} else if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT) {
			// do your task
		}
		super.onConfigurationChanged(newConfig);
	}

	@Override
	protected void onResume() {
		super.onResume();
		mView.onResume();
		moga.onResume();
	}
}
