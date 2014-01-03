package com.reicast.emulator;


/******************************************************************************/

import android.app.Activity;
import android.os.Handler;
import android.widget.Toast;

import com.bda.controller.Controller;
import com.bda.controller.ControllerListener;
import com.bda.controller.KeyEvent;
import com.bda.controller.MotionEvent;
import com.bda.controller.StateEvent;

/******************************************************************************/

/*

*/
public class MOGAInput
{
	static final int DELAY = 1000 / 50; // 50 Hz
	
	static final int ACTION_CONNECTED = Controller.ACTION_CONNECTED;
	static final int ACTION_DISCONNECTED = Controller.ACTION_DISCONNECTED;
	static final int ACTION_VERSION_MOGA = Controller.ACTION_VERSION_MOGA;
	static final int ACTION_VERSION_MOGAPRO = Controller.ACTION_VERSION_MOGAPRO;

	Controller mController = null;

    public boolean isActive = false;

	private static final int key_CONT_B 			= 0x0002;
	private static final int key_CONT_A 			= 0x0004;
	private static final int key_CONT_START 		= 0x0008;
	private static final int key_CONT_DPAD_UP 		= 0x0010;
	private static final int key_CONT_DPAD_DOWN 	= 0x0020;
	private static final int key_CONT_DPAD_LEFT 	= 0x0040;
	private static final int key_CONT_DPAD_RIGHT 	= 0x0080;
	private static final int key_CONT_Y 			= 0x0200;
	private static final int key_CONT_X 			= 0x0400;

	int[] map = new int[] {
						KeyEvent.KEYCODE_BUTTON_B, key_CONT_B,
						KeyEvent.KEYCODE_BUTTON_A, key_CONT_A,
						KeyEvent.KEYCODE_BUTTON_X, key_CONT_X,
						KeyEvent.KEYCODE_BUTTON_Y, key_CONT_Y,

						KeyEvent.KEYCODE_DPAD_UP, key_CONT_DPAD_UP,
						KeyEvent.KEYCODE_DPAD_DOWN, key_CONT_DPAD_DOWN,
						KeyEvent.KEYCODE_DPAD_LEFT, key_CONT_DPAD_LEFT,
						KeyEvent.KEYCODE_DPAD_RIGHT, key_CONT_DPAD_RIGHT,

						KeyEvent.KEYCODE_BUTTON_START, key_CONT_START,
				};

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

	protected void onCreate(Activity act)
	{
		this.act = act;

		mController = Controller.getInstance(act);
		mController.init();
		mController.setListener(new ExampleControllerListener(), new Handler());
	}

	protected void onDestroy()
	{
		mController.exit();
	}

	protected void onPause()
	{
		mController.onPause();
	}

	protected void onResume()
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
			JNIdc.hide_osd();
			for (int i = 0; i < map.length; i += 2) {
				if (map[i + 0] == event.getKeyCode()) {
					if (event.getAction() == 0) //FIXME to const
						GL2JNIView.kcode_raw[0] &= ~map[i + 1];
					else
						GL2JNIView.kcode_raw[0] |= map[i + 1];

					break;
				}
			}
		}

		public void onMotionEvent(MotionEvent event)
		{
			JNIdc.hide_osd();

			float LS_X = event.getAxisValue(MotionEvent.AXIS_X);
			float LS_Y = event.getAxisValue(MotionEvent.AXIS_Y);
			float L2 = event.getAxisValue(MotionEvent.AXIS_LTRIGGER);
			float R2 = event.getAxisValue(MotionEvent.AXIS_RTRIGGER);

			GL2JNIView.lt[0] = (int) (L2 * 255);
			GL2JNIView.rt[0] = (int) (R2 * 255);

			GL2JNIView.jx[0] = (int) (LS_X * 126);
			GL2JNIView.jy[0] = (int) (LS_Y * 126);

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
			JNIdc.hide_osd();

			if (event.getState() == StateEvent.STATE_CONNECTION && event.getAction() == ACTION_CONNECTED) {
        		Toast.makeText(act.getApplicationContext(), "MOGA Connected!", Toast.LENGTH_SHORT).show();
        		isActive = true;
			}
		}
	}
}
