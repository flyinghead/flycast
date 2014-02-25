package com.reicast.emulator.periph;


/******************************************************************************/

import java.util.HashMap;

import android.app.Activity;
import android.content.SharedPreferences;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.util.SparseArray;
import android.widget.Toast;

import com.bda.controller.Controller;
import com.bda.controller.ControllerListener;
import com.bda.controller.KeyEvent;
import com.bda.controller.MotionEvent;
import com.bda.controller.StateEvent;
import com.reicast.emulator.R;
import com.reicast.emulator.emu.GL2JNIView;
import com.reicast.emulator.emu.JNIdc;

/******************************************************************************/

/*

*/
public class MOGAInput
{
	private SharedPreferences prefs;

	static final int DELAY = 1000 / 50; // 50 Hz
	
	public static final int ACTION_CONNECTED = Controller.ACTION_CONNECTED;
	static final int ACTION_DISCONNECTED = Controller.ACTION_DISCONNECTED;
	static final int ACTION_VERSION_MOGA = Controller.ACTION_VERSION_MOGA;
	static final int ACTION_VERSION_MOGAPRO = Controller.ACTION_VERSION_MOGAPRO;

	public Controller mController = null;
	private Handler handler;
	private String notify;
	private Gamepad pad;

	Activity act;
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
		
		handler = new Handler();
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
			Integer playerNum = pad.deviceDescriptor_PlayerNum.get(pad.deviceId_deviceDescriptor.get(event.getControllerId()));

	    		if (playerNum == null)
				return;

	    		String id = pad.portId[playerNum];
	    		if (pad.custom[playerNum]) {
	    			if (event.getKeyCode() == prefs.getInt("l_button" + id, KeyEvent.KEYCODE_BUTTON_L1)) {
						simulatedTouchEvent(playerNum, 1.0f, 0.0f);
						simulatedTouchEvent(playerNum, 0.0f, 0.0f);
					}
					if (event.getKeyCode() == prefs.getInt("r_button" + id, KeyEvent.KEYCODE_BUTTON_R1)) {
						simulatedTouchEvent(playerNum, 0.0f, 1.0f);
						simulatedTouchEvent(playerNum, 0.0f, 0.0f);
					}
	    		}

			if(playerNum == 0)
				JNIdc.hide_osd();

			for (int i = 0; i < pad.map.length; i += 2) {
				if (pad.map[playerNum][i + 0] == event.getKeyCode()) {
					if (event.getAction() == 0) //FIXME to const
						GL2JNIView.kcode_raw[playerNum] &= ~pad.map[playerNum][i + 1];
					else
						GL2JNIView.kcode_raw[playerNum] |= pad.map[playerNum][i + 1];
					break;
				}
			}
		}
		
		public void simulatedTouchEvent(int playerNum, float L2, float R2) {
			if(playerNum == 0)
				JNIdc.hide_osd();
			pad.previousLS_X[playerNum] = pad.globalLS_X[playerNum];
			pad.previousLS_Y[playerNum] = pad.globalLS_Y[playerNum];
			pad.globalLS_X[playerNum] = 0;
			pad.globalLS_Y[playerNum] = 0;
			GL2JNIView.lt[playerNum] = (int) (L2 * 255);
			GL2JNIView.rt[playerNum] = (int) (R2 * 255);
			GL2JNIView.jx[playerNum] = (int) (0 * 126);
			GL2JNIView.jy[playerNum] = (int) (0 * 126);
		}

		public void onMotionEvent(MotionEvent event)
		{
			Integer playerNum = pad.deviceDescriptor_PlayerNum.get(pad.deviceId_deviceDescriptor.get(event.getControllerId()));

	    		if (playerNum == null)
				return;

			if(playerNum == 0)
				JNIdc.hide_osd();

			float S_X = event.getAxisValue(MotionEvent.AXIS_X);
			float S_Y = event.getAxisValue(MotionEvent.AXIS_Y);
			float L2 = event.getAxisValue(MotionEvent.AXIS_LTRIGGER);
			float R2 = event.getAxisValue(MotionEvent.AXIS_RTRIGGER);

			pad.previousLS_X[playerNum] = pad.globalLS_X[playerNum];
			pad.previousLS_Y[playerNum] = pad.globalLS_Y[playerNum];
			pad.globalLS_X[playerNum] = S_X;
			pad.globalLS_Y[playerNum] = S_Y;

			GL2JNIView.lt[playerNum] = (int) (L2 * 255);
			GL2JNIView.rt[playerNum] = (int) (R2 * 255);

			GL2JNIView.jx[playerNum] = (int) (S_X * 126);
			GL2JNIView.jy[playerNum] = (int) (S_Y * 126);

			/*
			for(final Entry<Integer, ExampleFloat> entry : mMotions.entrySet())
			{
				final int key = entry.getKey();
				final ExampleFloat value = entry.getValue();
				value.mValue = event.getAxisValue(key);
			}*/
		}

		public void onStateEvent(StateEvent event)
		{
			Integer playerNum = pad.deviceDescriptor_PlayerNum.get(pad.deviceId_deviceDescriptor.get(event.getControllerId()));

	    		if (playerNum == null)
				return;

			if(playerNum == 0)
				JNIdc.hide_osd();
			
			String id = pad.portId[playerNum];
			pad.custom[playerNum] = prefs.getBoolean("modified_key_layout" + id, false);

			if (event.getState() == StateEvent.STATE_CONNECTION && event.getAction() == ACTION_CONNECTED) {
        		int mControllerVersion = mController.getState(Controller.STATE_CURRENT_PRODUCT_VERSION);
        		if (mControllerVersion == Controller.ACTION_VERSION_MOGAPRO) {
        			pad.isActiveMoga[playerNum] = true;
        			pad.isMogaPro[playerNum] = true;
        			if (pad.custom[playerNum]) {
        				pad.map[playerNum] = pad.setModifiedKeys(id, playerNum, prefs);
        			} else {
        				pad.map[playerNum] = pad.getMogaController();
        			}
        			notify = act.getApplicationContext().getString(R.string.moga_pro_connect);
        		} else if (mControllerVersion == Controller.ACTION_VERSION_MOGA) {
        			pad.isActiveMoga[playerNum] = true;
        			pad.isMogaPro[playerNum] = false;
        			if (pad.custom[playerNum]) {
        				pad.map[playerNum] = pad.setModifiedKeys(id, playerNum, prefs);
        			} else {
        				pad.map[playerNum] = pad.getMogaController();
        			}
        			notify = act.getApplicationContext().getString(R.string.moga_connect);
        		}
        		if (notify != null && !notify.equals(null)) {
        			handler.post(new Runnable() {
    					public void run() {
    						Toast.makeText(act.getApplicationContext(), notify, Toast.LENGTH_SHORT).show();
    					}
    				});
        		}
			}
		}
	}
}
