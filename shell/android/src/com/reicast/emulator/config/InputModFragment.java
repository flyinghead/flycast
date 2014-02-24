package com.reicast.emulator.config;

import java.io.IOException;
import java.io.InputStream;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemSelectedListener;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ImageView;
import android.widget.Spinner;
import android.widget.TextView;

import com.reicast.emulator.R;

import de.ankri.views.Switch;

public class InputModFragment extends Fragment {

	private Activity parentActivity;
	private SharedPreferences mPrefs;
	private Switch switchModifiedLayoutEnabled;
	private Switch switchCompatibilityEnabled;
	private String player = "_A";
	private int sS = 2;
	private int playerNum = -1;
	private mapKeyCode mKey;

	// Container Activity must implement this interface
	public interface OnClickListener {
		public void onMainBrowseSelected(String path_entry, boolean games);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.input_mod_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		parentActivity = getActivity();

		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
			Runtime.getRuntime().freeMemory();
			System.gc();
		}

		mPrefs = PreferenceManager.getDefaultSharedPreferences(parentActivity);

		final String[] controllers = parentActivity.getResources()
				.getStringArray(R.array.controllers);

		Bundle b = getArguments();
		if (b != null) {
			playerNum = b.getInt("portNumber", -1);
		}

		Spinner player_spnr = (Spinner) getView().findViewById(
				R.id.player_spinner);
		ArrayAdapter<String> playerAdapter = new ArrayAdapter<String>(
				parentActivity, android.R.layout.simple_spinner_item,
				controllers);
		playerAdapter
				.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
		player_spnr.setAdapter(playerAdapter);
		if (playerNum != -1) {
			player_spnr.setSelection(playerNum, true);
		}
		player_spnr.setOnItemSelectedListener(new OnItemSelectedListener() {

			public void onItemSelected(AdapterView<?> parent, View view,
					int pos, long id) {
				String selection = parent.getItemAtPosition(pos).toString();
				player = "_"
						+ selection.substring(selection.lastIndexOf(" ") + 1,
								selection.length());
			}

			public void onNothingSelected(AdapterView<?> arg0) {
				if (playerNum != -1) {
					player = controllers[playerNum];
				}
			}

		});

		OnCheckedChangeListener modified_layout = new OnCheckedChangeListener() {
			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit()
						.putBoolean("modified_key_layout" + player, isChecked)
						.commit();
			}
		};
		switchModifiedLayoutEnabled = (Switch) getView().findViewById(
				R.id.switchModifiedLayoutEnabled);
		boolean layout = mPrefs.getBoolean("modified_key_layout" + player,
				false);
		if (layout) {
			switchModifiedLayoutEnabled.setChecked(true);
		} else {
			switchModifiedLayoutEnabled.setChecked(false);
		}
		switchModifiedLayoutEnabled.setOnCheckedChangeListener(modified_layout);

		OnCheckedChangeListener compat_mode = new OnCheckedChangeListener() {
			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				if (isChecked) {
					selectController();
				} else {
					mPrefs.edit().remove("controller" + player).commit();
				}
				mPrefs.edit()
						.putBoolean("controller_compat" + player, isChecked)
						.commit();
			}
		};
		switchCompatibilityEnabled = (Switch) getView().findViewById(
				R.id.switchCompatibilityEnabled);
		boolean compat = mPrefs.getBoolean("controller_compat" + player, false);
		if (compat) {
			switchCompatibilityEnabled.setChecked(true);
		} else {
			switchCompatibilityEnabled.setChecked(false);
		}
		switchCompatibilityEnabled.setOnCheckedChangeListener(compat_mode);

		mKey = new mapKeyCode(parentActivity);

		ImageView a_button_icon = (ImageView) getView().findViewById(
				R.id.a_button_icon);
		a_button_icon.setImageDrawable(getButtonImage(448 / sS, 0));
		final TextView a_button_text = (TextView) getView().findViewById(
				R.id.a_button_key);
		getKeyCode("a_button", a_button_text);
		Button a_button = (Button) getView().findViewById(R.id.a_button_edit);
		a_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("a_button", a_button_text);
			}
		});
		Button a_remove = (Button) getView().findViewById(R.id.remove_a_button);
		a_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("a_button", a_button_text);
			}
		});

		ImageView b_button_icon = (ImageView) getView().findViewById(
				R.id.b_button_icon);
		b_button_icon.setImageDrawable(getButtonImage(384 / sS, 0));
		final TextView b_button_text = (TextView) getView().findViewById(
				R.id.b_button_key);
		getKeyCode("b_button", b_button_text);
		Button b_button = (Button) getView().findViewById(R.id.b_button_edit);
		b_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("b_button", b_button_text);
			}
		});
		Button b_remove = (Button) getView().findViewById(R.id.remove_b_button);
		b_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("b_button", b_button_text);
			}
		});

		ImageView x_button_icon = (ImageView) getView().findViewById(
				R.id.x_button_icon);
		x_button_icon.setImageDrawable(getButtonImage(256 / sS, 0));
		final TextView x_button_text = (TextView) getView().findViewById(
				R.id.x_button_key);
		getKeyCode("x_button", x_button_text);
		Button x_button = (Button) getView().findViewById(R.id.x_button_edit);
		x_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("x_button", x_button_text);
			}
		});
		Button x_remove = (Button) getView().findViewById(R.id.remove_x_button);
		x_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("x_button", x_button_text);
			}
		});

		ImageView y_button_icon = (ImageView) getView().findViewById(
				R.id.y_button_icon);
		y_button_icon.setImageDrawable(getButtonImage(320 / sS, 0));
		final TextView y_button_text = (TextView) getView().findViewById(
				R.id.y_button_key);
		getKeyCode("y_button", y_button_text);
		Button y_button = (Button) getView().findViewById(R.id.y_button_edit);
		y_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("y_button", y_button_text);
			}
		});
		Button y_remove = (Button) getView().findViewById(R.id.remove_y_button);
		y_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("y_button", y_button_text);
			}
		});

		ImageView l_button_icon = (ImageView) getView().findViewById(
				R.id.l_button_icon);
		l_button_icon.setImageDrawable(getButtonImage(78 / sS, 64 / sS));
		final TextView l_button_text = (TextView) getView().findViewById(
				R.id.l_button_key);
		getKeyCode("l_button", l_button_text);
		Button l_button = (Button) getView().findViewById(R.id.l_button_edit);
		l_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("l_button", l_button_text);
			}
		});
		Button l_remove = (Button) getView().findViewById(R.id.remove_l_button);
		l_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("l_button", l_button_text);
			}
		});

		ImageView r_button_icon = (ImageView) getView().findViewById(
				R.id.r_button_icon);
		r_button_icon.setImageDrawable(getButtonImage(162 / sS, 64 / sS));
		final TextView r_button_text = (TextView) getView().findViewById(
				R.id.r_button_key);
		getKeyCode("r_button", r_button_text);
		Button r_button = (Button) getView().findViewById(R.id.r_button_edit);
		r_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("r_button", r_button_text);
			}
		});
		Button r_remove = (Button) getView().findViewById(R.id.remove_r_button);
		r_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("r_button", r_button_text);
			}
		});

        final TextView dpad_up_text = (TextView) getView().findViewById(
				R.id.dpad_up_key);
		getKeyCode("dpad_up", dpad_up_text);
		Button dpad_up = (Button) getView().findViewById(R.id.dpad_up_edit);
		dpad_up.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("dpad_up", dpad_up_text);
			}
		});
		Button up_remove = (Button) getView().findViewById(R.id.remove_dpad_up);
		up_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("dpad_up", dpad_up_text);
			}
		});

		final TextView dpad_down_text = (TextView) getView().findViewById(
				R.id.dpad_down_key);
		getKeyCode("dpad_down", dpad_down_text);
		Button dpad_down = (Button) getView().findViewById(R.id.dpad_down_edit);
		dpad_down.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("dpad_down", dpad_down_text);
			}
		});
		Button down_remove = (Button) getView().findViewById(
				R.id.remove_dpad_down);
		down_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("dpad_down", dpad_down_text);
			}
		});

		final TextView dpad_left_text = (TextView) getView().findViewById(
				R.id.dpad_left_key);
		getKeyCode("dpad_left", dpad_left_text);
		Button dpad_left = (Button) getView().findViewById(R.id.dpad_left_edit);
		dpad_left.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("dpad_left", dpad_left_text);
			}
		});
		Button left_remove = (Button) getView().findViewById(
				R.id.remove_dpad_left);
		left_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("dpad_left", dpad_left_text);
			}
		});

		final TextView dpad_right_text = (TextView) getView().findViewById(
				R.id.dpad_right_key);
		getKeyCode("dpad_right", dpad_right_text);
		Button dpad_right = (Button) getView().findViewById(
				R.id.dpad_right_edit);
		dpad_right.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("dpad_right", dpad_right_text);
			}
		});
		Button right_remove = (Button) getView().findViewById(
				R.id.remove_dpad_right);
		right_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("dpad_right", dpad_right_text);
			}
		});

		ImageView start_button_icon = (ImageView) getView().findViewById(
				R.id.start_button_icon);
		start_button_icon.setImageDrawable(getButtonImage(0, 64 / sS));
		final TextView start_button_text = (TextView) getView().findViewById(
				R.id.start_button_key);
		getKeyCode("start_button", start_button_text);
		Button start_button = (Button) getView().findViewById(
				R.id.start_button_edit);
		start_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("start_button", start_button_text);
			}
		});
		Button start_remove = (Button) getView()
				.findViewById(R.id.remove_start);
		start_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("start_button", start_button_text);
			}
		});

		ImageView select_button_icon = (ImageView) getView().findViewById(
				R.id.select_button_icon);
		select_button_icon.setImageResource(R.drawable.ic_drawer);
		final TextView select_button_text = (TextView) getView().findViewById(
				R.id.select_button_key);
		Button select_button = (Button) getView().findViewById(
				R.id.select_button_edit);
		select_button.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				mKey.intiateSearch("select_button", select_button_text);
			}
		});
		Button select_remove = (Button) getView()
				.findViewById(R.id.remove_select);
		select_remove.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				remKeyCode("select_button", select_button_text);
			}
		});
	}

	/**
	 * Retrieve an image to serve as a visual representation
	 * 
	 * @param int
	 *            The x start value of the image within the atlas
	 * @param int
	 *            The y start value of the image within the atlas
	 */
	private Drawable getButtonImage(int x, int y) {
		if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
			Runtime.getRuntime().freeMemory();
			System.gc();
		}
		try {
			InputStream bitmap = parentActivity.getAssets().open("buttons.png");
			BitmapFactory.Options options = new BitmapFactory.Options();
			options.inSampleSize = sS;
			Bitmap image = BitmapFactory.decodeStream(bitmap, null, options);
			bitmap.close();
			bitmap = null;
			if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
				Runtime.getRuntime().freeMemory();
				System.gc();
			}
			Matrix matrix = new Matrix();
			matrix.postScale(32, 32);
			Bitmap resizedBitmap = Bitmap.createBitmap(image, x, y, 64 / sS,
					64 / sS, matrix, true);
			BitmapDrawable bmd = new BitmapDrawable(resizedBitmap);
			image.recycle();
			image = null;
			return bmd;
		} catch (IOException e1) {
			e1.printStackTrace();
		} catch (OutOfMemoryError E) {
			if (sS == 2) {
				sS = 4;
				return getButtonImage(x, y);
			} else {
				E.printStackTrace();
				if (Build.VERSION.SDK_INT < Build.VERSION_CODES.HONEYCOMB) {
					Runtime.getRuntime().freeMemory();
					System.gc();
				}
			}
		}
		return parentActivity.getResources().getDrawable(R.drawable.input);
	}

	private void getKeyCode(final String button, final TextView output) {
		int keyCode = mPrefs.getInt(button + player, -1);
		if (keyCode != -1) {
			String label = output.getText().toString();
			if (label.contains(":")) {
				label = label.substring(0, label.indexOf(":"));
			}
			output.setText(label + ": " + String.valueOf(keyCode));
		}
	}

	/**
	 * Prompt the user to specify the controller to modify
	 * 
	 */
	private void selectController() {
		AlertDialog.Builder builder = new AlertDialog.Builder(parentActivity);
		builder.setTitle(getString(R.string.select_controller_title));
		builder.setMessage(getString(R.string.select_controller_message,
				String.valueOf(player.replace("_", ""))));
		builder.setNegativeButton("Cancel",
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						dialog.dismiss();
					}
				});
		builder.setOnKeyListener(new Dialog.OnKeyListener() {
			public boolean onKey(DialogInterface dialog, int keyCode,
					KeyEvent event) {
				mPrefs.edit()
						.putInt("controller" + player, event.getDeviceId())
						.commit();
				dialog.dismiss();
				return true;
			}
		});
		builder.create();
		builder.show();
	}

	private class mapKeyCode extends AlertDialog.Builder {
		private String button;
		private TextView output;
		private boolean isMapping;

		public mapKeyCode(Context c) {
			super(c);
		}

		/**
		 * Prompt the user for the button to be assigned
		 * 
		 * @param string
		 *            The name of the emulator button being defined
		 * @param textview
		 *            The output display for the assigned button value
		 */
		public void intiateSearch(String button, final TextView output) {
			this.button = button;
			this.output = output;
			isMapping = true;
			AlertDialog.Builder builder = new AlertDialog.Builder(
					parentActivity);
			builder.setTitle(getString(R.string.map_keycode_title));
			builder.setMessage(getString(R.string.map_keycode_message,
					button.replace("_", " ")));
			builder.setNegativeButton("Cancel",
					new DialogInterface.OnClickListener() {
						public void onClick(DialogInterface dialog, int which) {
							isMapping = false;
							dialog.dismiss();
						}
			});
			builder.setOnKeyListener(new Dialog.OnKeyListener() {
				public boolean onKey(DialogInterface dialog, int keyCode,
						KeyEvent event) {
					int value = mapButton(keyCode, event);
					isMapping = false;
					dialog.dismiss();
					if (value != -1) {
						String label = output.getText().toString();
						if (label.contains(":")) {
							label = label.substring(0, label.indexOf(":"));
						}
						output.setText(label + ": " + String.valueOf(value));
						return true;
					}
					return false;
				}
			});
			builder.create();
			builder.show();
		}

		/**
		 * Assign the user button to the emulator button
		 * 
		 * @param int
		 *            The keycode generated by the button being assigned
		 * @param keyevent
		 *            The keyevent generated by the button being assigned
		 */
		private int mapButton(int keyCode, KeyEvent event) {
			if (android.os.Build.MODEL.startsWith("R800")) {
				if (keyCode == KeyEvent.KEYCODE_MENU)
					return -1;
			} else {
				if (keyCode == KeyEvent.KEYCODE_BACK)
					return -1;
			}

			mPrefs.edit().putInt(button + player, keyCode).commit();

			return keyCode;
		}

		public boolean dispatchTouchEvent(MotionEvent ev) {
			if (isMapping) {
				if ((ev.getSource() & InputDevice.SOURCE_CLASS_JOYSTICK) != 0) {
					if (ev.getAxisValue(MotionEvent.AXIS_HAT_X) != 0) {

					}
					if (ev.getAxisValue(MotionEvent.AXIS_HAT_Y) != 0) {

					}
					if (ev.getAxisValue(MotionEvent.AXIS_Z) != 0) {

					}
					if (ev.getAxisValue(MotionEvent.AXIS_RZ) != 0) {

					}
					if (ev.getAxisValue(MotionEvent.AXIS_RTRIGGER) != 0) {

					}
					if (ev.getAxisValue(MotionEvent.AXIS_LTRIGGER) != 0) {

					}
					if (ev.getAxisValue(MotionEvent.AXIS_THROTTLE) != 0) {

					}
					if (ev.getAxisValue(MotionEvent.AXIS_BRAKE) != 0) {

					}
					String label = output.getText().toString();
					if (label.contains(":")) {
						label = label.substring(0, label.indexOf(":"));
					}
					output.setText(label + ": "
							+ String.valueOf(ev.getAction()));
				}

			}
			return dispatchTouchEvent(ev);
		}
	}

	private void remKeyCode(final String button, final TextView output) {
		mPrefs.edit().remove(button + player).commit();
		String label = output.getText().toString();
		if (label.contains(":")) {
			label = label.substring(0, label.indexOf(":"));
		}
		output.setText(label);
	}
}
