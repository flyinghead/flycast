package com.reicast.emulator.periph;


/******************************************************************************/

import java.util.Arrays;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.Log;

import com.bda.controller.Controller;
import com.bda.controller.ControllerListener;
import com.bda.controller.KeyEvent;
import com.bda.controller.MotionEvent;
import com.bda.controller.StateEvent;
import com.reicast.emulator.R;

/******************************************************************************/

/*

*/
public final class MOGAInput
{
	private SharedPreferences prefs;

	static final int DELAY = 1000 / 50; // 50 Hz
	
	public static final int ACTION_CONNECTED = Controller.ACTION_CONNECTED;
	static final int ACTION_DISCONNECTED = Controller.ACTION_DISCONNECTED;
	static final int ACTION_VERSION_MOGA = Controller.ACTION_VERSION_MOGA;
	static final int ACTION_VERSION_MOGAPRO = Controller.ACTION_VERSION_MOGAPRO;

	public Controller mController = null;
	private String notify;
	private Gamepad pad;

	private Activity act;

	public MOGAInput()
	{
		/*
		mStates.put(StateEvent.STATE_CONNECTION, new ExampleInteger("STATE_CONNECTION......"));
		mStates.put(StateEvent.STATE_POWER_LOW, new ExampleInteger("STATE_POWER_LOW......"));
		mStates.put(StateEvent.STATE_CURRENT_PRODUCT_VERSION, new ExampleInteger("STATE_CURRENT_PRODUCT_VERSION"));
		mStates.put(StateEvent.STATE_SUPPORTED_PRODUCT_VERSION, new ExampleInteger("STATE_SUPPORTED_PRODUCT_VERSION"));

		mKeys.put(KeyEvent.KEYCODE_DPAD_UP, new ExampleInteger("KEYCODE_DPAD_UP......"));
		mKeys.put(KeyEvent.KEYCODE_DPAD_DOWN, new ExampleInteger("KEYCODE_DPAD_DOWN......"));
		mKeys.put(KeyEvent.KEYCODE_DPAD_LEFT, new ExampleInteger("KEYCODE_DPAD_LEFT......"));
		mKeys.put(KeyEvent.KEYCODE_DPAD_RIGHT, new ExampleInteger("KEYCODE_DPAD_RIGHT......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_A, new ExampleInteger("KEYCODE_BUTTON_A......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_B, new ExampleInteger("KEYCODE_BUTTON_B......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_X, new ExampleInteger("KEYCODE_BUTTON_X......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_Y, new ExampleInteger("KEYCODE_BUTTON_Y......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_L1, new ExampleInteger("KEYCODE_BUTTON_L1......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_R1, new ExampleInteger("KEYCODE_BUTTON_R1......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_L2, new ExampleInteger("KEYCODE_BUTTON_L2......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_R2, new ExampleInteger("KEYCODE_BUTTON_R2......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_THUMBL, new ExampleInteger("KEYCODE_BUTTON_THUMBL......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_THUMBR, new ExampleInteger("KEYCODE_BUTTON_THUMBR......"));		
		mKeys.put(KeyEvent.KEYCODE_BUTTON_START, new ExampleInteger("KEYCODE_BUTTON_START......"));
		mKeys.put(KeyEvent.KEYCODE_BUTTON_SELECT, new ExampleInteger("KEYCODE_BUTTON_SELECT......"));

		mMotions.put(MotionEvent.AXIS_X, new ExampleFloat("AXIS_X........."));
		mMotions.put(MotionEvent.AXIS_Y, new ExampleFloat("AXIS_Y........."));
		mMotions.put(MotionEvent.AXIS_Z, new ExampleFloat("AXIS_Z........."));
		mMotions.put(MotionEvent.AXIS_RZ, new ExampleFloat("AXIS_RZ......."));
		mMotions.put(MotionEvent.AXIS_LTRIGGER, new ExampleFloat("AXIS_LTRIGGER........."));
		mMotions.put(MotionEvent.AXIS_RTRIGGER, new ExampleFloat("AXIS_RTRIGGER........."));
		*/
	}

	public void onCreate(Activity act, Gamepad pad) {
		this.act = act;

		this.pad = pad;

		prefs = PreferenceManager
				.getDefaultSharedPreferences(act.getApplicationContext());

		mController = Controller.getInstance(act);
		mController.init();
		mController.setListener(new ExampleControllerListener(), new Handler());
	}

	public void onDestroy()
	{
		mController.exit();
	}

	public void onPause()
	{
		mController.onPause();
	}

	public void onResume()
	{
		mController.onResume();

		/*
		for(final Entry<Integer, ExampleInteger> entry : mStates.entrySet())
		{
			final int key = entry.getKey();
			final ExampleInteger value = entry.getValue();
			value.mValue = mController.getState(key);
		}
		
		for(final Entry<Integer, ExampleInteger> entry : mKeys.entrySet())
		{
			final int key = entry.getKey();
			final ExampleInteger value = entry.getValue();
			value.mValue = mController.getKeyCode(key);
		}

		for(final Entry<Integer, ExampleFloat> entry : mMotions.entrySet())
		{
			final int key = entry.getKey();
			final ExampleFloat value = entry.getValue();
			value.mValue = mController.getAxisValue(key);
		}
		*/
	}

	class ExampleControllerListener implements ControllerListener
	{
		public void onKeyEvent(KeyEvent event)
		{
			// Handled by the primary controller interface
		}

		public void onMotionEvent(MotionEvent event)
		{
			// Handled by the primary controller interface
		}

		private void getCompatibilityMap(int playerNum, String id) {
			pad.name[playerNum] = prefs.getInt("controller" + id, -1);
			if (pad.name[playerNum] != -1) {
				pad.map[playerNum] = pad.setModifiedKeys(id, playerNum, prefs);
			}
		}

		private void initJoyStickLayout(int playerNum) {
			pad.globalLS_X[playerNum] = pad.previousLS_X[playerNum] = 0.0f;
			pad.globalLS_Y[playerNum] = pad.previousLS_Y[playerNum] = 0.0f;
		}

		private void notifyMogaConnected(final String notify, int playerNum) {
			String id = pad.portId[playerNum];
			pad.custom[playerNum] = prefs.getBoolean("modified_key_layout" + id, false);
			pad.compat[playerNum] = prefs.getBoolean("controller_compat" + id, false);
			pad.joystick[playerNum] = prefs.getBoolean("separate_joystick" + id, false);
			if (pad.compat[playerNum]) {
				getCompatibilityMap(playerNum, id);
			} else if (pad.custom[playerNum]) {
				pad.map[playerNum] = pad.setModifiedKeys(id, playerNum, prefs);
			} else {
				pad.map[playerNum] = pad.getMogaController();
			}
			initJoyStickLayout(playerNum);
		}

		public void onStateEvent(StateEvent event)
		{
			Integer playerNum = Arrays.asList(pad.name).indexOf(event.getControllerId());
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD && playerNum == -1) {
				playerNum = pad.deviceDescriptor_PlayerNum
						.get(pad.deviceId_deviceDescriptor.get(event.getControllerId()));
			} else {
				playerNum = -1;
			}

			if (playerNum == null || playerNum == -1) {
				return;
			}

			if (event.getState() == StateEvent.STATE_CONNECTION && event.getAction() == ACTION_CONNECTED) {
				int mControllerVersion = mController.getState(Controller.STATE_CURRENT_PRODUCT_VERSION);
				if (mControllerVersion == Controller.ACTION_VERSION_MOGAPRO) {
					pad.isMogaPro[playerNum] = true;
					pad.isActiveMoga[playerNum] = true;
					Log.d("com.reicast.emulator", act.getString(R.string.moga_pro_connect));
				} else if (mControllerVersion == Controller.ACTION_VERSION_MOGA) {
					pad.isMogaPro[playerNum] = false;
					pad.isActiveMoga[playerNum] = true;
					Log.d("com.reicast.emulator", act.getString(R.string.moga_connect));
				}
				if (pad.isActiveMoga[playerNum]) {
					notifyMogaConnected(notify, playerNum);
				}
			}
		}
	}
}
