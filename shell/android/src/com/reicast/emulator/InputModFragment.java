package com.reicast.emulator;

import de.ankri.views.Switch;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.TextView;
import android.widget.CompoundButton.OnCheckedChangeListener;

public class InputModFragment extends Fragment {

	private Activity parentActivity;
	private SharedPreferences mPrefs;
	private Switch switchModifiedLayoutEnabled;

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

		mPrefs = PreferenceManager
				.getDefaultSharedPreferences(parentActivity);
		
		OnCheckedChangeListener modified_layout = new OnCheckedChangeListener() {
			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				mPrefs.edit()
						.putBoolean("modified_key_layout", isChecked)
						.commit();
			}
		};
		switchModifiedLayoutEnabled = (Switch) getView().findViewById(
				R.id.switchModifiedLayoutEnabled);
		boolean layout = mPrefs.getBoolean(
				"modified_key_layout", false);
		if (layout) {
			switchModifiedLayoutEnabled.setChecked(true);
		} else {
			switchModifiedLayoutEnabled.setChecked(false);
		}
		switchModifiedLayoutEnabled.setOnCheckedChangeListener(modified_layout);
		
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {

			final TextView a_button_text = (TextView) getView()
					.findViewById(R.id.a_button_key);
			getKeyCode("a_button", a_button_text);
			Button a_button = (Button) getView()
					.findViewById(R.id.a_button_edit);
			a_button.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("a_button", a_button_text);
	    			} 
			});
			Button a_remove = (Button) getView()
					.findViewById(R.id.remove_a_button);
			a_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("a_button", a_button_text);
	    			} 
			});
			
			final TextView b_button_text = (TextView) getView()
					.findViewById(R.id.b_button_key);
			getKeyCode("b_button", b_button_text);
			Button b_button = (Button) getView()
					.findViewById(R.id.b_button_edit);
			b_button.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("b_button", b_button_text);
	    			} 
			});
			Button b_remove = (Button) getView()
					.findViewById(R.id.remove_b_button);
			b_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("b_button", b_button_text);
	    			} 
			});
			
			final TextView x_button_text = (TextView) getView()
					.findViewById(R.id.x_button_key);
			getKeyCode("x_button", x_button_text);
			Button x_button = (Button) getView()
					.findViewById(R.id.x_button_edit);
			x_button.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("x_button", x_button_text);
	    			} 
			});
			Button x_remove = (Button) getView()
					.findViewById(R.id.remove_x_button);
			x_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("x_button", x_button_text);
	    			} 
			});
			
			final TextView y_button_text = (TextView) getView()
					.findViewById(R.id.y_button_key);
			getKeyCode("y_button", y_button_text);
			Button y_button = (Button) getView()
					.findViewById(R.id.y_button_edit);
			y_button.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("y_button", y_button_text);
	    			} 
			});
			Button y_remove = (Button) getView()
				.findViewById(R.id.remove_y_button);
			y_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("y_button", y_button_text);
	    			} 
			});

			final TextView l_button_text = (TextView) getView()
					.findViewById(R.id.l_button_key);
			getKeyCode("l_button", l_button_text);
			Button l_button = (Button) getView()
					.findViewById(R.id.l_button_edit);
			l_button.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("l_button", l_button_text);
	    			} 
			});
			Button l_remove = (Button) getView()
					.findViewById(R.id.remove_l_button);
			l_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("l_button", l_button_text);
	    			} 
			});

			final TextView r_button_text = (TextView) getView()
					.findViewById(R.id.r_button_key);
			getKeyCode("r_button", r_button_text);
			Button r_button = (Button) getView()
					.findViewById(R.id.r_button_edit);
			r_button.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("r_button", r_button_text);
	    			} 
			});
			Button r_remove = (Button) getView()
					.findViewById(R.id.remove_r_button);
			r_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("r_button", r_button_text);
	    			} 
			});

			final TextView joystick_text = (TextView) getView()
					.findViewById(R.id.joystick_key);
			getKeyCode("joystick", joystick_text);
			Button joystick = (Button) getView()
					.findViewById(R.id.joystick_edit);
			joystick.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("joystick", joystick_text);
	    			} 
			});
			Button joystick_remove = (Button) getView()
					.findViewById(R.id.remove_joystick);
			joystick_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("joystick", joystick_text);
	    			} 
			});
			joystick.setEnabled(false);
			mPrefs.edit().remove("joystick").commit();
			// Still needs better support for identifying the entire stick
			
			final TextView dpad_up_text = (TextView) getView()
					.findViewById(R.id.dpad_up_key);
			getKeyCode("dpad_up", dpad_up_text);
			Button dpad_up = (Button) getView()
					.findViewById(R.id.dpad_up_edit);
			dpad_up.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("dpad_up", dpad_up_text);
	    			} 
			});
			Button up_remove = (Button) getView()
					.findViewById(R.id.remove_dpad_up);
			up_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("dpad_up", dpad_up_text);
	    			} 
			});
			
			final TextView dpad_down_text = (TextView) getView()
					.findViewById(R.id.dpad_down_key);
			getKeyCode("dpad_down", dpad_down_text);
			Button dpad_down = (Button) getView()
					.findViewById(R.id.dpad_down_edit);
			dpad_down.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("dpad_down", dpad_down_text);
	    			} 
			});
			Button down_remove = (Button) getView()
					.findViewById(R.id.remove_dpad_down);
			down_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("dpad_down", dpad_down_text);
	    			} 
			});

			final TextView dpad_left_text = (TextView) getView()
					.findViewById(R.id.dpad_left_key);
			getKeyCode("dpad_left", dpad_left_text);
			Button dpad_left = (Button) getView()
					.findViewById(R.id.dpad_left_edit);
			dpad_left.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("dpad_left", dpad_left_text);
	    			} 
			});
			Button left_remove = (Button) getView()
					.findViewById(R.id.remove_dpad_left);
			left_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("dpad_left", dpad_left_text);
	    			} 
			});
			
			final TextView dpad_right_text = (TextView) getView()
					.findViewById(R.id.dpad_right_key);
			getKeyCode("dpad_right", dpad_right_text);
			Button dpad_right = (Button) getView()
					.findViewById(R.id.dpad_right_edit);
			dpad_right.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("dpad_right", dpad_right_text);
	    			} 
			});
			Button right_remove = (Button) getView()
					.findViewById(R.id.remove_dpad_right);
			right_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("dpad_right", dpad_right_text);
	    			} 
			});
			
			final TextView start_button_text = (TextView) getView()
					.findViewById(R.id.start_button_key);
			getKeyCode("start_button", start_button_text);
			Button start_button = (Button) getView()
					.findViewById(R.id.start_button_edit);
			start_button.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				mapKeyCode("start_button", start_button_text);
	    			} 
			});
			Button start_remove = (Button) getView()
					.findViewById(R.id.remove_start);
			start_remove.setOnClickListener(new View.OnClickListener() {
	    			public void onClick(View v) {
	    				remKeyCode("start_button", start_button_text);
	    			} 
			});
		
		} else {

			switchModifiedLayoutEnabled.setEnabled(false);

		}
	}
	
	private void getKeyCode(final String button, final TextView output) {
		int keyCode = mPrefs.getInt(button, -1);
		if (keyCode != -1) {
			String label = output.getText().toString();
			if (label.contains(":")) {
				label = label.substring(0, label.indexOf(":"));
			}
			output.setText(label + ": " + String.valueOf(keyCode));
		}
	}
	
	private void mapKeyCode(final String button, final TextView output) {
	
		AlertDialog.Builder builder = new AlertDialog.Builder(parentActivity);
		builder.setTitle(getString(R.string.map_keycode_title));
		builder.setMessage(getString(R.string.map_keycode_message, button.replace("_", " ")));
		builder.setNegativeButton("Cancel",
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						dialog.dismiss();
					}
				});
		builder.setOnKeyListener(new Dialog.OnKeyListener() {
			public boolean onKey(DialogInterface dialog, int keyCode,
					KeyEvent event) {
				int value = mapButton(keyCode, event, button);
				dialog.dismiss();
				if (value != -1) {
					String label = output.getText().toString();
					if (label.contains(":")) {
						label = label.substring(0, label.indexOf(":"));
					}
					output.setText(label + ": " + String.valueOf(value));
					return true;
				} else {
					return false;
				}
				
			}
		});
		builder.create();
		builder.show();
	}

	private void remKeyCode(final String button, final TextView output) {
		mPrefs.edit().remove(button).commit();
		String label = output.getText().toString();
		if (label.contains(":")) {
			label = label.substring(0, label.indexOf(":"));
		}
		output.setText(label);
	}

	private int mapButton(int keyCode, KeyEvent event, String button) {
		if (keyCode == KeyEvent.KEYCODE_BACK)
			return -1;

		mPrefs.edit().putInt(button, keyCode).commit();

		return keyCode;
	}
}
