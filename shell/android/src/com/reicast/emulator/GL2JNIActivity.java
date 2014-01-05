package com.reicast.emulator;

import tv.ouya.console.api.OuyaController;
import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
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
	static boolean xbox = false, nVidia = false;
	float globalLS_X, globalLS_Y, previousLS_X, previousLS_Y;

	int map[];

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

		params = new LayoutParams(80, 80);

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

		int joys[] = InputDevice.getDeviceIds();
		for (int i = 0; i < joys.length; i++) {
			Log.d("reidc", "InputDevice ID: " + joys[i]);
			Log.d("reidc", "InputDevice Name: "
					+ InputDevice.getDevice(joys[i]).getName());
			if (InputDevice.getDevice(joys[i]).getName()
					.equals("Sony PLAYSTATION(R)3 Controller")) {
				map = new int[] {
						OuyaController.BUTTON_Y, key_CONT_B,
						OuyaController.BUTTON_U, key_CONT_A,
						OuyaController.BUTTON_O, key_CONT_X,
						OuyaController.BUTTON_A, key_CONT_Y,

						OuyaController.BUTTON_DPAD_UP, key_CONT_DPAD_UP,
						OuyaController.BUTTON_DPAD_DOWN, key_CONT_DPAD_DOWN,
						OuyaController.BUTTON_DPAD_LEFT, key_CONT_DPAD_LEFT,
						OuyaController.BUTTON_DPAD_RIGHT, key_CONT_DPAD_RIGHT,

						OuyaController.BUTTON_MENU, key_CONT_START,
						OuyaController.BUTTON_R1, key_CONT_START

					};
				} else if (InputDevice.getDevice(joys[i]).getName()
					.equals("Microsoft X-Box 360 pad")) {
					map = new int[] {
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
					xbox = true;
				} else if (InputDevice.getDevice(joys[i]).getName()
						.contains("NVIDIA Corporation NVIDIA Controller")) {
					map = new int[] {
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
					nVidia = true;
				} else if (!moga.isActive) { // Ouya controller
					map = new int[] {
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

		// When viewing a resource, pass its URI to the native code for opening
		Intent intent = getIntent();
		if (intent.getAction().equals(Intent.ACTION_VIEW))
			fileName = Uri.decode(intent.getData().toString());

		// Create the actual GLES view
		mView = new GL2JNIView(getApplication(), fileName, false, 24, 0);
		setContentView(mView);

		Toast.makeText(getApplicationContext(),
				"Press the back button for a menu", Toast.LENGTH_SHORT).show();
	}

	@Override
	public boolean onGenericMotionEvent(MotionEvent event) {
		// Log.w("INPUT", event.toString() + " " + event.getSource());
		// Get all the axis for the KeyEvent

		if (nVidia) {
		    JNIdc.hide_osd();
		}

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

			if (xbox || nVidia) {
				previousLS_X = globalLS_X;
				previousLS_Y = globalLS_Y;
				globalLS_X = LS_X;
				globalLS_Y = LS_Y;
			}

			GL2JNIView.lt = (int) (L2 * 255);
			GL2JNIView.rt = (int) (R2 * 255);

			GL2JNIView.jx = (int) (LS_X * 126);
			GL2JNIView.jy = (int) (LS_Y * 126);
		}
		
		}
		
		if ((xbox || nVidia) && globalLS_X == previousLS_X && globalLS_Y == previousLS_Y)
			// Only handle Left Stick on an Xbox 360 controller if there was some actual motion on the stick,
			// so otherwise the event can be handled as a DPAD event
			return false;
		else
			return true;
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

	boolean handle_key(int kc, boolean down) {
	    if (!moga.isActive) {

		boolean rav = false;
		for (int i = 0; i < map.length; i += 2) {
			if (map[i + 0] == kc) {
				if (down)
					GL2JNIView.kcode_raw &= ~map[i + 1];
				else
					GL2JNIView.kcode_raw |= map[i + 1];

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
		return handle_key(keyCode, false) || super.onKeyUp(keyCode, event);
	}

	public boolean onKeyDown(int keyCode, KeyEvent event) {
		if (handle_key(keyCode, true)) {
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
