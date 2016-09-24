package com.reicast.emulator.config;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Vibrator;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.CompoundButton;
import android.widget.CompoundButton.OnCheckedChangeListener;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TableLayout;
import android.widget.TextView;
import android.widget.Toast;

import de.ankri.views.Switch;

import com.bda.controller.Controller;
import com.bda.controller.ControllerListener;
import com.bda.controller.MotionEvent;
import com.bda.controller.StateEvent;
import com.reicast.emulator.MainActivity;
import com.reicast.emulator.R;
import com.reicast.emulator.periph.Gamepad;
import com.reicast.emulator.periph.MOGAInput;

public class InputFragment extends Fragment {

	private Activity parentActivity;
	private int listenForButton = 0;
	private AlertDialog alertDialogSelectController;
	private SharedPreferences sharedPreferences;
	private Switch switchTouchVibrationEnabled;
	private Switch micPluggedIntoFirstController;

	private Gamepad pad = new Gamepad();
	public MOGAInput moga = new MOGAInput();
	Vibrator vib;

	// Container Activity must implement this interface
	public interface OnClickListener {
		void onMainBrowseSelected(String path_entry, boolean games);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.input_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		parentActivity = getActivity();

		moga.onCreate(parentActivity, pad);
		moga.mListener.setPlayerNum(1);

		sharedPreferences = PreferenceManager
				.getDefaultSharedPreferences(parentActivity);

		Config.vibrationDuration = sharedPreferences.getInt(Config.pref_vibrationDuration, 20);
		vib = (Vibrator) parentActivity.getSystemService(Context.VIBRATOR_SERVICE);

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB) {
			ImageView icon_a = (ImageView) getView().findViewById(
					R.id.controller_icon_a);
			icon_a.setAlpha(0.8f);
			ImageView icon_b = (ImageView) getView().findViewById(
					R.id.controller_icon_b);
			icon_b.setAlpha(0.8f);
			ImageView icon_c = (ImageView) getView().findViewById(
					R.id.controller_icon_c);
			icon_c.setAlpha(0.8f);
			ImageView icon_d = (ImageView) getView().findViewById(
					R.id.controller_icon_d);
			icon_d.setAlpha(0.8f);
		}

		Button buttonLaunchEditor = (Button) getView().findViewById(
				R.id.buttonLaunchEditor);
		buttonLaunchEditor.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				Intent inte = new Intent(parentActivity, EditVJoyActivity.class);
				startActivity(inte);
			}
		});

		if (!MainActivity.isBiosExisting(parentActivity) || !MainActivity.isFlashExisting(parentActivity))
			buttonLaunchEditor.setEnabled(false);

		final TextView duration = (TextView) getView().findViewById(R.id.vibDuration_current);
		final LinearLayout vibLay = (LinearLayout) getView().findViewById(R.id.vibDuration_layout);
		final SeekBar vibSeek = (SeekBar) getView().findViewById(R.id.vib_seekBar);

		if (sharedPreferences.getBoolean(Config.pref_touchvibe, true)) {
		  vibLay.setVisibility(View.VISIBLE);
		} else {
		  vibLay.setVisibility(View.GONE);
		}

		duration.setText(String.valueOf(Config.vibrationDuration +  " ms"));
		vibSeek.setProgress(Config.vibrationDuration);

		vibSeek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
		    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
		      duration.setText(String.valueOf(progress + 5 + " ms"));
		    }

		    public void onStartTrackingTouch(SeekBar seekBar) {
		      // TODO Auto-generated method stub
		    }

		    public void onStopTrackingTouch(SeekBar seekBar) {
			int progress = seekBar.getProgress() + 5;
			sharedPreferences.edit().putInt(Config.pref_vibrationDuration, progress).commit();
			Config.vibrationDuration = progress;
			vib.vibrate(progress);
		    }
		});

		OnCheckedChangeListener touch_vibration = new OnCheckedChangeListener() {
			public void onCheckedChanged(CompoundButton buttonView,
					boolean isChecked) {
				sharedPreferences.edit()
						.putBoolean(Config.pref_touchvibe, isChecked).commit();
				vibLay.setVisibility( isChecked ? View.VISIBLE : View.GONE );
			}
		};
		switchTouchVibrationEnabled = (Switch) getView().findViewById(
				R.id.switchTouchVibrationEnabled);
		boolean vibrate = sharedPreferences.getBoolean(Config.pref_touchvibe, true);
		if (vibrate) {
			switchTouchVibrationEnabled.setChecked(true);
		} else {
			switchTouchVibrationEnabled.setChecked(false);
		}
		switchTouchVibrationEnabled.setOnCheckedChangeListener(touch_vibration);

		micPluggedIntoFirstController = (Switch) getView().findViewById(
				R.id.micInPort2);
		boolean micPluggedIn = sharedPreferences.getBoolean(Config.pref_mic,
				false);
		micPluggedIntoFirstController.setChecked(micPluggedIn);
		if (getActivity().getPackageManager().hasSystemFeature(
				PackageManager.FEATURE_MICROPHONE)) {
			// Microphone is present on the device
			micPluggedIntoFirstController
					.setOnCheckedChangeListener(new OnCheckedChangeListener() {
						public void onCheckedChanged(CompoundButton buttonView,
								boolean isChecked) {
							sharedPreferences.edit()
									.putBoolean(Config.pref_mic, isChecked)
									.commit();
						}
					});
		} else {
			micPluggedIntoFirstController.setEnabled(false);
		}

		Button buttonKeycodeEditor = (Button) getView().findViewById(
				R.id.buttonKeycodeEditor);
		buttonKeycodeEditor.setOnClickListener(new View.OnClickListener() {
			public void onClick(View v) {
				InputModFragment inputModFrag = new InputModFragment();
				getActivity()
						.getSupportFragmentManager()
						.beginTransaction()
						.replace(R.id.fragment_container, inputModFrag,
								"INPUT_MOD_FRAG").addToBackStack(null).commit();
			}
		});

		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {

			Button buttonSelectControllerPlayer1 = (Button) getView()
					.findViewById(R.id.buttonSelectControllerPlayer1);
			buttonSelectControllerPlayer1
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							selectController(1);
						}
					});
			Button buttonSelectControllerPlayer2 = (Button) getView()
					.findViewById(R.id.buttonSelectControllerPlayer2);
			buttonSelectControllerPlayer2
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							selectController(2);
						}
					});
			Button buttonSelectControllerPlayer3 = (Button) getView()
					.findViewById(R.id.buttonSelectControllerPlayer3);
			buttonSelectControllerPlayer3
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							selectController(3);
						}
					});
			Button buttonSelectControllerPlayer4 = (Button) getView()
					.findViewById(R.id.buttonSelectControllerPlayer4);
			buttonSelectControllerPlayer4
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							selectController(4);
						}
					});

			Button buttonRemoveControllerPlayer1 = (Button) getView()
					.findViewById(R.id.buttonRemoveControllerPlayer1);
			buttonRemoveControllerPlayer1
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							removeController(1);
						}
					});

			Button buttonRemoveControllerPlayer2 = (Button) getView()
					.findViewById(R.id.buttonRemoveControllerPlayer2);
			buttonRemoveControllerPlayer2
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							removeController(2);
						}
					});

			Button buttonRemoveControllerPlayer3 = (Button) getView()
					.findViewById(R.id.buttonRemoveControllerPlayer3);
			buttonRemoveControllerPlayer3
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							removeController(3);
						}
					});

			Button buttonRemoveControllerPlayer4 = (Button) getView()
					.findViewById(R.id.buttonRemoveControllerPlayer4);
			buttonRemoveControllerPlayer4
					.setOnClickListener(new View.OnClickListener() {
						public void onClick(View v) {
							removeController(4);
						}
					});

			updateControllers();

		} else {

			TableLayout input_devices = (TableLayout) parentActivity
					.findViewById(R.id.input_devices);
			input_devices.setVisibility(View.GONE);

		}

		updateVibration();
	}

	private void updateVibration() {
		boolean touchVibrationEnabled = sharedPreferences.getBoolean(
				Config.pref_touchvibe, true);
		switchTouchVibrationEnabled.setChecked(touchVibrationEnabled);
	}

	private void updateControllers() {
		String deviceDescriptorPlayer1 = sharedPreferences.getString(
				Gamepad.pref_player1, null);
		String deviceDescriptorPlayer2 = sharedPreferences.getString(
				Gamepad.pref_player2, null);
		String deviceDescriptorPlayer3 = sharedPreferences.getString(
				Gamepad.pref_player3, null);
		String deviceDescriptorPlayer4 = sharedPreferences.getString(
				Gamepad.pref_player4, null);

		String labelPlayer1 = null, labelPlayer2 = null, labelPlayer3 = null, labelPlayer4 = null;

		for (int devideId : InputDevice.getDeviceIds()) {
			InputDevice dev = InputDevice.getDevice(devideId);
			String descriptor = null;
			if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
				descriptor = dev.getDescriptor();
			} else {
				descriptor = dev.getName();
			}

			if (descriptor != null) {
				if (descriptor.equals(deviceDescriptorPlayer1))
					labelPlayer1 = dev.getName() + " (" + descriptor + ")";
				else if (descriptor.equals(deviceDescriptorPlayer2))
					labelPlayer2 = dev.getName() + " (" + descriptor + ")";
				else if (descriptor.equals(deviceDescriptorPlayer3))
					labelPlayer3 = dev.getName() + " (" + descriptor + ")";
				else if (descriptor.equals(deviceDescriptorPlayer4))
					labelPlayer4 = dev.getName() + " (" + descriptor + ")";
			}
		}

		TextView textViewDeviceDescriptorPlayer1 = (TextView) getView()
				.findViewById(R.id.textViewDeviceDescriptorPlayer1);
		Button buttonRemoveControllerPlayer1 = (Button) getView().findViewById(
				R.id.buttonRemoveControllerPlayer1);
		if (labelPlayer1 != null) {
			textViewDeviceDescriptorPlayer1.setText(labelPlayer1);
			buttonRemoveControllerPlayer1.setEnabled(true);
		} else {
			if (deviceDescriptorPlayer1 != null) {
				textViewDeviceDescriptorPlayer1
						.setText(getString(R.string.controller_not_connected)
								+ " (" + deviceDescriptorPlayer1 + ")");
				buttonRemoveControllerPlayer1.setEnabled(true);
			} else {
				textViewDeviceDescriptorPlayer1
						.setText(R.string.controller_none_selected);
				buttonRemoveControllerPlayer1.setEnabled(false);
			}
		}

		TextView textViewDeviceDescriptorPlayer2 = (TextView) getView()
				.findViewById(R.id.textViewDeviceDescriptorPlayer2);
		Button buttonRemoveControllerPlayer2 = (Button) getView().findViewById(
				R.id.buttonRemoveControllerPlayer2);
		if (labelPlayer2 != null) {
			textViewDeviceDescriptorPlayer2.setText(labelPlayer2);
			buttonRemoveControllerPlayer2.setEnabled(true);
		} else {
			if (deviceDescriptorPlayer2 != null) {
				textViewDeviceDescriptorPlayer2
						.setText(getString(R.string.controller_not_connected)
								+ " (" + deviceDescriptorPlayer2 + ")");
				buttonRemoveControllerPlayer2.setEnabled(true);
			} else {
				textViewDeviceDescriptorPlayer2
						.setText(R.string.controller_none_selected);
				buttonRemoveControllerPlayer2.setEnabled(false);
			}
		}

		TextView textViewDeviceDescriptorPlayer3 = (TextView) getView()
				.findViewById(R.id.textViewDeviceDescriptorPlayer3);
		Button buttonRemoveControllerPlayer3 = (Button) getView().findViewById(
				R.id.buttonRemoveControllerPlayer3);
		if (labelPlayer3 != null) {
			textViewDeviceDescriptorPlayer3.setText(labelPlayer3);
			buttonRemoveControllerPlayer3.setEnabled(true);
		} else {
			if (deviceDescriptorPlayer3 != null) {
				textViewDeviceDescriptorPlayer3
						.setText(getString(R.string.controller_not_connected)
								+ " (" + deviceDescriptorPlayer3 + ")");
				buttonRemoveControllerPlayer3.setEnabled(true);
			} else {
				textViewDeviceDescriptorPlayer3
						.setText(R.string.controller_none_selected);
				buttonRemoveControllerPlayer3.setEnabled(false);
			}
		}

		TextView textViewDeviceDescriptorPlayer4 = (TextView) getView()
				.findViewById(R.id.textViewDeviceDescriptorPlayer4);
		Button buttonRemoveControllerPlayer4 = (Button) getView().findViewById(
				R.id.buttonRemoveControllerPlayer4);
		if (labelPlayer4 != null) {
			textViewDeviceDescriptorPlayer4.setText(labelPlayer4);
			buttonRemoveControllerPlayer4.setEnabled(true);
		} else {
			if (deviceDescriptorPlayer4 != null) {
				textViewDeviceDescriptorPlayer4
						.setText(getString(R.string.controller_not_connected)
								+ " (" + deviceDescriptorPlayer4 + ")");
				buttonRemoveControllerPlayer4.setEnabled(true);
			} else {
				textViewDeviceDescriptorPlayer4
						.setText(R.string.controller_none_selected);
				buttonRemoveControllerPlayer4.setEnabled(false);
			}
		}
	}

	private void selectController(int playerNum) {
		listenForButton = playerNum;

		AlertDialog.Builder builder = new AlertDialog.Builder(parentActivity);
		builder.setTitle(R.string.select_controller_title);
		builder.setMessage(getString(R.string.select_controller_message,
				String.valueOf(listenForButton)));
		builder.setPositiveButton(R.string.cancel,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						listenForButton = 0;
						dialog.dismiss();
					}
				});
		builder.setNegativeButton(R.string.manual,
				new DialogInterface.OnClickListener() {
					public void onClick(DialogInterface dialog, int which) {
						InputModFragment inputModFrag = new InputModFragment();
						Bundle args = new Bundle();
						args.putInt("portNumber", listenForButton - 1);
						inputModFrag.setArguments(args);
						listenForButton = 0;
						getActivity()
								.getSupportFragmentManager()
								.beginTransaction()
								.replace(R.id.fragment_container, inputModFrag,
										"INPUT_MOD_FRAG").addToBackStack(null)
								.commit();
						dialog.dismiss();
					}
				});
		builder.setOnKeyListener(new Dialog.OnKeyListener() {
			public boolean onKey(DialogInterface dialog, int keyCode,
					KeyEvent event) {
				return mapDevice(keyCode, event);
			}
		});
		alertDialogSelectController = builder.create();
		alertDialogSelectController.show();
	}

	private boolean mapDevice(int keyCode, KeyEvent event) {
		if (keyCode == KeyEvent.KEYCODE_MENU
				|| keyCode == KeyEvent.KEYCODE_VOLUME_UP
				|| keyCode == KeyEvent.KEYCODE_VOLUME_DOWN)
			return false;
		if (!pad.IsXperiaPlay() && keyCode == KeyEvent.KEYCODE_BACK)
			return false;

		String descriptor = null;
		if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN) {
			if (pad.isActiveMoga[listenForButton]) {
				MogaListener config = new MogaListener(listenForButton);
				moga.mController.setListener(config, new Handler());
				descriptor = config.getController();
			}
			descriptor = InputDevice.getDevice(event.getDeviceId())
					.getDescriptor();
		} else {
			descriptor = InputDevice.getDevice(event.getDeviceId()).getName();
		}

		if (descriptor == null)
			return false;

		String deviceDescriptorPlayer1 = sharedPreferences.getString(
				Gamepad.pref_player1, null);
		String deviceDescriptorPlayer2 = sharedPreferences.getString(
				Gamepad.pref_player2, null);
		String deviceDescriptorPlayer3 = sharedPreferences.getString(
				Gamepad.pref_player3, null);
		String deviceDescriptorPlayer4 = sharedPreferences.getString(
				Gamepad.pref_player4, null);

		if (descriptor.equals(deviceDescriptorPlayer1)
				|| descriptor.equals(deviceDescriptorPlayer2)
				|| descriptor.equals(deviceDescriptorPlayer3)
				|| descriptor.equals(deviceDescriptorPlayer4)) {
			Toast.makeText(parentActivity, R.string.controller_already_in_use,
					Toast.LENGTH_SHORT).show();
			return true;
		}

		switch (listenForButton) {
		case 0:
			return false;
		case 1:
			sharedPreferences.edit()
					.putString(Gamepad.pref_player1, descriptor).commit();
			break;
		case 2:
			sharedPreferences.edit()
					.putString(Gamepad.pref_player2, descriptor).commit();
			break;
		case 3:
			sharedPreferences.edit()
					.putString(Gamepad.pref_player3, descriptor).commit();
			break;
		case 4:
			sharedPreferences.edit()
					.putString(Gamepad.pref_player4, descriptor).commit();
			break;
		}

		Log.d("New controller for port " + listenForButton + ":", descriptor);

		listenForButton = 0;
		alertDialogSelectController.cancel();
		updateControllers();

		return true;
	}

	private void removeController(int playerNum) {
		switch (playerNum) {
		case 1:
			sharedPreferences.edit().putString(Gamepad.pref_player1, null)
					.commit();
			break;
		case 2:
			sharedPreferences.edit().putString(Gamepad.pref_player2, null)
					.commit();
			break;
		case 3:
			sharedPreferences.edit().putString(Gamepad.pref_player3, null)
					.commit();
			break;
		case 4:
			sharedPreferences.edit().putString(Gamepad.pref_player4, null)
					.commit();
			break;
		}

		updateControllers();
	}

	private final class MogaListener implements ControllerListener {

		private int playerNum;
		private String controllerId;

		public MogaListener(int playerNum) {
			this.playerNum = playerNum;
		}

		public void onKeyEvent(com.bda.controller.KeyEvent event) {
			controllerId = String.valueOf(event.getControllerId());
		}

		public void onMotionEvent(MotionEvent arg0) {

		}

		public String getController() {
			return controllerId;
		}

		public void onStateEvent(StateEvent event) {
			if (event.getState() == StateEvent.STATE_CONNECTION &&
			    event.getAction() == MOGAInput.ACTION_CONNECTED) {

				int mControllerVersion = moga.mController
				        .getState(Controller.STATE_CURRENT_PRODUCT_VERSION);

				if (mControllerVersion == Controller.ACTION_VERSION_MOGA ||
				    mControllerVersion == Controller.ACTION_VERSION_MOGAPRO) {
					pad.isActiveMoga[playerNum] = true;
				}
			}
		}
	}
}
