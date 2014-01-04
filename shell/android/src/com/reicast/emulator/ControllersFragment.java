package com.reicast.emulator;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.os.Build;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

@TargetApi(Build.VERSION_CODES.JELLY_BEAN)
public class ControllersFragment extends Fragment {

	private Activity parentActivity;
	private int listenForButton = 0;
	private AlertDialog alertDialogSelectController;
	private SharedPreferences mPrefs;

	// Container Activity must implement this interface
	public interface OnClickListener {
		public void onMainBrowseSelected(String path_entry, boolean games);
	}

	@Override
	public View onCreateView(LayoutInflater inflater, ViewGroup container,
			Bundle savedInstanceState) {
		// Inflate the layout for this fragment
		return inflater.inflate(R.layout.controllers_fragment, container, false);
	}

	@Override
	public void onViewCreated(View view, Bundle savedInstanceState) {
		parentActivity = getActivity();

		mPrefs = PreferenceManager.getDefaultSharedPreferences(parentActivity);

		Button buttonSelectControllerPlayer1 = (Button) getView()
				.findViewById(R.id.buttonSelectControllerPlayer1);
		buttonSelectControllerPlayer1.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				selectController(1);
    			} 
		});
		Button buttonSelectControllerPlayer2 = (Button) getView()
				.findViewById(R.id.buttonSelectControllerPlayer2);
		buttonSelectControllerPlayer2.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				selectController(2);
    			} 
		});
		Button buttonSelectControllerPlayer3 = (Button) getView()
				.findViewById(R.id.buttonSelectControllerPlayer3);
		buttonSelectControllerPlayer3.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				selectController(3);
    			} 
		});
		Button buttonSelectControllerPlayer4 = (Button) getView()
				.findViewById(R.id.buttonSelectControllerPlayer4);
		buttonSelectControllerPlayer4.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				selectController(4);
    			} 
		});

		Button buttonRemoveControllerPlayer1 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer1);
		buttonRemoveControllerPlayer1.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				removeController(1);
    			} 
		});

		Button buttonRemoveControllerPlayer2 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer2);
		buttonRemoveControllerPlayer2.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				removeController(2);
    			} 
		});

		Button buttonRemoveControllerPlayer3 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer3);
		buttonRemoveControllerPlayer3.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				removeController(3);
    			} 
		});

		Button buttonRemoveControllerPlayer4 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer4);
		buttonRemoveControllerPlayer4.setOnClickListener(new View.OnClickListener() {
    			public void onClick(View v) {
				removeController(4);
    			} 
		});

		updateControllers();
	}

	private void updateControllers() {
		String deviceDescriptorPlayer1 = mPrefs.getString("device_descriptor_player_1", null);
		String deviceDescriptorPlayer2 = mPrefs.getString("device_descriptor_player_2", null);
		String deviceDescriptorPlayer3 = mPrefs.getString("device_descriptor_player_3", null);
		String deviceDescriptorPlayer4 = mPrefs.getString("device_descriptor_player_4", null);

		String labelPlayer1 = null, labelPlayer2 = null, labelPlayer3 = null, labelPlayer4 = null;

		for (int devideId : InputDevice.getDeviceIds()) {
			InputDevice dev = InputDevice.getDevice(devideId);
			String descriptor = dev.getDescriptor();

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
		Button buttonRemoveControllerPlayer1 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer1);
		if (deviceDescriptorPlayer1 != null) {
			textViewDeviceDescriptorPlayer1.setText(labelPlayer1);
			buttonRemoveControllerPlayer1.setEnabled(true);
		} else {
			textViewDeviceDescriptorPlayer1.setText(getString(R.string.controller_none));
			buttonRemoveControllerPlayer1.setEnabled(false);
		}

		TextView textViewDeviceDescriptorPlayer2 = (TextView) getView()
				.findViewById(R.id.textViewDeviceDescriptorPlayer2);
		Button buttonRemoveControllerPlayer2 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer2);
		if (deviceDescriptorPlayer2 != null) {
			textViewDeviceDescriptorPlayer2.setText(labelPlayer2);
			buttonRemoveControllerPlayer2.setEnabled(true);
		} else {
			textViewDeviceDescriptorPlayer2.setText(getString(R.string.controller_none));
			buttonRemoveControllerPlayer2.setEnabled(false);
		}

		TextView textViewDeviceDescriptorPlayer3 = (TextView) getView()
				.findViewById(R.id.textViewDeviceDescriptorPlayer3);
		Button buttonRemoveControllerPlayer3 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer3);
		if (deviceDescriptorPlayer3 != null) {
			textViewDeviceDescriptorPlayer3.setText(labelPlayer3);
			buttonRemoveControllerPlayer3.setEnabled(true);
		} else {
			textViewDeviceDescriptorPlayer3.setText(getString(R.string.controller_none));
			buttonRemoveControllerPlayer3.setEnabled(false);
		}

		TextView textViewDeviceDescriptorPlayer4 = (TextView) getView()
				.findViewById(R.id.textViewDeviceDescriptorPlayer4);
		Button buttonRemoveControllerPlayer4 = (Button) getView()
				.findViewById(R.id.buttonRemoveControllerPlayer4);
		if (deviceDescriptorPlayer4 != null) {
			textViewDeviceDescriptorPlayer4.setText(labelPlayer4);
			buttonRemoveControllerPlayer4.setEnabled(true);
		} else {
			textViewDeviceDescriptorPlayer4.setText(getString(R.string.controller_none));
			buttonRemoveControllerPlayer4.setEnabled(false);
		}
	}

	private void selectController(int playerNum) {
		listenForButton = playerNum;

		AlertDialog.Builder builder = new AlertDialog.Builder(parentActivity);
		builder.setTitle(getString(R.string.select_controller_title));
		builder.setMessage(getString(R.string.select_controller_message) + " " + String.valueOf(listenForButton) + ".");
		builder.setNegativeButton("Cancel", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which) {
				listenForButton = 0;
				dialog.dismiss();
			}
		});
		builder.setOnKeyListener(new Dialog.OnKeyListener() {
			@Override
			public boolean onKey(DialogInterface dialog, int keyCode, KeyEvent event) {
				mapDevice(event);
				return true;
			}
		});
		alertDialogSelectController = builder.create();
		alertDialogSelectController.show();
	}

	private void mapDevice(KeyEvent event) {
		String descriptor = InputDevice.getDevice(event.getDeviceId()).getDescriptor();

		if (descriptor == null)
			return;

		String deviceDescriptorPlayer1 = mPrefs.getString("device_descriptor_player_1", null);
		String deviceDescriptorPlayer2 = mPrefs.getString("device_descriptor_player_2", null);
		String deviceDescriptorPlayer3 = mPrefs.getString("device_descriptor_player_3", null);
		String deviceDescriptorPlayer4 = mPrefs.getString("device_descriptor_player_4", null);

		if (descriptor.equals(deviceDescriptorPlayer1) || descriptor.equals(deviceDescriptorPlayer2) ||
		descriptor.equals(deviceDescriptorPlayer3) || descriptor.equals(deviceDescriptorPlayer4)) {
			Toast.makeText(parentActivity, getString(R.string.controller_already_in_use), Toast.LENGTH_SHORT).show();
			return;
		}

		switch(listenForButton) {
			case 0:
				return;
			case 1:
				mPrefs.edit().putString("device_descriptor_player_1", descriptor).commit();
				break;
			case 2:
				mPrefs.edit().putString("device_descriptor_player_2", descriptor).commit();
				break;
			case 3:
				mPrefs.edit().putString("device_descriptor_player_3", descriptor).commit();
				break;
			case 4:
				mPrefs.edit().putString("device_descriptor_player_4", descriptor).commit();
				break;
		}
		
		Log.d("New controller for port " + String.valueOf(listenForButton) + ":", descriptor);

		listenForButton = 0;
		alertDialogSelectController.cancel();
		updateControllers();
	}

	private void removeController(int playerNum) {
		switch(playerNum) {
			case 1:
				mPrefs.edit().putString("device_descriptor_player_1", null).commit();
				break;
			case 2:
				mPrefs.edit().putString("device_descriptor_player_2", null).commit();
				break;
			case 3:
				mPrefs.edit().putString("device_descriptor_player_3", null).commit();
				break;
			case 4:
				mPrefs.edit().putString("device_descriptor_player_4", null).commit();
				break;
		}

		updateControllers();
	}
}
